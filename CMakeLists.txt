# Copyright (c) 2024 King
# 
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

cmake_minimum_required(VERSION 3.7)

project(JianziNote)

# vs需要配置unicode以正确处理汉字
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_definitions(-DUNICODE -D_UNICODE)
  add_compile_options("/utf-8")
  add_compile_options("/utf-8")
endif()

set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
option(PROJECT_LIBRARIES_STATIC "Build static library" ON)
if(PROJECT_LIBRARIES_STATIC)
    set(PROJECT_LINK_DYNAMIC_STATIC "STATIC")
else()
    set(PROJECT_LINK_DYNAMIC_STATIC "SHARED")
endif()

include_directories(
    "include"
)

link_directories(
    "lib"
)

add_subdirectory(JianziNote)
add_subdirectory(Jianzi2LaTeX)
