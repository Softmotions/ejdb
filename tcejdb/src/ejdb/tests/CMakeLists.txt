link_libraries(ejdb cunit)
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

foreach(TC RANGE 1 5)
	add_executable(ejdbtest${TC} ejdbtest${TC}.c)
	add_test(NAME ejdbtest${TC} WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:ejdbtest${TC}>)
endforeach()

