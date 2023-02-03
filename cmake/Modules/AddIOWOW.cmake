if(TARGET IOWOW::static)
  return()
endif()

find_package(IOWOW)

if(TARGET IOWOW::static)
  return()
endif()

set(IOWOW_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/include)

include(ExternalProject)

if(NOT DEFINED IOWOW_URL)
  set(IOWOW_URL
      https://github.com/Softmotions/iowow/archive/refs/heads/master.zip)
endif()

set(BYPRODUCT "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/libiowow-1.a")

set(CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBUILD_SHARED_LIBS=OFF
    -DBUILD_EXAMPLES=OFF)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${CMAKE_INSTALL_PREFIX}
                         ${CMAKE_BINARY_DIR})

list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)

set(SSUB "|")
foreach(
  extra
  ANDROID_ABI
  ANDROID_PLATFORM
  ARCHS
  CMAKE_C_COMPILER
  CMAKE_FIND_ROOT_PATH
  CMAKE_OSX_ARCHITECTURES
  CMAKE_TOOLCHAIN_FILE
  ENABLE_ARC
  ENABLE_BITCODE
  ENABLE_STRICT_TRY_COMPILE
  ENABLE_VISIBILITY
  PLATFORM
  TEST_TOOL_CMD)
  if(DEFINED ${extra})
    string(REPLACE ";" "${SSUB}" extra_sub "${${extra}}")
    list(APPEND CMAKE_ARGS "-D${extra}=${extra_sub}")
  endif()
endforeach()

message("IOWOW CMAKE_ARGS: ${CMAKE_ARGS}")

ExternalProject_Add(
  extern_iowow
  URL ${IOWOW_URL}
  DOWNLOAD_NAME iowow.zip
  TIMEOUT 360
  PREFIX ${CMAKE_BINARY_DIR}
  BUILD_IN_SOURCE OFF
  # DOWNLOAD_EXTRACT_TIMESTAMP ON
  UPDATE_COMMAND ""
  CMAKE_ARGS ${CMAKE_ARGS}
  LIST_SEPARATOR "${SSUB}"
  BUILD_BYPRODUCTS ${BYPRODUCT})

add_library(IOWOW::static STATIC IMPORTED GLOBAL)
set_target_properties(
  IOWOW::static
  PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
             IMPORTED_LOCATION ${BYPRODUCT}
             IMPORTED_LINK_INTERFACE_LIBRARIES "Threads::Threads;m")

add_dependencies(IOWOW::static extern_iowow)
