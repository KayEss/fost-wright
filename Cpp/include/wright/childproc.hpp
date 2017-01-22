/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#define BOOST_COROUTINE_NO_DEPRECATION_WARNING


#include <f5/threading/eventfd.hpp>

#include <wright/port.hpp>

#include <boost/circular_buffer.hpp>


namespace wright {


    /// The buffer size for each child
    const std::size_t buffer_size = 3;


    struct childproc final {
        pipe_in stdin;
        pipe_out stdout, stderr, resend;

        /// The command line argument list for the child process
        std::vector<char const *> argv;
        /// The PID that the child gets
        int pid;
        /// The current queue
        boost::circular_buffer<std::pair<std::string, std::shared_ptr<f5::eventfd::limiter::job>>> commands;

        childproc()
        : commands(buffer_size) {
        }
        childproc(const childproc &) = delete;
        childproc &operator = (const childproc &) = delete;

        ~childproc() {
            close();
        }

        /// Execute the process. Perform the clean up before making the execvpe call
        template<typename F>
        void fork_exec(F tidy) {
            pid = ::fork();
            if ( pid < 0 ) {
                throw std::system_error(errno, std::system_category());
            } else if ( pid == 0 ) {
                dup2(stdin.child(), STDIN_FILENO);
                dup2(stdout.child(), STDOUT_FILENO);
                dup2(stderr.child(), STDERR_FILENO);
                auto argv_copy = argv;
                tidy();
                ::execvp(argv_copy[0], const_cast<char *const *>(argv_copy.data()));
            }
        }

        /// Send a job to the child
        void write(boost::asio::io_service &ios, const std::string &command,
            boost::asio::yield_context &yield);
        /// Read the job that the child has done
        std::string read(boost::asio::io_service &ios, boost::asio::streambuf &buffer,
            boost::asio::yield_context yield);

        /// Close the pipes
        void close();
    };


}

