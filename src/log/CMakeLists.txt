file(GLOB_RECURSE SOURCES *.cpp)

set(DEPEND_DIRS ${CMAKE_SOURCE_DIR}/src/util)

add_library(Log STATIC ${SOURCES})
target_include_directories(Log PUBLIC ${DEPEND_DIRS})
target_include_directories(Log PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/appenders
    ${CMAKE_CURRENT_LIST_DIR}/converters
    ${CMAKE_CURRENT_LIST_DIR}/formatters
    ${CMAKE_CURRENT_LIST_DIR}/helpers)
    
target_link_libraries(Log PUBLIC Util)




