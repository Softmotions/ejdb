if (NOT CMAKE_HOST_UNIX OR NOT WIN32)
	return()
endif()

find_program(WINTOOLS_WINE_EXEC wine)
if (NOT WINTOOLS_WINE_EXEC)
	message("Wine executable not found! Failed to initiate wintools staff.")
	return()
endif()

find_program(WINTOOLS_WGET_EXEC wget)
if (NOT WINTOOLS_WGET_EXEC)
	message("Wget executable not found! Failed to initiate wintools staff.")
	return()
endif()

set(WINTOOLS_DIR ${CMAKE_BINARY_DIR}/WINTOOLS)
set(WINTOOLS_DL_ROOT "http://softmotions.com/windev")

if (NOT EXISTS ${WINTOOLS_DIR}) 
	file(MAKE_DIRECTORY ${WINTOOLS_DIR})
endif()

set(WINTOOLS_EXECS)
foreach (WINTOOLS_EXEC link.exe lib.exe mspdb100.dll)
	if (NOT EXISTS ${WINTOOLS_DIR}/${WINTOOLS_EXEC})
		add_custom_command(OUTPUT ${WINTOOLS_DIR}/${WINTOOLS_EXEC}
				   COMMAND ${WINTOOLS_WGET_EXEC} ${WINTOOLS_DL_ROOT}/${WINTOOLS_EXEC} -nv -O${WINTOOLS_DIR}/${WINTOOLS_EXEC}
				   WORKING_DIRECTORY ${WINTOOLS_DIR})
		list(APPEND WINTOOLS_EXECS ${WINTOOLS_DIR}/${WINTOOLS_EXEC})
	endif()
endforeach(WINTOOLS_EXEC)

add_custom_target(wintools_init 
				  DEPENDS ${WINTOOLS_EXECS})
			
if (CMAKE_SIZEOF_VOID_P MATCHES 8)
	set(WINTOOLS_LIB_MACHINE "X64")
else()
	set(WINTOOLS_LIB_MACHINE "X86")
endif()	
message("${PROJECT_NAME} WINTOOLS_LIB_MACHINE: ${WINTOOLS_LIB_MACHINE}")
	
macro(add_w32_importlib tgt libname wdir)
	add_custom_command(
		TARGET ${tgt}
		POST_BUILD
		COMMAND ${WINTOOLS_WINE_EXEC} ${WINTOOLS_DIR}/lib.exe /def:${libname}.def /machine:${WINTOOLS_LIB_MACHINE}
		WORKING_DIRECTORY ${wdir}
	)
endmacro(add_w32_importlib)


