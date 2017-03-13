/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exception.hpp>
#include <wright/exec.hpp>
#include <wright/exec.capacity.hpp>
#include <wright/exec.childproc.hpp>
#include <wright/net.server.hpp>

#include <f5/threading/boost-asio.hpp>

#include <sys/wait.h>


void wright::netvisor(const char *command) {
    /// Set up the child worker pool
    child_pool pool(c_children.value(), command);

    /// Set to true when the blocker has been signalled
    bool signalled{false};

    /// Stop on exception, one thread. We want one thread here so
    /// we don't have to worry about thread synchronisation when
    /// running code in the reactor
    f5::boost_asio::reactor_pool control([]() { return false; }, 1u);
    auto &ctrlios = control.get_io_service();
    /// This reactor pool is used for anything that doesn't involve
    /// control of the job queues. It can afford to continue after an
    /// exception and uses more threads.
    f5::boost_asio::reactor_pool auxilliary([]() { return true; }, 2u);
    auto &auxios = auxilliary.get_io_service();

    /// Go through each child and service them properly
    for ( auto &child : pool.children ) {
        /// Use a pointer which we can easily capture in lambdas
        auto *cp = &child;
        /// We also need to watch for a resend alert from the child process
        boost::asio::spawn(ctrlios, exception_decorator([&, cp](auto yield) {
            cp->handle_child_requests(ctrlios, signalled, yield);
        }, exit_on_error));
        /// Finally, drain the child's stderr
        boost::asio::spawn(auxios, exception_decorator([&, cp](auto yield) {
            cp->drain_stderr(auxios, yield);
        }));
    }

    /// Start the child signal processing
    pool.sigchild_handling(auxios);
    /// Track the worker capacity
    capacity workers{ctrlios, pool};

    /// Set up the network connection to the server
    auto cnx = rask::tcp_connect<connection>(
        fostlib::host{c_connect.value().value(), c_port.value()},
        ctrlios, connection::client_side, workers);
    fostlib::log::info(wright::c_exec_helper)
        ("", "Connection established")
        ("host", c_connect.value())
        ("port", c_port.value());

    /// Wait for the connetion to end...
    cnx->wait_for_close();

    /// Terminating. Wait for children
    for ( auto &child : pool.children ) {
        child.stdin.close();
        waitpid(child.pid, nullptr, 0);
    }

    std::cerr << fostlib::performance::current() << std::endl;
    std::cerr << fostlib::coerce<fostlib::json>(pool.job_times) << std::endl;
}

