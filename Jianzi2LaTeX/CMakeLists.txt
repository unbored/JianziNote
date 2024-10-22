set(MODULE_NAME jianzi2latex)

file(GLOB_RECURSE MODULE_SOURCE
    "*.h"
    "*.c"
    "*.hpp"
    "*.cpp"
)

include_directories(
    ${PROJECT_SOURCE_DIR}/JianziNote
)

set(MODULE_LIBRARIES
    JianziNote
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${MODULE_NAME})

add_executable(${MODULE_NAME} ${MODULE_SOURCE})
target_link_libraries(${MODULE_NAME} PRIVATE ${MODULE_LIBRARIES})
set_target_properties(${MODULE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
