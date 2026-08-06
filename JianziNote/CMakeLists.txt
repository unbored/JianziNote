# Copyright (c) 2024 King
# 
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

set(MODULE_NAME JianziNote)

message("Module ${MODULE_NAME}")

file(GLOB_RECURSE MODULE_SOURCE
    "*.h"
    "*.c"
    "*.hpp"
    "*.cpp"
)

find_package(SQLiteCpp CONFIG REQUIRED)
# SQLiteCpp's vcpkg target is directory-scoped by default. JianziNote is
# consumed from sibling CMake directories, so promote it and its SQLite
# dependency before exposing them through this library's public interface.
set_property(TARGET SQLiteCpp PROPERTY IMPORTED_GLOBAL TRUE)
if(TARGET unofficial::sqlite3::sqlite3)
    set_property(TARGET unofficial::sqlite3::sqlite3 PROPERTY IMPORTED_GLOBAL TRUE)
endif()
find_package(nlohmann_json CONFIG REQUIRED)
find_package(tinyutf8 CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Freetype REQUIRED)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

add_library(${MODULE_NAME} ${PROJECT_LINK_DYNAMIC_STATIC} ${MODULE_SOURCE})
target_link_libraries(${MODULE_NAME} PUBLIC SQLiteCpp)
target_link_libraries(${MODULE_NAME} PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(${MODULE_NAME} PUBLIC tinyutf8::tinyutf8)
target_link_libraries(${MODULE_NAME} PUBLIC magic_enum::magic_enum)
target_link_libraries(${MODULE_NAME} PUBLIC Freetype::Freetype)
