set(MODULE_NAME jianzi-sqlite-to-cbor)

add_executable(${MODULE_NAME} main.cpp)
target_include_directories(${MODULE_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/JianziNote)
target_link_libraries(${MODULE_NAME} PRIVATE JianziNote SQLiteCpp)
set_target_properties(${MODULE_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${MODULE_NAME}
  VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
