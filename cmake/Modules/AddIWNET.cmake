include(ExternalProject)

if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
  set(_UPDATE_DISCONNECTED 0)
else()
  set(_UPDATE_DISCONNECTED 1)
endif()

set(IWNET_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}")
file(MAKE_DIRECTORY ${IWNET_INCLUDE_DIR})

if("${IWNET_URL}" STREQUAL "")
  if(EXISTS ${CMAKE_SOURCE_DIR}/iwnet.zip)
    set(IWNET_URL ${CMAKE_SOURCE_DIR}/iwnet.zip)
  else()
    set(IWNET_URL ${CMAKE_SOURCE_DIR}/extra/iwnet)
  endif()
endif()

message("IWNET_URL: ${IWNET_URL}")

if(APPLE)
  set(BYPRODUCT "${CMAKE_BINARY_DIR}/lib/libiwnet-1.a")
else()
  set(BYPRODUCT "${CMAKE_BINARY_DIR}/src/extern_iwnet-build/src/libiwnet-1.a")
endif()

# In order to properly pass owner project CMAKE variables than contains
# semicolons, we used a specific separator for 'ExternalProject_Add', using the
# LIST_SEPARATOR parameter. This allows building fat binaries on macOS, etc.
# (ie: -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64") Otherwise, semicolons get
# replaced with spaces and CMake isn't aware of a multi-arch setup.
set(SSUB "^^")

set(CMAKE_ARGS
    -DOWNER_PROJECT_NAME=${PROJECT_NAME} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DASAN=${ASAN}
    -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${CMAKE_INSTALL_PREFIX})

foreach(
  extra
  CMAKE_FIND_ROOT_PATH
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

message("IWNET CMAKE_ARGS: ${CMAKE_ARGS}")

ExternalProject_Add(
  extern_iwnet
  URL ${IWNET_URL}
  DOWNLOAD_NAME iwnet.zip
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

add_library(IWNET::static STATIC IMPORTED GLOBAL)
set_target_properties(
  IWNET::static PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                           IMPORTED_LOCATION ${BYPRODUCT})

add_dependencies(IWNET::static IOWOW:static extern_iwnet)

list(PREPEND PROJECT_LLIBRARIES IWNET::static)
list(APPEND PROJECT_INCLUDE_DIRS ${IWNET_INCLUDE_DIR})
