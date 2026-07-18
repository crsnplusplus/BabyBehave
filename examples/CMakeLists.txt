cmake_minimum_required(VERSION 3.20)

project(BabyBehaveExamples VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(example_AlarmClock AlarmClock.cpp)
target_include_directories(example_AlarmClock PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(example_AlarmClock PRIVATE babybehave_coverage_flags babybehave_sanitizer_flags)

add_executable(example_Calculator Calculator.cpp)
target_include_directories(example_Calculator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(example_Calculator PRIVATE babybehave_coverage_flags babybehave_sanitizer_flags)

add_executable(example_Failing Failing.cpp)
target_include_directories(example_Failing PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(example_Failing PRIVATE babybehave_coverage_flags babybehave_sanitizer_flags)

add_executable(example_FlightBooking FlightBooking.cpp)
target_include_directories(example_FlightBooking PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(example_FlightBooking PRIVATE babybehave_coverage_flags babybehave_sanitizer_flags)

add_executable(example_Oven Oven.cpp)
target_include_directories(example_Oven PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(example_Oven PRIVATE babybehave_coverage_flags babybehave_sanitizer_flags)