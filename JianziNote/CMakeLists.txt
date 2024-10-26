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
find_package(tinyutf8 CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Freetype REQUIRED)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

add_library(${MODULE_NAME} ${PROJECT_LINK_DYNAMIC_STATIC} ${MODULE_SOURCE})
target_link_libraries(${MODULE_NAME} PUBLIC SQLiteCpp)
target_link_libraries(${MODULE_NAME} PUBLIC tinyutf8::tinyutf8)
target_link_libraries(${MODULE_NAME} PUBLIC magic_enum::magic_enum)
target_link_libraries(${MODULE_NAME} PUBLIC Freetype::Freetype)
