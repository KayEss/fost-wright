/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/net.packets.hpp>
#include <wright/net.server.hpp>


rask::protocol<std::function<void(wright::packet::in&)>> wright::g_proto(
    [](rask::control_byte control, wright::packet::in &packet) {
        fostlib::log::warning(c_exec_helper)
            ("", "Unknown control byte received")
            ("control", int(control));
    },
    {});

