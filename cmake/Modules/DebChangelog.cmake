## -*- mode:cmake; coding:utf-8; -*-
# Set output:
# CHANGELOG_LAST_LINE 
# CHANGELOG_LAST_VERSION - Last changelog version
# CHANGELOG_LAST_VERSION_MAJOR - Last changelog major version
# CHANGELOG_LAST_VERSION_MINOR - Last changelog minor version
# CHANGELOG_LAST_VERSION_PATCH - Last changelog patch version
# CHANGELOG_LAST_MESSAGE - Last changelog description

if (NOT DEB_CHANGELOG)
   set(DEB_CHANGELOG "${CMAKE_SOURCE_DIR}/Changelog")
endif()

if (NOT EXISTS ${DEB_CHANGELOG}) 
    if (DEB_CHANGELOG_REQUIRED)
        message(FATAL_ERROR "Missing required project changelog file: ${DEB_CHANGELOG}")
    else()
        message("Missing project changelog file: ${DEB_CHANGELOG}")        
        return()
    endif()
endif()
message(STATUS "Using project changelog file: ${DEB_CHANGELOG}")

file(STRINGS "${DEB_CHANGELOG}" _DEBCHANGELOGN)

foreach(CLINE IN LISTS _DEBCHANGELOGN)
    if (NOT CHANGELOG_LAST_VERSION)
        #ejdb (1.2.6) testing; urgency=low
        string(REGEX MATCH "^[A-Za-z0-9_].*[ \t]+\\((([0-9]+)\\.([0-9]+)\\.([0-9]+))\\)[ \t]+[A-Za-z]+;[ \t].*" _MATCHED "${CLINE}")
        if (_MATCHED)
            set(CHANGELOG_LAST_LINE "${_MATCHED}")
            set(CHANGELOG_LAST_VERSION "${CMAKE_MATCH_1}")
            set(CHANGELOG_LAST_VERSION_MAJOR "${CMAKE_MATCH_2}")
            set(CHANGELOG_LAST_VERSION_MINOR "${CMAKE_MATCH_3}")
            set(CHANGELOG_LAST_VERSION_PATCH "${CMAKE_MATCH_4}")
        endif()
    elseif(NOT CHANGELOG_LAST_MESSAGE)    
        string(REGEX MATCH "^[A-Za-z0-9_].*[ \t]+\\((([0-9]+)\\.([0-9]+)\\.([0-9]+))\\)[ \t]+[A-Za-z]+;[ \t].*" _MATCHED "${CLINE}")
        if (_MATCHED)
            string(STRIP "${_CDESC}" CHANGELOG_LAST_MESSAGE)
            return()
        endif()
        if (CLINE)
            string(REGEX MATCH "^[ \t]*\\-\\-[ \t]+" _MATCHED "${CLINE}")
            if (_MATCHED)
                string(STRIP "${_CDESC}" CHANGELOG_LAST_MESSAGE)
                return()
            endif()
            set(_CDESC "${_CDESC}\n${CLINE}")
        endif()
    endif()
endforeach(CLINE)

message(FATAL_ERROR "Invalid changelog file: ${DEB_CHANGELOG}")
