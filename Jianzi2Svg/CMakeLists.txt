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

set(JIANZINOTE_V2_TEST_LIBRARY "" CACHE FILEPATH "StrokeDesc v2 library used by the SVG smoke test")
set(JIANZINOTE_V2_TEST_TEXT "挑七" CACHE STRING "Text rendered by the StrokeDesc v2 SVG smoke test")
if(BUILD_TESTING AND EXISTS "${JIANZINOTE_V2_TEST_LIBRARY}")
  add_test(
      NAME jianzi2svg_smoke
      COMMAND ${CMAKE_COMMAND}
          -DEXECUTABLE=$<TARGET_FILE:jianzi2svg>
          -DLIBRARY=${JIANZINOTE_V2_TEST_LIBRARY}
          -DINPUT=${JIANZINOTE_V2_TEST_TEXT}
          -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/v2-smoke.svg
          -P ${CMAKE_CURRENT_SOURCE_DIR}/SvgSmokeTest.cmake)
endif()
