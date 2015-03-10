link_libraries(ejdb_p)
set(TEST_DATA_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_DATA_DIR})

add_executable(tcttest tcttest.c)
add_executable(tctmttest tctmttest.c)
add_dependencies(tcttest jbhmgr jbbmgr)
add_dependencies(tctmttest jbhmgr jbbmgr)

set_target_properties(tcttest tctmttest PROPERTIES
					  COMPILE_FLAGS "-DEJDB_STATIC")


add_test(NAME tcttest1 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 write casket 50000 5000 5 5)

add_test(NAME tcttest2 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 read casket)

add_test(NAME tcttest3 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 remove casket)

add_test(NAME tcttest4 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 write -mt -tl -td -rc 50 -lc 5 -nc 5 -xm 500000 
		-is -in -it -if -ix casket 5000 5000 5 5)
  
add_test(NAME tcttest5 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 read -mt -nb -rc 50 -lc 5 -nc 5 -xm 500000 casket)

add_test(NAME tcttest6 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 remove -mt -rc 50 -lc 5 -nc 5 -xm 500000 -df 5 casket)

add_test(NAME tcttest7 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 rcat -pn 500 -xm 50000 -df 5 -is casket 5000 5000 5 5)

add_test(NAME tcttest8 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 rcat -tl -td -pn 5000 -is -in casket 5000 500 5 15)

add_test(NAME tcttest9 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 rcat -nl -pn 500 -rl -is -in casket 5000 500 5 5)

add_test(NAME tcttest10 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 rcat -tb -pn 500 -is -in casket 5000 500 5 5)

add_test(NAME tcttest11 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 rcat -ru -pn 500 -is -in casket 5000 500 1 1)

add_test(NAME tcttest12 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 rcat -tl -td -ru -pn 500 -is -in casket 5000 500 1 1)

add_test(NAME tcttest13 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 list -pv casket) #check.out

add_test(NAME tcttest14 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 list -pv -px casket) #check.out

add_test(NAME tcttest15 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 misc casket 500)

add_test(NAME tcttest16 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 misc -tl -td casket 500)

add_test(NAME tcttest17 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 misc -mt -tb casket 500)

add_test(NAME tcttest18 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 wicked casket 5000)

add_test(NAME tcttest19 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 wicked -tl -td casket 5000)

add_test(NAME tcttest20 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 wicked -tl -td casket 5000)

add_test(NAME tcttest21 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 wicked -mt -tb casket 5000)

add_test(NAME tcttest22 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 wicked -tt casket 5000)

add_test(NAME tcttest23 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tcttest> 
		 wicked -tx casket 5000)

add_test(NAME tcttest24 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 write -xm 500000 -df 5 -tl -is -in casket 5 5000 500 5)

add_test(NAME tcttest25 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 read -xm 500000 -df 5 casket 5)

add_test(NAME tcttest26 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 read -xm 500000 -rnd casket 5)

add_test(NAME tcttest27 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 remove -xm 500000 casket 5)

add_test(NAME tcttest28 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 wicked casket 5 5000)

add_test(NAME tcttest29 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 wicked -tl -td casket 5 5000)

add_test(NAME tcttest30 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 typical -df 5 casket 5 5000 500)

add_test(NAME tcttest31 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 typical -rr 1000 casket 5 5000 500)

add_test(NAME tcttest32 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:tctmttest> 
		 typical -tl -rc 50000 -lc 5 -nc 5 casket 5 5000 500)

add_test(NAME tcttest33 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 create casket 3 1 1)

add_test(NAME tcttest34 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 setindex casket name)

add_test(NAME tcttest35 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 inform casket)

add_test(NAME tcttest36 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 put casket "" name mikio birth 19780211 lang ja,en,c)

add_test(NAME tcttest37 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 put casket "" name fal birth 19771007 lang ja)

add_test(NAME tcttest38 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 put casket "" name banana price 100)

add_test(NAME tcttest39 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 put -dc casket 3 color yellow)

add_test(NAME tcttest40 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 put -dk casket "" name melon price 1200 color green)

add_test(NAME tcttest41 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 put casket "" name void birth 20010101 lang en)

add_test(NAME tcttest42 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 out casket 5)

add_test(NAME tcttest43 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 get casket 1) #check.out

add_test(NAME tcttest44 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 get casket 2) #check.out

add_test(NAME tcttest45 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 get casket 3) #check.out

add_test(NAME tcttest46 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search casket) #check.out

add_test(NAME tcttest47 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search -m 10 -sk 1 -pv -ph casket) #check.out

add_test(NAME tcttest48 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search -m 10 -ord name STRDESC -pv -ph casket) #check.out

add_test(NAME tcttest49 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search -m 10 -ord name STRDESC -pv -ph casket 
		 name STRBW mi birth NUMBT 19700101,19791231 lang STRAND ja,en) #check.out

add_test(NAME tcttest50 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search -ord birth NUMDESC -pv -ms UNION casket
		 name STREQ mikio name STRINC fal name FTSEX "ba na na") #check.out

add_test(NAME tcttest51 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 setindex casket name) 

add_test(NAME tcttest52 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 setindex -it dec casket birth) 

add_test(NAME tcttest53 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 setindex casket lang) 

add_test(NAME tcttest54 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 list -pv casket) #check.out

add_test(NAME tcttest55 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 optimize casket) 

add_test(NAME tcttest56 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 put casket "" name tokyo country japan lang ja) 

add_test(NAME tcttest57 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search -m 10 -sk 1 -pv -ph casket) #check.out

add_test(NAME tcttest58 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search -m 10 -ord name STRDESC -pv -ph casket) #check.out

add_test(NAME tcttest59 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search -m 10 -ord name STRDESC -pv -ph casket
		 name STRBW mi birth NUMBT 19700101,19791231 lang STRAND ja,en) #check.out
		 		 
add_test(NAME tcttest60 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 search -ord price NUMDESC -ph -rm casket name STRINC a)

add_test(NAME tcttest61 WORKING_DIRECTORY ${TEST_DATA_DIR} 
		 COMMAND ${TEST_TOOL_CMD} $<TARGET_FILE:jbtmgr>
		 list -pv casket) #check.out

