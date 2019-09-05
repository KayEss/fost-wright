/**
    Copyright 2017-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#include <wright/configuration.hpp>

#include <fost/push_back>

#include <thread>


const fostlib::module wright::c_wright("wright");
const fostlib::module wright::c_exec_helper(c_wright, "exec-helper");


const fostlib::setting<int64_t>
        wright::c_child(__FILE__, "wright-exec-helper", "Child", 0, true);

const fostlib::setting<int64_t> wright::c_children(
        __FILE__,
        "wright-exec-helper",
        "Children",
        std::thread::hardware_concurrency(),
        true);

const fostlib::setting<bool> wright::c_can_die(
        __FILE__, "wright-exec-helper", "Simulator can die", true, true);

const fostlib::setting<bool> wright::c_simulate(
        __FILE__, "wright-exec-helper", "Simulate", false, true);

const fostlib::setting<unsigned> wright::c_sim_mean(
        __FILE__, "wright-exec-helper", "Simulated job mean", 1000, true);
const fostlib::setting<unsigned> wright::c_sim_sd(
        __FILE__,
        "wright-exec-helper",
        "Simulated job standard deviation",
        500,
        true);

const fostlib::setting<int> wright::c_resend_fd(
        __FILE__, "wright-exec-helper", "Resend FD", 0, true);

const fostlib::setting<fostlib::json> wright::c_exec(
        __FILE__,
        "wright-exec-helper",
        "Execute",
        []() {
            fostlib::json cmd;
            fostlib::push_back(cmd, "wright-exec-helper");
            fostlib::push_back(cmd, "--simulate"); // Simulate
            fostlib::push_back(cmd, "true");
            fostlib::push_back(cmd, "-b"); // No banner
            fostlib::push_back(cmd, "false");
            return cmd;
        }(),
        true);


const fostlib::setting<uint16_t> wright::c_port(
        __FILE__, "wright-exec-helper", "Server port", 7788, true);
const fostlib::setting<fostlib::nullable<fostlib::string>> wright::c_connect(
        __FILE__, "wright-exec-helper", "Connect to", fostlib::null, true);
const fostlib::setting<std::size_t> wright::c_overspill_cap_per_worker(
        __FILE__,
        "wright-exec-helper",
        "Target overspill capacity per worker",
        1,
        true);
