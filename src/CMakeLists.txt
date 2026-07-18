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

install(TARGETS BabyBehaveLib EXPORT BabyBehaveTargets)
