/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <wright/configuration.hpp>
#include <wright/exec.capacity.hpp>
#include <wright/net.connection.hpp>
#include <wright/net.packets.hpp>

#include <fost/log>


namespace {


    fostlib::performance p_accepted(wright::c_exec_helper, "jobs", "accepted");


}


wright::capacity::capacity(boost::asio::io_service &ios, child_pool &p)
: limit(ios, p.children.size() * wright::buffer_size), pool(p), overspill(ios) {
}


void wright::capacity::next_job(std::string job, boost::asio::yield_context &yield) {
    auto task = limit.next_job(yield); // Must do this first so it can block
    for ( auto &child : pool.children ) {
        if ( not child.commands.full() ) {
            child.write(limit.get_io_service(), job, yield);
            child.commands.push_back(wright::job{job, std::move(task)});
            ++p_accepted;
//             ++(child.counters->accepted);
            fostlib::log::debug(child.counters->reference)
                ("", "Given job to worker")
                ("pid", child.pid)
                ("job", job.c_str());
            return;
        }
    }
    for ( auto &cxv : connections ) {
        auto cnx = cxv.first.lock();
        if ( cnx && cxv.second.cap ) {
            cxv.second.work[job] = std::move(task);
            cnx->queue.produce(out::execute(std::move(job)));
            return;
        }
    }
    fostlib::log::error(c_exec_helper, "Got a job and nowhere to put it");
    fostlib::log::flush();
    std::exit(6);
}


void wright::capacity::additional(std::shared_ptr<connection> cnx, uint64_t cap) {
    auto found = connections.find(cnx);
    if ( found == connections.end() ) {
        connections[cnx] = remote{cap};
        limit.increase_limit(cap);
    } else {
        throw fostlib::exceptions::not_implemented(__func__, "Where the connection is already known");
    }
}


bool wright::capacity::all_done() const {
    if ( input_complete.load() ) {
        const auto outstanding = limit.outstanding();
        fostlib::log::info(c_exec_helper)
            ("", "Checking outstanding work")
            ("remaining", outstanding);
        return not outstanding;
    }
    return false;
}


void wright::capacity::wait_until_all_done(boost::asio::yield_context &yield) {
    limit.wait_for_all_outstanding(yield);
}

