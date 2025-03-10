cmake_minimum_required(VERSION 3.0)

project(BabyBehaveTests)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.16.0/googletest-1.16.0.tar.gz
)

FetchContent_MakeAvailable(googletest)

add_executable(test_TestContext test_Testcontext.cpp)
target_include_directories(test_TestContext PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(test_TestContext gtest_main)

add_executable(test_BabyBehaveTest test_BabyBehaveTest.cpp)
target_include_directories(test_BabyBehaveTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(test_BabyBehaveTest gtest_main)


enable_testing()