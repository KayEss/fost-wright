/*
    Copyright 2016-2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exec.hpp>
#include <wright/exec.childproc.hpp>
#include <wright/exec.logging.hpp>


#include <fost/main>


FSL_MAIN(
    L"wright-exec-helper",
    L"Wright execution helper\nCopyright (c) 2016-2017, Felspar Co. Ltd."
)(fostlib::ostream &out, fostlib::arguments &args) {
    /// Configure settings
    const fostlib::setting<fostlib::string> exec(__FILE__, wright::c_exec, args[0].value());
    args.commandSwitch("c", wright::c_child);
    args.commandSwitch("w", wright::c_children);
    args.commandSwitch("rfd", wright::c_resend_fd);
    args.commandSwitch("x", wright::c_simulate);

    if ( wright::c_simulate.value() ) {
        /// Simulate work by sleeping, and also keep crashing
        wright::echo(std::cin, out, std::cerr);
    } else if ( wright::c_child.value() ) {
        wright::fork_worker();
    } else {
        wright::parent_logging();
        wright::exec_helper(out);
    }

    return 0;
}

