set(CONQUEUE_DIRS ${CMAKE_CURRENT_LIST_DIR})
aux_source_directory(. CONQUEUE_SOURCES)


# 编译为静态库
add_library(Conqueue STATIC ${CONQUEUE_SOURCES})
target_include_directories(Conqueue PUBLIC ${CONQUEUE_DIRS})
