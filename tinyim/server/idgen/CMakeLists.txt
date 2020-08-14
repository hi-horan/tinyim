project(idgen_server)

# option(LINK_SO "Whether examples are linked dynamically" OFF)

# execute_process(
    # COMMAND bash -c "find ${PROJECT_SOURCE_DIR}/../.. -type d -regex \".*output/include$\" | head -n1 | xargs dirname | tr -d '\n'"
    # OUTPUT_VARIABLE OUTPUT_PATH
# )

# set(CMAKE_PREFIX_PATH ${OUTPUT_PATH})

# include(FindThreads)
# include(FindProtobuf)
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER idgen.proto)


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

# find_path(GFLAGS_INCLUDE_PATH gflags/gflags.h)
# find_library(GFLAGS_LIBRARY NAMES gflags libgflags)
# if ((NOT GFLAGS_INCLUDE_PATH) OR (NOT GFLAGS_LIBRARY))
    # message(FATAL_ERROR "Fail to find gflags")
# endif()
# include_directories(${GFLAGS_INCLUDE_PATH})

# execute_process(
    # COMMAND bash -c "grep \"namespace [_A-Za-z0-9]\\+ {\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $2}' | tr -d '\n'"
    # OUTPUT_VARIABLE GFLAGS_NS
# )
# if(${GFLAGS_NS} STREQUAL "GFLAGS_NAMESPACE")
    # execute_process(
        # COMMAND bash -c "grep \"#define GFLAGS_NAMESPACE [_A-Za-z0-9]\\+\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $3}' | tr -d '\n'"
        # OUTPUT_VARIABLE GFLAGS_NS
    # )
# endif()

# set(CMAKE_CPP_FLAGS "${DEFINE_CLOCK_GETTIME} -DGFLAGS_NS=${GFLAGS_NS}")

# if(CMAKE_VERSION VERSION_LESS "3.1.3")
    # if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    # endif()
    # if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    # endif()
# else()
    # set(CMAKE_CXX_STANDARD 11)
    # set(CMAKE_CXX_STANDARD_REQUIRED ON)
# endif()

# find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
# find_library(LEVELDB_LIB NAMES leveldb)
# if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    # message(FATAL_ERROR "Fail to find leveldb")
# endif()
# include_directories(${LEVELDB_INCLUDE_PATH})

# include_directories(${OPENSSL_INCLUDE_DIR})
# find_package(OpenSSL)

set(CMAKE_CXX_FLAGS "${CMAKE_CPP_FLAGS}  -O0 -g -D__const__= -pipe -W -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CPP_FLAGS} -pipe -fstrict-aliasing -Wno-invalid-offsetof")

add_executable(idgen_server idgen_server.cc idgen.cc ${PROTO_SRC} ${PROTO_HEADER})
# add_executable(idgen_client idgen_client.cc ${PROTO_SRC} ${PROTO_HEADER})

# set(DYNAMIC_LIB
    # ${CMAKE_THREAD_LIBS_INIT}
    # ${GFLAGS_LIBRARY}
    # ${PROTOBUF_LIBRARIES}
    # ${LEVELDB_LIB}
    # ${OPENSSL_CRYPTO_LIBRARY}
    # ${OPENSSL_SSL_LIBRARY}
    # dl
    # )
message("project_source_dir: ${PROJECT_SOURCE_DIR}")
message("cmake_current_binary_dir: ${CMAKE_CURRENT_BINARY_DIR}")
message("glog include: ${GFLAGS_INCLUDE_PATH}")

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/..
        ${PROJECT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_BINARY_DIR}
        ${BRPC_INCLUDE_PATH}
        ${GFLAGS_INCLUDE_PATH}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${BRPC_LIB}
        ${PROTOBUF_LIBRARIES}
        ${GFLAGS_LIBRARIES}
        OpenSSL::SSL
        glog::glog
        leveldb::leveldb
        # tinyim::util
        dl
)
# target_link_libraries(idgen_client ${BRPC_LIB} ${DYNAMIC_LIB})

add_executable(idgen_client idgen_client.cc ${PROTO_SRC} ${PROTO_HEADER})

target_include_directories(idgen_client
    PRIVATE
        ${PROJECT_SOURCE_DIR}/..
        ${PROJECT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_BINARY_DIR}
        ${BRPC_INCLUDE_PATH}
        ${GFLAGS_INCLUDE_PATH}
)

target_link_libraries(idgen_client
    PUBLIC
        ${BRPC_LIB}
        ${PROTOBUF_LIBRARIES}
        ${GFLAGS_LIBRARIES}
        OpenSSL::SSL
        glog::glog
        leveldb::leveldb
        # tinyim::util
        dl
)
