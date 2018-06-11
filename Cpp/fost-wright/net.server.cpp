/**
    Copyright 2017-2018, Felspar Co Ltd. <http://support.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/


#include <wright/configuration.hpp>
#include <wright/net.packets.hpp>
#include <wright/net.server.hpp>


const wright::protocol_definition wright::g_proto(
    [](fostlib::hod::control_byte control, auto cnx, auto &decode) {
        fostlib::log::warning(c_exec_helper)
            ("", "Unknown control byte received")
            ("control", int(control));
    },
    {
        { // Version 0
            {packet::version, in::version}
        },
        { // Version 1
            {packet::execute, in::execute},
            {packet::completed, in::completed},
            {packet::log_message, in::log_message}
        }
    });


namespace {
    void accept(
        boost::asio::io_service &ios,
        std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
        wright::capacity &cap
    ) {
        auto cnx = std::make_shared<wright::connection>(ios, wright::connection::server_side, cap);
        acceptor->async_accept(cnx->socket,
            [&ios, acceptor, cnx, &cap](const boost::system::error_code &error ) {
                accept(ios, acceptor, cap);
                if ( error ) {
                    fostlib::log::error(wright::c_exec_helper,
                        "Server accept", error.message().c_str());
                } else {
                    fostlib::log::info(wright::c_exec_helper, "Connection accepted");
                    cnx->process(cnx);
                }
            });
        fostlib::log::warning(wright::c_exec_helper)
            ("", "Waiting for new connection");
    }
}


void wright::start_server(
    boost::asio::io_service &listen_ios, boost::asio::io_service &sock_ios, uint16_t port, capacity &cap
) {
    fostlib::host h(0);
    boost::asio::ip::tcp::endpoint endpoint{h.address(), port};
    auto acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(listen_ios, endpoint);
    accept(sock_ios, acceptor, cap);
    fostlib::log::warning(wright::c_exec_helper)
        ("", "Started async acceptor")
        ("port", port);
}

