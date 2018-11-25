/*
    Copyright 2016-2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
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
