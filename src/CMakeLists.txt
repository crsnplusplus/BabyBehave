cmake_minimum_required(VERSION 3.0)

project(BabyBehaveProject)

set(CMAKE_CXX_STANDARD 20)

add_library(BabyBehaveLib STATIC
	bdd.cpp
	../include/BabyBehave/bdd.hpp
)

target_include_directories(BabyBehaveLib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(BabyBehaveLib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/)
