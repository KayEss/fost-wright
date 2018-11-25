/**
    Copyright 2017-2018, Felspar Co Ltd. <http://support.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/


#include <wright/configuration.hpp>
#include <wright/exception.hpp>
#include <wright/exec.hpp>
#include <wright/exec.capacity.hpp>
#include <wright/exec.childproc.hpp>
#include <wright/exec.watchdog.hpp>
#include <wright/net.server.hpp>

#include <f5/threading/reactor.hpp>
#include <fost/cli>
#include <fost/unicode>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <algorithm>
#include <future>

#include <signal.h>
#include <unistd.h>


using namespace std::literals::chrono_literals;


namespace {


    boost::asio::posix::stream_descriptor
            connect_stdin(boost::asio::io_service &ctrlios) {
        boost::asio::posix::stream_descriptor as_stdin{ctrlios};
        boost::system::error_code error;
        as_stdin.assign(dup(STDIN_FILENO), error);
        if (error) {
            std::cerr
                    << "Cannot assign stdin to the reactor pool. This "
                       "probably means you're trying to redirect a file rather "
                       "than pipe the commands\n\nI.e. try this:\n"
                       "   cat commands.txt | wright-exec-helper\n"
                       "instead of\n"
                       "   wright-exec-helper < commands.txt"
                    << std::endl;
            std::exit(1);
        }
        return as_stdin;
    }


}


void wright::exec_helper(std::ostream &out, const char *command) {
    /// The parent sets up the communications redirects etc and spawns
    /// child processes
    child_pool pool(c_children.value(), command);

    /// Set up a promise that we're going to wait to finish on
    std::promise<void> blocker;

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
    /// Make sure that both reactors are working correctly
    add_watchdog(ctrlios, auxios);
    add_watchdog(auxios, ctrlios);

    /// Process the other end of the signal handler pipe
    pool.sigchild_handling(auxios);
    /// Set up the child pool capacity
    capacity workers{ctrlios, pool};

    /// All the children need a presence in the reactor pool for
    /// their process requirement
    for (auto &child : pool.children) {
        auto *cp = &child;
        /// Each child will wait on the command, then write it
        /// the pipe for the process to execute and wait on the result
        boost::asio::spawn(
                ctrlios,
                exception_decorator(
                        [&, cp](auto yield) {
                            cp->handle_stdout(
                                    ctrlios, yield, workers.pool,
                                    [&](const std::string &job) {
                                        workers.job_done(job);
                                        std::cout << job << std::endl;
                                    });
                        },
                        exit_on_error));
        /// We also need to watch for a resend alert from the child process
        boost::asio::spawn(
                ctrlios,
                exception_decorator(
                        [&, cp](auto yield) {
                            cp->handle_child_requests(ctrlios, workers, yield);
                        },
                        exit_on_error));
        /// Finally, drain the child's stderr
        boost::asio::spawn(auxios, exception_decorator([&, cp](auto yield) {
                               cp->drain_stderr(auxios, yield);
                           }));
    }
    /// If the port setting is turned on then we will start the server
    if (c_port.value()) {
        start_server(auxios, ctrlios, c_port.value(), workers);
    }

    /// This process now needs to read from stdin and queue the jobs
    boost::asio::posix::stream_descriptor as_stdin{connect_stdin(ctrlios)};
    boost::asio::spawn(
            ctrlios,
            exception_decorator(
                    [&](auto yield) {
                        auto clear_overspill = [&]() {
                            for (auto job{workers.overspill.consume()}; job;
                                 job = workers.overspill.consume()) {
                                fostlib::log::debug(c_exec_helper)(
                                        "", "Fetched overspill job")(
                                        "job", (*job).c_str());
                                workers.next_job(std::move(*job), yield);
                            }
                        };
                        boost::asio::streambuf buffer;
                        while (as_stdin.is_open()) {
                            clear_overspill();
                            /// Then consume stdin
                            boost::system::error_code error;
                            auto bytes = boost::asio::async_read_until(
                                    as_stdin, buffer, '\n', yield[error]);
                            if (error) {
                                fostlib::log::info(c_exec_helper)(
                                        "",
                                        "Input error. Presumed end of work")(
                                        "error", error)("bytes", bytes);
                                break;
                            } else if (bytes) {
                                std::string line;
                                for (; bytes; --bytes) {
                                    char next = buffer.sbumpc();
                                    if (next != 0 && next != '\n') {
                                        line += next;
                                    }
                                }
                                workers.next_job(std::move(line), yield);
                            }
                        }
                        clear_overspill();
                        workers.input_complete = true;
                        workers.wait_until_all_done(yield);
                        blocker.set_value();
                    },
                    exit_on_error));

    /// This needs to block here until all processing is done
    auto blocker_ready = blocker.get_future();
    blocker_ready.wait();

    /// Terminating. Wait for children
    workers.close();

    std::cerr << fostlib::performance::current() << std::endl;
    std::cerr << fostlib::coerce<fostlib::json>(pool.job_times) << std::endl;
}
