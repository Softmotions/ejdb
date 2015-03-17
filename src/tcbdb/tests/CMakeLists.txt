link_libraries(ejdb_p)
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

add_executable(tcbtest tcbtest.c)
add_executable(tcbmttest tcbmttest.c)
add_dependencies(tcbtest jbhmgr)
add_dependencies(tcbmttest jbhmgr tchmttest)

set_target_properties(tcbtest tcbmttest PROPERTIES
					  COMPILE_FLAGS "-DEJDB_STATIC")

add_test(NAME tcbtest1 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 write casket 50000 5 5 5000 5 5)
		 
add_test(NAME tcbtest2 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 read casket)
		
add_test(NAME tcbtest3 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 remove casket)
		
add_test(NAME tcbtest4 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -rb 00001000 00002000 casket) #check.out

add_test(NAME tcbtest5 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -fm 000001 casket) #check.out
		 
add_test(NAME tcbtest6 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 write -mt -tl -td -ls 1024 casket 50000 5000 5000 5000 5 5)

add_test(NAME tcbtest6_2 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 read -mt -nb casket)

add_test(NAME tcbtest7 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 remove -mt casket)

add_test(NAME tcbtest8 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 write -tb -xm 50000 casket 50000 5 5 50000 5 5)

add_test(NAME tcbtest9 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 read -nl casket)

add_test(NAME tcbtest10 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 remove -df 5 casket)

add_test(NAME tcbtest11 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 rcat -lc 5 -nc 5 -df 5 -pn 500 casket 50000 5 5 5000 5 5)

add_test(NAME tcbtest12 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 rcat -tl -td -pn 5000 casket 50000 5 5 500 5 15)

add_test(NAME tcbtest13 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 rcat -nl -pn 5000 -rl casket 15000 5 5 500 5 5)

add_test(NAME tcbtest14 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 rcat -ca 1000 -tb -pn 5000 casket 15000 5 5 500 5 5)

add_test(NAME tcbtest15 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 rcat -ru -pn 500 casket 5000 5 5 500 1 1)

add_test(NAME tcbtest16 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 rcat -cd -tl -td -ru -pn 500 casket 5000 5 5 500 1 1)

add_test(NAME tcbtest17 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -pv casket) #check.out

add_test(NAME tcbtest18 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 queue casket 15000 5 5)

add_test(NAME tcbtest19 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 misc casket 5000)

add_test(NAME tcbtest20 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 misc -tl -td casket 5000)

add_test(NAME tcbtest21 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 misc -mt -tb casket 500)

add_test(NAME tcbtest22 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 wicked casket 50000)

add_test(NAME tcbtest23 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 wicked -tl -td casket 50000)

add_test(NAME tcbtest24 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 wicked -mt -tb casket 5000)

add_test(NAME tcbtest25 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 wicked -tt casket 5000)

add_test(NAME tcbtest26 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 wicked -tx casket 5000)

add_test(NAME tcbtest27 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 write -cd -lc 5 -nc 5 casket 5000 5 5 5 5 5)

add_test(NAME tcbtest28 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 read -cd -lc 5 -nc 5 casket)

add_test(NAME tcbtest29 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 remove -cd -lc 5 -nc 5 casket)

add_test(NAME tcbtest30 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -pv casket) #check.out

add_test(NAME tcbtest31 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 write -ci -td -lc 5 -nc 5 casket 5000 5 5 5 5 5)

add_test(NAME tcbtest32 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 read -ci -lc 5 -nc 5 casket)

add_test(NAME tcbtest33 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 remove -ci -lc 5 -nc 5 casket)

add_test(NAME tcbtest34 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -pv casket) #check.out

add_test(NAME tcbtest35 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 write -cj -tb -lc 5 -nc 5 casket 5000 5 5 5 5 5)

add_test(NAME tcbtest36 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 read -cj -lc 5 -nc 5 casket)

add_test(NAME tcbtest37 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbtest> 
		 remove -cj -lc 5 -nc 5 casket)
		 
add_test(NAME tcbtest38 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -pv casket) #check.out

add_test(NAME tcbtest39 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 write -df 5 -tl casket 5 5000 5 5 500 5)

add_test(NAME tcbtest40 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 read -df 5 casket 5)

add_test(NAME tcbtest41 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 read -rnd casket 5)

add_test(NAME tcbtest42 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 remove casket 5)

add_test(NAME tcbtest43 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 wicked -nc casket 5 5000)

add_test(NAME tcbtest44 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 wicked -tl -td casket 5 5000)

add_test(NAME tcbtest45 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> #review 
		 wicked -tb casket 5 5000)

add_test(NAME tcbtest46 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 typical -df 5 casket 5 50000 5 5)

add_test(NAME tcbtest47 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 typical -rr 1000 casket 5 50000 5 5)

add_test(NAME tcbtest48 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 typical -tl -nc casket 5 50000 5 5)

add_test(NAME tcbtest49 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcbmttest> 
		 race -df 5 casket 5 10000)

add_test(NAME tcbtest50 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 create casket 4 4 3 1 1)

add_test(NAME tcbtest51 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 inform casket)

add_test(NAME tcbtest52 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put casket one first)

add_test(NAME tcbtest53 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put casket two second)

add_test(NAME tcbtest54 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put -dk casket three third)

add_test(NAME tcbtest55 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put -dc casket three third)

add_test(NAME tcbtest56 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put -dc casket three third)

add_test(NAME tcbtest57 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put -dd casket three third)

add_test(NAME tcbtest58 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put -dd casket three third)

add_test(NAME tcbtest59 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put casket four fourth)

add_test(NAME tcbtest60 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put -dk casket five fifth)

add_test(NAME tcbtest61 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 out casket one)

add_test(NAME tcbtest62 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 out casket two)

add_test(NAME tcbtest63 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 get casket three) #check.out

add_test(NAME tcbtest64 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 get casket four) #check.out

add_test(NAME tcbtest65 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 get casket five) #check.out

add_test(NAME tcbtest66 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -pv casket) #check.out

add_test(NAME tcbtest67 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -j three -pv casket) #check.out

add_test(NAME tcbtest68 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 optimize casket) 

add_test(NAME tcbtest69 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 put -dc casket three third) 

add_test(NAME tcbtest70 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 get casket three) #check.out

add_test(NAME tcbtest71 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 get casket four) #check.out

add_test(NAME tcbtest72 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 get casket five) #check.out

add_test(NAME tcbtest73 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbbmgr>
		 list -pv casket) #check.out
