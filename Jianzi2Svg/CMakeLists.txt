set(MODULE_NAME jianzi2svg)

file(GLOB_RECURSE MODULE_SOURCE
    "*.h"
    "*.hpp"
    "*.cpp"
)

include_directories(
    ${PROJECT_SOURCE_DIR}/JianziNote
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${MODULE_NAME})

add_executable(${MODULE_NAME} ${MODULE_SOURCE})
target_link_libraries(${MODULE_NAME} PRIVATE JianziNote)
set_target_properties(${MODULE_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

if(BUILD_TESTING)
  add_test(
      NAME jianzi2svg_smoke
      COMMAND ${CMAKE_COMMAND}
          -DEXECUTABLE=$<TARGET_FILE:jianzi2svg>
          -DLIBRARY=${PROJECT_SOURCE_DIR}/bin/song_style_cbor/library.cbor
          -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/大九挑七.svg
          -P ${CMAKE_CURRENT_SOURCE_DIR}/SvgSmokeTest.cmake)
endif()
