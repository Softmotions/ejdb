include(ExternalProject)

set(FACIL_SOURCE_DIR "${CMAKE_BINARY_DIR}/src/extern_facil")
set(FACIL_BINARY_DIR "${CMAKE_BINARY_DIR}/src/extern_facil")
set(FACIL_INCLUDE_DIR "${FACIL_BINARY_DIR}/lib/facil")
set(FACIL_LIBRARY_DIR "${FACIL_BINARY_DIR}")

ExternalProject_Add(
  extern_facil
  GIT_REPOSITORY https://github.com/Softmotions/facil.io.git
  GIT_TAG 0.7.0.beta8
  # Remove in-source makefile to avoid clashing
  PATCH_COMMAND rm -f ./makefile
  PREFIX ${CMAKE_BINARY_DIR}
  BUILD_IN_SOURCE ON
  GIT_PROGRESS ON
  UPDATE_COMMAND ""
  INSTALL_COMMAND ""
  UPDATE_DISCONNECTED ON
  LOG_DOWNLOAD OFF
  LOG_UPDATE OFF
  LOG_BUILD OFF
  LOG_CONFIGURE OFF
  LOG_INSTALL OFF
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-fPIC
  BUILD_BYPRODUCTS "${FACIL_LIBRARY_DIR}/libfacil.io.a"
)

add_library(facil_s STATIC IMPORTED GLOBAL)
set_target_properties(
   facil_s
   PROPERTIES
   IMPORTED_LOCATION "${FACIL_LIBRARY_DIR}/libfacil.io.a"
)
add_dependencies(facil_s extern_facil)

list(APPEND PROJECT_LLIBRARIES facil_s)
list(APPEND PROJECT_INCLUDE_DIRS "${FACIL_INCLUDE_DIR}"
                                 "${FACIL_INCLUDE_DIR}/fiobj"
                                 "${FACIL_INCLUDE_DIR}/http"
                                 "${FACIL_INCLUDE_DIR}/cli"
                                 "${FACIL_INCLUDE_DIR}/tls")
