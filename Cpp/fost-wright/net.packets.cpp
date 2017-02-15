/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/net.packets.hpp>

#include <fost/log>


void wright::in::version(std::shared_ptr<connection> cnx, rask::tcp_decoder &decode) {
    fostlib::log::error(c_exec_helper)
        ("", "Version packet not processed");
}

