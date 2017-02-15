/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/net.packets.hpp>
#include <wright/net.server.hpp>

#include <fost/log>


void wright::connection::process_inbound(boost::asio::yield_context &yield) {
    receive_loop(*this, yield,
        [&](auto decode, uint8_t control, std::size_t bytes)
    {
        g_proto.dispatch(version(), control, shared_from_this(), decode);
    });
}


void wright::connection::process_outbound(boost::asio::yield_context  &yield) {
}

