add_executable(wright-exec-helper wright-exec-helper.cpp)
target_link_libraries(wright-exec-helper fost-cli fost-wright)
install(TARGETS wright-exec-helper EXPORT wright-exec-helper RUNTIME DESTINATION bin)

