cmake_minimum_required(VERSION 3.8)

# Set the project name and version
project(BabyBehaveExamples VERSION 1.0)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add executable target with source files
add_executable(example_AlarmClock AlarmClock.cpp)
# Specify the include directory path for the header file
target_include_directories(example_AlarmClock PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

# Add executable target with source files
add_executable(example_Calculator Calculator.cpp)
target_include_directories(example_Calculator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")