/*
    Copyright 2016-2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/childproc.hpp>
#include <wright/configuration.hpp>
#include <wright/echo.hpp>

#include <f5/threading/boost-asio.hpp>
#include <fost/cli>
#include <fost/counter>
#include <fost/main>
#include <fost/unicode>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <future>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cxxabi.h>


namespace {


    const auto rethrow = []() { throw; };
    const auto exception_decorator = [](auto fn, std::function<void(void)> recov = rethrow) {
        return [=](auto &&...a) {
                try {
                    return fn(a...);
                } catch ( boost::coroutines::detail::forced_unwind & ) {
                    throw;
                } catch ( std::exception &e ) {
                    std::cerr << e.what() << ": " << wright::c_child.value() << std::endl;
                    return recov();
                } catch ( ... ) {
                    std::cerr << "Unkown exception: " << wright::c_child.value() << " - "
                        << __cxxabiv1::__cxa_current_exception_type()->name() << std::endl;
                    return recov();
                }
            };
        };

    const fostlib::module exec_helper("exec_helper");
    fostlib::performance p_accepted(exec_helper, "jobs", "accepted");
    fostlib::performance p_completed(exec_helper, "jobs", "completed");

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


FSL_MAIN(
    L"wright-exec-helper",
    L"Wright execution helper\nCopyright (c) 2016, Felspar Co. Ltd."
)( fostlib::ostream &out, fostlib::arguments &args ) {
    /// Configure settings
    const fostlib::setting<fostlib::string> exec(__FILE__, wright::c_exec, args[0].value());
    args.commandSwitch("c", wright::c_child);
    args.commandSwitch("w", wright::c_children);
    args.commandSwitch("rfd", wright::c_resend_fd);
    args.commandSwitch("x", wright::c_simulate);

    if ( wright::c_simulate.value() ) {
        /// Simulate work by sleeping, and also keep crashing
        wright::echo(std::cin, out, std::cerr);
    } else if ( wright::c_child.value() ) {
        wright::fork_worker();
    } else {
        /// The parent sets up the communications redirects etc and spawns
        /// child processes
        std::vector<wright::childproc> children(wright::c_children.value());

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
        for ( auto child = 0; child < children.size(); ++child ) {
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
                        if ( not error && not ret.empty() && ret == cp->commands.front().first ) {
                            ++p_completed;
                            cp->commands.pop_front();
                            std::cerr << cp->pid << ": " << ret << std::endl;
                            out << ret << std::endl;
                            completed.insert(ret);
                            if ( in_closed && not signalled ) {
                                const auto working = std::count_if(children.begin(), children.end(),
                                        [](const auto &c) { return not c.commands.empty(); });
                                if ( working == 0 ) {
                                    signalled = true;
                                    blocker.set_value();
                                }
                            }
                        } else if ( error ) {
                            std::cerr << cp->pid << " read error: " << error << std::endl;
                        } else if ( not ret.empty() ) {
                            auto &command = cp->commands.front().first;
                            std::cerr << cp->pid << " ignored " <<
                                (ret == command ? "equal" : "not-equal") <<
                                "\n  input   : '" << ret << "' "<< ret.size() <<
                                "\n  expected: '" << command << "' " << command.size() << std::endl;
                        }
                    }
                    std::cerr << cp->pid << " done" << std::endl;
                }));
            /// We also need to watch for a resend alert from the child process
            boost::asio::spawn(ios, exception_decorator([&, cp](auto yield) {
                boost::asio::streambuf buffer;
                while ( cp->resend.parent(ios).is_open() ) {
                    auto bytes = boost::asio::async_read(cp->resend.parent(ios), buffer,
                        boost::asio::transfer_exactly(1), yield);
                    if ( bytes ) {
                        switch ( char byte = buffer.sbumpc() ) {
                        default:
                            std::cerr << "Got resend byte " << int(byte) << std::endl;
                            break;
                        case 'r':
                            if ( signalled ) {
                                /// The work is done, but the child seems
                                /// to be looping in an error. Kill it
                                ::kill(cp->pid, SIGINT);
                            } else {
                                std::cerr << "Child wants resend of jobs: " << cp->commands.size() << std::endl;
                                for ( auto &job : cp->commands ) {
                                    std::cerr << "Resending: " << job.first << std::endl;
                                    cp->write(ios, job.first, yield);
                                }
                            }
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
                        std::cerr << "Input pipe error " << error << " bytes: " << bytes << std::endl;
                        in_closed = true;
                        return;
                    } else if ( bytes ) {
                        while ( bytes-- ) {
                            char next = buffer.sbumpc();
                            if ( next != '\n' ) line += next;
                        }
                        std::cerr << line << std::endl;
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
                                std::cerr << child.pid << " dead. Time to PANIC..." << std::endl;
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
                        std::cerr << "Input pipe error " << error << " bytes: " << bytes << std::endl;
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
                std::cerr << "Input processing done" << std::endl;
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

    return 0;
}

