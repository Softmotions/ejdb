include(FindJava)
include(UseJava)
include(FindJNI)

if (NOT JNI_FOUND)
  message(FATAL_ERROR "No JNI headers found")
endif()

message("Java VERSION: ${Java_VERSION}")
message("Java EXECUTABLE: ${Java_JAVA_EXECUTABLE}")

set(CMAKE_JAVA_COMPILE_FLAGS "-source" "${Java_VERSION_MAJOR}"
                             "-target" "${Java_VERSION_MAJOR}")
add_jar(
  ejdb2jni_jar
  SOURCES
  main/java/com/softmotions/ejdb2/JQL.java
  main/java/com/softmotions/ejdb2/JQLCallback.java
  main/java/com/softmotions/ejdb2/EJDB2.java
  main/java/com/softmotions/ejdb2/EJDB2Exception.java
  main/java/com/softmotions/ejdb2/EJDB2Builder.java
  main/java/com/softmotions/ejdb2/IWKVOptions.java
  main/java/com/softmotions/ejdb2/example/EJDB2Example.java
  VERSION ${PROJECT_VERSION}
  OUTPUT_NAME ejdb2jni
  GENERATE_NATIVE_HEADERS ejdb2-jni-native-headers DESTINATION ${PROJECT_GENERATED_DIR})

install_jar(ejdb2jni_jar ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/java)

include_directories(${JNI_INCLUDE_DIRS})
add_library(ejdb2jni SHARED ejdb2jni.c)
add_dependencies(ejdb2jni ejdb2jni_jar)
target_link_libraries(ejdb2jni PUBLIC ejdb2_s ${JNI_LIBRARIES}
                      PRIVATE ejdb2-jni-native-headers)

if (APPLE)
  set_target_properties(ejdb2jni PROPERTIES SUFFIX ".jnilib")
endif()
set_target_properties(ejdb2jni PROPERTIES
                      VERSION ${PROJECT_VERSION}
                      SOVERSION ${PROJECT_VERSION_MAJOR}
                      DEFINE_SYMBOL IW_API_EXPORTS)

install(TARGETS ejdb2jni
        FRAMEWORK DESTINATION ${FRAMEWORK_INSTALL_DIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})


add_dependencies(ejdb2jni ejdb2jni_jar)

# Test casess
if (BUILD_TESTS)
  add_jar(
    ejdb2jni_test_jar
    SOURCES
    test/java/com/softmotions/ejdb2/TestEJDB2.java
    INCLUDE_JARS ejdb2jni_jar
    OUTPUT_NAME ejdb2jni_tests)

  get_target_property(_EJDB2JNI_JAR ejdb2jni_jar JAR_FILE)
  get_target_property(_EJDB2JNI_TEST_JAR ejdb2jni_test_jar JAR_FILE)

  message("Test: ${Java_JAVA_EXECUTABLE} -ea -Djava.library.path=. -cp ${_EJDB2JNI_JAR}:${_EJDB2JNI_TEST_JAR} com.softmotions.ejdb2.TestEJDB2")
  add_test(NAME ejdb2jni
           COMMAND ${Java_JAVA_EXECUTABLE}
                  -ea
                  #-verbose:jni
                  -Djava.library.path=.
                  -cp ${_EJDB2JNI_JAR}:${_EJDB2JNI_TEST_JAR}
                  com.softmotions.ejdb2.TestEJDB2
           )

endif (BUILD_TESTS)