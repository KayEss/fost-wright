/**
    Copyright 2017-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#pragma once


#include <fost/core>


namespace wright {


    /// Return the logging configuration for use by the child process
    fostlib::json child_logging();

    /// Return a logging configuration for use by the parent process
    fostlib::json parent_logging();

    /// Return a logging configuration for use when connected over the network
    fostlib::json network_logging();


}
