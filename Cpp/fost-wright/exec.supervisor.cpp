/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exception.hpp>
#include <wright/exec.hpp>
#include <wright/exec.capacity.hpp>
#include <wright/exec.childproc.hpp>
#include <wright/net.server.hpp>

#include <f5/threading/boost-asio.hpp>
#include <fost/cli>
#include <fost/unicode>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <future>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>


using namespace std::literals::chrono_literals;


namespace {


    fostlib::performance p_accepted(wright::c_exec_helper, "jobs", "accepted");
    fostlib::performance p_completed(wright::c_exec_helper, "jobs", "completed");


}


void wright::exec_helper(std::ostream &out, const char *command) {
    /// The parent sets up the communications redirects etc and spawns
    /// child processes
    child_pool pool(c_children.value(), command);

    /// Set up a promise that we're going to wait to finish on
    std::promise<void> blocker;
    /// Flag to tell us if the input stream has completed (closed)
    /// yet and if the blocker has been signalled
    bool in_closed{false}, signalled{false};

    /// Stop on exception, one thread. We want one thread here so
    /// we don't have to worry about thread synchronisation when
    /// running code in the reactor
    f5::boost_asio::reactor_pool control([]() { return false; }, 1u);
    auto &ctrlios = control.get_io_service();
    /// This reactor pool is used for anything that doesn't involve
    /// control of the job queues. It can afford to continue after an
    /// exception and uses more threads.
    f5::boost_asio::reactor_pool auxilliary([]() { return true; }, 2u);
    auto &auxios = auxilliary.get_io_service();

    /// All the children need a presence in the reactor pool for
    /// their process requirement
    for ( auto &child : pool.children ) {
        /// Each child will wait on the command, then write it
        /// the pipe for the process to execute and wait on the result
        auto *cp = &child;
        boost::asio::spawn(ctrlios, exception_decorator([&, cp](auto yield) {
                /// Wait for a job to be queued in their process ID
                boost::asio::streambuf buffer;
                while ( cp->stdout.parent(ctrlios).is_open() ) {
                    boost::system::error_code error;
                    auto ret = cp->read(ctrlios, buffer, yield[error]);
                    if ( not error && not ret.empty() &&
                            cp->commands.size() &&
                            ret == cp->commands.front().command )
                    {
                        ++p_completed;
//                         ++(cp->counters->completed);
                        pool.job_times.record(cp->commands.front().time);
                        cp->commands.pop_front();
                        if ( cp->commands.size() ) cp->commands.front().time.reset();
                        auto logger = fostlib::log::debug(c_exec_helper);
                        logger
                            ("", "Got result from child")
                            ("child", cp->pid)
                            ("result", ret.c_str());
                        out << ret << std::endl;
                        if ( in_closed && not signalled ) {
                            const auto working = std::count_if(pool.children.begin(), pool.children.end(),
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
                        if ( cp->commands.empty() ) {
                            fostlib::log::debug(c_exec_helper)
                                ("", "Ignored line from child")
                                ("input", "string", ret.c_str())
                                ("input", "size", ret.size());
                        } else {
                            auto &command = cp->commands.front().command;
                            fostlib::log::debug(c_exec_helper)
                                ("", "Ignored line from child")
                                ("input", "string", ret.c_str())
                                ("input", "size", ret.size())
                                ("expected", "string", command.c_str())
                                ("expected", "size", command.size())
                                ("match", ret == command);
                        }
                    }
                }
                fostlib::log::info(c_exec_helper)
                    ("", "Child done")
                    ("pid", cp->pid);
            }, exit_on_error));
        /// We also need to watch for a resend alert from the child process
        boost::asio::spawn(ctrlios, exception_decorator([&, cp](auto yield) {
            cp->handle_child_requests(ctrlios, signalled, yield);
        }, exit_on_error));
        /// Finally, drain the child's stderr
        boost::asio::spawn(auxios, exception_decorator([&, cp](auto yield) {
            cp->drain_stderr(auxios, yield);
        }));
    }
    /// Process the other end of the signal handler pipe
    pool.sigchild_handling(auxios);
    /// Set up the child pool capacity
    capacity workers{ctrlios, pool};
    /// If the port setting is turned on then we will start the server
    if ( c_port.value() ) {
        start_server(auxios, ctrlios, c_port.value(), workers);
    }

    /// This process now needs to read from stdin and queue the jobs
    boost::asio::posix::stream_descriptor as_stdin(ctrlios);
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
            std::exit(1);
        }
    }
    boost::asio::spawn(ctrlios, exception_decorator([&](auto yield) {
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
                std::string line;
                for ( ; bytes; --bytes ) {
                    char next = buffer.sbumpc();
                    if ( next != 0 && next != '\n' ) {
                        line += next;
                    }
                }
                workers.next_job(std::move(line), yield);
                ++p_accepted;
            }
        }
        in_closed = true;
    }, exit_on_error));

    /// This needs to block here until all processing is done
    auto blocker_ready = blocker.get_future();
    blocker_ready.wait();

    /// Terminating. Wait for children
    for ( auto &child : pool.children ) {
        child.stdin.close();
        waitpid(child.pid, nullptr, 0);
    }

    std::cerr << fostlib::performance::current() << std::endl;
    std::cerr << fostlib::coerce<fostlib::json>(pool.job_times) << std::endl;
}

