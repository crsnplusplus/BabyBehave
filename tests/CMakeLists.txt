cmake_minimum_required(VERSION 3.20)

project(BabyBehaveTests)

set(CMAKE_CXX_STANDARD 23)

enable_testing()

# Needed by the Gherkin test binaries below (test_Gherkin_Parser/
# test_Gherkin_Integration, and their tests/bdd counterparts) now that
# RunFeature()'s enableParallelScenarios=true path (bdd.hpp) dispatches
# Scenarios via std::async(std::launch::async, ...). Mirrors the same
# find_package(Threads REQUIRED) already done in examples/CMakeLists.txt for
# its own threaded examples. On toolchains where pthread symbols are merged
# into libc (e.g. glibc >= 2.34) this is close to a no-op, but it is a real,
# platform-dependent link error to skip it elsewhere.
find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.16.0/googletest-1.16.0.tar.gz
)

FetchContent_MakeAvailable(googletest)

add_executable(test_TestContext test_Testcontext.cpp)
target_link_libraries(test_TestContext BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(TestContext test_TestContext)

add_executable(test_BabyBehaveTest test_BabyBehaveTest.cpp)
target_link_libraries(test_BabyBehaveTest BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(BabyBehaveTest test_BabyBehaveTest)

# Each of the following is its own executable (rather than an extra source
# on test_BabyBehaveTest above) for two reasons: it keeps every new file's
# gcov data in its own dedicated object directory (bdd.hpp being
# header-only means each TU compiles its own private copy of its inline
# code, so per-TU isolation is what makes the coverage-ut target's
# per-unit reporting meaningful), and it keeps the death tests below in a
# process of their own -- gtest death tests fork/re-exec the test binary,
# and sharing a binary with other tests risks that re-exec interfering
# with their coverage data (observed as bogus 0%-covered gcov output when
# they were previously bundled into test_BabyBehaveTest as extra sources).
add_executable(test_BabyBehaveTest_StepTypes test_BabyBehaveTest_StepTypes.cpp)
target_link_libraries(test_BabyBehaveTest_StepTypes BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(BabyBehaveTestStepTypes test_BabyBehaveTest_StepTypes)

add_executable(test_BabyBehaveTest_Exceptions test_BabyBehaveTest_Exceptions.cpp)
target_link_libraries(test_BabyBehaveTest_Exceptions BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(BabyBehaveTestExceptions test_BabyBehaveTest_Exceptions)

add_executable(test_BabyBehaveTest_SoftCheckAndCollectFailures test_BabyBehaveTest_SoftCheckAndCollectFailures.cpp)
target_link_libraries(test_BabyBehaveTest_SoftCheckAndCollectFailures BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(BabyBehaveTestSoftCheckAndCollectFailures test_BabyBehaveTest_SoftCheckAndCollectFailures)

add_executable(test_BabyBehaveTest_CallbacksAndDeathTests test_BabyBehaveTest_CallbacksAndDeathTests.cpp)
target_link_libraries(test_BabyBehaveTest_CallbacksAndDeathTests BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(BabyBehaveTestCallbacksAndDeathTests test_BabyBehaveTest_CallbacksAndDeathTests)

# Deliberately compiles bdd.hpp with __cpp_lib_source_location undefined
# (see the .cpp file's header comment) to exercise the location.empty()
# branch that is otherwise dead code on this toolchain.
add_executable(test_BabyBehaveTest_NoSourceLocation test_BabyBehaveTest_NoSourceLocation.cpp)
target_link_libraries(test_BabyBehaveTest_NoSourceLocation BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags)
add_test(BabyBehaveTestNoSourceLocation test_BabyBehaveTest_NoSourceLocation)

# Gherkin interpreter tests (BabyBehave::BDD::Gherkin - see bdd.hpp and
# docs/design/gherkin-support.md). Each #defines BABYBEHAVE_ENABLE_GHERKIN
# itself before #include <BabyBehave/bdd.hpp> - the same pattern already
# used by examples/Gherkin*.cpp - rather than depending on the
# babybehave_gherkin_flags/BABYBEHAVE_ENABLE_GHERKIN_TESTS option defined in
# the root CMakeLists.txt (that option/interface library isn't actually
# linked into any target today - examples/CMakeLists.txt's Gherkin
# executables don't use it either - so these two follow the convention
# that's actually in effect, not the seemingly-unused option). Own
# executables for the same per-TU coverage-isolation and death-test-process-
# isolation reasons as every other file in this list (test_Gherkin_Integration
# in particular contains EXPECT_EXIT death tests).
add_executable(test_Gherkin_Parser test_Gherkin_Parser.cpp)
target_link_libraries(test_Gherkin_Parser BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags Threads::Threads)
add_test(GherkinParser test_Gherkin_Parser)

add_executable(test_Gherkin_Integration test_Gherkin_Integration.cpp)
target_link_libraries(test_Gherkin_Integration BabyBehave::BabyBehave gtest_main babybehave_coverage_flags babybehave_sanitizer_flags Threads::Threads)
add_test(GherkinIntegration test_Gherkin_Integration)

add_subdirectory(bdd)

# BabyBehaveTest's step narration ("Given a: .../With: ...", printed as each
# step runs - see bdd.hpp's PrintLine()) is the whole point of the
# self-hosted BBH suite under bdd/, but here it just interleaves with
# gtest's own "[ RUN ]"/"[ OK ]" output. Silenced only for ctest runs of the
# gtest-based binaries above; running one of them directly still shows
# narration for debugging (see bdd.hpp's SetNarrationEnabled()).
set_tests_properties(
  TestContext
  BabyBehaveTest
  BabyBehaveTestStepTypes
  BabyBehaveTestExceptions
  BabyBehaveTestSoftCheckAndCollectFailures
  BabyBehaveTestCallbacksAndDeathTests
  BabyBehaveTestNoSourceLocation
  GherkinParser
  GherkinIntegration
  PROPERTIES ENVIRONMENT "BABYBEHAVE_QUIET=1"
)
