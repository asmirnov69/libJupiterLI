add_library(libjupiterli OBJECT libjupiterli.cpp utils.cpp)
target_link_libraries(libjupiterli PRIVATE httplib::httplib)
