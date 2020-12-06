
# Executables
add_executable(jbs jbs.c)
target_link_libraries(jbs ejdb2_s)

if (DO_INSTALL_CORE)
  install(TARGETS jbs
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

