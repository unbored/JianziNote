if(NOT EXISTS "${LIBRARY}")
  message(FATAL_ERROR "Missing StrokeDesc v2 library: ${LIBRARY}")
endif()

get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
execute_process(
    COMMAND "${EXECUTABLE}" "${LIBRARY}" "${INPUT}" "${OUTPUT}"
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
file(READ "${OUTPUT}" svg)
if(NOT svg MATCHES "d=\"[^\"]+\"")
  message(FATAL_ERROR "SVG output contains no path data")
endif()
