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
        using weak_connection = std::weak_ptr<connection>;
        struct remote {
            uint64_t cap;
            std::map<std::string, std::unique_ptr<f5::eventfd::limiter::job>> work;
        };
        std::map<weak_connection, remote, std::owner_less<weak_connection>> connections;
    public:
        /// The child process pool
        child_pool &pool;
        /// Overspill for the capacity
        f5::boost_asio::channel<std::string> overspill;
        /// Atomic bool that is set to true when the input is complete
        std::atomic<bool> input_complete{false};

        /// Create the initial capacity based on the local workers
        capacity(boost::asio::io_service &ios, child_pool &pool);

        /// Give this task to a worker when one becomes available
        void next_job(std::string job, boost::asio::yield_context &yield);
        /// Mark (and count) a job as done
        void job_done(const std::string &job);
        /// Mark a network job as having been done
        void job_done(std::shared_ptr<connection> cnx, const std::string &job);
        /// Move all of the outstanding work for the connection to the
        /// overspill and the remove the connection as it is now dead.
        void overspill_work(std::shared_ptr<connection> cnx);

        /// Return the limit on the capacity
        auto size() const {
            return limit.limit();
        }
        /// Return the number of children
        auto children() const {
            return pool.children.size();
        }

        /// Register a network connection with its capacity
        void additional(std::shared_ptr<connection>, uint64_t);

        /// Returns the amount of work outstanding. A value of zero
        /// doesn't mean that no more work can be requested, only that
        /// there is currently none outstanding.
        std::size_t work_outstanding() const {
            return limit.outstanding();
        }

        /// Return that everything is done. No more work can be accepted.
        bool all_done() const;

        /// Wait until all of the outstanding work is done
        void wait_until_all_done(boost::asio::yield_context &yield);
    };


}

