cmake_minimum_required(VERSION 3.8)

project(BabyBehaveExamples VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(example_AlarmClock AlarmClock.cpp)
target_include_directories(example_AlarmClock PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

add_executable(example_Calculator Calculator.cpp)
target_include_directories(example_Calculator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

add_executable(example_Oven Oven.cpp)
target_include_directories(example_Oven PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
