# Find the IOWOW headers and libraries, attemting to import IOWOW exported targets,
#
#  IOWOW_INCLUDE_DIRS - The IOWOW include directory
#  IOWOW_LIBRARIES    - The libraries needed to use IOWOW
#  IOWOW_FOUND        - True if IOWOW found in system

find_path(IOWOW_INCLUDE_DIR NAMES iowow/iowow.h)
mark_as_advanced(IOWOW_INCLUDE_DIR)
find_library(IOWOW_LIBRARY NAMES
    iowow
    libiowow
    iowowlib
)
mark_as_advanced(IOWOW_LIBRARY)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IOWOW DEFAULT_MSG IOWOW_LIBRARY IOWOW_INCLUDE_DIR)

if(IOWOW_FOUND)
  set(IOWOW_LIBRARIES ${IOWOW_LIBRARY})
  set(IOWOW_INCLUDE_DIRS ${IOWOW_INCLUDE_DIR})
  find_path(IOWOW_EXPORTS_DIR
            NAMES "iowow/iowow-exports.cmake"
            PATHS /usr/share /usr/local/share)
  mark_as_advanced(IOWOW_EXPORTS_DIR)
  if(IOWOW_EXPORTS_DIR)
    if(EXISTS ${IOWOW_EXPORTS_DIR}/iowow/iowow-exports.cmake)
      include(${IOWOW_EXPORTS_DIR}/iowow/iowow-exports.cmake)
      message("-- Imported ${IOWOW_EXPORTS_DIR}/iowow/iowow-exports.cmake")
    endif()
    if(EXISTS ${IOWOW_EXPORTS_DIR}/iowow/iowow-static-exports.cmake)
      include(${IOWOW_EXPORTS_DIR}/iowow/iowow-static-exports.cmake)
      message("-- Imported ${IOWOW_EXPORTS_DIR}/iowow/iowow-static-exports.cmake")
    endif()
  endif(IOWOW_EXPORTS_DIR)
elseif(IOWOW_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find libiowow.")
endif(IOWOW_FOUND)


