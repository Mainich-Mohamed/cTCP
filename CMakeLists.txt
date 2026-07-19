cmake_minimum_required(VERSION 3.10)
project(cTCP C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")

add_executable(cTCP ${SOURCES})
target_include_directories(cTCP PRIVATE include)
