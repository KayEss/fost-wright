/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/exec.hpp>

#include <chrono>
#include <random>
#include <thread>

#include <unistd.h>


using namespace std::chrono_literals;


void wright::echo(std::istream &in, std::ostream &out, std::ostream &report) {
    decltype(std::chrono::high_resolution_clock::now()) from{}, to{};
    const auto epoch = from.time_since_epoch();
    decltype(to - from) total{};
    unsigned int captures{};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> rand(1000, 500);
    const auto crash_limit = 1500;

    std::string command;
    while ( in ) {
        std::getline(in, command);
        if ( in && not command.empty() ) {
            to = std::chrono::high_resolution_clock::now();
            if ( epoch < from.time_since_epoch() ) {
                total += (to - from);
                ++captures;
            }
            std::this_thread::sleep_for(rand(gen) * 1ms);
            if ( rand(gen) > crash_limit ) {
                report << "Crash during work... " << ::getpid() << std::endl;
                exit(3); // Simulate a crash
            }
            from = std::chrono::high_resolution_clock::now();
            out << command << std::endl;
        }
        if ( rand(gen) > crash_limit ) {
            out << "Uh oh, crashed" << std::endl;
            report << "Crash after work... " << ::getpid() << std::endl;
            exit(2); // Simulate a crash
        }
    }
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(total).count();
    report << (us / captures) << "us across " << captures << " loops" << std::endl;
}

