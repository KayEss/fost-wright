/**
    Copyright 2016-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#pragma once


#include <iostream>


namespace wright {


    /// Simulate the work being executed and report on the round trip time
    void echo(std::istream &in, std::ostream &out, std::ostream &report);

    /// Start child processes and route work to them
    void exec_helper(std::ostream &out, const char *command);

    /// Connect to a supervisor over the network
    void netvisor(const char *command);


}
