/**
    Copyright 2017-2018, Felspar Co Ltd. <http://support.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/


#pragma once


#include <wright/net.connection.hpp>


namespace wright {


    class capacity;


    /// The protocol description for Wright
    using protocol_definition = fostlib::hod::protocol<std::function<void(
            std::shared_ptr<connection>, fostlib::hod::tcp_decoder &)>>;
    extern const protocol_definition g_proto;


    /// Listen for inbound connections. The `listen` service is used for
    /// accepting connection and the `socket` service is used for the
    /// subsequent sockets.
    void start_server(
            boost::asio::io_service &listen_ios,
            boost::asio::io_service &socket_ios,
            uint16_t port,
            capacity &);

    /// Keep a connection to a server open
    std::shared_ptr<connection>
            connect_to_server(boost::asio::io_service &ios, fostlib::host);


}
