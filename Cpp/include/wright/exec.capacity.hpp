/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <wright/exec.childproc.hpp>


namespace wright {


    /// Used to manage the capacity of all workers
    class capacity {
        f5::eventfd::limiter limit;
        child_pool &pool;
    public:
        /// Create the initial capacity based on the local workers
        capacity(boost::asio::io_service &ios, child_pool &pool);

        /// Give this task to a worker when one becomes available
        void next_job(std::string job, boost::asio::yield_context &yield);
    };


}

