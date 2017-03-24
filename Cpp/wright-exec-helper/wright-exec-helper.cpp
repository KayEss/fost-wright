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
    const fostlib::setting<fostlib::json> exec{
        __FILE__, wright::c_exec, [&]() {
            auto exec = wright::c_exec.value();
            fostlib::jcursor(0).set(exec, fostlib::json(args[0].value()));
            return exec;
        }()};

    /// Process the command switches (that control the task)
    args.commandSwitch("-child", wright::c_child);
    args.commandSwitch("c", wright::c_connect);
    args.commandSwitch("-simulate", wright::c_simulate);

    auto run = [&](auto &logger, auto &task) {
        return [&](auto &&... a) {
            /// Build a suitable default loging configuration
            const fostlib::setting<fostlib::json> log_setting{
                __FILE__, settings.c_logging, logger()};
            /// Load any provided settings files
            std::vector<fostlib::settings> configuration;
            configuration.reserve(args.size());
            for ( std::size_t arg{1}; arg != args.size(); ++arg ) {
                std::cerr << "Loading config " << fostlib::json(args[arg].value());
                auto filename = fostlib::coerce<boost::filesystem::path>(args[arg].value());
                configuration.emplace_back(std::move(filename));
            }
            /// Process the command switches that alter behaviour
            args.commandSwitch("p", wright::c_port);
            args.commandSwitch("rfd", wright::c_resend_fd);
            args.commandSwitch("w", wright::c_children);
            args.commandSwitch("x", wright::c_exec);
            /// Load the standard settings
            fostlib::standard_arguments(settings, std::cerr, args);
            /// Start the logging
            fostlib::log::global_sink_configuration log_sinks(settings.c_logging.value());
            /// Run the task
            task(a...);
            fostlib::log::info(wright::c_exec_helper, "Everything done. Terminating normally");
            fostlib::log::flush();
        };
    };

    wright::exception_decorator([&]() {
        if ( wright::c_simulate.value() ) {
            /// These switches are used by the simulator
            args.commandSwitch("d", wright::c_can_die);
            args.commandSwitch("-sim-mean", wright::c_sim_mean);
            args.commandSwitch("-sim-sd", wright::c_sim_sd);
            /// Simulate work by sleeping, and also keep crashing
            wright::echo(std::cin, std::cout, std::cerr);
        } else if ( wright::c_connect.value() ) {
            run(wright::network_logging, wright::netvisor)(args[0].value().c_str());
        } else if ( wright::c_child.value() ) {
            run(wright::child_logging, wright::fork_worker)();
        } else {
            run(wright::parent_logging, wright::exec_helper)(std::cout, args[0].value().c_str());
        }
    }, [](){})();
    /// That last line though.
    /// * `[](){} ` An empty lambda in effect saying "don't re-throw the exception"
    /// * `() ` Immediately invoke the decorated function.

    return 0;
}

