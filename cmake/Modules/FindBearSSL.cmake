mark_as_advanced(BEARSSL_INCLUDE_DIRS BEARSSL_STATIC_LIB)

find_path(
  BEARSSL_INCLUDE_DIRS
  NAMES bearssl.h
  PATH_SUFFIXES bearssl ejdb2/bearssl)

find_library(BEARSSL_STATIC_LIB NAMES libbearssl.a)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BearSSL DEFAULT_MSG BEARSSL_INCLUDE_DIRS
                                  BEARSSL_STATIC_LIB)
