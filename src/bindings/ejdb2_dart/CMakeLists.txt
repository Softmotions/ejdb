# Build dart shared library

add_library(ejdb2_dart SHARED lib/ejdb2_dart.c)
target_link_libraries(ejdb2_dart ejdb2_s ${PROJECT_LLIBRARIES})

set(dart_COMPILE_FLAGS " ")
set(dart_LINK_FLAGS " ")

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(dart_LINK_FLAGS "-dynamic -undefined dynamic_lookup")
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(dart_COMPILE_FLAGS "${dart_COMPILE_FLAGS} -arch i386")
  else()
    set(dart_COMPILE_FLAGS "${dart_COMPILE_FLAGS} -arch x86_64")
  endif()
else()
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(dart_COMPILE_FLAGS "${dart_COMPILE_FLAGS} -m64")
  else()
    set(dart_COMPILE_FLAGS "${dart_COMPILE_FLAGS} -m32")
  endif()
endif()

set(DART_PUB_DIR ${CMAKE_CURRENT_BINARY_DIR}/ejdb2_dart)

configure_file(lib/ejdb2_dart.c ${DART_PUB_DIR}/lib/ejdb2_dart.c COPYONLY)
configure_file(lib/ejdb2_dart.dart ${DART_PUB_DIR}/lib/ejdb2_dart.dart COPYONLY)
configure_file(test/ejdb2_dart_test.dart ${DART_PUB_DIR}/test/ejdb2_dart_test.dart COPYONLY)
configure_file(example/example.dart ${DART_PUB_DIR}/example/example.dart COPYONLY)

configure_file(README.md ${DART_PUB_DIR}/README.md COPYONLY)
configure_file(LICENSE ${DART_PUB_DIR}/LICENSE COPYONLY)
configure_file(Changelog ${DART_PUB_DIR}/CHANGELOG.md COPYONLY)
configure_file(pubspec.yaml ${DART_PUB_DIR}/pubspec.yaml COPYONLY)
configure_file(.packages ${DART_PUB_DIR}/.packages COPYONLY)

set_target_properties(ejdb2_dart PROPERTIES
                      COMPILE_FLAGS ${dart_COMPILE_FLAGS}
                      LINK_FLAGS ${dart_LINK_FLAGS}
                      LIBRARY_OUTPUT_DIRECTORY ${DART_PUB_DIR}/lib)

# add_custom_command(
# TARGET foo POST_BUILD
# COMMAND ${CMAKE_COMMAND} -E copy
#         ${CMAKE_SOURCE_DIR}/test/input.txt
#         ${CMAKE_CURRENT_BINARY_DIR}/input.txt)

