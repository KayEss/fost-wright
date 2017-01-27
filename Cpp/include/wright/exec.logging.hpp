/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <fost/core>


namespace wright {


    /// Return the logging configuration for use by the child process
    fostlib::json child_logging();

    /// Return a logging configuration for use by the parent process
    fostlib::json parent_logging();


}

