/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <wright/exec.childproc.hpp>

#include <f5/threading/channel.hpp>


namespace wright {


    class connection;


    /// Used to manage the capacity of all workers
    class capacity {
        f5::eventfd::limiter limit;
        child_pool &pool;
        using weak_connection = std::weak_ptr<connection>;
        struct remote {
            uint64_t cap;
            std::map<std::string, std::unique_ptr<f5::eventfd::limiter::job>> work;
        };
        std::map<weak_connection, remote, std::owner_less<weak_connection>> connections;
    public:
        /// Overspill for the capacity
        f5::boost_asio::channel<std::string> overspill;
        /// Create the initial capacity based on the local workers
        capacity(boost::asio::io_service &ios, child_pool &pool);

        /// Give this task to a worker when one becomes available
        void next_job(std::string job, boost::asio::yield_context &yield);

        /// Return the limit on the capacity
        auto size() const {
            return limit.limit();
        }

        /// Register a network connection with its capacity
        void additional(std::shared_ptr<connection>, uint64_t);
    };


}

