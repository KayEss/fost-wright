add_executable(fost-wright-exec wright-exec-helper.cpp)
target_link_libraries(fost-wright-exec fost-cli fost-wright)
install(TARGETS fost-wright-exec EXPORT fost-webserver RUNTIME DESTINATION bin)

