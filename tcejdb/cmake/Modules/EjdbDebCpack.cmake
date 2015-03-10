
configure_file("${PROJECT_SOURCE_DIR}/EjdbDebCpackOptions.cmake.in",
			   "${PROJECT_BINARY_DIR}/EjdbDebCpackOptions.cmake"
				@ONLY)
				
set(CPACK_PROJECT_CONFIG_FILE 
	"${PROJECT_BINARY_DIR}/EjdbDebCpackOptions.cmake")
				
