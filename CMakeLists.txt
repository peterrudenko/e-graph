cmake_minimum_required(VERSION 3.11)

project(egraph CXX)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE "Debug")

include_directories(.)

add_executable(Tests Tests.cpp)
