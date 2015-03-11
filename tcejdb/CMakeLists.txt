cmake_minimum_required(VERSION 3.0)
set(PROJECT_NAME "ejdb")
set(PROJECT_VENDOR "Softmotions")
set(PROJECT_WEBSITE "http://ejdb.org")
set(PROJECT_MAINTAINER "Anton Adamansky <adamansky@gmail.com>")
set(PROJECT_DESCRIPTION_SUMMARY "Embedded JSON database library (EJDB)")
set(PROJECT_DESCRIPTION ${PROJECT_DESCRIPTION_SUMMARY})
set(PROJECT_PPA "ppa:adamansky/ejdb")

project(${PROJECT_NAME} VERSION 1.2.0 LANGUAGES C)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test cases" OFF)
option(PACKAGE_DEB "Build .deb instalation packages" OFF)
option(PACKAGE_TGZ "Build .tgz package archive" ON)
option(UPLOAD_PPA "Upload debian packages to the launchpad ppa repository" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

include(GNUInstallDirs)
include(EjdbUtils)

macro_ensure_out_of_source_build(
    "${CMAKE_PROJECT_NAME} requires an out of source build."
)

if (NOT PACKAGE_DEB)
	add_subdirectory(src)
endif()

#######################################################

set(CPACK_GENERATORS)
if (PACKAGE_TGZ)
	list(APPEND CPACK_GENERATORS "TGZ")
endif()
if (PACKAGE_DEB)
	list(APPEND CPACK_GENERATORS "DEB")
endif()

if (CPACK_GENERATORS)
    set(CPACK_GENERATOR "${CPACK_GENERATORS}")
	add_subdirectory(installer)
endif()

#########################################################

message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("CPACK_GENERATORS: ${CPACK_GENERATORS}")


