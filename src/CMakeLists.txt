if(NOT CMAKE_BUILD_TYPE)
  message(
    FATAL_ERROR
      "Please specify the build type -DCMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo"
  )
endif()

set(MODULES util jbl jql jbi)
set(PROJECT_LLIBRARIES)
set(PROJECT_INCLUDE_DIRS)
set(ALL_SRC)
set(ALL_HDRS)
set(PUB_HDRS)
set(PROJECT_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
list(APPEND PROJECT_INCLUDE_DIRS "${PROJECT_GENERATED_DIR}"
     "${CMAKE_CURRENT_SOURCE_DIR}")

if(APPLE)
  option(BUILD_FRAMEWORK "Build an OS X framework" OFF)
  set(FRAMEWORK_INSTALL_DIR
      "/Library/Frameworks"
      CACHE STRING "Directory to install frameworks to.")
endif()

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(TestBigEndian)

include(AddIOWOW)

if(ENABLE_HTTP)
  if(WIN32)
    message(FATAL_ERROR "ENABLE_HTTP option cannot be used in Windows build")
  endif()
  include(AddFacil)
  add_definitions(-DJB_HTTP)
  list(APPEND MODULES jbr)
endif()

if((CMAKE_BUILD_TYPE EQUAL Release) OR (CMAKE_BUILD_TYPE EQUAL RelWithDebInfo))
  add_definition(-DJB_RELEASE=1)
endif()

include(TestQsortR)
if(HAVE_QSORT_R)
  add_definitions(-DJB_HAVE_QSORT_R)
endif()

test_big_endian(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN EQUAL 1)
  add_definitions(-DIW_BIGENDIAN=1)
endif()

if(CMAKE_SIZEOF_VOID_P MATCHES 8)
  add_definitions(-DIW_64)
else()
  add_definitions(-DIW_32)
endif()

if(BUILD_TESTS)
  add_definitions(-DIW_TESTS)
endif()

find_package(Threads REQUIRED CMAKE_THREAD_PREFER_PTHREAD)
if(CMAKE_USE_WIN32_THREADS_INIT)
  add_definitions(-DJB_WIN32_THREADS)
elseif(CMAKE_USE_PTHREADS_INIT)
  add_definitions(-DJB_PTHREADS)
else()
  mesage(FATAL_ERROR "Unable to find suitable threading library")
endif(CMAKE_USE_WIN32_THREADS_INIT)

if(ANDROID)
  find_library(LOG_LIB log)
  if(NOT LOG_LIB)
    message(FATAL_ERROR "Library 'log' not FOUND")
  endif()
  list(APPEND PROJECT_LLIBRARIES "${LOG_LIB}")
endif()

if(NOT WIN32)
  list(APPEND PROJECT_LLIBRARIES ${CMAKE_THREAD_LIBS_INIT})
else()
  include(Win32LIBTools)
  check_include_file(windows.h HAVE_WINDOWS_H)
  if(NOT HAVE_WINDOWS_H)
    message(FATAL_ERROR "Unable to find windows.h include file")
  endif()

  set(IBERTY_FIND_REQUIRED ON)
  include(FindLibIberty)
  list(APPEND PROJECT_LLIBRARIES ${IBERTY_LIBRARIES})

  check_library_exists(winpthread pthread_exit "" HAVE_WINPTHREAD)
  if(NOT HAVE_WINPTHREAD)
    message(FATAL_ERROR "Unable to winpthread lib")
  endif()
  list(INSERT PROJECT_LLIBRARIES 0 -lwinpthread)
  add_definitions(-D_POSIX_THREAD_SAFE_FUNCTIONS)
endif()

foreach(HF IN ITEMS stdlib stddef stdint stdbool)
  string(TOUPPER "${HF}" UHF)
  check_include_file(${HF}.h "JB_HAVE_${UHF}")
  if(NOT JB_HAVE_${UHF})
    message(FATAL_ERROR "Include file '${HF}.h' not FOUND")
  endif()
endforeach()

add_definitions(-D_XOPEN_SOURCE=600)
add_definitions(-D_DEFAULT_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE)
add_definitions(-D_FILE_OFFSET_BITS=64)
if(APPLE)
  add_definitions(-D_DARWIN_C_SOURCE)
endif(APPLE)

file(GLOB ROOT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
list(APPEND ALL_SRC ${ROOT_SRC})

foreach(MODULE IN LISTS MODULES)
  file(GLOB MODULE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.c)
  file(GLOB MODULE_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.h)
  list(APPEND ALL_SRC ${MODULE_SRC})
  list(APPEND ALL_HDRS ${MODULE_HDRS})
  list(APPEND PROJECT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE})
endforeach(MODULE)

list(
  APPEND
  PUB_HDRS
  ${CMAKE_CURRENT_SOURCE_DIR}/ejdb2.h
  ${CMAKE_CURRENT_SOURCE_DIR}/jbl/jbl.h
  ${CMAKE_CURRENT_SOURCE_DIR}/jbr/jbr.h
  ${CMAKE_CURRENT_SOURCE_DIR}/jql/jql.h
  ${CMAKE_CURRENT_SOURCE_DIR}/util/lwre.h)

set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -std=gnu11 -fsigned-char -pedantic -Wfatal-errors -Wall -Wextra \
                  -Wno-sign-compare -Wno-unused-parameter -Wno-unknown-pragmas -Wno-unused-function \
                  -Wno-missing-field-initializers -Wno-missing-braces \
                  -Wno-shorten-64-to-32")

if(NOT WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-implicit-fallthrough -fPIC")
else()
  add_definitions(-D__USE_MINGW_ANSI_STDIO)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pedantic-ms-format")
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

if(ASAN)
  set(CMAKE_C_ASAN "-fsanitize=address -fno-omit-frame-pointer")
endif()

set(CMAKE_C_FLAGS_DEBUG
    "${CMAKE_C_ASAN} -O0 -g -ggdb -Werror -DDEBUG -D_DEBUG -UNDEBUG -Wno-unused-variable"
)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_ASAN} -O3 -DNDEBUG")
# set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,-s")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} -g")
set(CMAKE_C_FLAGS_RELEASEWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

if(CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tmpl/ejdb2cfg.h
               ${PROJECT_GENERATED_DIR}/ejdb2cfg.h)
file(GLOB PROJECT_GENERATED_HDRS ${PROJECT_GENERATED_DIR}/*.h)
list(APPEND ALL_HDRS ${PROJECT_GENERATED_HDRS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tmpl/libejdb2.pc.in
               ${PROJECT_GENERATED_DIR}/libejdb2.pc @ONLY)

if(DO_INSTALL_CORE)
  install(FILES ${PROJECT_GENERATED_DIR}/libejdb2.pc
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

list(REMOVE_DUPLICATES PROJECT_LLIBRARIES)
list(REMOVE_DUPLICATES PROJECT_INCLUDE_DIRS)
include_directories(${PROJECT_INCLUDE_DIRS})

foreach(MODULE IN LISTS MODULES)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/CMakeLists.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE})
  endif()
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/tools/CMakeLists.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/tools)
  endif()
  if(BUILD_TESTS AND EXISTS
                     ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/tests/CMakeLists.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/tests)
  endif()
  if(BUILD_EXAMPLES
     AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/examples/CMakeLists.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/examples)
  endif()
  if(BUILD_BENCHMARKS
     AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/benchmark/CMakeLists.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/benchmark)
  endif()
endforeach(MODULE)

if(ENABLE_HTTP)
  add_subdirectory(jbs)
endif()

if(BUILD_TESTS)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()

if(BUILD_EXAMPLES)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
endif()

if(BUILD_DART_BINDING)
  add_subdirectory(bindings/ejdb2_dart)
endif()

if(BUILD_JNI_BINDING)
  add_subdirectory(bindings/ejdb2_jni)
endif()

if(BUILD_NODEJS_BINDING)
  add_subdirectory(bindings/ejdb2_node)
endif()

if(BUILD_ANDROID_LIBS)
  add_subdirectory(bindings/ejdb2_android)
endif()

if(BUILD_REACT_NATIVE_BINDING)
  add_subdirectory(bindings/ejdb2_react_native)
endif()

if(BUILD_FLUTTER_BINDING)
  add_subdirectory(bindings/ejdb2_flutter)
endif()

if(BUILD_SWIFT_BINDING)
  add_subdirectory(bindings/ejdb2_swift)
endif()

if(NOT BUILD_SHARED_LIBS)
  add_definitions(-DIW_NODLL)
  add_library(ejdb2 STATIC ${ALL_SRC})
  add_library(ejdb2_s ALIAS ejdb2)
else()
  add_library(ejdb2 SHARED ${ALL_SRC})
  add_library(ejdb2_s STATIC ${ALL_SRC})
endif()

target_link_libraries(ejdb2 ${PROJECT_LLIBRARIES})
if(BUILD_SHARED_LIBS)
  target_link_libraries(ejdb2_s ${PROJECT_LLIBRARIES})
endif()

if(BUILD_SHARED_LIBS)
  if(WIN32)
    add_dependencies(ejdb2 wintools_init)
    set_target_properties(ejdb2 PROPERTIES LINK_FLAGS
                                           "-Wl,--output-def,libejdb2.def")
    add_w32_importlib(ejdb2 libejdb2 ${CMAKE_CURRENT_BINARY_DIR})

    if(DO_INSTALL_CORE)
      install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/libejdb2.def
              ${CMAKE_CURRENT_BINARY_DIR}/libejdb2.lib
              ${CMAKE_CURRENT_BINARY_DIR}/libejdb2.exp
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

  endif()
  set_target_properties(
    ejdb2
    PROPERTIES VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR}
               PUBLIC_HEADER "${PUB_HDRS}"
               DEFINE_SYMBOL IW_API_EXPORTS)

  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(
      TARGET ejdb2
      POST_BUILD
      COMMAND strip -s $<TARGET_FILE:ejdb2>)
  endif()

  set_target_properties(
    ejdb2_s PROPERTIES VERSION ${PROJECT_VERSION} COMPILE_FLAGS "-DIW_NODLL"
                       OUTPUT_NAME ejdb2-${PROJECT_VERSION_MAJOR})
else()
  set_target_properties(
    ejdb2
    PROPERTIES VERSION ${PROJECT_VERSION}
               PUBLIC_HEADER "${PUB_HDRS}"
               COMPILE_FLAGS "-DIW_NODLL"
               LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE} -s"
               OUTPUT_NAME ejdb2-${PROJECT_VERSION_MAJOR})
endif()

if(DO_INSTALL_CORE)
  install(
    TARGETS ejdb2
    EXPORT ejdb2-exports
    FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
  install(EXPORT ejdb2-exports
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})
endif()

if(BUILD_SHARED_LIBS AND DO_INSTALL_CORE)
  install(
    TARGETS ejdb2_s
    EXPORT ejdb2-static-exports
    FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
  install(EXPORT ejdb2-static-exports
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})
endif()

if(DO_INSTALL_CORE)
  # Install iowow headers included into ejdb2
  install(
    DIRECTORY ${IOWOW_INCLUDE_DIR}/${PROJECT_NAME}/iowow
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    COMPONENT headers
    FILES_MATCHING
    PATTERN "*.h")
  install(FILES ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/Changelog
          DESTINATION ${CMAKE_INSTALL_DOCDIR})
  install(
    FILES ${CMAKE_SOURCE_DIR}/README.md
    RENAME README
    DESTINATION ${CMAKE_INSTALL_DOCDIR})

  export(EXPORT ejdb2-exports)
  if(BUILD_SHARED_LIBS)
    export(EXPORT ejdb2-static-exports)
  endif(BUILD_SHARED_LIBS)

endif()

include(InstallRequiredSystemLibraries)

set(${PROJECT_NAME}_PUB_HDRS
    ${PUB_HDRS}
    CACHE INTERNAL "${PROJECT_NAME}: Public headers" FORCE)
set(${PROJECT_NAME}_INCLUDE_DIRS
    ${PROJECT_INCLUDE_DIRS}
    CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

message("")
message("CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
message("${PROJECT_NAME} LINK LIBS: ${PROJECT_LLIBRARIES}")
message("${PROJECT_NAME} ASAN: ${ASAN}")
message("\n${PROJECT_NAME} INCLUDE DIRS: ${PROJECT_INCLUDE_DIRS}")
message("\n${PROJECT_NAME} SOURCES: ${ALL_SRC}")
message("\n${PROJECT_NAME} PUB_HDRS: ${PUB_HDRS}")
message("\n${PROJECT_NAME} CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("${PROJECT_NAME} ANDROID_ABIS: ${ANDROID_ABIS}")
message("${PROJECT_NAME} ENABLE_HTTP: ${ENABLE_HTTP}")
message("${PROJECT_NAME} BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message("${PROJECT_NAME} BUILD_TESTS: ${BUILD_TESTS}")
message("${PROJECT_NAME} BUILD_EXAMPLES: ${BUILD_EXAMPLES}")
message("${PROJECT_NAME} BUILD_BENCHMARKS: ${BUILD_BENCHMARKS}")
message("${PROJECT_NAME} BUILD_DART_BINDING: ${BUILD_DART_BINDING}")
message("${PROJECT_NAME} BUILD_ANDROID_LIBS: ${BUILD_ANDROID_LIBS}")
message("${PROJECT_NAME} BUILD_JNI_BINDING: ${BUILD_JNI_BINDING}")
message("${PROJECT_NAME} BUILD_NODEJS_BINDING: ${BUILD_NODEJS_BINDING}")
message(
  "${PROJECT_NAME} BUILD_REACT_NATIVE_BINDING: ${BUILD_REACT_NATIVE_BINDING}")
message("${PROJECT_NAME} BUILD_FLUTTER_BINDING: ${BUILD_FLUTTER_BINDING}")
message("${PROJECT_NAME} BUILD_SWIFT_BINDING: ${BUILD_SWIFT_BINDING}")
