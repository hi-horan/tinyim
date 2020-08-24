project(dbproxy)

set(Protobuf_IMPORT_DIRS "${CMAKE_SOURCE_DIR}/tinyim/server")
message("------------Protobuf_IMPORT_DIRS: ${Protobuf_IMPORT_DIRS}")
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER dbproxy.proto)

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
    dbproxy.cc

    ${PROTO_SRC}
    ${PROTO_HEADER}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/..
        ${PROJECT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_BINARY_DIR}
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
        dl
)
