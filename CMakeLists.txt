cmake_minimum_required(VERSION 3.16)
project(tinyim)

set(CMAKE_CXX_STANDARD 20)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(glog REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gflags REQUIRED)
find_package(leveldb REQUIRED)
find_package(OpenSSL REQUIRED)
# find_package(toml11 REQUIRED)    # for reading config file
# find_package(RapidJSON REQUIRED)
# find_package(hiredis REQUIRED)
find_package(MySQL REQUIRED)
find_package(Soci REQUIRED)

# arget_include_directories(${PROJECT_NAME} PUBLIC ${MySQL_INCLUDE_DIRS})
# target_link_libraries(${PROJECT_NAME} PUBLIC ${MySQL_LIBRARIES})

message("project_source_dir: ${PROJECT_SOURCE_DIR}")
message("cmake_current_binary_dir: ${CMAKE_CURRENT_BINARY_DIR}")

include(CompileProto)
set(PROTO_FILES common/messages.proto
                access/access.proto
                idgen/idgen.proto
                logic/logic.proto
                dbproxy/dbproxy.proto
)
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/tinyim/server)
set(PROTOC_FLAGS ${PROTOC_FLAGS} -I${PROTOBUF_INCLUDE_DIR})
compile_proto(PROTO_HDRS PROTO_SRCS ${PROJECT_BINARY_DIR}/tinyim/server
                                    ${PROJECT_BINARY_DIR}/tinyim/server
                                    ${PROJECT_SOURCE_DIR}/tinyim/server
                                    "${PROTO_FILES}")
add_library(tinyim_proto_lib OBJECT ${PROTO_SRCS} ${PROTO_HDRS})
add_library(tinyim::proto ALIAS tinyim_proto_lib)
target_include_directories(tinyim_proto_lib
    PRIVATE
        ${PROJECT_BINARY_DIR}/tinyim/server
)

add_subdirectory(tinyim/cmd_client)
add_subdirectory(tinyim/server/access)
add_subdirectory(tinyim/server/idgen)

add_subdirectory(tinyim/server/dbproxy)
add_subdirectory(tinyim/server/logic)
