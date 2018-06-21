/**
    Copyright 2017-2018, Felspar Co Ltd. <http://support.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/


#include <wright/configuration.hpp>
#include <wright/exec.capacity.hpp>
#include <wright/net.connection.hpp>
#include <wright/net.packets.hpp>

#include <fost/log>

#include <sys/wait.h>


namespace {


    fostlib::performance p_accepted(wright::c_exec_helper, "jobs", "accepted");
    fostlib::performance p_completed(wright::c_exec_helper, "jobs", "completed");


}


wright::capacity::capacity(boost::asio::io_service &ios, child_pool &p)
: limit(ios, p.children.size() * wright::buffer_size), pool(p), overspill(ios) {
}


void wright::capacity::next_job(std::string job, boost::asio::yield_context yield) {
    /// First of all we wait for a spare slot in one of the work queues.
    /// The limit capacity must exactly equal the total slot capacity in
    /// all queues.
    auto task = limit.next_job(yield);
    ++p_accepted;
    /// Try to put the work out over the network first before doing anything
    /// locally.
    for ( auto &cxv : connections ) {
        auto cnx = cxv.first.lock();
        if ( cnx && cxv.second.cap > cxv.second.work.size() ) {
            cxv.second.work[job] = std::move(task);
            cnx->queue.produce(out::execute(std::move(job)));
            return;
        }
    }
    do {
        /** Do a rotate left first so we won't try the
            same child two times in a row without trying
            the others first. This should spread the jobs
            out across.
        */
        ++child_index;
        child_index = child_index % pool.children.size();
    } while ( pool.children[child_index].commands.full() );
    auto &child{pool.children[child_index]};
    child.write(limit.get_io_service(), job, yield);
    child.commands.push_back(wright::job{job, std::move(task)});
}


void wright::capacity::job_done(const std::string &job) {
    ++p_completed;
}


void wright::capacity::job_done(std::shared_ptr<connection> cnx, const std::string &job) {
    auto &rmt = connections[cnx];
    auto pos = rmt.work.find(job);
    if ( pos == rmt.work.end() ) {
        fostlib::log::error(c_exec_helper)
            ("", "Got a job that isn't outstanding for this network connection")
            ("connection", "id", cnx->id)
            ("job", job.c_str());
    } else {
        rmt.work.erase(pos);
        job_done(job);
        std::cout << job << std::endl;
    }
}


void wright::capacity::overspill_work(std::shared_ptr<connection> cnx) {
    auto logger{fostlib::log::debug(c_exec_helper)};
    logger("", "Redistributing work");
    std::size_t redist{};
    /**
        If the incoming connection wasn't actually a real one (some people
        try to talk to it via things like HTTP by accident) then the connection
        won't be found in the `connections` mapping so there isn't anything
        to do here.
    */
    auto prmt = connections.find(cnx);
    if ( prmt != connections.end() ) {
        for ( auto &w : prmt->second.work ) {
            overspill.produce(std::string(w.first));
            ++redist;
        }
        logger("jobs", redist);
        logger("limit", limit.decrease_limit(prmt->second.cap));
        connections.erase(prmt);
    }
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


void wright::capacity::wait_until_all_done(boost::asio::yield_context yield) {
    limit.wait_for_all_outstanding(yield);
}


void wright::capacity::close() {
    connection::close_all();
    for ( auto &child : pool.children ) {
        child.stdin.close();
        waitpid(child.pid, nullptr, 0);
    }
}

