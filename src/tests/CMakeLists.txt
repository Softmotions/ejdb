link_libraries(ejdb2_s ${CUNIT_LIBRARIES})
include_directories(${CUNIT_INCLUDE_DIRS})

set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

set(TESTS ejdb_test1
  ejdb_test2
  ejdb_test3
  ejdb_test4
  )

foreach (TN IN ITEMS ${TESTS})
  add_executable(${TN} ${TN}.c)
  set_target_properties(${TN} PROPERTIES
    COMPILE_FLAGS "-DIW_STATIC")
  add_test(NAME ${TN} WORKING_DIRECTORY ${TEST_DATA_DIR}
    COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:${TN}>)
endforeach ()
