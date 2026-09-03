# deps

- cpp-httplib
- DO NOT USE: paho.mqtt.cpp
- use https://github.com/eclipse-paho/paho.mqtt.embedded-c for jli c++ clients

# getting code

DONT FORGET ABOUT SUBMODULES!!!!

git clone <repo url>
git submodule update --init --recursive

or

git clone --recurse-submodules <repo>

# build

```
cmake -S . -B build
cmake --build build
```
