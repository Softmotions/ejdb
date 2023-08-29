# See https://github.com/richfelker/musl-cross-make

if(NOT MUSL_HOME)
  set(MUSL_HOME $ENV{MUSL_HOME})
endif()
if(NOT MUSL_HOME)
  message(FATAL_ERROR "Please setup MUSL_HOME environment variable")
endif()

set(MUSL_CFG x86_64-linux-musl)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CROSS_HOST x86_64-unknown-linux)

set(CMAKE_FIND_ROOT_PATH "${MUSL_HOME}/${MUSL_CFG};${FIND_ROOT};${CMAKE_BINARY_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

set(CMAKE_C_COMPILER ${MUSL_HOME}/bin/${MUSL_CFG}-gcc)
set(CMAKE_CXX_COMPILER ${MUSL_HOME}/bin/${MUSL_CFG}-g++)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
