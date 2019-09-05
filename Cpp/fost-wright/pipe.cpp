/**
    Copyright 2016-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#include <wright/pipe.hpp>

#include <system_error>

#include <unistd.h>


std::pair<int, int>
        wright::detail::pipe_fds(std::size_t parent, std::size_t child) {
    std::array<int, 2> p{{0, 0}};
    if (::pipe(p.data()) < 0)
        throw std::system_error(errno, std::system_category());
    return std::make_pair(p[parent], p[child]);
}


int wright::detail::close(int &fd) {
    if (fd) ::close(fd);
    return fd = 0;
}


int wright::detail::dup(int fd) {
    auto nfd = ::dup(fd);
    if (nfd < 0) throw std::system_error(errno, std::system_category());
    return nfd;
}
