if (NOT CMAKE_BUILD_TYPE)
  message(FATAL_ERROR "Please specify the build type -DCMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo")
endif()

set(MODULES jbj)
set(PROJECT_LLIBRARIES)
set(PROJECT_INCLUDE_DIRS)
set(ALL_SRC)
set(ALL_HDRS)
set(PUB_HDRS)
set(PROJECT_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
list(APPEND PROJECT_INCLUDE_DIRS "${PROJECT_GENERATED_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

if(APPLE)
  option(BUILD_FRAMEWORK "Build an OS X framework" OFF)
  set(FRAMEWORK_INSTALL_DIR "/Library/Frameworks" CACHE STRING "Directory to install frameworks to.")
endif()

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(TestBigEndian)

if ((CMAKE_BUILD_TYPE EQUAL Release) OR (CMAKE_BUILD_TYPE EQUAL RelWithDebInfo))
  add_definition(-DJB_RELEASE=1)
endif()

TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if (IS_BIG_ENDIAN EQUAL 1)
  add_definitions(-DJB_BIGENDIAN=1)
endif()

if(CMAKE_SIZEOF_VOID_P MATCHES 8)
  add_definitions(-DJB_64=1)
else()
  add_definitions(-DJB_32=1)
endif()

if (BUILD_TESTS)
 add_definitions(-DJB_TESTS=1)
endif()

find_package(Threads REQUIRED CMAKE_THREAD_PREFER_PTHREAD)
if (CMAKE_USE_WIN32_THREADS_INIT)
  add_definitions(-DJB_WIN32_THREADS)
elseif (CMAKE_USE_PTHREADS_INIT)
  add_definitions(-DJB_PTHREADS)
else()
  mesage(FATAL_ERROR "Unable to find suitable threading library")
endif(CMAKE_USE_WIN32_THREADS_INIT)

if (NOT WIN32)
  list(APPEND PROJECT_LLIBRARIES ${CMAKE_THREAD_LIBS_INIT})
else()
  include(Win32LIBTools)
  
  check_include_file(windows.h HAVE_WINDOWS_H)
	if (NOT HAVE_WINDOWS_H)
		message(FATAL_ERROR "Unable to find windows.h include file")
	endif()
  
  set(IBERTY_FIND_REQUIRED, ON)
  include(FindLibIberty)
  list(APPEND PROJECT_LLIBRARIES ${IBERTY_LIBRARIES})
  
  check_library_exists(winpthread pthread_exit "" HAVE_WINPTHREAD)
	if (NOT HAVE_WINPTHREAD)
		message(FATAL_ERROR "Unable to winpthread lib")
	endif()
	list(INSERT PROJECT_LLIBRARIES 0 -lwinpthread)
  add_definitions(-D_POSIX_THREAD_SAFE_FUNCTIONS)
endif()

foreach(HF IN ITEMS stdlib stddef stdint stdbool stdatomic)
    string(TOUPPER "${HF}" UHF)
    check_include_file(${HF}.h "JB_HAVE_${UHF}")
    if (NOT JB_HAVE_${UHF})
      message(FATAL_ERROR "Include file '${HF}.h' not FOUND")
    endif()
endforeach()

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE)
add_definitions(-D_FILE_OFFSET_BITS=64)

list(APPEND ALL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/jb.c)

foreach(MODULE IN LISTS MODULES)
    file(GLOB MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.c)
    file(GLOB MODULE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.h)
    list(APPEND ALL_SRC ${MODULE_SRC})
    list(APPEND ALL_HDRS ${MODULE_HDRS})
    list(APPEND PROJECT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE})
endforeach(MODULE)

list(APPEND PUB_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/jb.h
                     ${CMAKE_CURRENT_SOURCE_DIR}/jbdefs.h
                     ${CMAKE_CURRENT_SOURCE_DIR}/jbi/jbi.h
                     )
                                 
list(REMOVE_DUPLICATES PROJECT_LLIBRARIES)
list(REMOVE_DUPLICATES PROJECT_INCLUDE_DIRS)
include_directories(${PROJECT_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -fsigned-char -pedantic -Wfatal-errors -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-unknown-pragmas -Wno-unused-function -Wno-missing-field-initializers -Wno-missing-braces")
if (NOT WIN32) 
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pedantic-ms-format")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

if (ASAN)
  set(CMAKE_C_ASAN "-fsanitize=address -fno-omit-frame-pointer")
endif()

set(CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb -Werror -DDEBUG -D_DEBUG -UNDEBUG -Wno-unused-variable ${CMAKE_C_ASAN}")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG ${CMAKE_C_ASAN}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} -g")

if (CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  if (BUILD_SHARED_LIBS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
  endif()
endif()

if (CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
  if (BUILD_SHARED_LIBS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
  endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tmpl/jbcfg.h ${PROJECT_GENERATED_DIR}/jbcfg.h)
file(GLOB PROJECT_GENERATED_HDRS ${PROJECT_GENERATED_DIR}/*.h)
list(APPEND ALL_HDRS ${PROJECT_GENERATED_HDRS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tmpl/libejdb2.pc.in ${PROJECT_GENERATED_DIR}/libejdb2.pc @ONLY)
install(FILES ${PROJECT_GENERATED_DIR}/libejdb2.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

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
    if (BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/examples/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/examples)
    endif()
    if (BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/benchmark/CMakeLists.txt)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/benchmark)
    endif()
endforeach(MODULE)

if (NOT BUILD_SHARED_LIBS)
  add_definitions(-DJB_NODLL)
  add_library(ejdb2 STATIC ${ALL_SRC})
  add_library(ejdb2_s ALIAS ejdb2)
else()
  add_library(ejdb2 SHARED ${ALL_SRC})
  add_library(ejdb2_s STATIC ${ALL_SRC})
endif()

target_link_libraries(ejdb2 ${PROJECT_LLIBRARIES})
if (BUILD_SHARED_LIBS)
  target_link_libraries(ejdb2_s ${PROJECT_LLIBRARIES})
endif()

if (BUILD_SHARED_LIBS)
    if (WIN32)
      add_dependencies(ejdb2 wintools_init)
      set_target_properties(ejdb2 PROPERTIES LINK_FLAGS "-Wl,--output-def,libejdb2.def")
      add_w32_importlib(ejdb2 libejdb2 ${CMAKE_CURRENT_BINARY_DIR})
      install(FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/libejdb2.def
        ${CMAKE_CURRENT_BINARY_DIR}/libejdb2.lib
        ${CMAKE_CURRENT_BINARY_DIR}/libejdb2.exp DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
    set_target_properties(ejdb2 PROPERTIES
                          VERSION ${PROJECT_VERSION}
                          SOVERSION ${PROJECT_VERSION_MAJOR}
                          PUBLIC_HEADER "${PUB_HDRS}"
                          DEFINE_SYMBOL JB_API_EXPORTS)
    set_target_properties(ejdb2_s PROPERTIES
                          VERSION ${PROJECT_VERSION}
                          COMPILE_FLAGS "-DJB_NODLL"
                          OUTPUT_NAME ejdb2-${PROJECT_VERSION_MAJOR})                          
else()
    set_target_properties(ejdb2 PROPERTIES
                          VERSION ${PROJECT_VERSION}
                          PUBLIC_HEADER "${PUB_HDRS}"
                          COMPILE_FLAGS "-DJB_NODLL"
                          OUTPUT_NAME ejdb2-${PROJECT_VERSION_MAJOR})
endif()
