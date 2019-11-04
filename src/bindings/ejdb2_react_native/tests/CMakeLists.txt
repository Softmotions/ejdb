include(CTest)

add_custom_command(
  OUTPUT node_modules
  COMMAND ${YARN_EXEC} cache clean
  COMMAND ${YARN_EXEC} add ../binding/ejdb2_react_native.tgz
  COMMAND ${YARN_EXEC} install
  DEPENDS ejdb2_react_native
  ${CMAKE_CURRENT_SOURCE_DIR}/package.json
  VERBATIM
)

add_custom_target(
  ejdb2_react_native_tests ALL
  COMMAND ${YARN_EXEC} build
  DEPENDS node_modules
)

add_test(NAME ejdb2_react_native
  COMMAND ${YARN_EXEC} test
)