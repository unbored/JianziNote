# Copyright (c) 2024 King
# 
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(JianziNote)

include(CTest)

# vs需要配置unicode以正确处理汉字
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # CMake's MSVC defaults contain /EHsc. Remove it in this directory scope
  # before the transitive no-exceptions target adds /EHs-c-.
  string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  add_definitions(-DUNICODE -D_UNICODE)
  add_compile_options("/utf-8")
endif()

add_library(JianziNoExceptions INTERFACE)
target_compile_definitions(JianziNoExceptions INTERFACE JSON_NOEXCEPTION)
if(MSVC)
    target_compile_definitions(JianziNoExceptions INTERFACE _HAS_EXCEPTIONS=0)
    target_compile_options(JianziNoExceptions INTERFACE /EHs-c-)
else()
    target_compile_options(JianziNoExceptions INTERFACE -fno-exceptions)
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

option(JIANZINOTE_BUILD_TOOLS "Build the JianziNote command-line tools" ON)
if(JIANZINOTE_BUILD_TOOLS)
    add_subdirectory(Jianzi2LaTeX)
    add_subdirectory(Jianzi2Lilypond)
    add_subdirectory(Jianzi2Svg)
endif()

if(BUILD_TESTING)
    add_executable(result-test tests/ResultTest.cpp)
    target_include_directories(result-test PRIVATE ${PROJECT_SOURCE_DIR}/JianziNote)
    target_link_libraries(result-test PRIVATE JianziNoExceptions)
    add_test(NAME result COMMAND result-test)

    add_executable(stroke-desc-runtime-test tests/StrokeDescRuntimeTest.cpp)
    target_link_libraries(stroke-desc-runtime-test PRIVATE JianziNote)
    target_include_directories(stroke-desc-runtime-test PRIVATE ${PROJECT_SOURCE_DIR}/JianziNote)
    add_test(NAME stroke_desc_runtime COMMAND stroke-desc-runtime-test)

    add_executable(utf8-test tests/Utf8Test.cpp)
    target_link_libraries(utf8-test PRIVATE JianziNote)
    add_test(NAME utf8 COMMAND utf8-test)

    add_executable(jianzi-instance-test tests/JianziInstanceTest.cpp)
    target_link_libraries(jianzi-instance-test PRIVATE JianziNote)
    target_include_directories(jianzi-instance-test PRIVATE ${PROJECT_SOURCE_DIR}/JianziNote)
    if(EXISTS "${JIANZINOTE_V2_TEST_LIBRARY}")
        add_test(
            NAME jianzi_instance
            COMMAND jianzi-instance-test ${JIANZINOTE_V2_TEST_LIBRARY} ${JIANZINOTE_V2_TEST_TEXT})
    endif()
endif()
