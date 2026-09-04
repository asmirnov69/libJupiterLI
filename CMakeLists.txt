cmake_minimum_required(VERSION 3.22)

set(CMAKE_VERBOSE_MAKEFILE OFF)

project(libjupiterli)

#exec_program(git ARGS "rev-parse --show-toplevel" OUTPUT_VARIABLE top)
execute_process(
    COMMAND git rev-parse --show-toplevel
    OUTPUT_VARIABLE top
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message("top is ${top}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++20 -Wall")
#set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-z,defs")

# Dependencies
#set(HTTPLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
#set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)
#add_subdirectory(external/cpp-httplib ${CMAKE_BINARY_DIR}/cpp-httplib)
add_subdirectory(external/cpp-httplib)

# mqtt
set(PAHO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/paho.mqtt.embedded-c)
include_directories(${top}/external/paho.mqtt.embedded-c/MQTTPacket/src)

include_directories(${top}/include)

add_subdirectory(src)
add_subdirectory(tests)
