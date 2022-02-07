include(ExternalProject)

if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
  set(_UPDATE_DISCONNECTED 0)
else()
  set(_UPDATE_DISCONNECTED 1)
endif()

set(WSLAY_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include")
file(MAKE_DIRECTORY ${WSLAY_INCLUDE_DIR})

if("${WSLAY_URL}" STREQUAL "")
  if(EXISTS ${CMAKE_SOURCE_DIR}/wslay.zip)
    set(WSLAY_URL ${CMAKE_SOURCE_DIR}/wslay.zip)
  else()
    set(WSLAY_URL ${CMAKE_SOURCE_DIR}/extra/wslay)
  endif()
endif()

message("WSLAY_URL: ${WSLAY_URL}")

if(APPLE)
  set(BYPRODUCT "${CMAKE_BINARY_DIR}/lib/libwslay.a")
else()
  set(BYPRODUCT "${CMAKE_BINARY_DIR}/src/extern_wslay-build/lib/libwslay.a")
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

list(APPEND CMAKE_ARGS "-DCMAKE_C_FLAGS=-fPIC")
message("WSLAY CMAKE_ARGS: ${CMAKE_ARGS}")

ExternalProject_Add(
  extern_wslay
  URL ${WSLAY_URL}
  DOWNLOAD_NAME wslay.zip
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
endif()

add_library(WSLAY::static STATIC IMPORTED GLOBAL)
set_target_properties(
  WSLAY::static PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                           IMPORTED_LOCATION ${BYPRODUCT})

add_dependencies(WSLAY::static extern_wslay)

list(PREPEND PROJECT_LLIBRARIES WSLAY::static)
list(APPEND PROJECT_INCLUDE_DIRS ${WSLAY_INCLUDE_DIR})
