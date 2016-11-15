/*
    Copyright 2016, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <f5/threading/boost-asio.hpp>
#include <f5/threading/eventfd.hpp>
#include <fost/cli>
#include <fost/main>
#include <fost/unicode>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <chrono>
#include <future>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>


using namespace std::chrono_literals;


namespace {
    void echo(std::istream &in, fostlib::ostream &out) {
        std::string command;
        while ( in ) {
            std::getline(in, command);
            std::cerr << '>' << command << std::endl;
            if ( in && not command.empty() ) {
                std::this_thread::sleep_for(2s);
                std::cerr << '<' << command << std::endl;
                out << command << std::endl;
            }
        }
    }

    /// The child number. Zero means the parent process
    const fostlib::setting<int64_t> c_child(__FILE__, "wright-exec-helper",
        "Child", 0, true);
    const fostlib::setting<int64_t> c_children(__FILE__, "wright-exec-helper",
        "Children", std::thread::hardware_concurrency(), true);
}


struct childproc {
    std::array<int, 2> in{{0, 0}}, out{{0, 0}};
    std::unique_ptr<boost::asio::posix::stream_descriptor> inp, outp;
    int pid;
    std::string command;
    std::shared_ptr<f5::eventfd::limiter::job> task;

    childproc() {
        if ( ::pipe(in.data()) < 0 ) {
            throw std::system_error(errno, std::system_category());
        }
        if ( ::pipe(out.data()) < 0 ) {
            throw std::system_error(errno, std::system_category());
        }
    }
    childproc(const childproc &) = delete;
    childproc &operator = (const childproc &) = delete;

    void write(
        boost::asio::io_service &ios,
        std::string c,
        boost::asio::yield_context &yield
    ) {
        command = c;
        if ( not inp ) {
            inp = std::make_unique<boost::asio::posix::stream_descriptor>(ios, in[1]);
            in[1] = 0;
        }
        boost::asio::streambuf newline;
        newline.sputc('\n');
        std::array<boost::asio::streambuf::const_buffers_type, 2>
            buffer{{{command.data(), command.size()}, newline.data()}};
        boost::asio::async_write(*inp, buffer, yield);
    }
    boost::asio::posix::stream_descriptor &read(boost::asio::io_service &ios) {
        if ( not outp ) {
            outp = std::make_unique<boost::asio::posix::stream_descriptor>(ios, out[0]);
            out[0] = 0;
        }
        return *outp;
    }
    std::string read(
        boost::asio::io_service &ios,
        boost::asio::streambuf &buffer,
        boost::asio::yield_context yield
    ) {
        boost::system::error_code error;
        auto bytes = boost::asio::async_read_until(read(ios), buffer, '\n', yield[error]);
        if ( error ) {
            std::cerr << "Error: " << error << std::endl;
        } else if ( bytes ) {
            buffer.commit(bytes);
            std::istream in(&buffer);
            std::string line;
            std::getline(in, line);
            return line;
        } else {
            std::cerr << "No bytes read" << std::endl;
        }
        return std::string();
    }

    void close() {
        if ( in[0] > 0 ) ::close(in[0]);
        if ( in[1] > 0 ) ::close(in[1]);
        in = {{0, 0}};
        if ( out[0] > 0 ) ::close(out[0]);
        if ( out[1] > 0 ) ::close(out[1]);
        out = {{0, 0}};
        inp.reset(nullptr);
        outp.reset(nullptr);
    }

    ~childproc() {
        close();
    }
};


FSL_MAIN(
    L"wright-exec-helper",
    L"Wright execution helper\nCopyright (c) 2016, Felspar Co. Ltd."
)( fostlib::ostream &out, fostlib::arguments &args ) {
    /// Configure settings
    const fostlib::setting<fostlib::string> c_exec(__FILE__, "wright-exec-helper",
        "Execute", args[0].value(), true);
    args.commandSwitch("c", c_child);
    args.commandSwitch("w", c_children);

    if ( c_child.value() ) {
        /// Child process needs to do the right thing
        echo(std::cin, out);
    } else {
        /// The parent sets up the communications redirects etc and spawns
        /// child processes
        std::cerr << "Parent process spawning " << c_children.value() << " children\n";
        std::vector<childproc> children(c_children.value());

        /// Set up the argument vector for the child
        const auto command = c_exec.value();
        std::vector<char const *> argv;
        argv.push_back(command.c_str());
        if ( command == argv[0] ) {
            argv.push_back("-c");
            argv.push_back(nullptr); // holder for the child number
            argv.push_back("-b"); // No banner
            argv.push_back("false");
        }
        argv.push_back(nullptr);

        /// For each child go through and fork and execvpe it
        for ( auto child = 0; child < c_children.value(); ++child ) {
            std::cerr << "Fork child " << child+1 << ": " << argv[0] << std::endl;
            auto child_number = std::to_string(child + 1);
            argv[2] = child_number.c_str();
            children[child].pid = fork();
            if ( children[child].pid < 0 ) {
                throw std::system_error(errno, std::system_category());
            } else if ( children[child].pid == 0 ) {
                dup2(children[child].in[0], STDIN_FILENO);
                dup2(children[child].out[1], STDOUT_FILENO);
                children.clear(); // closes the pipes in this process
                execvp(argv[0], const_cast<char *const *>(argv.data()));
            }
        }
        std::cerr << "All children started. Waiting for commands" << std::endl;

        {
            /// Set up a promise that we're going to wait to finish on
            std::promise<void> blocker;

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
                auto proc = [&](auto yield) {
                        /// Wait for a job to be queued in their process ID
                        boost::asio::streambuf buffer;
                        while ( child.read(ios).is_open() ) {
                            auto ret = child.read(ios, buffer, yield);
                            child.task->done([](auto e, auto b) {});
                            child.task.reset();
                            child.command.empty();
                            out << '*' << ret << std::endl;
                        }
                    };
                boost::asio::spawn(ios, proc);
            }

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
                    exit(2);
                }
            }
            auto proc = [&](auto yield) {
                    f5::eventfd::limiter limit{ios, yield, children.size()};
                    boost::asio::streambuf buffer;
                    while ( as_stdin.is_open() ) {
                        boost::system::error_code error;
                        auto bytes = boost::asio::async_read_until(as_stdin, buffer, '\n', yield[error]);
                        if ( error ) {
                            std::cerr << "Error: " << error << std::endl;
                            break;
                        } else if ( bytes ) {
                            buffer.commit(bytes);
                            std::istream in(&buffer);
                            std::string line;
                            std::getline(in, line);
                            auto task = ++limit; // Must do this first so it can block
                            for ( auto &child : children ) {
                                if ( not child.task ) {
                                    child.write(ios, line, yield);
                                    child.task = task;
                                    break;
                                }
                            }
                        } else {
                            std::cerr << "No bytes read" << std::endl;
                        }
                    }
                    std::cerr << "Done -- unblocking" << std::endl;
                    blocker.set_value(); // Not the final location for this
                };
            boost::asio::spawn(ios, proc);

            /// This needs to block here until all processing is done
            auto blocker_ready = blocker.get_future();
            blocker_ready.wait();
        }

        /// Terminating. Wait for children
        std::cerr << "Terminating" << std::endl;
        for ( auto &child : children ) {
            child.close();
            waitpid(child.pid, nullptr, 0);
        }
    }
    return 0;
}

