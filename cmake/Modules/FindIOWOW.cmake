mark_as_advanced(IOWOW_INCLUDE_DIRS IOWOW_STATIC_LIB)

find_path(
  IOWOW_INCLUDE_DIRS
  NAMES iowow/iowow.h
  PATH_SUFFIXES ejdb2)

find_library(IOWOW_STATIC_LIB NAMES libiowow-1.a)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IOWOW DEFAULT_MSG IOWOW_INCLUDE_DIRS
                                  IOWOW_STATIC_LIB)
if(IOWOW_FOUND)
  add_library(IOWOW::static STATIC IMPORTED GLOBAL)
  set_target_properties(
    IOWOW::static
    PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
               IMPORTED_LOCATION
               "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/libiowow-1.a"
               IMPORTED_LINK_INTERFACE_LIBRARIES "Threads::Threads;m")
endif()
