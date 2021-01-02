# Build dart binding shared library

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.txt _VERSION)
set_property(GLOBAL PROPERTY EJDB2_DART_VERSION_PROPERTY
                             "${PROJECT_VERSION}${_VERSION}")
set(EJDB2_DART_VERSION "${PROJECT_VERSION}${_VERSION}")

add_library(ejdb2_dart SHARED lib/ejdb2_dart.c)
target_link_libraries(ejdb2_dart ejdb2_s ${PROJECT_LLIBRARIES})

set(dart_COMPILE_FLAGS " ")
set(dart_LINK_FLAGS " ")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(dart_LINK_FLAGS "-dynamic -undefined dynamic_lookup")
endif()

set(DART_PUB_DIR ${CMAKE_CURRENT_BINARY_DIR}/ejdb2_dart)

configure_file(lib/ejdb2_dart.c ${DART_PUB_DIR}/lib/ejdb2_dart.c COPYONLY)
configure_file(lib/ejdb2_dart.dart ${DART_PUB_DIR}/lib/ejdb2_dart.dart COPYONLY)
configure_file(test/ejdb2_dart_test.dart
               ${DART_PUB_DIR}/test/ejdb2_dart_test.dart COPYONLY)
configure_file(example/example.dart ${DART_PUB_DIR}/example/example.dart
               COPYONLY)
configure_file(example/isolate.dart ${DART_PUB_DIR}/example/isolate.dart
               COPYONLY)

configure_file(README.md ${DART_PUB_DIR}/README.md COPYONLY)
configure_file(LICENSE ${DART_PUB_DIR}/LICENSE COPYONLY)
configure_file(CHANGELOG.md ${DART_PUB_DIR}/CHANGELOG.md @ONLY)
configure_file(pubspec.yaml.in ${DART_PUB_DIR}/pubspec.yaml @ONLY)
configure_file(analysis_options.yaml ${DART_PUB_DIR}/analysis_options.yaml
               COPYONLY)
configure_file(.packages ${DART_PUB_DIR}/.packages COPYONLY)

set_target_properties(
  ejdb2_dart
  PROPERTIES COMPILE_FLAGS ${dart_COMPILE_FLAGS} LINK_FLAGS ${dart_LINK_FLAGS}
             LIBRARY_OUTPUT_DIRECTORY ${DART_PUB_DIR}/lib)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_custom_command(
    TARGET ejdb2_dart
    POST_BUILD
    COMMAND strip -s $<TARGET_FILE:ejdb2_dart>)
endif()

set_target_properties(
  ejdb2_dart
  PROPERTIES VERSION ${PROJECT_VERSION} OUTPUT_NAME ejdb2dart
             SOVERSION ${PROJECT_VERSION_MAJOR} DEFINE_SYMBOL IW_API_EXPORTS)

install(
  TARGETS ejdb2_dart
  FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(FILES ${DART_PUB_DIR}/lib/ejdb2_dart.dart ${DART_PUB_DIR}/README.md
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/dart)

install(FILES ${DART_PUB_DIR}/example/example.dart
              ${DART_PUB_DIR}/example/isolate.dart
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/dart/example)

if(BUILD_TESTS)
  find_program(DART_EXEC dart HINTS /usr/bin /usr/local/bin)
  if(DART_EXEC MATCHES "DART_EXEC-NOTFOUND")
    message(FATAL_ERROR "`dart` executable not found")
  endif()
  find_program(PUB_EXEC pub /usr/bin /usr/local/bin)
  if(PUB_EXEC MATCHES "PUB_EXEC-NOTFOUND")
    set(PUB_EXEC ${DART_EXEC} pub)
  endif()

  add_custom_command(
    COMMAND ${PUB_EXEC} get
    OUTPUT ${DART_PUB_DIR}/pubspec.lock
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pubspec.yaml
    WORKING_DIRECTORY ${DART_PUB_DIR}
    VERBATIM)
  add_custom_target(pub_get ALL DEPENDS ${DART_PUB_DIR}/pubspec.lock)

  add_test(
    NAME ejdb2dart
    COMMAND ${DART_EXEC} --enable-asserts ./test/ejdb2_dart_test.dart
    WORKING_DIRECTORY ${DART_PUB_DIR})
endif(BUILD_TESTS)
