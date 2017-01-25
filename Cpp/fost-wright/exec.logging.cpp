/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exec.logging.hpp>

#include <fost/insert>
#include <fost/push_back>


namespace {


    fostlib::setting<fostlib::json> c_logging(__FILE__,
        "wright-exec-helper", "Logging sinks",
        []() {
            fostlib::json ret, sink;
            fostlib::insert(sink, "name", "stdout");
            fostlib::insert(sink, "configuration", "channel", "stderr");
            fostlib::insert(sink, "configuration", "log-level", 0);
            fostlib::insert(sink, "configuration", "color", true);
            fostlib::push_back(ret, "sinks", sink);
            return ret;
        }());


}


fostlib::json wright::parent_logging() {
    return fostlib::json();
}

