link_libraries(ejdb_p)
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

add_executable(tchtest tchtest.c)
add_executable(tchmttest tchmttest.c)
add_dependencies(tchtest jbhmgr)
add_dependencies(tchmttest jbhmgr)

set_target_properties(tchtest tchmttest PROPERTIES
					  COMPILE_FLAGS "-DEJDB_STATIC")

add_test(NAME tchtest1 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 write casket 50000 5000 5 5)

add_test(NAME tchtest2 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 read casket)

add_test(NAME tchtest3 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 remove casket)

add_test(NAME tchtest4 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 write -mt -tl -td -rc 50 -xm 500000 casket 50000 5000 5 5)

add_test(NAME tchtest5 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 read -mt -nb -rc 50 -xm 500000 casket)
		 
add_test(NAME tchtest6 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 remove -mt -rc 50 -xm 500000 casket)

add_test(NAME tchtest7 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 write -as -tb -rc 50 -xm 500000 casket 50000 50000 5 5)

add_test(NAME tchtest8 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 read -nl -rc 50 -xm 500000 casket)

add_test(NAME tchtest9 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 remove -rc 50 -xm 500000 -df 5 casket)

add_test(NAME tchtest10 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 rcat -pn 500 -xm 50000 -df 5 casket 50000 5000 5 5)

add_test(NAME tchtest11 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 rcat -tl -td -pn 5000 casket 50000 500 5 15)

add_test(NAME tchtest12 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 rcat -nl -pn 500 -rl casket 5000 500 5 5)

add_test(NAME tchtest13 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 rcat -tb -pn 500 casket 5000 500 5 5)

add_test(NAME tchtest14 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 rcat -ru -pn 500 casket 5000 500 1 1)

add_test(NAME tchtest15 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 rcat -tl -td -ru -pn 500 casket 5000 500 1 1)
		 		 
add_test(NAME tchtest16 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 list -pv casket)

add_test(NAME tchtest17 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 list -pv -fm 1 -px casket)

add_test(NAME tchtest18 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 misc casket 5000)

add_test(NAME tchtest19 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 misc -tl -td casket 5000)

add_test(NAME tchtest20 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 misc -mt -tb casket 500)

add_test(NAME tchtest21 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 wicked casket 50000)

add_test(NAME tchtest22 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 wicked -tl -td casket 50000)

add_test(NAME tchtest23 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 wicked -mt -tb casket 5000)

add_test(NAME tchtest24 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 wicked -tt casket 5000)

add_test(NAME tchtest25 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchtest> 
		 wicked -tx casket 5000)
		 
add_test(NAME tchtest26 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 write -xm 500000 -df 5 -tl casket 5 5000 500 5)
		 
add_test(NAME tchtest27 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 read -xm 500000 -df 5 casket 5)
		 
add_test(NAME tchtest28 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 read -xm 500000 -rnd casket 5)
		 		 
add_test(NAME tchtest29 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 remove -xm 500000 casket 5)
		 		 
add_test(NAME tchtest30 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 wicked -nc casket 5 5000)
		 		 
add_test(NAME tchtest31 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 wicked -tl -td casket 5 5000)
		 		 
add_test(NAME tchtest32 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 wicked -tb casket 5 5000)
		 		 
add_test(NAME tchtest33 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 typical -df 5 casket 5 50000 5000)

add_test(NAME tchtest34 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 typical -rr 1000 casket 5 50000 5000)

add_test(NAME tchtest35 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 typical -tl -rc 50000 -nc casket 5 50000 5000)

add_test(NAME tchtest36 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tchmttest> 
		 race -df 5 casket 5 10000)
		 
add_test(NAME tchtest37 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 create casket 3 1 1)
		 
add_test(NAME tchtest38 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 inform casket)
		 
add_test(NAME tchtest39 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put casket one first)
		 
add_test(NAME tchtest40 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put casket two second)
		 
add_test(NAME tchtest41 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put -dk casket three third)

add_test(NAME tchtest42 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put -dc casket three third)

add_test(NAME tchtest43 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put -dc casket three third)

add_test(NAME tchtest44 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put -dc casket three third)

add_test(NAME tchtest45 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put casket four fourth)

add_test(NAME tchtest46 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put -dk casket five fifth)

add_test(NAME tchtest47 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 out casket one)

add_test(NAME tchtest48 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 out casket two)

add_test(NAME tchtest49 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 get casket three) #check.out
		 
add_test(NAME tchtest50 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 get casket four) #check.out
		 
add_test(NAME tchtest51 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 get casket five) #check.out
		 
add_test(NAME tchtest52 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 list -pv casket) #check.out
		 
add_test(NAME tchtest53 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 optimize casket)
		 
add_test(NAME tchtest54 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 put -dc casket three third)

add_test(NAME tchtest55 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 get casket three) #check.out
		 
add_test(NAME tchtest56 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 get casket three) #check.out

add_test(NAME tchtest57 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 get casket four) #check.out
		 
add_test(NAME tchtest58 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 get casket five) #check.out
		 
add_test(NAME tchtest59 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbhmgr>
		 list -pv casket) #check.out

