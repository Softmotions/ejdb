cmake_minimum_required(VERSION 3.4)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
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
set(PROJECT_DESCRIPTION_SUMMARY "Embeddable JSON database engine with network support (EJDB2)")
set(PROJECT_DESCRIPTION "Embeddable JSON database engine with network support (EJDB2)")
set(CHANGELOG_MESSAGE ${CHANGELOG_LAST_MESSAGE})
set(PROJECT_PPA "ppa:adamansky/ejdb2")
set(PROJECT_PPA_USER "adamansky")

set(PROJECT_VERSION_MAJOR ${CHANGELOG_LAST_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${CHANGELOG_LAST_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${CHANGELOG_LAST_VERSION_PATCH})
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(${PROJECT_NAME}_VERSION ${PROJECT_VERSION})
set(${PROJECT_NAME}_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(${PROJECT_NAME}_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(${PROJECT_NAME}_VERSION_PATCH ${PROJECT_VERSION_PATCH})

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_HTTP "Enable HTTP endpoint and standalone server (jbs)" ON)
option(BUILD_TESTS "Build test cases" OFF)
option(ASAN "Turn on address sanitizer" OFF)
option(BUILD_EXAMPLES "Build example projects" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_JNI_BINDING "Build Java native binding" OFF)
option(BUILD_DART_BINDING "Build Dart VM binding" OFF)
option(BUILD_NODEJS_BINDING "Build Node.js binding" OFF)
option(PACKAGE_DEB "Build .deb instalation packages" OFF)
option(PACKAGE_RPM "Build .rpm instalation packages" OFF)
option(PACKAGE_TGZ "Build .tgz package archive" ON)
option(PACKAGE_ZIP "Build .zip package archive" ON)
option(ENABLE_PPA "Enable PPA package build" OFF)
option(UPLOAD_PPA "Upload debian packages to the launchpad ppa repository" OFF)
option(PPA_DEBIAN_VERSION "PPA version suffix for debian packages" "ppa1")
option(PROJECT_PPA_DISTRIB_TARGET "Ubuntu PPA distrubutive names" "bionic")

if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif(POLICY CMP0042)

if (DEFINED CMAKE_TOOLCHAIN_FILE)
  get_filename_component(_CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE} ABSOLUTE)
  set(CMAKE_TOOLCHAIN_FILE, ${_CMAKE_TOOLCHAIN_FILE} FORCE)
endif()

include(GitRevision)
include(GNUInstallDirs)
include(ProjectUtils)

macro_ensure_out_of_source_build(
    "${CMAKE_PROJECT_NAME} requires an out of source build."
)

if(BUILD_TESTS)
    include(CTest)
    find_package(CUnit REQUIRED)
endif(BUILD_TESTS)

if(UPLOAD_PPA)
    set(ENABLE_PPA ON)
endif(UPLOAD_PPA)

add_subdirectory(man)

if(NOT ENABLE_PPA)
    add_subdirectory(src)
else()
    set(PACKAGE_DEB ON)
endif()

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

if(CPACK_GENERATORS)
    set(CPACK_GENERATOR "${CPACK_GENERATORS}")
    set(CPACK_SOURCE_IGNORE_FILES
            "/mxe/"
            "/build/"
            "/\\\\.git/"
            "/\\\\.clang/"
            "/\\\\.vscode/"
            "/\\\\.codelite/"
            "/\\\\.idea/"
            "/cmake-.*/"
            "\\\\.sh$"
            "\\\\.project$"
            "\\\\.workspace$"
            "\\\\.iml$"
            "\\\\.mk$"
            "\\\\.astylerc$"
            "\\\\.editorconfig$"
            "/Makefile$"
    )
    set(PROJECT_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    add_subdirectory(installer)
endif(CPACK_GENERATORS)

message("${PROJECT_NAME} GIT_REVISION: ${GIT_REVISION}")
message("${PROJECT_NAME} CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("${PROJECT_NAME} CPACK_GENERATORS: ${CPACK_GENERATORS}")
if (MXE_HOME)
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
