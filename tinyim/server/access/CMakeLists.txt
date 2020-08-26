project(access)

# message("Protobuf_IMPORT_DIRS: ${Protobuf_IMPORT_DIRS}")
# protobuf_generate_cpp(PROTO_SRC1 PROTO_HEADER1 messages.proto)
# protobuf_generate_cpp(PROTO_SRC2 PROTO_HEADER2 access.proto)


set(BRPC_INCLUDE_PATH "/home/horan/projects/incubator-brpc/zbuild/output/include")
set(BRPC_LIB "/home/horan/projects/incubator-brpc/zbuild/output/lib/libbrpc.a")

find_path(BRPC_INCLUDE_PATH NAMES brpc/server.h)
if(LINK_SO)
    find_library(BRPC_LIB NAMES brpc)
else()
    find_library(BRPC_LIB NAMES libbrpc.a brpc)
endif()


if((NOT BRPC_INCLUDE_PATH) OR (NOT BRPC_LIB))
    message(FATAL_ERROR "Fail to find brpc")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -D__const__= -pipe -W -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstrict-aliasing -Wno-invalid-offsetof")

add_executable(${PROJECT_NAME}
    access.cc

    access_service.cc
    access_service.h

    # ../../util/consistent_hash.h
    # ${CMAKE_BINARY_DIR}/tinyim/server/dbproxy/dbproxy.pb.cc
    # ${CMAKE_BINARY_DIR}/tinyim/server/dbproxy/dbproxy.pb.h

    # ${PROTO_SRC1}
    # ${PROTO_HEADER1}

    # ${PROTO_SRC2}
    # ${PROTO_HEADER2}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tinyim
        ${CMAKE_SOURCE_DIR}/tinyim/server
        # ${PROJECT_SOURCE_DIR}/..
        # ${PROJECT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/tinyim/server
        ${BRPC_INCLUDE_PATH}
        ${GFLAGS_INCLUDE_PATH}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${BRPC_LIB}
        ${PROTOBUF_LIBRARIES}
        ${GFLAGS_LIBRARIES}
        OpenSSL::SSL
        glog::glog
        leveldb::leveldb
        # tinyim::util
        tinyim::proto
        dl
)
