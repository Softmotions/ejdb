cmake_minimum_required(VERSION 3.1)
set(PROJECT_NAME "ejdb")

option(WITH_TESTS "Build EJDB test cases" OFF)
option(ENABLE_STATIC_LIB "Build ejdb static library" OFF)

project(${PROJECT_NAME}
		VERSION 1.2.0
		LANGUAGES C)
	
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
		
set(EJDB_LLIBRARIES)
set(EJDB_INCLUDE_DIRS)
set(ALL_SRC)
set(ALL_HDRS)
		
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
include(EjdbUtils)

macro_ensure_out_of_source_build(
    "${CMAKE_PROJECT_NAME} requires an out of source build."
)

if (NOT CMAKE_BUILD_TYPE)
message(FATAL_ERROR "Please specify the build type -DCMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo")
endif(NOT CMAKE_BUILD_TYPE)


set(MODULES tcutil tchdb tcbdb tctdb tcfdb bson ejdb)

set(EJDB_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/src/generated)
list(APPEND EJDB_INCLUDE_DIRS "${EJDB_GENERATED_DIR}")

foreach(MODULE IN LISTS MODULES)
	list(APPEND EJDB_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}")
	file(GLOB MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/*.c)
	file(GLOB MODULE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/*.h)
	list(APPEND ALL_SRC ${MODULE_SRC})
	list(APPEND ALL_HDRS ${MODULE_HDRS})
endforeach(MODULE)

if (ENABLE_STATIC_LIB)
add_library(ejdb STATIC ${ALL_SRC})
else()
add_library(ejdb SHARED ${ALL_SRC})
endif(ENABLE_STATIC_LIB)

set_target_properties(ejdb PROPERTIES
					  VERSION ${ejdb_VERSION}
					  SOVERSION ${ejdb_VERSION_MAJOR})


find_package(Threads REQUIRED CMAKE_THREAD_PREFER_PTHREAD)
if (CMAKE_USE_WIN32_THREADS_INIT)
add_definitions(-D_TC_WIN32_THREADS)
elseif (CMAKE_USE_PTHREADS_INIT)
add_definitions(-D_TC_PTHREADS)
else()
mesage(FATAL_ERROR "Unable to find suitable threading library")
endif(CMAKE_USE_WIN32_THREADS_INIT)
list(APPEND EJDB_LLIBRARIES ${CMAKE_THREAD_LIBS_INIT})

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

include(CheckIncludeFile)
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

#todo check the host system!

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fsigned-char -Wall -pedantic")
set(CMAKE_C_FLAGS_DEBUG "-O0 -Wfatal-errors -Werror -DDEBUG -D_DEBUG -UNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -Wfatal-errors -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/basedefs.h.in ${EJDB_GENERATED_DIR}/basedefs.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/myconf.h.in ${EJDB_GENERATED_DIR}/myconf.h)
file(GLOB GENERATED_HDRS ${EJDB_GENERATED_DIR}/*.h)
list(APPEND ALL_HDRS ${GENERATED_HDRS})

if (WITH_TESTS) 
	include(CTest)
	find_package(CUnit REQUIRED)
endif(WITH_TESTS)

foreach(MODULE IN LISTS MODULES)
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE})
	endif()
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/tools/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/tools)
	endif()
	if (WITH_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/tests/CMakeLists.txt)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/${MODULE}/tests)
	endif() 
endforeach(MODULE)

message("")
message("#############################################")
message(" ENABLE_STATIC_LIB: ${ENABLE_STATIC_LIB}")
message(" WITH_TESTS: ${WITH_TESTS}")
message(" LINK LIBS: ${EJDB_LLIBRARIES}")
message(" INCLUDE DIRS: ${EJDB_INCLUDE_DIRS}")
message(" SOURCES: ${ALL_SRC}")
message("#############################################")
message("")
