include(ExternalProject)

if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
  set(_UPDATE_DISCONNECTED 0)
else()
  set(_UPDATE_DISCONNECTED 1)
endif()

set(BEARSSL_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")
file(MAKE_DIRECTORY ${BEARSSL_INCLUDE_DIR})

if("${BEARSSL_URL}" STREQUAL "")
  if(EXISTS ${CMAKE_SOURCE_DIR}/bearssl.zip)
    set(BEARSSL_URL ${CMAKE_SOURCE_DIR}/bearssl.zip)
  else()
    set(BEARSSL_URL ${CMAKE_SOURCE_DIR}/extra/BearSSL)
  endif()
endif()

message("BEARSSL_URL: ${BEARSSL_URL}")

if(APPLE)
  set(BYPRODUCT "${CMAKE_BINARY_DIR}/lib/libbearssl.a")
else()
  set(BYPRODUCT "${CMAKE_BINARY_DIR}/src/extern_bearssl-build/src/libbearssl.a")
endif()

# In order to properly pass owner project CMAKE variables than contains
# semicolons, we used a specific separator for 'ExternalProject_Add', using the
# LIST_SEPARATOR parameter. This allows building fat binaries on macOS, etc.
# (ie: -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64") Otherwise, semicolons get
# replaced with spaces and CMake isn't aware of a multi-arch setup.
set(SSUB "^^")

foreach(
  extra
  CMAKE_OSX_ARCHITECTURES
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
    # Replace occurences of ";" with our custom separator and append to
    # CMAKE_ARGS
    string(REPLACE ";" "${SSUB}" extra_sub "${${extra}}")
    list(APPEND CMAKE_ARGS "-D${extra}=${extra_sub}")
  endif()
endforeach()

message("BEARSSL CMAKE_ARGS: ${CMAKE_ARGS}")

ExternalProject_Add(
  extern_bearssl
  URL ${BEARSSL_URL}
  DOWNLOAD_NAME bearassl.zip
  TIMEOUT 360
  PREFIX ${CMAKE_BINARY_DIR}
  BUILD_IN_SOURCE OFF
  UPDATE_COMMAND ""
  UPDATE_DISCONNECTED ${_UPDATE_DISCONNECTED}
  LIST_SEPARATOR "${SSUB}"
  CMAKE_ARGS ${CMAKE_ARGS}
  BUILD_BYPRODUCTS ${BYPRODUCT})

if(DO_INSTALL_CORE)
  install(FILES "${BYPRODUCT}" DESTINATION ${CMAKE_INSTALL_LIBDIR})

  add_library(BEARSSL::static STATIC IMPORTED GLOBAL)
  set_target_properties(
    BEARSSL::static
    PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
               IMPORTED_LOCATION ${BYPRODUCT}
               INTERFACE_INCLUDE_DIRECTORIES "${BEARSSL_INCLUDE_DIR}"
               INTERFACE_LINK_LIBRARIES "BEARSSL::static")

  add_dependencies(BEARSSL::static extern_bearssl)
endif()
