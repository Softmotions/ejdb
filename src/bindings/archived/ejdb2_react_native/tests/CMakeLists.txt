include(CTest)

add_custom_command(
  OUTPUT node_modules
  COMMAND ${YARN_EXEC} --non-interactive --no-progress  install
  DEPENDS ejdb2_react_native
  ${CMAKE_CURRENT_SOURCE_DIR}/package.json
  VERBATIM
)

add_custom_target(
  ejdb2_react_native_tests ALL
  COMMAND ${YARN_EXEC} --non-interactive --no-progress build-dev
  DEPENDS node_modules
)

add_test(NAME ejdb2_react_native
  COMMAND ${YARN_EXEC} --non-interactive --no-progress test-dev
)