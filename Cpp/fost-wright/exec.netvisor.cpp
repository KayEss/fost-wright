/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exec.hpp>
#include <wright/exec.childproc.hpp>
#include <wright/net.server.hpp>

#include <f5/threading/boost-asio.hpp>


void wright::netvisor() {
    fostlib::host end{c_connect.value().value(), c_port.value()};
    std::cout << end << std::endl;
}

