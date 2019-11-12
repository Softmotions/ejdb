if (NOT DEFINED REACT_NATIVE_PUB_DIR)
  set(REACT_NATIVE_PUB_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif ()

find_program(YARN_EXEC yarn)
if (YARN_EXEC MATCHES "YARN_EXEC-NOTFOUND")
  message(FATAL_ERROR "`yarn` executable not found")
endif ()

set(ANDROID_LIBS_DIR "${REACT_NATIVE_PUB_DIR}/android/libs")
foreach (AABI IN ITEMS ${ANDROID_ABIS})
  list(APPEND ANDROID_ABIS_LIBS "android_${AABI}")
endforeach ()

add_custom_target(
  ejdb2_react_native ALL
  DEPENDS ${ANDROID_ABIS_LIBS}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/../../ejdb2_android/libs ${ANDROID_LIBS_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../README.md ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${YARN_EXEC} pack --filename ${REACT_NATIVE_PUB_DIR}/ejdb2_react_native.tgz
  WORKING_DIRECTORY ${REACT_NATIVE_PUB_DIR}
)

