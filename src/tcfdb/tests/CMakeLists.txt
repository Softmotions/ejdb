link_libraries(ejdb_p)
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

add_executable(tcftest tcftest.c)
add_executable(tcfmttest tcfmttest.c)
add_dependencies(tcftest jbfmgr)
add_dependencies(tcfmttest jbfmgr)

set_target_properties(tcftest tcfmttest PROPERTIES
					  COMPILE_FLAGS "-DEJDB_STATIC")


add_test(NAME tcftest1 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 write casket 50000 50)
		 
add_test(NAME tcftest2 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 read casket)
		 
add_test(NAME tcftest3 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 remove casket)

add_test(NAME tcftest4 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 write casket 50000 50)

add_test(NAME tcftest5 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 read -mt -nb casket)

add_test(NAME tcftest6 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 remove -mt casket)

add_test(NAME tcftest7 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 rcat -pn 500 casket 50000 50)

add_test(NAME tcftest8 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 rcat -nl -pn 500 -rl casket 5000 500)

add_test(NAME tcftest9 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 rcat -pn 500 -ru casket 5000 500)

add_test(NAME tcftest10 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 list -pv casket) #check.out

add_test(NAME tcftest11 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 list -pv -ri "[100,200)" -px casket) #check.out

add_test(NAME tcftest12 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 misc casket 5000) 

add_test(NAME tcftest13 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 misc -mt -nl casket 500) 

add_test(NAME tcftest14 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 wicked casket 50000) 

add_test(NAME tcftest15 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcftest> 
		 wicked -mt -nb casket 50000) 

add_test(NAME tcftest16 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 write casket 5 5000 50) 

add_test(NAME tcftest17 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 read casket 5) 

add_test(NAME tcftest18 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 read -rnd casket 5) 

add_test(NAME tcftest19 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 remove casket 5) 

add_test(NAME tcftest20 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 wicked -nc casket 5 5000) 

add_test(NAME tcftest21 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 wicked casket 5 5000) 

add_test(NAME tcftest22 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 typical casket 5 50000 50) 

add_test(NAME tcftest23 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 typical -rr 1000 casket 5 50000 50) 

add_test(NAME tcftest24 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcfmttest> 
		 typical -nc casket 5 50000 50) 

add_test(NAME tcftest25 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 create casket 50) 

add_test(NAME tcftest26 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 inform casket) 

add_test(NAME tcftest27 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 put casket 1 first) 

add_test(NAME tcftest28 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 put casket 2 second) 

add_test(NAME tcftest29 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 put -dk casket 3 third) 

add_test(NAME tcftest30 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 put -dc casket 3 third) 

add_test(NAME tcftest31 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 put -dc casket 3 third) 

add_test(NAME tcftest32 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		 put -dc casket 3 third) 

add_test(NAME tcftest33 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  put casket 4 fourth) 

add_test(NAME tcftest34 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  put -dk casket 5 fifth) 

add_test(NAME tcftest35 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  out casket 1) 

add_test(NAME tcftest36 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  out casket 2) 

add_test(NAME tcftest37 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  get casket 3) #check.out

add_test(NAME tcftest38 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  get casket 4) #check.out

add_test(NAME tcftest39 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  get casket 5) #check.out

add_test(NAME tcftest40 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  list -pv casket) #check.out

add_test(NAME tcftest41 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  optimize casket 5)

add_test(NAME tcftest42 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  put -dc casket 3 third)

add_test(NAME tcftest43 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  get casket 3) #check.out

add_test(NAME tcftest44 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  get casket 4) #check.out

add_test(NAME tcftest45 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  get casket 5) #check.out

add_test(NAME tcftest46 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbfmgr>
		  list -pv casket) #check.out

