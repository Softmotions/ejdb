include(FindCURL)
if (NOT CURL_FOUND)
  message(FATAL_ERROR "Cannot find libcurl library")
endif ()

link_libraries(ejdb2_s ${CUNIT_LIBRARIES} ${CURL_LIBRARIES})
include_directories(${CUNIT_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src/tests)

set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})
set(TESTS jbr_test1)

foreach (TN IN ITEMS ${TESTS})
  add_executable(${TN} ${TN}.c)
  set_target_properties(${TN} PROPERTIES
    COMPILE_FLAGS "-DIW_STATIC")
  add_test(NAME ${TN} WORKING_DIRECTORY ${TEST_DATA_DIR}
    COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:${TN}>)
endforeach ()
