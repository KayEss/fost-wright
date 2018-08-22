/**
    Copyright 2016-2018, Felspar Co Ltd. <http://support.felspar.com/>

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
*/


#include <cstddef>
#include <experimental/optional>
#include <tuple>
#include <utility>

#include <boost/asio.hpp>


namespace wright {


    namespace detail {


        /// Return two pipe filedescriptors
        std::pair<int, int> pipe_fds(std::size_t, std::size_t);
        /// Duplicate a file descriptor
        int dup(int);
        /// Close the file descriptor and set to zer
        int close(int &fd);

        /// A pipe for use between a parent process and its child. The
        /// direction of the pipe is controlled by the template parameters.
        /// Applications should use the pipe_in or pipe_out classes instead.
        template<std::size_t PFD, std::size_t CFD>
        class pipe final {
            int parent_fd = 0;
            std::experimental::optional<boost::asio::posix::stream_descriptor> parent_sd;
            int child_fd = 0;
        public:
            /// Create a new pipe
            pipe() {
                std::tie(parent_fd, child_fd) = detail::pipe_fds(PFD, CFD);
            }
            /// Make movable
            pipe(pipe &&p)
            : parent_fd(p.parent_fd),
                parent_sd(std::move(p.parent_sd)),
                child_fd(p.child_fd)
            {
                p.parent_fd = 0;
                p.child_fd = 0;
            }
            /// Close the file handles
            ~pipe() {
                close();
            }
            /// Close the file handles
            void close() {
                detail::close(parent_fd);
                if ( parent_sd ) (*parent_sd).close();
                detail::close(child_fd);
            }

            /// Return a copy of the descriptor for the child end. Should be
            /// passed dup2 to set up child end of pipe.
            int child() const {
                return child_fd;
            }
            /// Return the parent in a form usable for ASIO
            boost::asio::posix::stream_descriptor &parent(boost::asio::io_service &ios) {
                if ( not parent_sd ) {
                    parent_sd = boost::asio::posix::stream_descriptor(ios, detail::dup(parent_fd));
                }
                return parent_sd.value();
            }
        };


    }


    /// Represents a pipe used to connect to STDIN on a child process
    using pipe_in = detail::pipe<1u, 0u>;


    /// Represents a pipe used to connect to STDOUT (or STDERR) on a child
    /// process
    using pipe_out = detail::pipe<0u, 1u>;


}

