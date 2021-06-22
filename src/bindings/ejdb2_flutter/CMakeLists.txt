file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.txt _VERSION)
set_property(GLOBAL PROPERTY EJDB2_FLUTTER_VERSION_PROPERTY
                             "${PROJECT_VERSION}.${_VERSION}")
set(EJDB2_FLUTTER_VERSION "${PROJECT_VERSION}.${_VERSION}")


file(
  COPY .
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
  REGEX "(/\\.gradle)|(/node_modules)|(/build$)|(/libs)|(/jniLibs)" EXCLUDE
)

set(ANDROID_LIBS_DIR "${CMAKE_CURRENT_BINARY_DIR}/android/src/main/jniLibs")
set(PUB_PUBLISH_DIR "${CMAKE_CURRENT_BINARY_DIR}/pub_publish")

foreach (AABI IN ITEMS ${ANDROID_ABIS})
  list(APPEND ANDROID_ABIS_LIBS "android_${AABI}")
endforeach()

add_custom_target(
  ejdb2_flutter ALL
  DEPENDS ${ANDROID_ABIS_LIBS}
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${ANDROID_LIBS_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/../ejdb2_android/libs ${ANDROID_LIBS_DIR}

  # Prepare pub publish dir:

  COMMAND ${CMAKE_COMMAND} -E make_directory ${PUB_PUBLISH_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/LICENSE ${PUB_PUBLISH_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/README.md ${PUB_PUBLISH_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/analysis_options.yaml ${PUB_PUBLISH_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/lib ${PUB_PUBLISH_DIR}/lib

  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/ios ${PUB_PUBLISH_DIR}/ios

  COMMAND ${CMAKE_COMMAND} -E make_directory ${PUB_PUBLISH_DIR}/android
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PUB_PUBLISH_DIR}/android/src
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/android/gradlew* ${PUB_PUBLISH_DIR}/android
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/android/*.gradle ${PUB_PUBLISH_DIR}/android
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/android/gradle.properties ${PUB_PUBLISH_DIR}/android
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/android/src ${PUB_PUBLISH_DIR}/android/src
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/android/gradle ${PUB_PUBLISH_DIR}/android/gradle

  COMMAND ${CMAKE_COMMAND} -E make_directory ${PUB_PUBLISH_DIR}/example
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/example/ios ${PUB_PUBLISH_DIR}/example/ios

  COMMAND ${CMAKE_COMMAND} -E make_directory ${PUB_PUBLISH_DIR}/example/android
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PUB_PUBLISH_DIR}/example/android/app
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/example/*.md ${PUB_PUBLISH_DIR}/example
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/example/*.yaml ${PUB_PUBLISH_DIR}/example
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/example/lib ${PUB_PUBLISH_DIR}/example/lib
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/example/test_driver ${PUB_PUBLISH_DIR}/example/test_driver
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/example/android/*.gradle ${PUB_PUBLISH_DIR}/example/android
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/example/android/gradlew* ${PUB_PUBLISH_DIR}/example/android
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/example/android/gradle.properties ${PUB_PUBLISH_DIR}/example/android
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/example/android/app/*.gradle ${PUB_PUBLISH_DIR}/example/android/app
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/example/android/app/src ${PUB_PUBLISH_DIR}/example/android/app/src
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/example/android/gradle ${PUB_PUBLISH_DIR}/example/android/gradle
)

configure_file(pubspec.yaml.in ${PUB_PUBLISH_DIR}/pubspec.yaml @ONLY)
configure_file(CHANGELOG.md ${PUB_PUBLISH_DIR}/CHANGELOG.md @ONLY)