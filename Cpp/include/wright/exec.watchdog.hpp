/**
    Copyright 2017-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#pragma once


#include <boost/asio/io_service.hpp>


namespace wright {


    /// Add a watchdog that ensures that the monitored io_service is making
    /// progress on work.
    void add_watchdog(
            boost::asio::io_service &to, boost::asio::io_service &monitored);


}
