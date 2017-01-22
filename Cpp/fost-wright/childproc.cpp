/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/childproc.hpp>

#include <iostream>


namespace {
    const struct newl {
        newl() {
            buffer.sputc('\n');
        }
        boost::asio::streambuf buffer;
    } newline;
}


void wright::childproc::write(
    boost::asio::io_service &ios,
    const std::string &command,
    boost::asio::yield_context &yield
) {
    std::array<boost::asio::streambuf::const_buffers_type, 2>
        buffer{{{command.data(), command.size()}, newline.buffer.data()}};
    boost::asio::async_write(stdin.parent(ios), buffer, yield);
}


std::string wright::childproc::read(
    boost::asio::io_service &ios,
    boost::asio::streambuf &buffer,
    boost::asio::yield_context yield
) {
    boost::system::error_code error;
    auto bytes = boost::asio::async_read_until(stdout.parent(ios), buffer, '\n', yield[error]);
    if ( error ) {
        std::cerr << pid << " read error: " << error << std::endl;
    } else if ( bytes ) {
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
        for ( ; bytes; --bytes ) {
            char next = buffer.sbumpc();
            if ( next == 0 || next == '\n' ) {
//                 std::cerr << pid << " dropped " << int(next) << std::endl;
            } else {
                line += next;
            }
        }
        return line;
    }
    return std::string();
}


void wright::childproc::close() {
    stdout.close();
    stdin.close();
}

