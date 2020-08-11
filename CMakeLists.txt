cmake_minimum_required(VERSION 3.16)
project(tinyim)


set(CMAKE_CXX_STANDARD 20)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(glog REQUIRED)
# find_package(Libevent REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gflags REQUIRED)
find_package(leveldb REQUIRED)
find_package(OpenSSL REQUIRED)
# find_package(toml11 REQUIRED)    # for reading config file
# find_package(RapidJSON REQUIRED) # for sso
# find_package(hiredis REQUIRED)
# find_package(MySQL REQUIRED)
# find_package(Soci REQUIRED)

# arget_include_directories(${PROJECT_NAME} PUBLIC ${MySQL_INCLUDE_DIRS})
# target_link_libraries(${PROJECT_NAME} PUBLIC ${MySQL_LIBRARIES})

# include_directories(${PROJECT_SOURCE_DIR}/tinyim/server)
message("project_source_dir: ${PROJECT_SOURCE_DIR}")
message("cmake_current_binary_dir: ${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(tinyim/util)
# add_subdirectory(tinyim/codec)
# add_subdirectory(tinyim/server/access)
# add_subdirectory(tinyim/server/sso)
add_subdirectory(tinyim/server/idgen)
# add_subdirectory(tinyim/client)
