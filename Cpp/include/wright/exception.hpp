/**
    Copyright 2017-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#pragma once


#include <fost/log>

#include <functional>
#include <iostream>

#include <boost/coroutine/exceptions.hpp>

#include <cxxabi.h>


namespace wright {


    /// Exception recovery function that re-throws the exception.
    const auto rethrow = []() { throw; };

    /// An exception recovery handler we can use here
    const auto exit_on_error = []() {
        fostlib::log::flush();
        std::exit(9);
    };


    /// Wrap a function to display exceptions
    const auto exception_decorator = [](auto fn,
                                        std::function<void(void)> recov =
                                                rethrow) {
        return [=](auto &&... a) {
            try {
                return fn(a...);
            } catch (boost::coroutines::detail::forced_unwind &) {
                throw;
            } catch (fostlib::exceptions::exception &e) {
                fostlib::log::flush();
                std::cerr << e << std::endl;
                return recov();
            } catch (std::exception &e) {
                fostlib::log::flush();
                std::cerr << e.what() << ": " << wright::c_child.value()
                          << std::endl;
                return recov();
            } catch (...) {
                fostlib::log::flush();
                std::cerr << "Unkown exception: " << wright::c_child.value()
                          << " - "
                          << __cxxabiv1::__cxa_current_exception_type()->name()
                          << std::endl;
                return recov();
            }
        };
    };


}
