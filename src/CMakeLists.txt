add_library(libjupiterli OBJECT libjupiterli.cpp mqtt-client.cpp utils.cpp)
target_link_libraries(libjupiterli PRIVATE httplib::httplib MQTTPacketClient)
