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


namespace {
    fostlib::performance p_out_version(wright::c_exec_helper, "network", "out", "version");
    fostlib::performance p_in_version(wright::c_exec_helper, "network", "in", "version");
    fostlib::performance p_out_log_message(wright::c_exec_helper, "network", "out", "log_message");
    fostlib::performance p_in_log_message(wright::c_exec_helper, "network", "in", "log_message");
}


rask::out_packet wright::out::version() {
    ++p_out_version;
    rask::out_packet packet{0x80};
    packet << g_proto.max_version();
    return packet;
}
void wright::in::version(std::shared_ptr<connection> cnx, rask::tcp_decoder &packet) {
    ++p_in_version;
    const auto version = rask::read<uint8_t>(packet);
    const auto old_version = cnx->version(g_proto, version);
    fostlib::log::info(c_exec_helper)
        ("", "Version packet processed")
        ("packet", "version", version)
        ("protocol", "version", g_proto.max_version())
        ("old", "version", old_version)
        ("negotiated", "version", cnx->version());
}


rask::out_packet wright::out::log_message(const fostlib::log::message &m) {
    ++p_out_log_message;
    auto msg = fostlib::json::unparse(
        fostlib::coerce<fostlib::json>(m), false);
    rask::out_packet packet{0xe0};
    packet << fostlib::coerce<fostlib::utf8_string>(msg);
    return packet;
}
void wright::in::log_message(std::shared_ptr<connection> cnx, rask::tcp_decoder &packet) {
    ++p_in_log_message;
    auto msg = rask::read<fostlib::utf8_string>(packet);
    fostlib::log::log(fostlib::log::message(
        cnx->reference, fostlib::json::parse(fostlib::coerce<fostlib::string>(msg))));
}

