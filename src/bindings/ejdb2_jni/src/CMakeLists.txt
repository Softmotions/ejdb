include(FindJava)
include(UseJava)
include(FindJNI)

if(NOT JNI_FOUND)
  if(ANDROID_PLATFORM)
    list(FILTER JNI_INCLUDE_DIRS EXCLUDE REGEX NOTFOUND)
    list(FILTER JNI_LIBRARIES EXCLUDE REGEX NOTFOUND)
    list(LENGTH JNI_INCLUDE_DIRS _JNI_INCLUDE_DIRS_LEN)
    if(_JNI_INCLUDE_DIRS_LEN LESS "1")
      message(FATAL_ERROR "No JNI headers found ${JNI_INCLUDE_DIRS}")
    endif()
  else()
    message(FATAL_ERROR "No JNI headers found ${JNI_INCLUDE_DIRS}")
  endif()
endif()

message("Java VERSION: ${Java_VERSION}")
message("Java EXECUTABLE: ${Java_JAVA_EXECUTABLE}")

if(ANDROID_PLATFORM)
  set(CMAKE_JAVA_COMPILE_FLAGS "-source" "1.8" "-target" "1.8")
  add_jar(
    ejdb2jni_jar
    SOURCES android/java/com/softmotions/ejdb2/JQL.java
            android/java/com/softmotions/ejdb2/JQLCallback.java
            android/java/com/softmotions/ejdb2/EJDB2.java
            android/java/com/softmotions/ejdb2/EJDB2Exception.java
            android/java/com/softmotions/ejdb2/EJDB2Builder.java
            android/java/com/softmotions/ejdb2/IWKVOptions.java
    VERSION ${PROJECT_VERSION}
    OUTPUT_NAME
      ejdb2
      GENERATE_NATIVE_HEADERS
      ejdb2-jni-native-headers
      DESTINATION
      ${PROJECT_GENERATED_DIR})
else()
  add_jar(
    ejdb2jni_jar
    SOURCES main/java/com/softmotions/ejdb2/JSON.java
            main/java/com/softmotions/ejdb2/JSONException.java
            main/java/com/softmotions/ejdb2/JQL.java
            main/java/com/softmotions/ejdb2/JQLCallback.java
            main/java/com/softmotions/ejdb2/JQLJsonCallback.java
            main/java/com/softmotions/ejdb2/EJDB2.java
            main/java/com/softmotions/ejdb2/EJDB2Exception.java
            main/java/com/softmotions/ejdb2/EJDB2Builder.java
            main/java/com/softmotions/ejdb2/IWKVOptions.java
    VERSION ${PROJECT_VERSION}
    OUTPUT_NAME ejdb2 GENERATE_NATIVE_HEADERS ejdb2-jni-native-headers
                DESTINATION ${PROJECT_GENERATED_DIR})
endif()

install_jar(ejdb2jni_jar ${CMAKE_INSTALL_DATAROOTDIR}/java)

include_directories(${JNI_INCLUDE_DIRS})
add_library(ejdb2jni SHARED ejdb2jni.c)
add_dependencies(ejdb2jni ejdb2jni_jar)

if(ANDROID)
  set(JNI_LIBRARIES "-ldl -llog")
endif()

if(${CMAKE_VERSION} VERSION_LESS "3.11")
  create_javah(
    TARGET ejdb2-jni-native-headers-old
    CLASSES com.softmotions.ejdb2.EJDB2 com.softmotions.ejdb2.JQL
    CLASSPATH ejdb2jni_jar
    DEPENDS ejdb2jni_jar
    OUTPUT_DIR ${PROJECT_GENERATED_DIR})
  add_dependencies(ejdb2jni ejdb2-jni-native-headers-old)
  target_link_libraries(ejdb2jni PUBLIC ${JNI_LIBRARIES} ejdb2_s)
else()
  target_link_libraries(
    ejdb2jni
    PUBLIC ${JNI_LIBRARIES} ejdb2_s
    PRIVATE ejdb2-jni-native-headers)
endif()

if(APPLE)
  set_target_properties(ejdb2jni PROPERTIES SUFFIX ".jnilib")
endif()

set_target_properties(
  ejdb2jni
  PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR}
             LINK_FLAGS_RELEASE "${LINK_FLAGS_RELEASE} -s" DEFINE_SYMBOL
                                                           IW_API_EXPORTS)

install(
  TARGETS ejdb2jni
  FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

add_dependencies(ejdb2jni ejdb2jni_jar)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Add symlink /usr/java/packages/lib/libejdb2jni.so -> installed
  # libejdb2jni.so
  set(_EJDB2JNI_PATH
      ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/$<TARGET_FILE_NAME:ejdb2jni>
  )
  set(_EJDB2JNI_FILE
      ${CMAKE_SHARED_LIBRARY_PREFIX}ejdb2jni${CMAKE_SHARED_LIBRARY_SUFFIX})

  if(NOT POLICY CMP0087)
    add_custom_command(
      TARGET ejdb2jni
      PRE_BUILD
      COMMAND ln -sf ${_EJDB2JNI_PATH} ${CMAKE_BINARY_DIR}/${_EJDB2JNI_FILE})
  else()
    install(CODE "execute_process( \
                  COMMAND ln -fs ${_EJDB2JNI_PATH} ${_EJDB2JNI_FILE} \
                )")
  endif()

  install(FILES ${CMAKE_BINARY_DIR}/${_EJDB2JNI_FILE}
          DESTINATION /usr/java/packages/lib)
endif()

# Test cases
if(BUILD_TESTS AND NOT ANDROID_PLATFORM)
  add_jar(
    ejdb2jni_test_jar
    SOURCES test/java/com/softmotions/ejdb2/TestEJDB2.java
    INCLUDE_JARS ejdb2jni_jar
    OUTPUT_NAME ejdb2jni_tests)

  get_target_property(_EJDB2JNI_JAR ejdb2jni_jar JAR_FILE)
  get_target_property(_EJDB2JNI_TEST_JAR ejdb2jni_test_jar JAR_FILE)

  message(
    "Test: ${Java_JAVA_EXECUTABLE} -ea -Djava.library.path=. -cp ${_EJDB2JNI_JAR}:${_EJDB2JNI_TEST_JAR} com.softmotions.ejdb2.TestEJDB2"
  )
  add_test(
    NAME ejdb2jni
    COMMAND
      ${Java_JAVA_EXECUTABLE} -ea
      # -verbose:jni
      -Djava.library.path=. -cp ${_EJDB2JNI_JAR}:${_EJDB2JNI_TEST_JAR}
      com.softmotions.ejdb2.TestEJDB2)

endif()
