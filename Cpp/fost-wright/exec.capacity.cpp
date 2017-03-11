/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/exec.capacity.hpp>


wright::capacity::capacity(boost::asio::io_service &ios, child_pool &p)
: limit(ios, p.children.size() * wright::buffer_size), pool(p) {
}


void wright::capacity::next_job(std::string job, boost::asio::yield_context &yield) {
    auto task = limit.next_job(yield); // Must do this first so it can block
    for ( auto &child : pool.children ) {
        if ( not child.commands.full() ) {
            child.write(limit.get_io_service(), job, yield);
            child.commands.push_back(wright::job{job, std::move(task)});
            return;
        }
    }
}

