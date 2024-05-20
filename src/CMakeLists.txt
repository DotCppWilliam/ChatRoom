add_subdirectory(util)
add_subdirectory(log)
add_subdirectory(net)
add_subdirectory(protobuf)
add_subdirectory(conqueue)


add_executable(${PROJECT_NAME} test.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE Net)