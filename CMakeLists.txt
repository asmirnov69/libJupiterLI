cmake_minimum_required(VERSION 3.22)

set(CMAKE_VERBOSE_MAKEFILE ON)

project(libjupiterli)
exec_program(git ARGS "rev-parse --show-toplevel" OUTPUT_VARIABLE top)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++20 -Wall")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-z,defs")

# get access to cpp-httplib/httplib.h
include_directories("..")
include_directories(${top}/include)

add_subdirectory(src)
add_subdirectory(tests)
