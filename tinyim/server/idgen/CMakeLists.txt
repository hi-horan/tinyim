project(idgen_server)

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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -O0 -g -D__const__= -pipe -W -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe -fstrict-aliasing -Wno-invalid-offsetof")

add_executable(idgen_server idgen_server.cc idgen.cc)

message("project_source_dir: ${PROJECT_SOURCE_DIR}")
message("cmake_current_binary_dir: ${CMAKE_CURRENT_BINARY_DIR}")
message("glog include: ${GFLAGS_INCLUDE_PATH}")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tinyim
        ${CMAKE_SOURCE_DIR}/tinyim/server
        ${CMAKE_BINARY_DIR}/tinyim/server
        ${CMAKE_CURRENT_BINARY_DIR}
        ${BRPC_INCLUDE_PATH}
        ${GFLAGS_INCLUDE_PATH}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${BRPC_LIB}
        ${PROTOBUF_LIBRARIES}
        ${GFLAGS_LIBRARIES}
        # OpenSSL::SSL
        glog::glog
        leveldb::leveldb
        tinyim::proto
        dl
)

add_executable(idgen_client idgen_client.cc)

target_include_directories(idgen_client
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tinyim
        ${CMAKE_SOURCE_DIR}/tinyim/server
        ${CMAKE_BINARY_DIR}/tinyim/server
        ${CMAKE_CURRENT_BINARY_DIR}
        ${BRPC_INCLUDE_PATH}
        ${GFLAGS_INCLUDE_PATH}
)

target_link_libraries(idgen_client
    PRIVATE
        ${BRPC_LIB}
        ${PROTOBUF_LIBRARIES}
        ${GFLAGS_LIBRARIES}
        # OpenSSL::SSL
        glog::glog
        # leveldb::leveldb
        tinyim::proto
        dl
)
