/**
    Copyright 2017-2019, Felspar Co Ltd. <http://support.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/


#include <wright/configuration.hpp>
#include <wright/exec.logging.hpp>
#include <wright/net.packets.hpp>

#include <fost/insert>
#include <fost/log>
#include <fost/push_back>


fostlib::json wright::parent_logging() {
    fostlib::json ret, sink;
    fostlib::insert(sink, "name", "stdout");
    fostlib::insert(sink, "configuration", "channel", "stderr");
    fostlib::insert(
            sink, "configuration", "log-level",
            fostlib::log::warning_level_tag::level());
    fostlib::insert(sink, "configuration", "color", true);
    fostlib::push_back(ret, "sinks", sink);
    return ret;
}


namespace {

    struct rawfd {
        int fd;

        rawfd(const fostlib::json &conf)
        : fd(fostlib::coerce<int>(conf["fd"])) {}

        bool operator()(const fostlib::log::message &m) {
            auto msg = fostlib::json::unparse(
                    fostlib::coerce<fostlib::json>(m), false);
            ::write(fd, msg.memory().data(), msg.memory().size() + 1);
            return true;
        }
    };

    const fostlib::log::global_sink<rawfd> rawfd_logger("wright.rawfd");

}

fostlib::json wright::child_logging() {
    fostlib::json ret, sink;
    fostlib::insert(sink, "name", "wright.rawfd");
    fostlib::insert(sink, "configuration", "fd", c_resend_fd.value());
    fostlib::push_back(ret, "sinks", sink);
    return ret;
}


namespace {

    struct netlog {
        const std::size_t level;
        netlog(const fostlib::json &conf)
        : level{not conf.isobject()
                        ? 1000u
                        : fostlib::coerce<fostlib::nullable<std::size_t>>(
                                  conf["level"])
                                  .value_or(1000u)} {}

        bool operator()(const fostlib::log::message &m) {
            if (m.level() >= level) {
                wright::connection::broadcast(
                        [&m]() { return wright::out::log_message(m); });
            }
            return true;
        }
    };

    const fostlib::log::global_sink<netlog> netlog_logger("wright.network");

}

fostlib::json wright::network_logging() {
    fostlib::json ret, screen, net;
    fostlib::insert(screen, "name", "stdout");
    fostlib::insert(screen, "configuration", "log-level", 0);
    fostlib::insert(screen, "configuration", "color", true);
    fostlib::push_back(ret, "sinks", screen);
    fostlib::insert(net, "name", "wright.network");
    fostlib::push_back(ret, "sinks", net);
    return ret;
}
