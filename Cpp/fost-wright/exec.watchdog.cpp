/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exec.watchdog.hpp>

#include <fost/counter>

#include <boost/asio/deadline_timer.hpp>


namespace {


    fostlib::performance p_dogs(wright::c_exec_helper, "watchdogs", "created");
    fostlib::performance p_beats(wright::c_exec_helper, "watchdogs", "beats");


    struct dog {
        boost::asio::deadline_timer heartbeat, killer;

        dog(boost::asio::io_service &i, boost::asio::io_service &m)
        : heartbeat(m), killer(i) {
            ++p_dogs;
        }
    };


    void beat(std::shared_ptr<dog> watcher) {
        watcher->killer.expires_from_now(boost::posix_time::seconds(2));
        watcher->killer.async_wait([](boost::system::error_code error) {
            if ( not error ) {
                fostlib::log::critical(wright::c_exec_helper, "Watchdog killer activated");
                fostlib::log::flush();
                std::exit(11);
            }
        });
        watcher->heartbeat.expires_from_now(boost::posix_time::seconds(1));
        watcher->heartbeat.async_wait([watcher](boost::system::error_code error) {
            if ( not error ) {
                ++p_beats;
                beat(watcher);
            }
        });
    }


}


void wright::add_watchdog(boost::asio::io_service &to, boost::asio::io_service &monitored) {
    auto watcher = std::make_shared<dog>(to, monitored);
    beat(watcher);
}

