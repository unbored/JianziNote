if(NOT EXISTS "${DATABASE}")
  message(FATAL_ERROR "Missing source database: ${DATABASE}")
endif()

file(REMOVE_RECURSE "${LIBRARY_DIRECTORY}")
execute_process(
    COMMAND "${MIGRATOR}" "${DATABASE}" "${LIBRARY_DIRECTORY}"
    RESULT_VARIABLE migration_result
    OUTPUT_VARIABLE migration_stdout
    ERROR_VARIABLE migration_stderr)
if(NOT migration_result EQUAL 0)
  message(FATAL_ERROR "CBOR migration failed (${migration_result}): ${migration_stdout}${migration_stderr}")
endif()

set(LIBRARY "${LIBRARY_DIRECTORY}/library.cbor")
if(NOT EXISTS "${LIBRARY}")
  message(FATAL_ERROR "Migration did not create ${LIBRARY}")
endif()

get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
execute_process(
    COMMAND "${EXECUTABLE}" "${LIBRARY}" "大九挑七" "${OUTPUT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "jianzi2svg failed (${result}): ${stdout}${stderr}")
endif()
if(NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR "jianzi2svg did not create ${OUTPUT}")
endif()
file(SIZE "${OUTPUT}" size)
if(size LESS 64)
  message(FATAL_ERROR "SVG output is unexpectedly small: ${size} bytes")
endif()
