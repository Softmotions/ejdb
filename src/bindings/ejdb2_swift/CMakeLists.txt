
find_program(SWIFT_EXEC swift)
if (SWIFT_EXEC MATCHES "SWIFT_EXEC-NOTFOUND")
  message(FATAL_ERROR "`swift` executable not found")
endif ()

file(
  COPY .
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
  REGEX "(/\\.build)|(.+\\.db)" EXCLUDE
)

list(TRANSFORM PROJECT_INCLUDE_DIRS PREPEND "-Xcc;-I" OUTPUT_VARIABLE SWIFT_IDIRS)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(SWIFT_BUILD_TYPE "release")
else()
  set(SWIFT_BUILD_TYPE "debug")
endif()

add_custom_target(
  ejdb2_swift_binding ALL
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/EJDB2Swift
  COMMAND swift build
               -c ${SWIFT_BUILD_TYPE}
               -Xlinker -L${CMAKE_BINARY_DIR}/src
               -Xcc -DINPROJECT_BUILD
               ${SWIFT_IDIRS}
  COMMAND_EXPAND_LISTS
)

add_dependencies(ejdb2_swift_binding ejdb2)

if (BUILD_TESTS)

  if(${CMAKE_VERSION} VERSION_LESS "3.16.0")

    add_test(
      NAME ejdb2_swift
      COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src;DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src"
              swift test
                  -Xlinker -L${CMAKE_BINARY_DIR}/src
                  -Xcc -DINPROJECT_BUILD
                  ${SWIFT_IDIRS}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/EJDB2Swift
    )

  else()

    add_test(
      NAME ejdb2_swift
      COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src;DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src"
              swift test
                  -Xlinker -L${CMAKE_BINARY_DIR}/src
                  -Xcc -DINPROJECT_BUILD
                  ${SWIFT_IDIRS}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/EJDB2Swift
      COMMAND_EXPAND_LISTS
    )
  endif()

endif()





