if (NOT DEFINED HAVE_QSORT_R)
  try_compile(HAVE_QSORT_R
            "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/"
            "${CMAKE_CURRENT_LIST_DIR}/TestQSortR.c")
endif()


