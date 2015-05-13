if(APPLE)
	option(BUILD_FRAMEWORK "Build an OS X framework" OFF)
	set(FRAMEWORK_INSTALL_DIR "/Library/Frameworks" CACHE STRING "Directory to install frameworks to.")
endif()

set(EJDB_LLIBRARIES)
set(EJDB_INCLUDE_DIRS)
set(ALL_SRC)
set(ALL_HDRS)
set(PUB_HDRS)

set(MODULES tcutil tchdb tcbdb tctdb tcfdb bson ejdb)
set(EJDB_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
if (WIN32)
	include(Win32LIBTools)
endif()

if (NOT CMAKE_BUILD_TYPE)
	message(FATAL_ERROR "Please specify the build type -DCMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo")
endif(NOT CMAKE_BUILD_TYPE)

list(APPEND EJDB_INCLUDE_DIRS "${EJDB_GENERATED_DIR}")

foreach(MODULE IN LISTS MODULES)
	list(APPEND EJDB_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}")
	file(GLOB MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.c)
	file(GLOB MODULE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.h)
	list(APPEND ALL_SRC ${MODULE_SRC})
	list(APPEND ALL_HDRS ${MODULE_HDRS})
	list(APPEND PUB_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/${MODULE}.h)
endforeach(MODULE)

add_library(ejdb SHARED ${ALL_SRC})
if (NOT BUILD_SHARED_LIBS)
	add_definitions(-DEJDB_NODLL)
endif()

add_library(ejdb_p STATIC ${ALL_SRC})

find_package(Threads REQUIRED CMAKE_THREAD_PREFER_PTHREAD)
if (CMAKE_USE_WIN32_THREADS_INIT)
	add_definitions(-D_TC_WIN32_THREADS)
elseif (CMAKE_USE_PTHREADS_INIT)
	add_definitions(-D_TC_PTHREADS)
else()
	mesage(FATAL_ERROR "Unable to find suitable threading library")
endif(CMAKE_USE_WIN32_THREADS_INIT)

if (NOT WIN32)
	list(APPEND EJDB_LLIBRARIES ${CMAKE_THREAD_LIBS_INIT})
endif()

find_library(RT_LIB rt)
if (RT_LIB)
    list(APPEND EJDB_LLIBRARIES "${RT_LIB}")
else()
    message(WARNING "Library 'librt' not FOUND")
endif()

find_library(M_LIB m)
if (NOT M_LIB)
	message(FATAL_ERROR "Library 'libm' not FOUND")
endif(NOT M_LIB)
list(APPEND EJDB_LLIBRARIES "${M_LIB}")

find_package(BZip2)
if (BZIP2_FOUND)
	list(APPEND EJDB_LLIBRARIES ${BZIP2_LIBRARIES})
	list(APPEND EJDB_INCLUDE_DIRS ${BZIP2_INCLUDE_DIR})
endif(BZIP2_FOUND)

find_package(ZLIB)
if (ZLIB_FOUND)
	list(APPEND EJDB_LLIBRARIES ${ZLIB_LIBRARIES})
	list(APPEND EJDB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
endif(ZLIB_FOUND)

find_package(LibLZMA)
if (LIBLZMA_FOUND)
	list(APPEND EJDB_LLIBRARIES ${LIBLZMA_LIBRARIES})
	list(APPEND EJDB_INCLUDE_DIRS ${LIBLZMA_INCLUDE_DIR})
endif(LIBLZMA_FOUND)

find_package(Lzo)
if (LZO_FOUND)
	list(APPEND EJDB_LLIBRARIES ${LZO_LIBRARIES})
	list(APPEND EJDB_INCLUDE_DIRS ${LZO_INCLUDE_DIR})
endif(LZO_FOUND)

if (WIN32)
	check_include_file(windows.h HAVE_WINDOWS_H)
	if (NOT HAVE_WINDOWS_H)
		message(FATAL_ERROR "Unable to find windows.h include file")
	endif()
	check_include_file(time.h HAVE_TIME_H)
	if (NOT HAVE_TIME_H)
		message(FATAL_ERROR "Unable to find time.h include file")
	endif()
	check_include_file(pcre.h HAVE_PCRE_H)
	if (NOT HAVE_PCRE_H)
		message(FATAL_ERROR "Unable to find pcre.h include file")
	endif()
	check_include_file(pcreposix.h HAVE_PCREPOSIX_H)
	if (NOT HAVE_PCREPOSIX_H)
		message(FATAL_ERROR "Unable to find pcreposix.h include file")
	endif()
	check_library_exists(pcre pcre_compile "" HAVE_PCRE_LIB)
	#check_library_exists(pcreposix regexec "" HAVE_PCREPOSIX_LIB)
	if (NOT HAVE_PCRE_LIB)
		message(FATAL_ERROR "Unable to find pcre lib")
	endif()
	check_library_exists(winpthread pthread_exit "" HAVE_WINPTHREAD)
	if (NOT HAVE_WINPTHREAD)
		message(FATAL_ERROR "Unable to winpthread lib")
	endif()
	list(INSERT EJDB_LLIBRARIES 0 -lwinpthread -lpcreposix -lpcre)
	add_definitions(-DPCRE_STATIC)
    add_definitions(-D_POSIX_THREAD_SAFE_FUNCTIONS)
    
else()

	check_include_file(glob.h HAVE_GLOB_H)
	if (NOT HAVE_GLOB_H)
		message(FATAL_ERROR "Unable to find glob.h include file")
	endif()	
endif(WIN32)

foreach(HF IN ITEMS stdlib stdint unistd dirent stddef)
	string(TOUPPER "${HF}" UHF)
	check_include_file(${HF}.h "_TC_HAVE_${UHF}")
	if (NOT _TC_HAVE_${UHF})
		message(FATAL_ERROR "Include file '${HF}.h' not FOUND")
	endif()
endforeach(HF)

list(REMOVE_DUPLICATES EJDB_LLIBRARIES)
list(REMOVE_DUPLICATES EJDB_INCLUDE_DIRS)
include_directories(${EJDB_INCLUDE_DIRS})

target_link_libraries(ejdb ${EJDB_LLIBRARIES})
target_link_libraries(ejdb_p ${EJDB_LLIBRARIES})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fsigned-char -pedantic -Wfatal-errors -Wno-unknown-pragmas")
if (NOT WIN32) ## todo review
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif(NOT WIN32)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g -Werror -DDEBUG -D_DEBUG -UNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} -g")

if (WIN32)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pedantic-ms-format")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif(WIN32)

if (CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
	if (BUILD_SHARED_LIBS)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
	endif(BUILD_SHARED_LIBS)
endif(CMAKE_COMPILER_IS_GNUCXX)

if (CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
	if (BUILD_SHARED_LIBS)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
	endif(BUILD_SHARED_LIBS)
endif(CMAKE_COMPILER_IS_GNUCC)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/basedefs.h.in ${EJDB_GENERATED_DIR}/basedefs.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/myconf.h.in ${EJDB_GENERATED_DIR}/myconf.h)
file(GLOB EJDB_GENERATED_HDRS ${EJDB_GENERATED_DIR}/*.h)
list(APPEND ALL_HDRS ${EJDB_GENERATED_HDRS})

if (NOT APPLE AND NOT BUILD_FRAMEWORK)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libejdb.pc.in ${EJDB_GENERATED_DIR}/libejdb.pc @ONLY)
	install(FILES ${EJDB_GENERATED_DIR}/libejdb.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif(NOT APPLE AND NOT BUILD_FRAMEWORK)

foreach(MODULE IN LISTS MODULES)
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE})
	endif()
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/tools/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/tools)
	endif()
	if (BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/tests/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/tests)
	endif() 
endforeach(MODULE)

list(APPEND PUB_HDRS 
			${EJDB_GENERATED_DIR}/basedefs.h
			${CMAKE_CURRENT_SOURCE_DIR}/ejdb/ejdb_private.h)

set_target_properties(ejdb PROPERTIES
					  VERSION ${PROJECT_VERSION}
					  SOVERSION ${PROJECT_VERSION_MAJOR}
					  PUBLIC_HEADER "${PUB_HDRS}"
					  DEFINE_SYMBOL EJDB_API_EXPORTS)
if (WIN32)
	add_dependencies(ejdb wintools_init)
	set_target_properties(ejdb PROPERTIES LINK_FLAGS "-Wl,--output-def,libejdb.def")
	add_w32_importlib(ejdb libejdb ${CMAKE_CURRENT_BINARY_DIR})
	install(FILES 
			${CMAKE_CURRENT_BINARY_DIR}/libejdb.def
			${CMAKE_CURRENT_BINARY_DIR}/libejdb.lib
			${CMAKE_CURRENT_BINARY_DIR}/libejdb.exp
            DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

set_target_properties(ejdb_p PROPERTIES
					  VERSION ${PROJECT_VERSION}
					  COMPILE_FLAGS "-DEJDB_NODLL"
					  OUTPUT_NAME ejdb-${PROJECT_VERSION_MAJOR})
					  
install(TARGETS ejdb
	EXPORT ejdb-exports
	FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(EXPORT ejdb-exports
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})

install(TARGETS ejdb_p
	EXPORT ejdb-static-exports
	FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(FILES 
		${CMAKE_SOURCE_DIR}/LICENSE
		${CMAKE_SOURCE_DIR}/INSTALL
		${CMAKE_SOURCE_DIR}/Changelog
	    DESTINATION ${CMAKE_INSTALL_DOCDIR})

install(FILES 
		${CMAKE_SOURCE_DIR}/README.md
		RENAME README
	    DESTINATION ${CMAKE_INSTALL_DOCDIR})

if (CMAKE_VERSION VERSION_GREATER 3.0)
	export(EXPORT ejdb-exports)	
	export(EXPORT ejdb-static-exports)	
else()
	export(TARGETS ejdb FILE ejdb-exports.cmake)	
	export(TARGETS ejdb_p FILE ejdb-static-exports.cmake)	
endif()


if (BUILD_SAMPLES)
	add_subdirectory(samples)
endif()

#todo?
include(InstallRequiredSystemLibraries)

message("")
message("LINK LIBS: ${EJDB_LLIBRARIES}")
message("\nINCLUDE DIRS: ${EJDB_INCLUDE_DIRS}")
message("\nSOURCES: ${ALL_SRC}")
message("\nPUB_HDRS: ${PUB_HDRS}")
message("\nCMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message("BUILD_TESTS: ${BUILD_TESTS}")
message("BUILD_SAMPLES: ${BUILD_SAMPLES}")
