aux_source_directory(. SOURCES)
set(UTIL_DIRS ${CMAKE_CURRENT_LIST_DIR})

add_library(Util STATIC ${SOURCES})
target_include_directories(Util PUBLIC ${UTIL_DIRS})
