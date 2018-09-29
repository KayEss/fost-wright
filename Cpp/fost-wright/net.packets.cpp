/**
    Copyright 2017-2018, Felspar Co Ltd. <http://support.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/


#include <wright/configuration.hpp>
#include <wright/exec.capacity.hpp>
#include <wright/net.packets.hpp>

#include <fost/log>
#include <fost/hod/decoder-io.hpp>
#include <fost/unicode>


namespace {
    fostlib::performance p_out_version(wright::c_exec_helper, "network", "out", "version");
    fostlib::performance p_in_version(wright::c_exec_helper, "network", "in", "version");
}
fostlib::hod::out_packet wright::out::version(capacity &cap) {
    const std::size_t total_capacity = cap.size() +
        c_overspill_cap_per_worker.value() * cap.children();
    ++p_out_version;
    fostlib::hod::out_packet packet{packet::version};
    packet << g_proto.max_version();
    packet << uint64_t{total_capacity};
    return packet;
}
void wright::in::version(std::shared_ptr<connection> cnx, fostlib::hod::tcp_decoder &packet) {
    ++p_in_version;
    const auto version = fostlib::hod::read<uint8_t>(packet);
    const auto old_version = cnx->version(g_proto, version);
    auto logger = fostlib::log::info(c_exec_helper);
    logger
        ("version", "packet", version)
        ("version", "protocol", g_proto.max_version())
        ("version", "old", old_version)
        ("version","negotiated",  cnx->version());

    if ( packet.size() ) {
        const auto capacity = fostlib::hod::read<uint64_t>(packet);
        if ( cnx->peer == connection::server_side ) {
            logger("", "Version packet processed -- adding capacity")
                ("capacity", "remote", capacity)
                ("capacity", "local", "old", cnx->capacity.size());
            cnx->capacity.additional(cnx, capacity);
            logger("capacity", "local", "new", cnx->capacity.size());
        } else {
            logger("", "Version packet processed")
                ("capacity", "remote", capacity);
        }
    } else {
        logger("", "Version packet processed (without capacity)");
    }
}


namespace {
    fostlib::performance p_out_execute(wright::c_exec_helper, "network", "out", "execute");
    fostlib::performance p_in_execute(wright::c_exec_helper, "network", "in", "execute");
}
fostlib::hod::out_packet wright::out::execute(std::string job) {
    ++p_out_execute;
    fostlib::hod::out_packet packet(packet::execute);
    packet << f5::u8view(job);
    return packet;
}
void wright::in::execute(std::shared_ptr<connection> cnx, fostlib::hod::tcp_decoder &packet) {
    ++p_in_execute;
    cnx->capacity.overspill.produce(fostlib::hod::read<fostlib::utf8_string>(packet).underlying());
}
namespace {
    fostlib::performance p_out_completed(wright::c_exec_helper, "network", "out", "completed");
    fostlib::performance p_in_completed(wright::c_exec_helper, "network", "in", "completed");
}
fostlib::hod::out_packet wright::out::completed(const std::string &job) {
    ++p_out_completed;
    fostlib::hod::out_packet packet(packet::completed);
    packet << f5::u8view(job);
    return packet;
}
void wright::in::completed(std::shared_ptr<connection> cnx, fostlib::hod::tcp_decoder &packet) {
    ++p_in_completed;
    cnx->capacity.job_done(cnx, fostlib::hod::read<fostlib::utf8_string>(packet).underlying());
}


namespace {
    fostlib::performance p_out_log_message(wright::c_exec_helper, "network", "out", "log_message");
    fostlib::performance p_in_log_message(wright::c_exec_helper, "network", "in", "log_message");
}
fostlib::hod::out_packet wright::out::log_message(const fostlib::log::message &m) {
    ++p_out_log_message;
    auto msg = fostlib::json::unparse(
        fostlib::coerce<fostlib::json>(m), false);
    fostlib::hod::out_packet packet{packet::log_message};
    packet << fostlib::coerce<fostlib::utf8_string>(msg);
    return packet;
}
void wright::in::log_message(std::shared_ptr<connection> cnx, fostlib::hod::tcp_decoder &packet) {
    ++p_in_log_message;
    auto msg = fostlib::hod::read<fostlib::utf8_string>(packet);
    fostlib::log::log(fostlib::log::message(
        cnx->reference, fostlib::json::parse(fostlib::coerce<fostlib::string>(msg))));
}

