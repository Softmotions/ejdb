file(
  COPY .
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
  REGEX "(/\\.gradle)|(/node_modules)|(/build$)|(/libs)|(/jniLibs)" EXCLUDE
)

add_subdirectory(binding)
if (BUILD_TESTS)
  add_subdirectory(tests)
endif()
