# Executables
add_executable(jbs jbs.c)
target_link_libraries(jbs ejdb2_s)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_custom_command(
    TARGET jbs
    POST_BUILD
    COMMAND strip -s $<TARGET_FILE:jbs>)
endif()

if(DO_INSTALL_CORE)
  install(TARGETS jbs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
