# cmake -H. -Bbuild
# cmake --build build

cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_STANDARD 20)
project(CPP-Channels)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

include_directories(include)
file(GLOB SOURCES "src/*.cpp")
add_executable(channels_test ${SOURCES})
