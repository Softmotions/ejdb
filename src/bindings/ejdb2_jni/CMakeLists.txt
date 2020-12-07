add_subdirectory(src)

get_target_property(DEPLOY_JAR_FILE ejdb2jni_jar JAR_FILE)
configure_file(maven-deploy.sh ${CMAKE_CURRENT_BINARY_DIR} @ONLY)
