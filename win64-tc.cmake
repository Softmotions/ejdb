if (NOT MXE_HOME)
	set(MXE_HOME $ENV{MXE_HOME})
endif()
if (NOT MXE_HOME)
	message(FATAL_ERROR "Please setup MXE_HOME environment variable")
endif()

if (NOT MXE_CFG)
    set(MXE_CFG $ENV{MXE_CFG})
    if (NOT MXE_CFG)
		set(MXE_CFG "x86_64-w64-mingw32.static")
	endif()
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(MSYS 1)
set(CMAKE_FIND_ROOT_PATH ${MXE_HOME}/usr/${MXE_CFG})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_C_COMPILER ${MXE_HOME}/usr/bin/${MXE_CFG}-gcc)
set(CMAKE_CXX_COMPILER ${MXE_HOME}/usr/bin/${MXE_CFG}-g++)
set(CMAKE_Fortran_COMPILER ${MXE_HOME}/usr/bin/${MXE_CFG}-gfortran)
set(CMAKE_RC_COMPILER ${MXE_HOME}/usr/bin/${MXE_CFG}-windres)
set(CMAKE_MODULE_PATH "${MXE_HOME}/src/cmake" ${CMAKE_MODULE_PATH}) # For mxe FindPackage scripts
#set(CMAKE_INSTALL_PREFIX ${MXE_HOME}/usr/x86_64-w64-mingw32.static CACHE PATH "Installation Prefix")
set(CMAKE_CROSS_COMPILING ON) # Workaround for http://www.cmake.org/Bug/view.php?id=14075
set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff <FLAGS> <DEFINES> -o <OBJECT> <SOURCE>") # Workaround for buggy windres rules
set(PKG_CONFIG_EXECUTABLE ${MXE_HOME}/usr/bin/${MXE_CFG}-pkg-config)
