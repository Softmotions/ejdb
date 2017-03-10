link_libraries(ejdb_p ${CUNIT_LIBRARIES})
include_directories(${CUNIT_INCLUDE_DIRS})

set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

foreach(TC RANGE 1 6)
	add_executable(ejdbtest${TC} ejdbtest${TC}.c)
	set_target_properties(ejdbtest${TC} PROPERTIES 
						  COMPILE_FLAGS "-DEJDB_STATIC")
	add_test(NAME ejdbtest${TC} WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:ejdbtest${TC}>)
endforeach()

