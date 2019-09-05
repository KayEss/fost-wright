/**
    Copyright 2017-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#include <wright/configuration.hpp>
#include <wright/exception.hpp>
#include <wright/exec.capacity.hpp>
#include <wright/exec.childproc.hpp>

#include <fost/log>

#include <boost/asio/spawn.hpp>

#include <iostream>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>


using namespace std::literals::chrono_literals;


namespace {


    fostlib::performance p_crashes(wright::c_exec_helper, "child", "crashed");
    fostlib::performance p_resent(wright::c_exec_helper, "jobs", "resent");


}


void wright::fork_worker() {
    /// Read in the command line we're going to run
    std::vector<fostlib::string> argvs;
    argvs.reserve(c_exec.value().size());
    std::vector<char const *> argv;
    for (const auto &item : c_exec.value()) {
        auto str = fostlib::coerce<fostlib::string>(item);
        argv.push_back(str.shrink_to_fit());
        argvs.push_back(str);
    }
    argv.push_back(nullptr);
    /// Fork and loop until done
    while (true) {
        int pid = ::fork();
        if (pid < 0) {
            fostlib::log::critical(c_exec_helper)("", "Fork failed")(
                    "parent", ::getpid());
            fostlib::log::flush();
            std::exit(5);
        } else if (pid == 0) {
            ::execvp(argv.front(), const_cast<char *const *>(argv.data()));
            std::cerr << "Child process failed to start:";
            for (auto part : argv)
                if (part) std::cerr << " '" << part << '\'';
            std::cerr << std::endl;
            const char dead[] = "x";
            ::write(wright::c_resend_fd.value(), dead, 1u);
            return;
        } else {
            fostlib::log::info(c_exec_helper)("", "Started child process")(
                    "pid", pid)("resend-fd", wright::c_resend_fd.value());
            int status;
            waitpid(pid, &status, 0);
            if (WEXITSTATUS(status) == 0) {
                fostlib::log::info(c_exec_helper)("", "Child completed")(
                        "pid", pid);
                return;
            }
            fostlib::log::warning(c_exec_helper)(
                    "", "Child errored -- requesting resend")("pid", pid)(
                    "status", status);
            /// Send a resend instruction to the parent process
            /// Write a single byte into the pipe
            const char resend[] = "r";
            ::write(wright::c_resend_fd.value(), resend, 1u);
        }
    }
}


/*
 * wright::childproc::counter_store
 */


wright::childproc::counter_store::counter_store(std::size_t n)
: reference(c_exec_helper, std::to_string(n)),
  accepted(reference, "jobs", "accepted"),
  completed(reference, "jobs", "completed") {}


/*
 * wright::childproc
 */


wright::childproc::childproc(std::size_t n, const char *command)
: number(n),
  counters(new counter_store{n}),
  argx(fostlib::json::unparse(c_exec.value(), false)),
  backchannel_fd(std::to_string(::dup(resend.child()))),
  commands(buffer_size) {
    argv.push_back(command);
    argv.push_back("--child");
    argv.push_back(counters->reference.name()); // child number
    argv.push_back("-b"); // No banner
    argv.push_back("false");
    argv.push_back("-rfd"); // Rsend FD number
    argv.push_back(backchannel_fd.c_str()); // holder for the FD number
    argv.push_back("-x"); // Program arguments
    argv.push_back(argx.shrink_to_fit());
    argv.push_back(nullptr);
}


wright::childproc::childproc(childproc &&p)
: stdin(std::move(p.stdin)),
  stdout(std::move(p.stdout)),
  stderr(std::move(p.stderr)),
  resend(std::move(p.resend)),
  number(p.number),
  counters(std::move(p.counters)),
  argv(std::move(p.argv)),
  pid(p.pid),
  commands(std::move(p.commands)) {}


namespace {
    const struct newl {
        newl() { buffer.sputc('\n'); }
        boost::asio::streambuf buffer;
    } newline;
}


void wright::childproc::write(
        boost::asio::io_service &ios,
        const std::string &command,
        boost::asio::yield_context yield) {
    std::array<boost::asio::streambuf::const_buffers_type, 2> buffer{
            {{command.data(), command.size()}, newline.buffer.data()}};
    boost::system::error_code error;
    boost::asio::async_write(stdin.parent(ios), buffer, yield[error]);
    if (error) {
        fostlib::log::critical(counters->reference)(
                "", "Error writing to pipe for child")("error", error);
        fostlib::log::flush();
        std::exit(7);
    }
}


std::string wright::childproc::read(
        boost::asio::io_service &ios,
        boost::asio::streambuf &buffer,
        boost::asio::yield_context yield) {
    boost::system::error_code error;
    auto bytes = boost::asio::async_read_until(
            stdout.parent(ios), buffer, '\n', yield[error]);
    if (error) {
        std::cerr << pid << " read error: " << error << std::endl;
    } else if (bytes) {
        /// For some reason I totally fail to understand we can actually
        /// end up reading a sequence of zero byte values at the start
        /// of the line. These need to be filtered out as they appear to
        /// be totally spurious. They fail the string comparison when we
        /// check if the line read matches the job we sent.
        ///
        /// At least that can happen if we give the buffer to a
        /// `std::istream`. It doesn't seem to happen when pulling the
        /// characters off one at a time.
        std::string line;
        line.reserve(bytes);
        for (; bytes; --bytes) {
            char next = buffer.sbumpc();
            if (next == 0 || next == '\n') {
                //                 std::cerr << pid << " dropped " << int(next)
                //                 << std::endl;
            } else {
                line += next;
            }
        }
        return line;
    }
    return std::string();
}


void wright::childproc::handle_child_requests(
        boost::asio::io_service &ctrlios,
        capacity &cap,
        boost::asio::yield_context yield) {
    boost::asio::streambuf buffer;
    boost::system::error_code error;
    while (resend.parent(ctrlios).is_open()) {
        auto bytes = boost::asio::async_read(
                resend.parent(ctrlios), buffer,
                boost::asio::transfer_exactly(1), yield[error]);
        if (bytes && not error) {
            switch (char byte = buffer.sbumpc()) {
            default:
                fostlib::log::warning(counters->reference)(
                        "",
                        "Got unknown resend request byte")("byte", int(byte));
                break;
            case 'r':
                ++p_crashes;
                if (cap.input_complete.load() && commands.empty()) {
                    fostlib::log::error(
                            counters->reference,
                            "Child had to re-start worker, but all work is "
                            "done");
                    /// The work is done, but the child seems
                    /// to be looping in an error. Kill it
                    ::kill(pid, SIGTERM);
                } else {
                    auto logger = fostlib::log::debug(counters->reference);
                    logger("", "Resending jobs for child")("child", pid)(
                            "job", "count", commands.size());
                    fostlib::json jobs;
                    for (auto &job : commands) {
                        fostlib::push_back(jobs, job.command);
                        write(ctrlios, job.command, yield);
                        ++p_resent;
                    }
                    if (jobs.size()) logger("job", "list", jobs);
                }
                break;
            case 'x': {
                fostlib::log::critical(
                        counters->reference,
                        "Child process failed to execvp worker process");
                fostlib::log::flush();
                std::exit(10);
            }
            case '{': {
                fostlib::string jsonstr{"{"};
                auto bytes = boost::asio::async_read_until(
                        resend.parent(ctrlios), buffer, 0, yield[error]);
                if (not error) {
                    for (; bytes; --bytes) {
                        char next = buffer.sbumpc();
                        if (next) jsonstr += next;
                    }
                }
                auto parsed = fostlib::json::parse(jsonstr);
                try {
                    fostlib::log::log(
                            fostlib::log::message(counters->reference, parsed));
                } catch (fostlib::exceptions::exception &e) {
                    throw fostlib::exceptions::not_implemented(
                            __func__,
                            "Could not turn create a log message from JSON",
                            parsed);
                }
                break;
            }
            }
        } else {
            fostlib::log::critical(c_exec_helper)(
                    "", "Error reading from child pipe")("error", error)(
                    "bytes", bytes);
            fostlib::log::flush();
            std::exit(8);
        }
    }
}


void wright::childproc::drain_stderr(
        boost::asio::io_service &auxios, boost::asio::yield_context yield) {
    boost::asio::streambuf buffer;
    fostlib::string line;
    while (stderr.parent(auxios).is_open()) {
        boost::system::error_code error;
        auto bytes = boost::asio::async_read_until(
                stderr.parent(auxios), buffer, '\n', yield[error]);
        if (error) {
            fostlib::log::error(counters->reference)(
                    "", "Error reading child stderr")("error", error)(
                    "bytes", bytes);
            return;
        } else if (bytes) {
            while (bytes--) {
                char next = buffer.sbumpc();
                if (next != '\n') line += next;
            }
            auto parsed = fostlib::json::parse(line, fostlib::json(line));
            fostlib::log::warning(counters->reference)("", "Child stderr")(
                    "child", pid)("stderr", parsed);
            line.clear();
        }
    }
}


void wright::childproc::handle_stdout(
        boost::asio::io_service &ctrlios,
        boost::asio::yield_context yield,
        child_pool &pool,
        std::function<void(const std::string &)> job_done) {
    boost::asio::streambuf buffer;
    while (stdout.parent(ctrlios).is_open()) {
        boost::system::error_code error;
        auto ret = read(ctrlios, buffer, yield[error]);
        if (not error && not ret.empty() && commands.size()
            && ret == commands.front().command) {
            //             ++(counters->completed);
            pool.job_times.record(commands.front().time);
            commands.pop_front();
            if (commands.size()) commands.front().time.reset();
            auto logger = fostlib::log::debug(c_exec_helper);
            logger("", "Got result from child")("child", pid)(
                    "result", ret.c_str());
            job_done(ret);
        } else if (error) {
            fostlib::log::warning(c_exec_helper)(
                    "", "Read error from child stdout")("child", pid)(
                    "error", error);
        } else if (not ret.empty()) {
            if (commands.empty()) {
                fostlib::log::debug(c_exec_helper)(
                        "", "Ignored line from child")(
                        "input", "string",
                        ret.c_str())("input", "size", ret.size());
            } else {
                auto &command = commands.front().command;
                fostlib::log::debug(c_exec_helper)(
                        "", "Ignored line from child")(
                        "input", "string",
                        ret.c_str())("input", "size", ret.size())(
                        "expected", "string",
                        command.c_str())("expected", "size", command.size())(
                        "match", ret == command);
            }
        }
    }
    fostlib::log::info(c_exec_helper)(
            "", "Child done because it's pipe closed")("pid", pid);
}


void wright::childproc::close() {
    stdin.close();
    stdout.close();
    stderr.close();
    resend.close();
}


/*
 * wright::child_pool
 */


namespace {


    /// Pipe used to signal the event loop that a child has died
    std::unique_ptr<wright::pipe_out> sigchild;

    void sigchild_handler(int sig) {
        const char child[] = "c";
        /// Write a single byte into the pipe
        ::write(sigchild->child(), child, 1u);
    }
    void attach_sigchild_handler() {
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0; // No options needed
        sa.sa_handler = sigchild_handler;
        if (::sigaction(SIGCHLD, &sa, nullptr) < 0) {
            throw fostlib::exceptions::not_implemented(
                    __func__, "Failed to establish signal handler for SIGCHLD");
        }
    }
    auto sigchild_reactor(
            boost::asio::io_service &auxios, wright::child_pool &pool) {
        return [&](auto yield) {
            boost::asio::streambuf buffer;
            boost::system::error_code error;
            while (sigchild->parent(auxios).is_open()) {
                const auto bytes = boost::asio::async_read(
                        sigchild->parent(auxios), buffer,
                        boost::asio::transfer_exactly(1), yield[error]);
                if (bytes && not error) {
                    for (auto remaining{bytes}; remaining; --remaining) {
                        switch (char byte = buffer.sbumpc()) {
                        default:
                            std::cerr << "Got signal byte " << int(byte)
                                      << std::endl;
                            break;
                        case 'c':
                            for (auto &child : pool.children) {
                                int status{};
                                if (child.pid
                                    == waitpid(child.pid, &status, WNOHANG)) {
                                    if (child.commands.empty()) {
                                        fostlib::log::warning(
                                                child.counters->reference)(
                                                "",
                                                "Immediate child has died with "
                                                "an empty command queue. "
                                                "Leaving child not without "
                                                "restarting")(
                                                "child", "pid", child.pid);
                                    } else {
                                        const bool wifexited =
                                                WIFEXITED(status);
                                        const auto wexitstatus = wifexited
                                                ? fostlib::json(
                                                        WEXITSTATUS(status))
                                                : fostlib::json();
                                        const bool wifsignaled =
                                                WIFSIGNALED(status);
                                        const auto wtermsig = wifsignaled
                                                ? fostlib::json(
                                                        WTERMSIG(status))
                                                : fostlib::json();
                                        const auto wstopsig = wifsignaled
                                                ? fostlib::json(
                                                        WSTOPSIG(status))
                                                : fostlib::json();
                                        fostlib::log::critical(
                                                wright::c_exec_helper)(
                                                "",
                                                "Immediate child dead -- Time "
                                                "to PANIC")(
                                                "child", "number",
                                                child.number)(
                                                "child", "pid", child.pid)(
                                                "status", "WIFEXITED",
                                                wifexited)(
                                                "status", "WIFSIGNALED",
                                                wifsignaled)(
                                                "status", "WEXITSTATUS",
                                                wexitstatus)(
                                                "status", "WTERMSIG", wtermsig)(
                                                "status", "WSTOPSIG", wstopsig);
                                        fostlib::log::flush();
                                        std::exit(4);
                                    }
                                }
                            }
                        }
                    }
                } else {
                    fostlib::log::critical(wright::c_exec_helper)(
                            "",
                            "Error reading bytes from SIGCHLD handler pipe")(
                            "error", error)("bytes", bytes);
                    fostlib::log::flush();
                    std::exit(9);
                }
            }
        };
    }


}


wright::child_pool::child_pool(std::size_t number, const char *command)
: job_times(5ms, 1.2, 200, 24ms) {
    children.reserve(number);
    /// For each child go through and fork and execvp it
    for (std::size_t child{}; child < number; ++child) {
        children.emplace_back(child + 1, command);
        children[child].fork_exec([&]() {
            for (auto &child : children) { child.close(); }
        });
    }
    /// Now that we have children, we're going to want to deal with
    /// their deaths
    sigchild = std::make_unique<wright::pipe_out>();
    attach_sigchild_handler();
}

void wright::child_pool::sigchild_handling(boost::asio::io_service &ios) {
    /// Process the other end of the signal handler pipe
    boost::asio::spawn(ios, exception_decorator(sigchild_reactor(ios, *this)));
}
