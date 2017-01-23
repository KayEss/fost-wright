/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>

#include <thread>


const fostlib::setting<int64_t> wright::c_child(
    __FILE__, "wright-exec-helper", "Child", 0, true);

const fostlib::setting<int64_t> wright::c_children(
    __FILE__, "wright-exec-helper",
    "Children", std::thread::hardware_concurrency(), true);

const fostlib::setting<bool> wright::c_simulate(
    __FILE__, "wright-exec-helper", "Simulate", false, true);

const fostlib::setting<int> wright::c_resend_fd(
    __FILE__, "wright-exec-helper", "Resend FD", 0, true);

const fostlib::setting<fostlib::string> wright::c_exec(
    __FILE__, "wright-exec-helper", "Execute", "wright-exec-helper", true);
