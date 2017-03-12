/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/rask/protocol>
#include <f5/threading/channel.hpp>

#include <future>


namespace wright {


    /// Stores the capacity
    class capacity;


    /// Hold the connection state
    class connection final :
        public rask::tcp_connection,
        public std::enable_shared_from_this<connection>
    {
        std::promise<void> blocker;
    public:
        /// The outbound queue for this connection
        f5::boost_asio::channel<rask::out_packet> queue;
        /// The total capacity of this side of the connection
        wright::capacity &capacity;
        /// Reference used for logging etc.
        const fostlib::module reference;

        /// Create a connection to store the socket
        connection(boost::asio::io_service &ios, peering p, wright::capacity &cap);

        /// Block waiting for the connection to close
        void wait_for_close();

        /// Broadcast a message to all connections
        static std::size_t broadcast(std::function<rask::out_packet(void)>);

    protected:
        /// Process inbound messages
        void process_inbound(boost::asio::yield_context &) override;
        /// The outbound message stream
        void process_outbound(boost::asio::yield_context &) override;
        /// The connection has been established
        void established() override;
    };


}

