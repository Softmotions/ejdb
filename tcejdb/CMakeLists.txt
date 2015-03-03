cmake_minimum_required(VERSION 3.0)
set(PROJECT_NAME "ejdb")

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build EJDB test cases" OFF)

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

set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
set(EXEC_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE PATH "Installation prefix for executables and object code libraries" FORCE)
set(BIN_INSTALL_DIR ${EXEC_INSTALL_PREFIX}/bin CACHE PATH "Installation prefix for user executables" FORCE)
set(LIB_INSTALL_DIR ${EXEC_INSTALL_PREFIX}/lib${LIB_SUFFIX} CACHE PATH  "Installation prefix for object code libraries" FORCE)
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/include/ejdb CACHE PATH "Installation prefix for C header files" FORCE)
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
if (NOT ENABLE_SHARED)
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
target_link_libraries(ejdb_p ${EJDB_LLIBRARIES})

#todo check the host system!

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fsigned-char -pedantic")
set(CMAKE_C_FLAGS_DEBUG "-O0 -Wfatal-errors -Werror -DDEBUG -D_DEBUG -UNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -Wfatal-errors -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2")

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
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/libejdb.pc.in ${EJDB_GENERATED_DIR}/libejdb.pc)
	install(FILES ${EJDB_GENERATED_DIR}/libejdb.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
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
					  DEFINE_SYMBOL EJDB_NODLL)

install(TARGETS ejdb
	FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
	LIBRARY DESTINATION ${LIB_INSTALL_DIR}
	RUNTIME DESTINATION ${BIN_INSTALL_DIR}
	ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
	PUBLIC_HEADER DESTINATION ${INCLUDE_INSTALL_DIR}
)

message("")
message("#############################################")
message(" BUILD_STATIC_LIB: ${BUILD_STATIC_LIB}")
message(" BUILD_TESTS: ${BUILD_TESTS}")
message(" LINK LIBS: ${EJDB_LLIBRARIES}")
message(" INCLUDE DIRS: ${EJDB_INCLUDE_DIRS}")
message(" SOURCES: ${ALL_SRC}")
message(" PUB_HDRS: ${PUB_HDRS}")
message("#############################################")
message("")
