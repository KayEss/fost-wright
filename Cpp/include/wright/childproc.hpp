/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <f5/threading/eventfd.hpp>

#include <wright/port.hpp>

#include <boost/circular_buffer.hpp>


namespace wright {


    /// The buffer size for each child
    const std::size_t buffer_size = 3;


    struct childproc {
        pipe_in stdin;
        pipe_out stdout;

        int pid;
        boost::circular_buffer<std::pair<std::string, std::shared_ptr<f5::eventfd::limiter::job>>> commands;

        childproc()
        : commands(buffer_size) {
        }
        childproc(const childproc &) = delete;
        childproc &operator = (const childproc &) = delete;

        /// Send a job to the child
        void write(
            boost::asio::io_service &ios,
            const std::string &command,
            boost::asio::yield_context &yield
        ) {
            boost::asio::streambuf newline;
            newline.sputc('\n');
            std::array<boost::asio::streambuf::const_buffers_type, 2>
                buffer{{{command.data(), command.size()}, newline.data()}};
            boost::asio::async_write(stdin.parent(ios), buffer, yield);
        }
        /// Read the job that the child has done
        std::string read(
            boost::asio::io_service &ios,
            boost::asio::streambuf &buffer,
            boost::asio::yield_context yield
        ) {
            auto bytes = boost::asio::async_read_until(stdout.parent(ios), buffer, '\n', yield);
            if ( bytes ) {
                buffer.commit(bytes);
                std::istream in(&buffer);
                std::string line;
                std::getline(in, line);
                return line;
            }
            return std::string();
        }

        void close() {
            stdout.close();
            stdin.close();
        }

        ~childproc() {
            close();
        }
    };



}

