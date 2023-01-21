mark_as_advanced(IWNET_INCLUDE_DIRS IWNET_STATIC_LIB)

find_path(
  IWNET_INCLUDE_DIRS
  NAMES iwnet/iwnet.h
  PATH_SUFFIXES ejdb2)

find_library(IWNET_STATIC_LIB NAMES libiwnet-1.a)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IWNET DEFAULT_MSG IWNET_INCLUDE_DIRS
                                  IWNET_STATIC_LIB)
if(IWNET_FOUND)
  add_library(IWNET::static STATIC IMPORTED GLOBAL)
  set_target_properties(
    IWNET::static
    PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
               IMPORTED_LOCATION
               "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/libiwnet-1.a"
               IMPORTED_LINK_INTERFACE_LIBRARIES "IOWOW::static")
endif()
