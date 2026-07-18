cmake_minimum_required(VERSION 3.20)
project(BabyBehave)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
