message(STATUS "Resolving GIT Version")
set(GIT_REVISION "")
find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY "${local_dir}"
    OUTPUT_VARIABLE GIT_REVISION
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "GIT hash: ${GIT_REVISION}")
else()
  message(STATUS "GIT not found")
endif()



