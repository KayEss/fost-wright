/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exec.hpp>

#include <fost/timer>

#include <chrono>
#include <random>
#include <thread>

#include <unistd.h>


using namespace std::chrono_literals;


void wright::echo(std::istream &in, std::ostream &out, std::ostream &report) {
    bool first = true;
    fostlib::timer time;
    fostlib::time_profile<std::chrono::microseconds> times(5us, 1.2, 5);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> rand(c_sim_mean.value(), c_sim_sd.value());
    const auto crash_limit = c_sim_mean.value() + c_sim_sd.value();

    std::string command;
    while (in) {
        std::getline(in, command);
        if (in && not command.empty()) {
            if (not first) times.record(time);
            first = false;
            std::this_thread::sleep_for(rand(gen) * 1ms);
            if (rand(gen) > crash_limit && c_can_die.value()) {
                report << "Crash during work... " << ::getpid() << std::endl;
                exit(3); // Simulate a crash
            }
            time.reset();
            out << command << std::endl;
        }
        if (rand(gen) > crash_limit && c_can_die.value()) {
            out << "Uh oh, crashed" << std::endl;
            report << "Crash after work... " << ::getpid() << std::endl;
            exit(2); // Simulate a crash
        }
    }
    report << fostlib::json::unparse(
            fostlib::coerce<fostlib::json>(times), false)
           << std::endl;
}
