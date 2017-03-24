/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#include <boost/asio/io_service.hpp>


namespace wright {


    /// Add a watchdog that ensures that the monitored io_service is making
    /// progress on work.
    void add_watchdog(boost::asio::io_service &to, boost::asio::io_service &monitored);


}

