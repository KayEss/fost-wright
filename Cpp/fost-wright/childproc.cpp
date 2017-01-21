/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/childproc.hpp>


void wright::childproc::write(
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


std::string wright::childproc::read(
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


void wright::childproc::close() {
    stdout.close();
    stdin.close();
}

