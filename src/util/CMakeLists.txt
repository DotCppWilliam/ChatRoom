aux_source_directory(. SOURCES)

add_library(Util STATIC ${SOURCES})
target_include_directories(Util PUBLIC ${CMAKE_CURRENT_LIST_DIR})
