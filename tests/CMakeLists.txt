add_executable(producer.tsk producer.cpp)
target_link_libraries(producer.tsk libjupiterli)

add_executable(mqtt-test.tsk mqtt-test.cpp)
target_link_libraries(mqtt-test.tsk MQTTPacketClient)
