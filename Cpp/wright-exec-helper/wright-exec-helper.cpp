/*
    Copyright 2016, Felspar Co Ltd. http://support.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <fost/cli>
#include <fost/main>
#include <fost/unicode>


using namespace fostlib;


FSL_MAIN(
    L"wright-exec-helper",
    L"Wright execution helper\nCopyright (c) 2016, Felspar Co. Ltd."
)( fostlib::ostream &out, fostlib::arguments &args ) {
    std::string command;
    while ( std::cin ) {
        std::getline(std::cin, command);
        if ( std::cin && not command.empty() ) out << command << std::endl;
    }
    return 0;
}

