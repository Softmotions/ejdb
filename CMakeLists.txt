cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
include(CMakeToolsHelpers OPTIONAL)
set(DEB_CHANGELOG_REQUIRED ON)
set(DEB_CHANGELOG "${CMAKE_CURRENT_SOURCE_DIR}/Changelog")
unset(CHANGELOG_LAST_VERSION)
unset(CHANGELOG_LAST_MESSAGE)
include(DebChangelog)

set(PROJECT_NAME "ejdb2")
project(${PROJECT_NAME} C)

set(PROJECT_VENDOR "Softmotions (https://softmotions.com)")
set(PROJECT_WEBSITE "http://ejdb.org")
set(PROJECT_MAINTAINER "Anton Adamansky <adamansky@gmail.com>")
set(PROJECT_DESCRIPTION_SUMMARY "Embeddable JSON database engine (EJDB2).")
set(PROJECT_DESCRIPTION "Embeddable JSON database engine (EJDB2).")
set(CHANGELOG_MESSAGE ${CHANGELOG_LAST_MESSAGE})
set(PROJECT_PPA "ppa:adamansky/ejdb2")
set(PROJECT_PPA_USER "adamansky")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")

set(PROJECT_VERSION_MAJOR ${CHANGELOG_LAST_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${CHANGELOG_LAST_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${CHANGELOG_LAST_VERSION_PATCH})
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set(${PROJECT_NAME}_VERSION ${PROJECT_VERSION})
set(${PROJECT_NAME}_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(${PROJECT_NAME}_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(${PROJECT_NAME}_VERSION_PATCH 0)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_HTTP "Enable HTTP endpoint and standalone server (jbs)" ON)
option(INSTALL_FLAT_SUBPROJECT_INCLUDES
       "Install iowow/iwnet headers into CMAKE_INSTALL_PREFIX/{iowow,iwnet} dir"
       OFF)
option(BUILD_TESTS "Build test cases" OFF)
option(ASAN "Turn on address sanitizer" OFF)
option(BUILD_EXAMPLES "Build example projects" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_JNI_BINDING "Build Java native binding" OFF)
option(BUILD_ANDROID_LIBS "Build Android libs" OFF)
option(BUILD_DART_BINDING "Build Dart VM binding" OFF)
option(BUILD_NODEJS_BINDING "Build Node.js binding" OFF)
option(BUILD_REACT_NATIVE_BINDING "Build React Native binding" OFF)
option(BUILD_FLUTTER_BINDING "Build Flutter binding" OFF)
option(BUILD_SWIFT_BINDING "Build Swift binding" OFF)
option(PACKAGE_DEB "Build .deb instalation packages" OFF)
option(PACKAGE_RPM "Build .rpm instalation packages" OFF)
option(PACKAGE_TGZ "Build .tgz package archive" ON)
option(PACKAGE_ZIP "Build .zip package archive" ON)
option(ENABLE_PPA "Enable PPA package build" OFF)
option(UPLOAD_PPA "Upload debian packages to the launchpad ppa repository" OFF)

set(PPA_DEBIAN_VERSION
    "ppa1"
    CACHE STRING "PPA version suffix for debian packages")

set(PROJECT_PPA_DISTRIB_TARGET
    "focal"
    CACHE STRING "Ubuntu PPA distribution names")

set(ANDROID_ABIS
    "x86;x86_64;arm64-v8a;armeabi-v7a"
    CACHE STRING "Android ABI list")

set(ANDROID_AVD
    "TestingAVD"
    CACHE STRING "Android virtual device name for tests")

set(CPACK_SET_DESTDIR ON)

if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif(POLICY CMP0042)

if(POLICY CMP0087)
  cmake_policy(SET CMP0087 NEW)
endif(POLICY CMP0087)

if(BUILD_REACT_NATIVE_BINDING
   OR BUILD_FLUTTER_BINDING
   OR BUILD_ANDROID_LIBS)
  set(ENABLE_HTTP OFF)
  set(BUILD_ANDROID_LIBS ON)
endif()

if(ANDROID_ABI OR IOS)
  set(ENABLE_HTTP OFF)
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE)
  get_filename_component(_CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE} ABSOLUTE)
  set(CMAKE_TOOLCHAIN_FILE ${_CMAKE_TOOLCHAIN_FILE})
endif()

include(GitRevision)
include(GNUInstallDirs)
include(ProjectUtils)

macro_ensure_out_of_source_build(
  "${CMAKE_PROJECT_NAME} requires an out of source build.")

if(BUILD_TESTS)
  include(CTest)
  find_package(CUnit REQUIRED)
endif(BUILD_TESTS)

if(UPLOAD_PPA)
  set(ENABLE_PPA ON)
  set(PACKAGE_DEB ON)
endif(UPLOAD_PPA)

set(CPACK_GENERATORS)
if(PACKAGE_TGZ)
  list(APPEND CPACK_GENERATORS "TGZ")
endif()
if(PACKAGE_ZIP)
  list(APPEND CPACK_GENERATORS "ZIP")
endif()
if(PACKAGE_DEB)
  list(APPEND CPACK_GENERATORS "DEB")
endif()
if(PACKAGE_RPM)
  list(APPEND CPACK_GENERATORS "RPM")
endif()

set(DO_INSTALL_CORE ON)

add_subdirectory(man)

if(NOT ENABLE_PPA)
  add_subdirectory(src)
endif()

if(CPACK_GENERATORS)
  set(CPACK_GENERATOR "${CPACK_GENERATORS}")
  set(CPACK_SOURCE_IGNORE_FILES
      "/src/bindings/ejdb2_android"
      "/src/bindings/ejdb2_dart"
      "/src/bindings/ejdb2_flutter"
      "/src/bindings/ejdb2_node"
      "/src/bindings/ejdb2_react_native"
      "/src/bindings/ejdb2_swift"
      "/mxe/"
      "/build/"
      "/target/"
      "/docker/"
      "/benchmark/"
      "/node_modules/"
      "/cmake-.*/"
      "/Makefile$"
      "hints\\\\.txt$"
      "/\\\\.clang/"
      "/\\\\.cache/"
      "/\\\\.git/"
      "/\\\\.settings/"
      "\\\\.classpath$"
      "\\\\.editorconfig$"
      "\\\\.log$"
      "\\\\.mk$"
      "uncrustify\\\\.cfg")
  set(PROJECT_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  add_subdirectory(installer)
endif(CPACK_GENERATORS)

message("${PROJECT_NAME} GIT_REVISION: ${GIT_REVISION}")
message("${PROJECT_NAME} CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("${PROJECT_NAME} CPACK_GENERATORS: ${CPACK_GENERATORS}")
message("${PROJECT_NAME} CMAKE_GENERATOR: ${CMAKE_GENERATOR}")

if(MXE_HOME)
  message("${PROJECT_NAME} MXE_HOME: ${MXE_HOME}")
endif()

if(CMAKE_SYSTEM_NAME)
  message("${PROJECT_NAME} CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
endif()
message("${PROJECT_NAME} CPU: ${CMAKE_SYSTEM_PROCESSOR}")

if(CMAKE_SIZEOF_VOID_P)
  message("${PROJECT_NAME} SIZEOF *VOID: ${CMAKE_SIZEOF_VOID_P}")
endif()
message("${PROJECT_NAME} PROJECT: ${CHANGELOG_LAST_LINE}")

if(CHANGELOG_MESSAGE)
  message("${PROJECT_NAME} CHANGELOG_MESSAGE:\n  ${CHANGELOG_MESSAGE}")
endif()
