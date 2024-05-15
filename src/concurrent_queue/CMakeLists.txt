aux_source_directory(. CONQUEUE_SOURCES)
set(CONQUEUE_DIRS ${CMAKE_CURRENT_LIST_DIR})

add_library(ConQueue STATIC ${CONQUEUE_SOURCES})
target_include_directories(ConQueue PUBLIC ${CONQUEUE_DIRS})

