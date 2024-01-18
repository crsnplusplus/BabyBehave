cmake_minimum_required(VERSION 3.0)

project(BabyBehaveTests)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.10.0
)

FetchContent_MakeAvailable(googletest)

add_executable(test_TestContext test_Testcontext.cpp)
target_include_directories(test_TestContext PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(test_TestContext gtest_main)

add_executable(test_BabyBehaveTest test_BabyBehaveTest.cpp)
target_include_directories(test_BabyBehaveTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(test_BabyBehaveTest gtest_main)


enable_testing()