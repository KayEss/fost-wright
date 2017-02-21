/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/net.packets.hpp>

#include <fost/log>
#include <fost/rask/decoder-io.hpp>


void wright::in::version(std::shared_ptr<connection> cnx, rask::tcp_decoder &packet) {
    const auto version = rask::read<uint8_t>(packet);
    const auto old_version = cnx->version(g_proto, version);
    fostlib::log::error(c_exec_helper)
        ("", "Version packet not processed")
        ("packet", "version", version)
        ("protocol", "version", g_proto.max_version())
        ("old", "version", old_version)
        ("negotiated", "version", cnx->version());
}

