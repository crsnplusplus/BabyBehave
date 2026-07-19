cmake_minimum_required(VERSION 3.20)
project(BabyBehave VERSION 0.7.19 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# --- Code coverage (gcov/lcov) -------------------------------------------
#
# Usage:
#   cmake -B build -DBABYBEHAVE_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build -j
#   ctest --test-dir build
#   lcov --capture --directory build --output-file coverage.info
#   genhtml coverage.info --output-directory coverage-report
#
# (If lcov/genhtml are both found on the system, a convenience
# `coverage-report` target is also generated below that does the capture +
# genhtml steps for you; see BABYBEHAVE_ENABLE_COVERAGE handling further
# down.)
option(BABYBEHAVE_ENABLE_COVERAGE "Enable gcov/lcov code coverage instrumentation (GCC/Clang debug builds only)" OFF)

add_library(babybehave_coverage_flags INTERFACE)
if(BABYBEHAVE_ENABLE_COVERAGE)
	if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Release")
		message(STATUS "BABYBEHAVE_ENABLE_COVERAGE is ON but CMAKE_BUILD_TYPE is '${CMAKE_BUILD_TYPE}'; "
			"coverage results are most meaningful with -DCMAKE_BUILD_TYPE=Debug (unoptimized, no inlining).")
	endif()
	target_compile_options(babybehave_coverage_flags INTERFACE --coverage)
	target_link_options(babybehave_coverage_flags INTERFACE --coverage)

	find_program(LCOV_PATH lcov)
	find_program(GENHTML_PATH genhtml)
	if(LCOV_PATH AND GENHTML_PATH)
		add_custom_target(coverage-report
			COMMAND ${LCOV_PATH} --capture --directory ${CMAKE_BINARY_DIR} --output-file ${CMAKE_BINARY_DIR}/coverage.info
			COMMAND ${GENHTML_PATH} ${CMAKE_BINARY_DIR}/coverage.info --output-directory ${CMAKE_BINARY_DIR}/coverage-report
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT "Generating lcov HTML coverage report in ${CMAKE_BINARY_DIR}/coverage-report"
			VERBATIM
		)
	else()
		message(STATUS "lcov and/or genhtml not found; skipping 'coverage-report' target (run lcov/genhtml manually instead).")
	endif()
endif()

# --- Sanitizers (ASan + UBSan) --------------------------------------------
#
# Usage:
#   cmake -B build -DBABYBEHAVE_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build -j
#   ctest --test-dir build --output-on-failure
option(BABYBEHAVE_ENABLE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

add_library(babybehave_sanitizer_flags INTERFACE)
if(BABYBEHAVE_ENABLE_SANITIZERS)
	if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Release")
		message(STATUS "BABYBEHAVE_ENABLE_SANITIZERS is ON but CMAKE_BUILD_TYPE is '${CMAKE_BUILD_TYPE}'; "
			"sanitizer diagnostics are most useful with -DCMAKE_BUILD_TYPE=Debug.")
	endif()
	target_compile_options(babybehave_sanitizer_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(babybehave_sanitizer_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

add_subdirectory(src)
add_subdirectory(examples)

enable_testing()
add_subdirectory(tests)

# --- Install / find_package support ---------------------------------------
#
# BabyBehaveLib (aliased as BabyBehave::BabyBehave) is header-only, so this
# is just headers + a CMake package config; nothing to link/version by ABI.
install(DIRECTORY include/BabyBehave DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT BabyBehaveTargets
	NAMESPACE BabyBehave::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BabyBehave
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
	${CMAKE_SOURCE_DIR}/cmake/BabyBehaveConfig.cmake.in
	${CMAKE_BINARY_DIR}/BabyBehaveConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BabyBehave
)
write_basic_package_version_file(
	${CMAKE_BINARY_DIR}/BabyBehaveConfigVersion.cmake
	COMPATIBILITY SameMajorVersion
	ARCH_INDEPENDENT
)
install(FILES
	${CMAKE_BINARY_DIR}/BabyBehaveConfig.cmake
	${CMAKE_BINARY_DIR}/BabyBehaveConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BabyBehave
)

# --- Coverage reporting targets (gcov only; no lcov/genhtml/gcovr) -------
#
# Two independent bdd.hpp coverage measurements:
#   coverage-ut  - bdd.hpp as exercised by the gtest unit test suite only
#                  (test_TestContext + test_BabyBehaveTest)
#   coverage-bbh - bdd.hpp as exercised by the self-hosted BabyBehave
#                  self-test suite only (tests/bdd/test_SelfTest*.cpp)
#
# Because bdd.hpp is header-only, each translation unit that includes it
# compiles and instruments its own private copy of its inline code, with
# its own .gcno/.gcda pair inside that target's own CMakeFiles/<target>.dir
# directory. Pointing gcov's --object-directory only at the object
# directories that belong to one measurement's binaries keeps the two
# reports naturally isolated from each other -- no counter reset needed
# between the two, only between repeated runs of the *same* report (hence
# the stale .gcda removal below).
#
# Usage:
#   cmake --build build --target coverage-ut
#   cmake --build build --target coverage-bbh
if(BABYBEHAVE_ENABLE_COVERAGE)
	find_program(GCOV_PATH gcov)
	if(NOT GCOV_PATH)
		message(STATUS "gcov not found; skipping 'coverage-ut'/'coverage-bbh' targets.")
	else()
		set(_babybehave_coverage_script "${CMAKE_SOURCE_DIR}/cmake/coverage_report.cmake")

		set(_tc_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_TestContext.dir")
		set(_bbt_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_BabyBehaveTest.dir")
		set(_tc_obj "${_tc_objdir}/test_Testcontext.cpp.o")
		set(_bbt_obj "${_bbt_objdir}/test_BabyBehaveTest.cpp.o")
		set(_tc_gcda "${_tc_objdir}/test_Testcontext.cpp.gcda")
		set(_bbt_gcda "${_bbt_objdir}/test_BabyBehaveTest.cpp.gcda")

		# Each of these is its OWN executable (see tests/CMakeLists.txt) so
		# its gcov data lives in its own object directory -- both for
		# per-unit reporting clarity and so the death tests in
		# test_BabyBehaveTest_CallbacksAndDeathTests (which fork/re-exec
		# the test binary) can never interfere with another unit's
		# coverage data by sharing a process with it.
		set(_bbt2_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_BabyBehaveTest_StepTypes.dir")
		set(_bbt2_obj "${_bbt2_objdir}/test_BabyBehaveTest_StepTypes.cpp.o")
		set(_bbt2_gcda "${_bbt2_objdir}/test_BabyBehaveTest_StepTypes.cpp.gcda")
		set(_bbt3_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_BabyBehaveTest_Exceptions.dir")
		set(_bbt3_obj "${_bbt3_objdir}/test_BabyBehaveTest_Exceptions.cpp.o")
		set(_bbt3_gcda "${_bbt3_objdir}/test_BabyBehaveTest_Exceptions.cpp.gcda")
		set(_bbt4_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_BabyBehaveTest_SoftCheckAndCollectFailures.dir")
		set(_bbt4_obj "${_bbt4_objdir}/test_BabyBehaveTest_SoftCheckAndCollectFailures.cpp.o")
		set(_bbt4_gcda "${_bbt4_objdir}/test_BabyBehaveTest_SoftCheckAndCollectFailures.cpp.gcda")
		set(_bbt5_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_BabyBehaveTest_CallbacksAndDeathTests.dir")
		set(_bbt5_obj "${_bbt5_objdir}/test_BabyBehaveTest_CallbacksAndDeathTests.cpp.o")
		set(_bbt5_gcda "${_bbt5_objdir}/test_BabyBehaveTest_CallbacksAndDeathTests.cpp.gcda")

		# Compiles bdd.hpp with __cpp_lib_source_location undefined, which is
		# the only way to exercise SafeInvokeExceptionCallback()'s
		# location.empty() branch (dead code on a toolchain that has
		# source_location) -- see tests/test_BabyBehaveTest_NoSourceLocation.cpp.
		set(_bbt6_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_BabyBehaveTest_NoSourceLocation.dir")
		set(_bbt6_obj "${_bbt6_objdir}/test_BabyBehaveTest_NoSourceLocation.cpp.o")
		set(_bbt6_gcda "${_bbt6_objdir}/test_BabyBehaveTest_NoSourceLocation.cpp.gcda")

		# Gherkin interpreter gtest suites (BabyBehave::BDD::Gherkin - see
		# tests/CMakeLists.txt's own comment on these two targets). Own
		# object directories, same as every other unit above; included here
		# (v0.9.0 Phase 0) so bdd.hpp's Gherkin namespace - previously
		# entirely absent from coverage-ut's measured line set, since no UT
		# unit instantiated it - is actually counted.
		set(_gp_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_Gherkin_Parser.dir")
		set(_gp_obj "${_gp_objdir}/test_Gherkin_Parser.cpp.o")
		set(_gp_gcda "${_gp_objdir}/test_Gherkin_Parser.cpp.gcda")
		set(_gi_objdir "${CMAKE_BINARY_DIR}/tests/CMakeFiles/test_Gherkin_Integration.dir")
		set(_gi_obj "${_gi_objdir}/test_Gherkin_Integration.cpp.o")
		set(_gi_gcda "${_gi_objdir}/test_Gherkin_Integration.cpp.gcda")

		# BABYBEHAVE_QUIET=1: these are the gtest-based UT binaries (see
		# tests/CMakeLists.txt's set_tests_properties() for the ctest-side
		# equivalent) - their step narration otherwise interleaves with
		# gtest's own "[ RUN ]"/"[ OK ]" output.
		add_custom_target(coverage-ut
			COMMAND ${CMAKE_COMMAND} -E rm -f "${_tc_gcda}" "${_bbt_gcda}" "${_bbt2_gcda}" "${_bbt3_gcda}" "${_bbt4_gcda}" "${_bbt5_gcda}" "${_bbt6_gcda}" "${_gp_gcda}" "${_gi_gcda}"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_TestContext> || true"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_BabyBehaveTest> || true"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_BabyBehaveTest_StepTypes> || true"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_BabyBehaveTest_Exceptions> || true"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_BabyBehaveTest_SoftCheckAndCollectFailures> || true"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_BabyBehaveTest_CallbacksAndDeathTests> || true"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_BabyBehaveTest_NoSourceLocation> || true"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_Gherkin_Parser> || true"
			COMMAND sh -c "BABYBEHAVE_QUIET=1 $<TARGET_FILE:test_Gherkin_Integration> || true"
			COMMAND ${CMAKE_COMMAND}
				-DCOV_LABEL=UT
				-DCOV_OUTFILE=${CMAKE_BINARY_DIR}/coverage-ut.txt
				-DCOV_WORKDIR=${CMAKE_BINARY_DIR}/coverage-ut
				-DCOV_GCOV=${GCOV_PATH}
				-DCOV_COMBINE=ON
				"-DCOV_UNITS=test_TestContext|${_tc_objdir}|${_tc_obj};test_BabyBehaveTest|${_bbt_objdir}|${_bbt_obj};test_BabyBehaveTest_StepTypes|${_bbt2_objdir}|${_bbt2_obj};test_BabyBehaveTest_Exceptions|${_bbt3_objdir}|${_bbt3_obj};test_BabyBehaveTest_SoftCheckAndCollectFailures|${_bbt4_objdir}|${_bbt4_obj};test_BabyBehaveTest_CallbacksAndDeathTests|${_bbt5_objdir}|${_bbt5_obj};test_BabyBehaveTest_NoSourceLocation|${_bbt6_objdir}|${_bbt6_obj};test_Gherkin_Parser|${_gp_objdir}|${_gp_obj};test_Gherkin_Integration|${_gi_objdir}|${_gi_obj}"
				-P "${_babybehave_coverage_script}"
			DEPENDS test_TestContext test_BabyBehaveTest test_BabyBehaveTest_StepTypes test_BabyBehaveTest_Exceptions test_BabyBehaveTest_SoftCheckAndCollectFailures test_BabyBehaveTest_CallbacksAndDeathTests test_BabyBehaveTest_NoSourceLocation test_Gherkin_Parser test_Gherkin_Integration
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT "Measuring bdd.hpp coverage from the gtest unit test suite (UT)"
			VERBATIM
		)

		set(_st_objdir "${CMAKE_BINARY_DIR}/tests/bdd/CMakeFiles/test_bdd_SelfTest.dir")
		set(_st_obj "${_st_objdir}/test_SelfTest.cpp.o")
		set(_st_gcda "${_st_objdir}/test_SelfTest.cpp.gcda")

		set(_st2_objdir "${CMAKE_BINARY_DIR}/tests/bdd/CMakeFiles/test_bdd_SelfTest_ExceptionCoverage.dir")
		set(_st2_obj "${_st2_objdir}/test_SelfTest_ExceptionCoverage.cpp.o")
		set(_st2_gcda "${_st2_objdir}/test_SelfTest_ExceptionCoverage.cpp.gcda")

		# These three exercise paths no other BBH scenario reaches: the two
		# Default*Exit binaries install no custom callbacks, so
		# BabyBehaveTest's genuine default failure callbacks fire and
		# std::exit() -- hence "|| true" below, same treatment as the
		# death-test-containing UT unit above. NoSourceLocation compiles
		# bdd.hpp with __cpp_lib_source_location undefined, exercising the
		# location.empty() branch.
		set(_st3_objdir "${CMAKE_BINARY_DIR}/tests/bdd/CMakeFiles/test_bdd_SelfTest_DefaultConditionExit.dir")
		set(_st3_obj "${_st3_objdir}/test_SelfTest_DefaultConditionExit.cpp.o")
		set(_st3_gcda "${_st3_objdir}/test_SelfTest_DefaultConditionExit.cpp.gcda")

		set(_st4_objdir "${CMAKE_BINARY_DIR}/tests/bdd/CMakeFiles/test_bdd_SelfTest_DefaultExceptionExit.dir")
		set(_st4_obj "${_st4_objdir}/test_SelfTest_DefaultExceptionExit.cpp.o")
		set(_st4_gcda "${_st4_objdir}/test_SelfTest_DefaultExceptionExit.cpp.gcda")

		set(_st5_objdir "${CMAKE_BINARY_DIR}/tests/bdd/CMakeFiles/test_bdd_SelfTest_NoSourceLocation.dir")
		set(_st5_obj "${_st5_objdir}/test_SelfTest_NoSourceLocation.cpp.o")
		set(_st5_gcda "${_st5_objdir}/test_SelfTest_NoSourceLocation.cpp.gcda")

		# Gherkin self-hosted probes (v0.9.0 Phase 0) - same per-TU
		# object-directory isolation as every unit above, applied to
		# BabyBehave::BDD::Gherkin (StepRegistry + RunFeature) instead of the
		# non-Gherkin fluent API. test_bdd_SelfTest_Gherkin_DefaultFailureExit
		# gets the same "|| true" treatment as the two Default*Exit units
		# above, for the same reason: it installs no custom
		# GherkinFailureCallback, so RunFeature()'s genuine default
		# (print + std::exit) fires for real.
		set(_stg_objdir "${CMAKE_BINARY_DIR}/tests/bdd/CMakeFiles/test_bdd_SelfTest_Gherkin.dir")
		set(_stg_obj "${_stg_objdir}/test_SelfTest_Gherkin.cpp.o")
		set(_stg_gcda "${_stg_objdir}/test_SelfTest_Gherkin.cpp.gcda")

		set(_stg2_objdir "${CMAKE_BINARY_DIR}/tests/bdd/CMakeFiles/test_bdd_SelfTest_Gherkin_DefaultFailureExit.dir")
		set(_stg2_obj "${_stg2_objdir}/test_SelfTest_Gherkin_DefaultFailureExit.cpp.o")
		set(_stg2_gcda "${_stg2_objdir}/test_SelfTest_Gherkin_DefaultFailureExit.cpp.gcda")

		add_custom_target(coverage-bbh
			COMMAND ${CMAKE_COMMAND} -E rm -f "${_st_gcda}" "${_st2_gcda}" "${_st3_gcda}" "${_st4_gcda}" "${_st5_gcda}" "${_stg_gcda}" "${_stg2_gcda}"
			COMMAND sh -c "$<TARGET_FILE:test_bdd_SelfTest> || true"
			COMMAND sh -c "$<TARGET_FILE:test_bdd_SelfTest_ExceptionCoverage> || true"
			COMMAND sh -c "$<TARGET_FILE:test_bdd_SelfTest_DefaultConditionExit> || true"
			COMMAND sh -c "$<TARGET_FILE:test_bdd_SelfTest_DefaultExceptionExit> || true"
			COMMAND sh -c "$<TARGET_FILE:test_bdd_SelfTest_NoSourceLocation> || true"
			COMMAND sh -c "$<TARGET_FILE:test_bdd_SelfTest_Gherkin> || true"
			COMMAND sh -c "$<TARGET_FILE:test_bdd_SelfTest_Gherkin_DefaultFailureExit> || true"
			COMMAND ${CMAKE_COMMAND}
				-DCOV_LABEL=BBH
				-DCOV_OUTFILE=${CMAKE_BINARY_DIR}/coverage-bbh.txt
				-DCOV_WORKDIR=${CMAKE_BINARY_DIR}/coverage-bbh
				-DCOV_GCOV=${GCOV_PATH}
				-DCOV_COMBINE=ON
				"-DCOV_UNITS=test_bdd_SelfTest|${_st_objdir}|${_st_obj};test_bdd_SelfTest_ExceptionCoverage|${_st2_objdir}|${_st2_obj};test_bdd_SelfTest_DefaultConditionExit|${_st3_objdir}|${_st3_obj};test_bdd_SelfTest_DefaultExceptionExit|${_st4_objdir}|${_st4_obj};test_bdd_SelfTest_NoSourceLocation|${_st5_objdir}|${_st5_obj};test_bdd_SelfTest_Gherkin|${_stg_objdir}|${_stg_obj};test_bdd_SelfTest_Gherkin_DefaultFailureExit|${_stg2_objdir}|${_stg2_obj}"
				-P "${_babybehave_coverage_script}"
			DEPENDS test_bdd_SelfTest test_bdd_SelfTest_ExceptionCoverage test_bdd_SelfTest_DefaultConditionExit test_bdd_SelfTest_DefaultExceptionExit test_bdd_SelfTest_NoSourceLocation test_bdd_SelfTest_Gherkin test_bdd_SelfTest_Gherkin_DefaultFailureExit
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT "Measuring bdd.hpp coverage from the self-hosted BabyBehave self-test suite (BBH)"
			VERBATIM
		)

		# --- Coverage gates (CI-enforced minimum) ---------------------------
		#
		# coverage-ut-gate/coverage-bbh-gate depend on (and so first rebuild)
		# coverage-ut/coverage-bbh above, then fail the build via
		# check_coverage_threshold.cmake if that report's bdd.hpp "Lines
		# executed" percentage is below BABYBEHAVE_MIN_COVERAGE_PERCENT.
		# coverage-gate runs both and is what CI invokes; it only succeeds
		# when BOTH the UT and BBH measurements clear the threshold.
		#
		# Usage:
		#   cmake --build build --target coverage-gate
		#   cmake --build build --target coverage-gate -- (or re-run cmake) \
		#     -DBABYBEHAVE_MIN_COVERAGE_PERCENT=95   # to require a different bar
		set(BABYBEHAVE_MIN_COVERAGE_PERCENT "90" CACHE STRING
			"Minimum required bdd.hpp 'Lines executed' percentage for coverage-ut-gate/coverage-bbh-gate/coverage-gate to pass")

		set(_babybehave_coverage_gate_script "${CMAKE_SOURCE_DIR}/cmake/check_coverage_threshold.cmake")

		add_custom_target(coverage-ut-gate
			COMMAND ${CMAKE_COMMAND}
				-DCOV_FILE=${CMAKE_BINARY_DIR}/coverage-ut.txt
				-DCOV_MIN_PERCENT=${BABYBEHAVE_MIN_COVERAGE_PERCENT}
				-DCOV_LABEL=UT
				-P "${_babybehave_coverage_gate_script}"
			DEPENDS coverage-ut
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT "Enforcing bdd.hpp UT coverage >= ${BABYBEHAVE_MIN_COVERAGE_PERCENT}%"
			VERBATIM
		)

		add_custom_target(coverage-bbh-gate
			COMMAND ${CMAKE_COMMAND}
				-DCOV_FILE=${CMAKE_BINARY_DIR}/coverage-bbh.txt
				-DCOV_MIN_PERCENT=${BABYBEHAVE_MIN_COVERAGE_PERCENT}
				-DCOV_LABEL=BBH
				-P "${_babybehave_coverage_gate_script}"
			DEPENDS coverage-bbh
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT "Enforcing bdd.hpp BBH coverage >= ${BABYBEHAVE_MIN_COVERAGE_PERCENT}%"
			VERBATIM
		)

		add_custom_target(coverage-gate DEPENDS coverage-ut-gate coverage-bbh-gate)
	endif()
endif()
