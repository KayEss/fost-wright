add_library(fost-wright
        configuration.cpp
        exec.capacity.cpp
        exec.childproc.cpp
        exec.echo.cpp
        exec.logging.cpp
        exec.netvisor.cpp
        exec.supervisor.cpp
        exec.watchdog.cpp
        net.connection.cpp
        net.packets.cpp
        net.server.cpp
        pipe.cpp
    )
target_include_directories(fost-wright PUBLIC ../include)
target_link_libraries(fost-wright boost_coroutine fost-hod)
set_target_properties(fost-wright PROPERTIES DEBUG_POSTFIX "-d")
install(TARGETS fost-wright LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ../include/wright DESTINATION include)
