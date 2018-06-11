link_libraries(ejdb2_s ${CUNIT_LIBRARIES})
include_directories(${CUNIT_INCLUDE_DIRS})

set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

foreach(TN IN ITEMS jql_test1)
    add_executable(${TN} ${TN}.c)
    set_target_properties(${TN} PROPERTIES
                          COMPILE_FLAGS "-DIW_STATIC")
    add_test(NAME ${TN} WORKING_DIRECTORY ${TEST_DATA_DIR}
             COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:${TN}>)
endforeach()
