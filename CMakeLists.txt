# cmake -H. -Bbuild
# cmake --build build

cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
project(CPP-Channels)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)
add_executable(point_test ${PROJECT_SOURCE_DIR}/src/pointTest.cpp)
