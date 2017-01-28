/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exception.hpp>
#include <wright/exec.hpp>
#include <wright/exec.childproc.hpp>

#include <f5/threading/boost-asio.hpp>
#include <fost/cli>
#include <fost/counter>
#include <fost/unicode>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <future>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>


namespace {


    fostlib::performance p_accepted(wright::c_exec_helper, "jobs", "accepted");
    fostlib::performance p_completed(wright::c_exec_helper, "jobs", "completed");

    /// Pipe used to signall the event loop that a child has died
    std::unique_ptr<wright::pipe_out> sigchild;

    void sigchild_handler(int sig) {
        const char child[] = "c";
        /// Write a single byte into the pipe
        ::write(sigchild->child(), child, 1u);
    }
    void attach_sigchild_handler() {
        struct sigaction sa;
        ::sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0; // No options needed
        sa.sa_handler = sigchild_handler;
        if ( ::sigaction(SIGCHLD, &sa, nullptr) < 0 ) {
            throw fostlib::exceptions::not_implemented(__func__,
                "Failed to establish signal handler for SIGCHLD");
        }
    }


}


void wright::exec_helper(std::ostream &out) {
    /// The parent sets up the communications redirects etc and spawns
    /// child processes
    std::vector<wright::childproc> children;

    /// Set up the argument vector for the child
    const auto command = wright::c_exec.value();
    std::vector<char const *> argv;
    argv.push_back(command.c_str());
    if ( command == argv[0] ) {
        argv.push_back("-c");
        argv.push_back(nullptr); // holder for the child number
        argv.push_back("-b"); // No banner
        argv.push_back("false");
        argv.push_back("-rfd"); // Rsend FD number
        argv.push_back(nullptr); // holder for the FD number
    }
    argv.push_back(nullptr);

    /// For each child go through and fork and execvpe it
    for ( std::size_t child{}; child < wright::c_children.value(); ++child ) {
        children.emplace_back(child + 1);
        children[child].argv = argv;
        auto child_number = std::to_string(child + 1);
        children[child].argv[2] = child_number.c_str();
        auto resend_fd = std::to_string(::dup(children[child].resend.child()));
        children[child].argv[6] = resend_fd.c_str();
        children[child].fork_exec([&]() { children.clear(); });
    }
    /// Now that we have children, we're going to want to deal with
    /// their deaths
    sigchild = std::make_unique<wright::pipe_out>();
    attach_sigchild_handler();

    /// Set up a promise that we're going to wait to finish on
    std::promise<void> blocker;
    /// Flag to tell us if the input stream has completed (closed)
    /// yet and if the blocker has been signalled
    bool in_closed{false}, signalled{false};

    /// Add in some tracking for now so we can make sure that we have all
    /// of the jobs done
    std::set<std::string> requested, completed;

    /// Stop on exception, one thread. We want one thread here so
    /// we don't have to worry about thread synchronisation when
    /// running code in the reactor
    f5::boost_asio::reactor_pool coordinator([]() { return false; }, 1u);
    auto &ios = coordinator.get_io_service();

    /// All the children need a presence in the reactor pool for
    /// their process requirement
    for ( auto &child : children ) {
        /// Each child will wait on the command, then write it
        /// the pipe for the process to execute and wait on the result
        auto *cp = &child;
        boost::asio::spawn(ios, exception_decorator([&, cp](auto yield) {
                /// Wait for a job to be queued in their process ID
                boost::asio::streambuf buffer;
                while ( cp->stdout.parent(ios).is_open() ) {
                    boost::system::error_code error;
                    auto ret = cp->read(ios, buffer, yield[error]);
                    if ( not error && not ret.empty() &&
                            cp->commands.size() &&
                            ret == cp->commands.front().first )
                    {
                        ++p_completed;
                        cp->commands.pop_front();
                        auto logger = fostlib::log::debug(c_exec_helper);
                        logger
                            ("", "Got result from child")
                            ("child", cp->pid)
                            ("result", ret.c_str());
                        out << ret << std::endl;
                        completed.insert(ret);
                        if ( in_closed && not signalled ) {
                            const auto working = std::count_if(children.begin(), children.end(),
                                    [](const auto &c) { return not c.commands.empty(); });
                            logger("still-working", working);
                            if ( working == 0 ) {
                                signalled = true;
                                blocker.set_value();
                            }
                        }
                    } else if ( error ) {
                        fostlib::log::warning(c_exec_helper)
                            ("", "Read error from child stdout")
                            ("child", cp->pid)
                            ("error", error);
                    } else if ( not ret.empty() ) {
                        auto &command = cp->commands.front().first;
                        fostlib::log::debug(c_exec_helper)
                            ("", "Ignored line from child")
                            ("input", "string", ret.c_str())
                            ("input", "size", ret.size())
                            ("expected", "string", command.c_str())
                            ("expected", "size", command.size())
                            ("match", ret == command);
                    }
                }
                fostlib::log::info(c_exec_helper)
                    ("", "Child done")
                    ("pid", cp->pid);
            }));
        /// We also need to watch for a resend alert from the child process
        boost::asio::spawn(ios, exception_decorator([&, cp](auto yield) {
            boost::asio::streambuf buffer;
            boost::system::error_code error;
            while ( cp->resend.parent(ios).is_open() ) {
                auto bytes = boost::asio::async_read(cp->resend.parent(ios), buffer,
                    boost::asio::transfer_exactly(1), yield);
                if ( bytes ) {
                    switch ( char byte = buffer.sbumpc() ) {
                    default:
                        fostlib::log::warning(cp->reference)
                            ("", "Got unkown resend request byte")
                            ("byte", int(byte));
                        break;
                    case 'r': {
                        if ( signalled ) {
                            /// The work is done, but the child seems
                            /// to be looping in an error. Kill it
                            ::kill(cp->pid, SIGINT);
                        } else {
                            auto logger = fostlib::log::debug(cp->reference);
                            logger
                                ("", "Resending jobs for child")
                                ("child", cp->pid)
                                ("job", "count", cp->commands.size());
                            fostlib::json jobs;
                            for ( auto &job : cp->commands ) {
                                fostlib::push_back(jobs, job.first);
                                cp->write(ios, job.first, yield);
                            }
                            if ( jobs.size() ) logger("job", "list", jobs);
                        }
                        break;}
                    case '{': {
                        fostlib::string jsonstr{"{"};
                        auto bytes = boost::asio::async_read_until(cp->resend.parent(ios), buffer, 0, yield[error]);
                        if ( not error ) {
                            for ( ; bytes; --bytes ) {
                                char next = buffer.sbumpc();
                                if ( next ) jsonstr += next;
                            }
                        }
                        fostlib::log::log(fostlib::log::message(cp->reference, fostlib::json::parse(jsonstr)));
                        break;}
                    }
                }
            }
        }));
        /// Finally, drain the child's stderr
        boost::asio::spawn(ios, exception_decorator([&, cp](auto yield) {
            boost::asio::streambuf buffer;
            std::string line;
            while ( cp->stderr.parent(ios).is_open() ) {
                boost::system::error_code error;
                auto bytes = boost::asio::async_read_until(cp->stderr.parent(ios), buffer, '\n', yield[error]);
                if ( error ) {
                    fostlib::log::error(c_exec_helper)
                        ("", "Error on child stderr")
                        ("error", error)
                        ("bytes", bytes);
                    return;
                } else if ( bytes ) {
                    while ( bytes-- ) {
                        char next = buffer.sbumpc();
                        if ( next != '\n' ) line += next;
                    }
                    fostlib::log::debug(c_exec_helper)
                        ("", "Child stderr")
                        ("child", cp->pid)
                        ("stderr", line.c_str());
                    line.clear();
                }
            }
        }));
    }
    /// Process the other end of the signal handler pipe
    boost::asio::spawn(ios, exception_decorator([&](auto yield) {
        boost::asio::streambuf buffer;
        while ( sigchild->parent(ios).is_open() ) {
            auto bytes = boost::asio::async_read(sigchild->parent(ios), buffer,
                boost::asio::transfer_exactly(1), yield);
            if ( bytes ) {
                switch ( char byte = buffer.sbumpc() ) {
                default:
                    std::cerr << "Got signal byte " << int(byte) << std::endl;
                    break;
                case 'c':
                    for ( auto &child : children ) {
                        if ( child.pid == waitpid(child.pid, nullptr, WNOHANG) ) {
                            fostlib::log::critical(c_exec_helper)
                                ("", "Immediate child dead -- Time to PANIC")
                                ("child", "number", child.number)
                                ("child", "pid", child.pid);
                            fostlib::log::flush();
                            std::exit(4);
                        }
                    }
                }
            }
        }
    }));

    /// This process now needs to read from stdin and queue the jobs
    boost::asio::posix::stream_descriptor as_stdin(ios);
    {
        boost::system::error_code error;
        as_stdin.assign(dup(STDIN_FILENO), error);
        if ( error ) {
            std::cerr << "Cannot assign stdin to the reactor pool. This "
                "probably means you're trying to redirect a file rather "
                "than pipe the commands\n\nI.e. try this:\n"
                "   cat commands.txt | wright-exec-helper\n"
                "instead of\n"
                "   wright-exec-helper < commands.txt" << std::endl;
            std::exit(2);
        }
    }
    boost::asio::spawn(ios, exception_decorator([&](auto yield) {
            f5::eventfd::limiter limit{ios, yield, children.size() * wright::buffer_size};
            boost::asio::streambuf buffer;
            while ( as_stdin.is_open() ) {
                boost::system::error_code error;
                auto bytes = boost::asio::async_read_until(as_stdin, buffer, '\n', yield[error]);
                if ( error ) {
                    fostlib::log::info(c_exec_helper)
                        ("", "Input error. Presumed end of work")
                        ("error", error)
                        ("bytes", bytes);
                    in_closed = true;
                    return;
                } else if ( bytes ) {
                    buffer.commit(bytes);
                    std::istream in(&buffer);
                    std::string line;
                    std::getline(in, line);
                    auto task = ++limit; // Must do this first so it can block
                    for ( auto &child : children ) {
                        if ( not child.commands.full() ) {
                            child.write(ios, line, yield);
                            child.commands.push_back(std::make_pair(line, std::move(task)));
                            requested.insert(line);
                            ++p_accepted;
                            break;
                        }
                    }
                }
            }
            in_closed = true;
        }));

    /// This needs to block here until all processing is done
    auto blocker_ready = blocker.get_future();
    blocker_ready.wait();

    /// Terminating. Wait for children
    for ( auto &child : children ) {
        child.stdin.close();
        waitpid(child.pid, nullptr, 0);
    }

    std::cerr << fostlib::performance::current() << std::endl;
    const bool success = (requested == completed);
    std::cerr << (success ? "All done" : "Some wrong") << std::endl;
}
