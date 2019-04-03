# Build dart shared library

add_library(ejdb2_dart SHARED lib/ejdb2_dart.c)
target_link_libraries(ejdb2_dart ejdb2_s)

set(dart_COMPILE_FLAGS "")
set(dart_LINK_FLAGS "-Wl,-undefined,dynamic_lookup")

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(dart_COMPILE_FLAGS "${dart_COMPILE_FLAGS} -arch i386")
  else()
    set(dart_COMPILE_FLAGS "${dart_COMPILE_FLAGS} -arch x86_64")
  endif()
else()
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(dart_COMPILE_FLAGS "${dart_COMPILE_FLAGS} -m64")
  else()
    set(dart_COMPILE_FLAGS "${dart_COMPILE_FLAGS} -m32")
  endif()
endif()


set_target_properties(ejdb2_dart PROPERTIES
                      COMPILE_FLAGS ${dart_COMPILE_FLAGS}
                      LINK_FLAGS ${dart_LINK_FLAGS})

