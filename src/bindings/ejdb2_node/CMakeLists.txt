file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.txt _VERSION)
set_property(GLOBAL PROPERTY EJDB2_NODE_VERSION_PROPERTY
                             "${PROJECT_VERSION}.${_VERSION}")
set(EJDB2_NODE_VERSION "${PROJECT_VERSION}.${_VERSION}")

if(NOT DEFINED NODE_PUB_DIR)
  set(NODE_PUB_DIR ${CMAKE_CURRENT_BINARY_DIR}/ejdb2_node)
endif()
if(NOT DEFINED NODE_BIN_ROOT)
  set(NODE_BIN_ROOT ${NODE_PUB_DIR})
endif()

# Nodejs process.arch: 'arm', 'arm64', 'ia32', 'ppc', 'ppc64', 's390', 's390x',
# 'x32', and 'x64
if(MSVC64 OR MINGW64)
  set(PROCESSOR x64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(amd64.*|x86_64.*|AMD64.*)")
  set(PROCESSOR x64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i686.*|i386.*|x86.*)")
  set(PROCESSOR x32)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*)")
  set(PROCESSOR arm64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm.*|ARM.*)")
  set(PROCESSOR arm)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64.*")
  set(PROCESSOR ppc64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)")
  set(PROCESSOR ppc)
else()
  set(PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
endif()

string(TOLOWER ${CMAKE_SYSTEM_NAME}-${PROCESSOR} NODE_BIN_SUFFIX)
set(NODE_BIN_DIR ${NODE_BIN_ROOT}/${NODE_BIN_SUFFIX})

find_program(YARN_EXEC yarn)
if(YARN_EXEC MATCHES "YARN_EXEC-NOTFOUND")
  message(FATAL_ERROR "`yarn` executable not found")
endif()

add_library(ejdb2_node MODULE ejdb2_node.c)
target_link_libraries(ejdb2_node ejdb2_s ${PROJECT_LLIBRARIES})

set_target_properties(
  ejdb2_node
  PROPERTIES OUTPUT_NAME ejdb2_node
             PREFIX ""
             SUFFIX ".node"
             LIBRARY_OUTPUT_DIRECTORY ${NODE_BIN_DIR})

target_compile_options(ejdb2_node PRIVATE -DIW_API_EXPORTS
                                          -DNODE_ADDON_API_DISABLE_DEPRECATED)

if(APPLE)
  set_target_properties(
    ejdb2_node PROPERTIES LINK_FLAGS "-rdynamic -undefined dynamic_lookup")
else()
  set_target_properties(
    ejdb2_node PROPERTIES LINK_FLAGS
                          "-rdynamic -Wl,--unresolved-symbols=ignore-all")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_custom_command(
    TARGET ejdb2_node
    POST_BUILD
    COMMAND ${STRIP_CMD} $<TARGET_FILE:ejdb2_node>)
endif()

configure_file(package.json ${NODE_PUB_DIR}/package.json @ONLY)
configure_file(CHANGELOG.md ${NODE_PUB_DIR}/CHANGELOG.md @ONLY)
configure_file(README.md ${NODE_PUB_DIR}/README.md COPYONLY)
configure_file(yarn.lock ${NODE_PUB_DIR}/yarn.lock COPYONLY)
configure_file(index.js ${NODE_PUB_DIR}/index.js COPYONLY)
configure_file(index.d.ts ${NODE_PUB_DIR}/index.d.ts COPYONLY)
configure_file(binary.js ${NODE_PUB_DIR}/binary.js COPYONLY)
configure_file(utils.js ${NODE_PUB_DIR}/utils.js COPYONLY)
configure_file(install.js ${NODE_PUB_DIR}/install.js COPYONLY)
configure_file(test.js ${NODE_PUB_DIR}/test.js COPYONLY)

add_custom_command(
  COMMAND yarn --non-interactive --no-progress install
  OUTPUT ${NODE_PUB_DIR}/node_modules
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/yarn.lock
          ${CMAKE_CURRENT_SOURCE_DIR}/package.json
  WORKING_DIRECTORY ${NODE_PUB_DIR}
  VERBATIM)
add_custom_target(yarn ALL DEPENDS ${NODE_PUB_DIR}/node_modules)

if(BUILD_TESTS)
  add_test(
    NAME ejdb2node
    COMMAND ${YARN_EXEC} --non-interactive --no-progress run test
    WORKING_DIRECTORY ${NODE_PUB_DIR})
endif()
