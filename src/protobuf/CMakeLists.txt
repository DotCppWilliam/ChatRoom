set(PROTOBUF_DIRS ${CMAKE_CURRENT_LIST_DIR})
set(PROTOBUF_SOURCES
    msg.pb.cc 
    proto_msg.cpp 
)

add_library(Protobuf STATIC ${PROTOBUF_SOURCES})
target_include_directories(Protobuf PUBLIC ${PROTOBUF_DIRS})
target_link_libraries(Protobuf PRIVATE Net)