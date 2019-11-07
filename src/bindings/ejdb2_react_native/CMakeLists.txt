file(
  COPY .
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

add_subdirectory(binding)
if (BUILD_TESTS)
  add_subdirectory(tests)
endif()
