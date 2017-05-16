cmake_minimum_required(VERSION 2.6)
#cmake_policy(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
set(DEB_CHANGELOG_REQUIRED ON)
include(DebChangelog)

set(PROJECT_NAME "ejdb")
set(PROJECT_VENDOR "Softmotions")
set(PROJECT_WEBSITE "http://ejdb.org")
set(PROJECT_MAINTAINER "Anton Adamansky <adamansky@gmail.com>")
set(PROJECT_DESCRIPTION_SUMMARY "Embedded JSON database library (EJDB)")
set(PROJECT_DESCRIPTION "EJDB aims to be a fast MongoDB-like library which can be embedded into C/C++ applications under terms of LGPL license.")
set(CHANGELOG_MESSAGE ${CHANGELOG_LAST_MESSAGE})
set(PROJECT_PPA "ppa:adamansky/ejdb")
set(PROJECT_PPA_USER "adamansky")
set(PROJECT_PPA_DISTRIB_TARGET vivid trusty precise)

project(${PROJECT_NAME} C)

set(PROJECT_VERSION_MAJOR ${CHANGELOG_LAST_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${CHANGELOG_LAST_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${CHANGELOG_LAST_VERSION_PATCH})
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
set(${PROJECT_NAME}_VERSION ${PROJECT_VERSION})
set(${PROJECT_NAME}_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(${PROJECT_NAME}_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(${PROJECT_NAME}_VERSION_PATCH ${PROJECT_VERSION_PATCH})

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test cases" OFF)
option(BUILD_SAMPLES "Build ejdb sample projects" OFF)
option(PACKAGE_DEB "Build .deb instalation packages" OFF)
option(PACKAGE_TGZ "Build .tgz package archive" ON)
option(PACKAGE_ZIP "Build .zip package archive" ON)
option(ENABLE_PPA "Enable PPA package build" OFF)
option(UPLOAD_PPA "Upload debian packages to the launchpad ppa repository" OFF)
option(PPA_DEBIAN_VERSION "PPA version suffix for debian packages" "ppa1")
option("PROJECT_PPA_DISTRIB_TARGET" "Ubuntu PPA distrubutive names" "vivid;trusty;precise")

if (POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif(POLICY CMP0042)

if (CMAKE_SIZEOF_VOID_P MATCHES 8)
	set(PROJECT_ARCH "x86_64")
	add_definitions("-D_FILE_OFFSET_BITS=64")
else(CMAKE_SIZEOF_VOID_P MATCHES 8)
	set(PROJECT_ARCH "x86")
endif(CMAKE_SIZEOF_VOID_P MATCHES 8)

include(GNUInstallDirs)
include(EjdbUtils)

macro_ensure_out_of_source_build(
    "${CMAKE_PROJECT_NAME} requires an out of source build."
)

if (BUILD_TESTS) 
	include(CTest)
	find_package(CUnit REQUIRED)
endif(BUILD_TESTS)

if (UPLOAD_PPA)
	set(ENABLE_PPA ON)
endif()

if (NOT ENABLE_PPA)
	add_subdirectory(src)
else()
	set(PACKAGE_DEB ON)
endif()

if(NOT CPACK_PACKAGING_INSTALL_PREFIX)
	set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
endif()

set(CPACK_GENERATORS)
if (PACKAGE_TGZ)
	list(APPEND CPACK_GENERATORS "TGZ")
endif()
if (PACKAGE_ZIP)
	list(APPEND CPACK_GENERATORS "ZIP")
endif()
if (PACKAGE_DEB)
	list(APPEND CPACK_GENERATORS "DEB")
endif()

if (CPACK_GENERATORS)
    set(CPACK_GENERATOR "${CPACK_GENERATORS}")
	set(CPACK_SOURCE_IGNORE_FILES 
		"ejdb\\\\.mk$"
		"\\\\.project$"
		"\\\\.workspace$"
		"\\\\.iml$"
		"\\\\.cmake_dirty$"
		"ctest\\\\.cmake"
		"compile_commands\\\\.json$"
		"/Makefile$"
	)
	add_subdirectory(installer)
endif()

message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("CPACK_GENERATORS: ${CPACK_GENERATORS}")
if (MXE_HOME)
	message("MXE_HOME: ${MXE_HOME}")
endif()
if (CMAKE_SYSTEM_NAME)
	message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
endif()
if (PROJECT_ARCH)
	message("PROJECT_ARCH: ${PROJECT_ARCH}")
endif()
message("PROJECT: ${CHANGELOG_LAST_LINE}")
if (CHANGELOG_MESSAGE)
    message("CHANGELOG_MESSAGE:\n  ${CHANGELOG_MESSAGE}")
endif()
