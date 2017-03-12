/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <wright/net.server.hpp>


namespace wright {


    class capacity;


    /// Inbound packet handlers
    namespace in {


        /// Process an inbound version packet
        void version(std::shared_ptr<connection> cnx, rask::tcp_decoder &decode);

        /// Log message
        void log_message(std::shared_ptr<connection> cnx, rask::tcp_decoder &decode);


    }


    /// Outbound packet producers
    namespace out {


        /// Create a version packet
        rask::out_packet version(capacity &);

        /// Send a job over the wire
        rask::out_packet execute(std::string);

        /// Log message
        rask::out_packet log_message(const fostlib::log::message &m);


    }


}

