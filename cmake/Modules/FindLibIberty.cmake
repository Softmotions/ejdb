# - Find Iberty
# This module finds libiberty.
#
# It sets the following variables:
#  IBERTY_LIBRARIES     - The libiberty library to link against.

# For Debian <= wheezy, use libiberty_pic.a from binutils-dev
# For Debian >= jessie, use libiberty.a from libiberty-dev
# For all RHEL/Fedora, use libiberty.a from binutils-devel
FIND_LIBRARY( IBERTY_LIBRARIES 
	      NAMES iberty_pic iberty 
	      HINTS ${IBERTY_LIBRARIES}
	      PATHS
	      /usr/lib
	      /usr/lib64
	      /usr/local/lib
	      /usr/local/lib64
	      /opt/local/lib
	      /opt/local/lib64
 	      /sw/lib
	      ENV LIBRARY_PATH
	      ENV LD_LIBRARY_PATH
	      )

IF (IBERTY_LIBRARIES)

   # show which libiberty was found only if not quiet
   MESSAGE( STATUS "Found libiberty: ${IBERTY_LIBRARIES}")

   SET(IBERTY_FOUND TRUE)

ELSE (IBERTY_LIBRARIES)

   IF (IBERTY_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find libiberty. Try to install binutil-devel?")
   ELSE()
      MESSAGE(STATUS "Could not find libiberty; downloading binutils and building PIC libiberty.")
   ENDIF (IBERTY_FIND_REQUIRED)

ENDIF (IBERTY_LIBRARIES)
