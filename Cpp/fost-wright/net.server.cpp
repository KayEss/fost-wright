/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/net.packets.hpp>
#include <wright/net.server.hpp>


wright::protocol_definition wright::g_proto(
    [](rask::control_byte control, auto cnx, auto &decode) {
        fostlib::log::warning(c_exec_helper)
            ("", "Unknown control byte received")
            ("control", int(control));
    },
    {
        { // Version 0
            {0x80, in::version}
        }
    });


namespace {
    void accept(
        boost::asio::io_service &ios, std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor
    ) {
        auto cnx = std::make_shared<wright::connection>(ios);
        acceptor->async_accept(cnx->socket,
            [&ios, acceptor, cnx](const boost::system::error_code &error ) {
                accept(ios, acceptor);
                if ( error ) {
                    fostlib::log::error(wright::c_exec_helper,
                        "Server accept", error.message().c_str());
                } else {
                    fostlib::log::info(wright::c_exec_helper, "Connection accepted");
                    cnx->process(cnx);
                }
            });
        fostlib::log::debug(wright::c_exec_helper, "Waiting for new connection");
    }
}


void wright::start_server(
    boost::asio::io_service &listen_ios, boost::asio::io_service &sock_ios, uint16_t port
) {
    fostlib::host h(0);
    boost::asio::ip::tcp::endpoint endpoint{h.address(), port};
    auto acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(listen_ios, endpoint);
    accept(sock_ios, acceptor);
}

