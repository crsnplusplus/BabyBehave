cmake_minimum_required(VERSION 3.20)

project(BabyBehaveProject)

set(CMAKE_CXX_STANDARD 23)

# BabyBehaveLib is header-only (bdd.cpp is an empty translation unit and is
# intentionally not compiled into this target); INTERFACE reflects that.
add_library(BabyBehaveLib INTERFACE)
add_library(BabyBehave::BabyBehave ALIAS BabyBehaveLib)
set_target_properties(BabyBehaveLib PROPERTIES EXPORT_NAME BabyBehave)

target_include_directories(BabyBehaveLib INTERFACE
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(BabyBehaveLib INTERFACE cxx_std_23)

# bdd.hpp's Tree narration style spells its box-drawing/check-mark glyphs as
# \u escapes (see include/BabyBehave/bdd.hpp's kTreeCheckMark and friends).
# GCC/Clang default to a UTF-8 execution charset and encode those correctly
# with no extra flag; MSVC does not, and needs /utf-8 (source AND execution
# charset) to encode them into valid UTF-8 output bytes instead of mangling
# or rejecting them. INTERFACE so it propagates to every consumer through
# BabyBehave::BabyBehave, same as this being header-only requires anyway.
target_compile_options(BabyBehaveLib INTERFACE
	$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

install(TARGETS BabyBehaveLib EXPORT BabyBehaveTargets)
