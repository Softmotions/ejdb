if (BUILD_SHARED_LIBS)
	link_libraries(ejdb)  #shared lib linking
else()
	link_libraries(ejdb_p) #static linking
endif()

add_executable(sample1 sample1.c)
