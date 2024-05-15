set(HEADER_DIR
    "${CMAKE_CURRENT_LIST_DIR}/poller"
    "${CMAKE_CURRENT_LIST_DIR}/thread"
)

set(SOURCES 
    "${CMAKE_CURRENT_LIST_DIR}/thread/task_executor.cpp"
)


add_library(Net STATIC ${SOURCES})
target_include_directories(Net PUBLIC ${HEADER_DIR})
target_link_libraries(Net PRIVATE Log ConQueue)