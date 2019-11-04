if (NOT DEFINED ANDROID_NDK_HOME)
  if (DEFINED ENV{ANDROID_NDK_HOME})
    set(ANDROID_NDK_HOME $ENV{ANDROID_NDK_HOME})
  else ()
    message(FATAL_ERROR "Missing required ANDROID_NDK_HOME option/env. Use -DANDROID_NDK_HOME=...")
  endif ()
endif ()

if (NOT DEFINED REACT_NATIVE_PUB_DIR)
  set(REACT_NATIVE_PUB_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif ()

find_program(YARN_EXEC yarn)
if (YARN_EXEC MATCHES "YARN_EXEC-NOTFOUND")
  message(FATAL_ERROR "`yarn` executable not found")
endif ()

set(ANDROID_BUILD_DIR "${CMAKE_SOURCE_DIR}/build_android")
set(ANDROID_LIBS_DIR "${REACT_NATIVE_PUB_DIR}/android/libs")

foreach (AABI IN ITEMS ${ANDROID_ABIS})
  list(APPEND ANDROID_ABIS_LIBS "${ANDROID_LIBS_DIR}/${AABI}/libejdb2jni.so")
  add_custom_command(
    OUTPUT ${ANDROID_LIBS_DIR}/${AABI}/libejdb2jni.so
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${ANDROID_BUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ANDROID_BUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} -S ${CMAKE_SOURCE_DIR}
    -B ${ANDROID_BUILD_DIR}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_JNI_BINDING=ON
    -DENABLE_HTTP=OFF
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake
    -DANDROID_ABI=${AABI}
    -DANDROID_PLATFORM=android-21
    -DANDROID_NATIVE_API_LEVEL=21
    COMMAND ${CMAKE_COMMAND} --build ${ANDROID_BUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ANDROID_LIBS_DIR}/${AABI}
    COMMAND ${CMAKE_COMMAND} -E copy ${ANDROID_BUILD_DIR}/src/bindings/ejdb2_jni/src/libejdb2jni.so ${ANDROID_LIBS_DIR}/${AABI}
    COMMAND ${CMAKE_COMMAND} -E copy ${ANDROID_BUILD_DIR}/src/bindings/ejdb2_jni/src/ejdb2jni.jar ${ANDROID_LIBS_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${ANDROID_BUILD_DIR}
    VERBATIM
  )
endforeach ()

add_custom_target(
  ejdb2_react_native ALL
  DEPENDS ${ANDROID_ABIS_LIBS}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../README.md ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${YARN_EXEC} pack --filename ${REACT_NATIVE_PUB_DIR}/ejdb2_react_native.tgz
  WORKING_DIRECTORY ${REACT_NATIVE_PUB_DIR}
)

