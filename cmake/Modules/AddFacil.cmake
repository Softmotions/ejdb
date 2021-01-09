include(ExternalProject)

if (${CMAKE_VERSION} VERSION_LESS "3.8.0")
  set(_UPDATE_DISCONNECTED 0)
else()
  set(_UPDATE_DISCONNECTED 1)
endif()

set(FACIL_SOURCE_DIR "${CMAKE_BINARY_DIR}/src/extern_facil")
set(FACIL_BINARY_DIR "${CMAKE_BINARY_DIR}/src/extern_facil")
set(FACIL_INCLUDE_DIR "${FACIL_BINARY_DIR}/lib/facil")
set(FACIL_LIBRARY_DIR "${FACIL_BINARY_DIR}")

if("${FACIL_URL}" STREQUAL "")
  if(EXISTS ${CMAKE_SOURCE_DIR}/facil.zip)
    set(FACIL_URL ${CMAKE_SOURCE_DIR}/facil.zip)
  else()
    set(FACIL_URL https://github.com/Softmotions/facil.io/archive/master.zip)
  endif()
endif()

message("FACIL_URL: ${FACIL_URL}")

set(CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
               -DCMAKE_C_FLAGS=-fPIC -fvisibility=hidden)

foreach(extra
              CMAKE_C_COMPILER
              CMAKE_TOOLCHAIN_FILE
              ANDROID_PLATFORM
              ANDROID_ABI
              TEST_TOOL_CMD
              PLATFORM
              ENABLE_BITCODE
              ENABLE_ARC
              ENABLE_VISIBILITY
              ENABLE_STRICT_TRY_COMPILE
              ARCHS)
  if(DEFINED ${extra})
    list(APPEND CMAKE_ARGS "-D${extra}=${${extra}}")
  endif()
endforeach()
message("FACIL CMAKE_ARGS: ${CMAKE_ARGS}")

ExternalProject_Add(
  extern_facil
  URL ${FACIL_URL}
  DOWNLOAD_NAME facil.zip
  TIMEOUT 360
  # Remove in-source makefile to avoid clashing
  PATCH_COMMAND rm -f ./makefile
  PREFIX ${CMAKE_BINARY_DIR}
  BUILD_IN_SOURCE ON
  GIT_PROGRESS ON
  UPDATE_COMMAND ""
  INSTALL_COMMAND ""
  UPDATE_DISCONNECTED ${_UPDATE_DISCONNECTED}
  LOG_DOWNLOAD OFF
  LOG_UPDATE OFF
  LOG_BUILD OFF
  LOG_CONFIGURE OFF
  LOG_INSTALL OFF
  CMAKE_ARGS ${CMAKE_ARGS}
  BUILD_BYPRODUCTS "${FACIL_LIBRARY_DIR}/libfacil.io.a"
)

add_library(facil_s STATIC IMPORTED GLOBAL)
set_target_properties(
   facil_s
   PROPERTIES
   IMPORTED_LOCATION "${FACIL_LIBRARY_DIR}/libfacil.io.a"
)
add_dependencies(facil_s extern_facil)

if (DO_INSTALL_CORE)
  install(FILES "${FACIL_LIBRARY_DIR}/libfacil.io.a"
          RENAME "libfacilio-1.a"
          DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

list(APPEND PROJECT_LLIBRARIES facil_s)
list(APPEND PROJECT_INCLUDE_DIRS "${FACIL_INCLUDE_DIR}"
                                 "${FACIL_INCLUDE_DIR}/fiobj"
                                 "${FACIL_INCLUDE_DIR}/http"
                                 "${FACIL_INCLUDE_DIR}/cli"
                                 "${FACIL_INCLUDE_DIR}/tls")
