/*
    Copyright 2016-2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exception.hpp>
#include <wright/exec.hpp>
#include <wright/exec.childproc.hpp>
#include <wright/exec.logging.hpp>


#include <fost/main>


int main(int argc, char *argv[]) {
    fostlib::loaded_settings settings{"wright-exec-helper",
        "Wright execution helper\nCopyright (c) 2016-2017, Felspar Co. Ltd."};
    fostlib::arguments args(argc, argv);
    /// Configure more settings
    const fostlib::setting<fostlib::string> exec(__FILE__, wright::c_exec, args[0].value());
    args.commandSwitch("c", wright::c_child);
    args.commandSwitch("w", wright::c_children);
    args.commandSwitch("rfd", wright::c_resend_fd);
    args.commandSwitch("x", wright::c_simulate);

    auto run = [&](auto &logger, auto &task) {
        return [&](auto &&... a) {
            /// Build a suitable default loging configuration
            const fostlib::setting<fostlib::json> log_setting{
                __FILE__, settings.c_logging, logger()};
            /// Load the standard settings
            fostlib::standard_arguments(settings, std::cerr, args);
            /// Start the logging
            fostlib::log::global_sink_configuration log_sinks(settings.c_logging.value());
            /// Run the task
            task(a...);
        };
    };

    wright::exception_decorator([&]() {
        if ( wright::c_simulate.value() ) {
            /// Simulate work by sleeping, and also keep crashing
            wright::echo(std::cin, std::cout, std::cerr);
        } else if ( wright::c_child.value() ) {
            run(wright::child_logging, wright::fork_worker)();
        } else {
            run(wright::parent_logging, wright::exec_helper)(std::cout);
        }
    }, [](){})();
    /// That last line though.
    /// * `[](){} ` An empty lambda in effect saying "don't re-throw the exception"
    /// * `() ` Immediately invoke the decorated function.

    return 0;
}

