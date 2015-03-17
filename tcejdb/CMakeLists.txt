cmake_minimum_required(VERSION 2.8.12)
set(PROJECT_NAME "ejdb")
set(PROJECT_VENDOR "Softmotions")
set(PROJECT_WEBSITE "http://ejdb.org")
set(PROJECT_MAINTAINER "Anton Adamansky <adamansky@gmail.com>")
set(PROJECT_DESCRIPTION_SUMMARY "Embedded JSON database library (EJDB)")
set(PROJECT_DESCRIPTION 
"EJDB aims to be a fast MongoDB-like library which can be embedded into C/C++ applications under terms of LGPL license.")
set(PROJECT_PPA "ppa:adamansky/ejdb")

project(${PROJECT_NAME} C)

set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 2)
set(PROJECT_VERSION_PATCH 2)
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
option(ENABLE_PPA "Enable PPA package build" OFF)
option(UPLOAD_PPA "Upload debian packages to the launchpad ppa repository" OFF)
option(MXE_HOME "Home of MXE (mxe.cc) windows cross build environment")

if (CMAKE_SIZEOF_VOID_P MATCHES 8)
	set(PROJECT_ARCH "x86_64")
else(CMAKE_SIZEOF_VOID_P MATCHES 8)
	set(PROJECT_ARCH "x86")
endif(CMAKE_SIZEOF_VOID_P MATCHES 8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

include(GNUInstallDirs)
include(EjdbUtils)

macro_ensure_out_of_source_build(
    "${CMAKE_PROJECT_NAME} requires an out of source build."
)

if (UPLOAD_PPA)
	set(ENABLE_PPA ON)
endif()

if (NOT ENABLE_PPA)
	add_subdirectory(src)
else()
	set(PACKAGE_DEB ON)
endif()

set(CPACK_GENERATORS)
if (PACKAGE_TGZ)
	list(APPEND CPACK_GENERATORS "TGZ")
endif()
if (PACKAGE_DEB)
	list(APPEND CPACK_GENERATORS "DEB")
endif()

if (CPACK_GENERATORS)
    set(CPACK_GENERATOR "${CPACK_GENERATORS}")
	set(CPACK_SOURCE_IGNORE_FILES 
		"tcejdb.mk"
		"tcejdb.project"
		".cmake_dirty"
		"ctest.cmake"
	)
	add_subdirectory(installer)
endif()

message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("CPACK_GENERATORS: ${CPACK_GENERATORS}")
if (MXE_HOME)
	message("MXE_HOME: ${MXE_HOME}")
endif()
