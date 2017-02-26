/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exec.logging.hpp>

#include <fost/insert>
#include <fost/log>
#include <fost/push_back>


fostlib::json wright::parent_logging() {
    fostlib::json ret, sink;
    fostlib::insert(sink, "name", "stdout");
    fostlib::insert(sink, "configuration", "channel", "stderr");
    fostlib::insert(sink, "configuration", "log-level", fostlib::log::warning_level_tag::level());
    fostlib::insert(sink, "configuration", "color", true);
    fostlib::push_back(ret, "sinks", sink);
    return ret;
}


namespace {

    struct rawfd {
        int fd;

        rawfd(const fostlib::json &conf)
        : fd(fostlib::coerce<int>(conf["fd"])) {
        }

        bool operator () (const fostlib::log::message &m) {
            auto msg = fostlib::json::unparse(
                fostlib::coerce<fostlib::json>(m), false);
            ::write(fd, msg.c_str(), msg.length() + 1);
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


fostlib::json wright::network_logging() {
    fostlib::json ret, screen;
    fostlib::insert(screen, "name", "stdout");
    fostlib::insert(screen, "configuration", "log-level", 0);
    fostlib::insert(screen, "configuration", "color", true);
    fostlib::push_back(ret, "sinks", screen);
    return ret;
}
