rm *.o *.tsk
g++ --std=c++20 -g -c -I ../../libjupiterli/cpp ../../libjupiterli/cpp/jupiterli.cpp
g++ --std=c++20 -g -c -I ../../libjupiterli/cpp producer.cpp
g++ --std=c++20 -g producer.o jupiterli.o -o producer.tsk
