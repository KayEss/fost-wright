/*
    Copyright 2016, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <fost/cli>
#include <fost/main>
#include <fost/unicode>

#include <chrono>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>


using namespace std::chrono_literals;


namespace {
    void echo(std::istream &in, fostlib::ostream &out) {
        std::string command;
        while ( in ) {
            std::getline(in, command);
            if ( in && not command.empty() ) {
                std::this_thread::sleep_for(2s);
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
    int pid;

    childproc() {
        if ( pipe(in.data()) < 0 ) {
            throw std::system_error(errno, std::system_category());
        }
        if ( pipe(out.data()) < 0 ) {
            throw std::system_error(errno, std::system_category());
        }
    }

    void close() {
        if ( in[0] > 0 ) ::close(in[0]);
        if ( in[1] > 0 ) ::close(in[1]);
        in = {{0, 0}};
        if ( out[0] > 0 ) ::close(out[0]);
        if ( out[1] > 0 ) ::close(out[1]);
        out = {{0, 0}};
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
        argv.push_back("-c");
        argv.push_back(nullptr); // holder for the child number
        argv.push_back("-b"); // No banner
        argv.push_back("false");
        argv.push_back(nullptr);

        /// For each child go through and fork and execvpe it
        for ( auto child = 0; child < c_children.value(); ++child ) {
            std::cerr << "Fork child " << child << ": " << argv[0] << std::endl;
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

        std::this_thread::sleep_for(5s);

        /// Terminating. Wait for children
        std::cerr << "Terminating" << std::endl;
        for ( auto &child : children ) {
            child.close();
            waitpid(child.pid, nullptr, 0);
        }
    }
    return 0;
}

