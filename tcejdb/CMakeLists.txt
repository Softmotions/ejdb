cmake_minimum_required(VERSION 3.0)
set(PROJECT_NAME "ejdb")
set(PROJECT_VENDOR "Softmotions")
set(PROJECT_DESCRIPTION_SUMMARY "Embedded JSON database library (EJDB)")
set(PROJECT_WEBSITE "http://ejdb.org")
project(${PROJECT_NAME} VERSION 1.2.0 LANGUAGES C)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test cases" OFF)
option(PACKAGE_DEB "Build .deb instalation packages" OFF)
option(PACKAGE_TGZ "Build .tgz package archive" ON)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_C_COMPILER_VERSION VERSION_LESS 4.7)
        message(FATAL_ERROR "GCC version must be at least 4.7!")
    endif()
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    if (CMAKE_C_COMPILER_VERSION VERSION_LESS 3.4)
        message(FATAL_ERROR "Clang version must be at least 3.4!")
    endif()
else()
    message(FATAL_ERROR "You are using an unsupported compiler! EJDB has only been tested \
	with Clang >= 3.4 and GCC >= 4.7")
endif()

include(GNUInstallDirs)

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
set(EJDB_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/src/generated)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(InstallRequiredSystemLibraries)
include(EjdbUtils)

macro_ensure_out_of_source_build(
    "${CMAKE_PROJECT_NAME} requires an out of source build."
)

if (NOT CMAKE_BUILD_TYPE)
	message(FATAL_ERROR "Please specify the build type -DCMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo")
endif(NOT CMAKE_BUILD_TYPE)

list(APPEND EJDB_INCLUDE_DIRS "${EJDB_GENERATED_DIR}")

foreach(MODULE IN LISTS MODULES)
	list(APPEND EJDB_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}")
	file(GLOB MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/*.c)
	file(GLOB MODULE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/*.h)
	list(APPEND ALL_SRC ${MODULE_SRC})
	list(APPEND ALL_HDRS ${MODULE_HDRS})
	list(APPEND PUB_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/${MODULE}.h)
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

#todo check the host system!

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fsigned-char -pedantic -Wfatal-errors")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -Werror -DDEBUG -D_DEBUG -UNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -D_DEBUG -UNDEBUG")

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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/basedefs.h.in ${EJDB_GENERATED_DIR}/basedefs.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/myconf.h.in ${EJDB_GENERATED_DIR}/myconf.h)
file(GLOB EJDB_GENERATED_HDRS ${EJDB_GENERATED_DIR}/*.h)
list(APPEND ALL_HDRS ${EJDB_GENERATED_HDRS})

if (NOT APPLE AND NOT BUILD_FRAMEWORK)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/libejdb.pc.in ${EJDB_GENERATED_DIR}/libejdb.pc @ONLY)
	install(FILES ${EJDB_GENERATED_DIR}/libejdb.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif(NOT APPLE AND NOT BUILD_FRAMEWORK)

if (BUILD_TESTS) 
	include(CTest)
	find_package(CUnit REQUIRED)
endif(BUILD_TESTS)

foreach(MODULE IN LISTS MODULES)
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE})
	endif()
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/tools/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/tools)
	endif()
	if (BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/tests/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/tests)
	endif() 
endforeach(MODULE)

list(APPEND PUB_HDRS 
			${EJDB_GENERATED_DIR}/basedefs.h
			${CMAKE_CURRENT_SOURCE_DIR}/src/ejdb/ejdb_private.h)

set_target_properties(ejdb PROPERTIES
					  VERSION ${ejdb_VERSION}
					  SOVERSION ${ejdb_VERSION_MAJOR}
					  PUBLIC_HEADER "${PUB_HDRS}"
					  DEFINE_SYMBOL EJDB_API_EXPORTS)

set_target_properties(ejdb_p PROPERTIES
					  VERSION ${ejdb_VERSION}
					  COMPILE_FLAGS "-DEJDB_NODLL")

install(TARGETS ejdb
	EXPORT ejdb-exports
	FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ejdb-exports
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})
		
install(FILES 
		${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
		${CMAKE_CURRENT_SOURCE_DIR}/README
		${CMAKE_CURRENT_SOURCE_DIR}/Changelog
	    DESTINATION ${CMAKE_INSTALL_DOCDIR})


export(EXPORT ejdb-exports)		

#######################################################

set(CPACK_GENERATORS)
if (PACKAGE_TGZ)
	list(APPEND CPACK_GENERATORS "TGZ")
endif()
if (PACKAGE_DEB)
	list(APPEND CPACK_GENERATORS "DEB")
endif()

set(CPACK_SOURCE_GENERATOR "TGZ")

if (CPACK_GENERATORS)
	set(CPACK_GENERATOR "${CPACK_GENERATORS}")
	set(CPACK_PACKAGE_VERSION ${ejdb_VERSION})
	set(CPACK_PACKAGE_VERSION_MAJOR ${ejdb_VERSION_MAJOR})
	set(CPACK_PACKAGE_VERSION_MINOR ${ejdb_VERSION_MINOR})
	set(CPACK_PACKAGE_VERSION_PATCH ${ejdb_VERSION_PATCH})
	set(CPACK_PACKAGE_VENDOR ${PROJECT_VENDOR})
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION_SUMMARY})
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
	set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README")
	set(CPACK_RESOURCE_FILE_WELCOME "${CMAKE_SOURCE_DIR}/README")	
	set(CPACK_PACKAGE_FILE_NAME 
	"${PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
	if (PACKAGE_DEB)
		include(EjdbDebCpack)
	endif()
	include(CPack)
endif()

#########################################################

message("")
message("#############################################")
message("LINK LIBS: ${EJDB_LLIBRARIES}")
message("\nINCLUDE DIRS: ${EJDB_INCLUDE_DIRS}")
message("\nSOURCES: ${ALL_SRC}")
message("\nPUB_HDRS: ${PUB_HDRS}")
message("\nCMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message("BUILD_TESTS: ${BUILD_TESTS}")
message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("CPACK_GENERATORS: ${CPACK_GENERATORS}")
message("#############################################")
message("")

