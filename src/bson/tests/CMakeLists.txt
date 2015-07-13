link_libraries(ejdb_p ${CUNIT_LIBRARIES})
include_directories(${CUNIT_INCLUDE_DIRS})

set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

add_executable(bsontest bsontest.c)
set_target_properties(bsontest
					  PROPERTIES
					  COMPILE_FLAGS "-DEJDB_STATIC")
add_test(NAME bsontest 
		WORKING_DIRECTORY ${TEST_DATA_DIR}
		COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:bsontest>)

