

macro(MACRO_ENSURE_OUT_OF_SOURCE_BUILD MSG)
	string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" insource)
	get_filename_component(PARENTDIR ${CMAKE_SOURCE_DIR} PATH)
	string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${PARENTDIR}" insourcesubdir)
    if(insource OR insourcesubdir)
        message(FATAL_ERROR "${MSG}")
    endif(insource OR insourcesubdir)
endmacro(MACRO_ENSURE_OUT_OF_SOURCE_BUILD)
