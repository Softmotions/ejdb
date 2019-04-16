include(FindJava)
include(UseJava)
include(FindJNI)

if (NOT JNI_FOUND)
  message(FATAL_ERROR "No JNI headers found")
endif()

message("Java VERSION: ${Java_VERSION}")

add_jar(
  ejdb2jni_classes
  SOURCES
  main/java/com/softmotions/ejdb2/EJDB2.java
  main/java/com/softmotions/ejdb2/EJDB2Exception.java
  main/java/com/softmotions/ejdb2/EJDB2Options.java
  main/java/com/softmotions/ejdb2/IWKVOptions.java
  OUTPUT_NAME ejdb2jni-${PROJECT_VERSION}
  GENERATE_NATIVE_HEADERS ejdb2-jni-native-headers DESTINATION ${PROJECT_GENERATED_DIR})

include_directories(${JNI_INCLUDE_DIRS})

add_library(ejdb2jni SHARED ejdb2jni.c)

add_dependencies(ejdb2jni ejdb2jni_classes)

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

