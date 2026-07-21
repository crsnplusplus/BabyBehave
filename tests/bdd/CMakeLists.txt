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

# Coverage variants: re-run SelfTest with different BABYBEHAVE_STYLE values
# to cover the ParseNarrationStyleEnv() branches for "arrow", "tree", and invalid values.
add_test(NAME SelfTestNarrationStyleArrow COMMAND test_bdd_SelfTest)
set_tests_properties(SelfTestNarrationStyleArrow PROPERTIES ENVIRONMENT "BABYBEHAVE_STYLE=arrow")

add_test(NAME SelfTestNarrationStyleTree COMMAND test_bdd_SelfTest)
set_tests_properties(SelfTestNarrationStyleTree PROPERTIES ENVIRONMENT "BABYBEHAVE_STYLE=tree")

add_test(NAME SelfTestNarrationStyleBogus COMMAND test_bdd_SelfTest)
set_tests_properties(SelfTestNarrationStyleBogus PROPERTIES ENVIRONMENT "BABYBEHAVE_STYLE=bogus")

# Gherkin interpreter self-hosted probes (BabyBehave::BDD::Gherkin -
# StepRegistry + RunFeature - see bdd.hpp and docs/design/gherkin-support.md).
# Same rationale as every file above: its own executable so new scenarios
# can be added independently, and so this measurement stays a genuinely
# separate bdd.hpp coverage source from the gtest-based UT half in
# tests/test_Gherkin_Parser.cpp / tests/test_Gherkin_Integration.cpp (see
# coverage-bbh in the root CMakeLists.txt).
# Needed now that RunFeature()'s enableParallelScenarios=true path (bdd.hpp)
# dispatches Scenarios via std::async(std::launch::async, ...) - both
# executables below include bdd.hpp's Gherkin support and call RunFeature().
# See tests/CMakeLists.txt for the matching rationale on its own
# find_package(Threads REQUIRED) call.
find_package(Threads REQUIRED)

add_executable(test_bdd_SelfTest_Gherkin test_SelfTest_Gherkin.cpp)
target_link_libraries(test_bdd_SelfTest_Gherkin PRIVATE BabyBehave::BabyBehave babybehave_coverage_flags babybehave_sanitizer_flags Threads::Threads)
add_test(NAME SelfTestGherkin COMMAND test_bdd_SelfTest_Gherkin)

# Same rationale as test_bdd_SelfTest_DefaultConditionExit/
# test_bdd_SelfTest_DefaultExceptionExit above, applied to Gherkin's own
# default GherkinFailureCallback (impl::DefaultGherkinFailureAction): no
# custom callback installed, so the real print+exit default fires and the
# process is EXPECTED to exit non-zero.
add_executable(test_bdd_SelfTest_Gherkin_DefaultFailureExit test_SelfTest_Gherkin_DefaultFailureExit.cpp)
target_link_libraries(test_bdd_SelfTest_Gherkin_DefaultFailureExit PRIVATE BabyBehave::BabyBehave babybehave_coverage_flags babybehave_sanitizer_flags Threads::Threads)
add_test(NAME SelfTestGherkinDefaultFailureExit COMMAND test_bdd_SelfTest_Gherkin_DefaultFailureExit)
set_tests_properties(SelfTestGherkinDefaultFailureExit PROPERTIES WILL_FAIL TRUE)
