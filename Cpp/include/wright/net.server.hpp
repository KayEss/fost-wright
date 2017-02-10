/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/rask/protocol>


namespace wright {


    namespace packet {
        /// In bound packet
        class in;
    }


    /// The protocol description for Wright
    extern rask::protocol<std::function<void(wright::packet::in&)>> g_proto;


    /// Hold the connection state
    class connection final : public fostlib::rask_tcp {
    public:
        /// Create a connection that can be used to accept inbound connections
        connection(boost::asio::io_service &ios)
        : rask_tcp(ios) {
        }

    protected:
        /// The inbound message stream
        void process_inbound(boost::asio::yield_context &) override;
        /// The outbound message stream
        void process_outbound(boost::asio::yield_context &) override;
    };


    /// Listen for inbound connections. The `listen` service is used for
    /// accepting connection and the `socket` service is used for the
    /// subsequent sockets.
    void start_server(boost::asio::io_service &listen, boost::asio::io_service &socket,
        uint16_t port);


}

