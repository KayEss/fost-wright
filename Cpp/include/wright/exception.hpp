/*
    Copyright 2017, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#pragma once


#define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#define BOOST_COROUTINE_NO_DEPRECATION_WARNING


#include <functional>
#include <iostream>

#include <boost/coroutine/exceptions.hpp>

#include <cxxabi.h>


namespace wright {


    /// Exception recovery function that re-throws the exception.
    const auto rethrow = []() { throw; };

    /// Wrap a function to display exceptions
    const auto exception_decorator = [](auto fn, std::function<void(void)> recov = rethrow) {
        return [=](auto &&...a) {
                try {
                    return fn(a...);
                } catch ( boost::coroutines::detail::forced_unwind & ) {
                    throw;
                } catch ( std::exception &e ) {
                    std::cerr << e.what() << ": " << wright::c_child.value() << std::endl;
                    return recov();
                } catch ( ... ) {
                    std::cerr << "Unkown exception: " << wright::c_child.value() << " - "
                        << __cxxabiv1::__cxa_current_exception_type()->name() << std::endl;
                    return recov();
                }
            };
        };


}
