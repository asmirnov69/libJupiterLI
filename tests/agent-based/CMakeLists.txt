include_directories(${top}/src)
link_directories(${top}/src)

add_executable(producer.tsk producer.cpp)
target_link_libraries(producer.tsk jupiterli)

add_executable(agent.tsk agent.cpp)
target_link_libraries(agent.tsk jupiterli)

