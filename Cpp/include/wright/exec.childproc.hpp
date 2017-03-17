/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <f5/threading/eventfd.hpp>
#include <fost/counter>
#include <fost/timer>

#include <wright/pipe.hpp>

#include <boost/circular_buffer.hpp>


namespace wright {


    class capacity;
    struct child_pool;


    /// The buffer size for each child
    const std::size_t buffer_size = 3;


    /// The main body loop for the child process
    void fork_worker();


    struct job {
        std::string command;
        std::shared_ptr<f5::eventfd::limiter::job> limiter;
        fostlib::timer time;
    };


    struct childproc final : boost::noncopyable {
        pipe_in stdin;
        pipe_out stdout, stderr, resend;

        /// The child number
        const std::size_t number;
        struct counter_store {
            counter_store(std::size_t);
            /// The child module
            const fostlib::module reference;
            /// Performance counters
            fostlib::performance accepted, completed;
        };
        std::unique_ptr<counter_store> counters;
        /// The command line argument list for the child process
        fostlib::string argx;
        std::vector<char const *> argv;
        /// The string version of the backchannel FD
        std::string backchannel_fd;
        /// The PID that the child gets
        int pid;
        /// The current queue
        boost::circular_buffer<job> commands;

        childproc(std::size_t n, const char *);
        childproc(childproc &&);
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
                tidy();
                ::execvp(argv.front(), const_cast<char *const *>(argv.data()));
            }
        }

        /// Send a job to the child
        void write(boost::asio::io_service &ios, const std::string &command,
            boost::asio::yield_context &yield);
        /// Read the job that the child has done
        std::string read(boost::asio::io_service &ios, boost::asio::streambuf &buffer,
            boost::asio::yield_context yield);

        /// Handle requests from the child
        void handle_child_requests(boost::asio::io_service &ctrlios, capacity &,
            boost::asio::yield_context &yield);
        /// Drain stderr for the child, transforming into log messages
        void drain_stderr(boost::asio::io_service &auxios, boost::asio::yield_context &yield);
        /// Handle stdout which will be used to print the completed jobs
        void handle_stdout(boost::asio::io_service &ctrlios, boost::asio::yield_context &yield,
                child_pool &pool, std::function<void(const std::string &)> job_done);

        /// Close the pipes
        void close();
    };


    struct child_pool {
        /// Construct the pool with the specified number of children
        child_pool( std::size_t number, const char *command);

        /// Start child signal processing
        void sigchild_handling(boost::asio::io_service &ios);

        /// The children
        std::vector<childproc> children;
        /// We want to store statistics about the work done
        fostlib::time_profile<std::chrono::milliseconds> job_times;
    };


}

