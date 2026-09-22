# Copyright (c) 2024 King
# 
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

set(MODULE_NAME JianziNote)

message("Module ${MODULE_NAME}")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

add_library(${MODULE_NAME} ${PROJECT_LINK_DYNAMIC_STATIC}
    BoundingBox.cpp
    Cbor.cpp
    Jianzi.cpp
    StrokeDescRenderer.cpp
    Utf8.cpp
)
target_include_directories(${MODULE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${MODULE_NAME} PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(${MODULE_NAME} PRIVATE utf8cpp::utf8cpp)

option(JIANZINOTE_BUILD_FONT_READER "Build the optional FreeType font reader" ON)
if(JIANZINOTE_BUILD_FONT_READER)
    find_package(Freetype REQUIRED)
    add_library(JianziFontReader ${PROJECT_LINK_DYNAMIC_STATIC} FTFontReader.cpp)
    target_include_directories(JianziFontReader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(JianziFontReader PUBLIC JianziNote Freetype::Freetype)
endif()
