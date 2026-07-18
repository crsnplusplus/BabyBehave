# Self-hosted "BabyBehave testing BabyBehave" suite: each test_SelfTest*.cpp
# below is its own standalone executable/main(), driving bdd.hpp only
# through its own public fluent API (Given/With/When/Then/..., TestContext,
# SoftCheck, SetCollectFailuresMode, ...). This is the BBH half of the two
# independent bdd.hpp coverage measurements (see coverage-bbh in the root
# CMakeLists.txt); the gtest-based UT half lives in tests/ directly.
#
# Kept as one executable per file (rather than one shared binary) so new
# scenario files can be added independently without touching this list's
# other entries or racing on a shared main().

add_executable(test_bdd_SelfTest test_SelfTest.cpp)
target_link_libraries(test_bdd_SelfTest PRIVATE BabyBehave::BabyBehave babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(NAME SelfTest COMMAND test_bdd_SelfTest)

add_executable(test_bdd_SelfTest_ExceptionCoverage test_SelfTest_ExceptionCoverage.cpp)
target_link_libraries(test_bdd_SelfTest_ExceptionCoverage PRIVATE BabyBehave::BabyBehave babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(NAME SelfTestExceptionCoverage COMMAND test_bdd_SelfTest_ExceptionCoverage)

# These two intentionally have NO custom callbacks installed, so
# BabyBehaveTest's genuine default failure callbacks fire for real and
# call std::exit(EXIT_FAILURE) -- the only way to exercise that default
# path, which every OTHER BBH scenario file deliberately avoids by
# installing capture-instead-of-exit callbacks. WILL_FAIL TRUE tells
# ctest that a non-zero exit is the expected, passing outcome.
add_executable(test_bdd_SelfTest_DefaultConditionExit test_SelfTest_DefaultConditionExit.cpp)
target_link_libraries(test_bdd_SelfTest_DefaultConditionExit PRIVATE BabyBehave::BabyBehave babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(NAME SelfTestDefaultConditionExit COMMAND test_bdd_SelfTest_DefaultConditionExit)
set_tests_properties(SelfTestDefaultConditionExit PROPERTIES WILL_FAIL TRUE)

add_executable(test_bdd_SelfTest_DefaultExceptionExit test_SelfTest_DefaultExceptionExit.cpp)
target_link_libraries(test_bdd_SelfTest_DefaultExceptionExit PRIVATE BabyBehave::BabyBehave babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(NAME SelfTestDefaultExceptionExit COMMAND test_bdd_SelfTest_DefaultExceptionExit)
set_tests_properties(SelfTestDefaultExceptionExit PROPERTIES WILL_FAIL TRUE)

# Deliberately compiles bdd.hpp with __cpp_lib_source_location undefined
# (see the .cpp file's header comment) to exercise the location.empty()
# branch that is otherwise dead code on this toolchain.
add_executable(test_bdd_SelfTest_NoSourceLocation test_SelfTest_NoSourceLocation.cpp)
target_link_libraries(test_bdd_SelfTest_NoSourceLocation PRIVATE BabyBehave::BabyBehave babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(NAME SelfTestNoSourceLocation COMMAND test_bdd_SelfTest_NoSourceLocation)
