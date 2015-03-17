link_libraries(ejdb_p)
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

add_executable(tcutest tcutest.c)
add_executable(tcumttest tcumttest.c)

set_target_properties(tcutest tcumttest 
					  PROPERTIES
					  COMPILE_FLAGS "-DEJDB_STATIC")

add_test(NAME tcutest1 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 xstr 50000)

add_test(NAME tcutest2 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 list -rd 50000)

add_test(NAME tcutest3 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 map -rd -tr 50000)
		 
add_test(NAME tcutest4 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 map -rd -tr -rnd -dc 50000)

add_test(NAME tcutest5 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 tree -rd -tr 50000)
		 
add_test(NAME tcutest6 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 tree -rd -tr -rnd -dc 50000)
		 
add_test(NAME tcutest7 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 mdb -rd -tr 50000)

add_test(NAME tcutest8 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 mdb -rd -tr -rnd -dc 50000)

add_test(NAME tcutest9 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 mdb -rd -tr -rnd -dpr 50000)

add_test(NAME tcutest10 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 ndb -rd -tr 50000)

add_test(NAME tcutest11 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 ndb -rd -tr -rnd -dc 50000)

add_test(NAME tcutest12 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 ndb -rd -tr -rnd -dpr 50000)

add_test(NAME tcutest13 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 misc 500)

add_test(NAME tcutest14 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcutest> 
		 wicked 50000)

add_test(NAME tcumttest1 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 combo 5 50000 500)

add_test(NAME tcumttest2 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 combo -rnd 5 50000 500)

add_test(NAME tcumttest3 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 typical 5 50000 5000)

add_test(NAME tcumttest4 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 typical -rr 1000 5 50000 5000)

add_test(NAME tcumttest5 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 typical -nc 5 50000 5000)

add_test(NAME tcumttest6 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 combo -tr 5 50000 500)

add_test(NAME tcumttest7 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 combo -tr -rnd 5 50000 500)

add_test(NAME tcumttest8 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 typical -tr 5 50000 5000)

add_test(NAME tcumttest9 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 typical -tr -rr 1000 5 50000 5000)

add_test(NAME tcumttest10 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcumttest> 
		 typical -tr -nc 5 50000 5000)
