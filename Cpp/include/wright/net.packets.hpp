/**
    Copyright 2017-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#pragma once


#include <wright/net.server.hpp>


namespace wright {


    class capacity;


    /// Packet control numbers
    namespace packet {
        enum control_numbers {
            version = 0x80,
            execute = 0x90,
            completed = 0x91,
            log_message = 0xe0
        };
    }


    /// Inbound packet handlers
    namespace in {


        /// Process an inbound version packet
        void
                version(std::shared_ptr<connection> cnx,
                        fostlib::hod::tcp_decoder &decode);

        /// A job has been recived
        void
                execute(std::shared_ptr<connection> cnx,
                        fostlib::hod::tcp_decoder &decode);
        /// A job has been completed
        void completed(
                std::shared_ptr<connection> cnx,
                fostlib::hod::tcp_decoder &decode);

        /// Log message
        void log_message(
                std::shared_ptr<connection> cnx,
                fostlib::hod::tcp_decoder &decode);


    }


    /// Outbound packet producers
    namespace out {


        /// Create a version packet
        fostlib::hod::out_packet version(capacity &);

        /// Send a job over the wire
        fostlib::hod::out_packet execute(std::string);
        fostlib::hod::out_packet completed(const std::string &);

        /// Log message
        fostlib::hod::out_packet log_message(const fostlib::log::message &m);


    }


}
