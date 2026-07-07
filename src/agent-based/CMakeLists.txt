#include_directories(${top}/include ${top}/src ${KVAN_HOME}/cpp/include ${VRGS_HOME}/include)
file(GLOB SRC_FILES *.cpp)
add_library(jupiterli SHARED ${SRC_FILES})
