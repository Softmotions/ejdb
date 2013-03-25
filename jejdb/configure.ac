# Source of configuration for EJDB Java Binding


#================================================================
# Generic Settings
#================================================================

test -n "$CFLAGS" && MYCFLAGS="$CFLAGS $MYCFLAGS"
test -n "$CPPFLAGS" && MYCPPFLAGS="$CPPFLAGS $MYCPPFLAGS"
test -n "$LDFLAGS" && MYLDFLAGS="$LDFLAGS $MYLDFLAGS"

AC_INIT(jejdb, 1.0.1)
AC_PREFIX_DEFAULT(/usr)

# Package information
MYLIBVER=9
MYLIBREV=11
MYFORMATVER="1.0"

# Targets
MYLIBRARYFILES=""

# Building flags
MYCFLAGS="$MYCFLAGS -std=c99 -Wall -fPIC -fsigned-char -O2"
MYCPPFLAGS="-I. -I. -I../tcejdb -I$JDK_HOME/include -I$JDK_HOME/include/linux -I\$(INCLUDEDIR) -I$HOME/include -I/usr/local/include"
MYCPPFLAGS="$MYCPPFLAGS -D_UNICODE -DNDEBUG -D_GNU_SOURCE=1 -D_REENTRANT -D__EXTENSIONS__"
MYLDFLAGS="-L. -L\$(LIBDIR) -L$HOME/lib -L/usr/local/lib"
MYCMDLDFLAGS=""
MYRUNPATH="\$(LIBDIR)"
MYLDLIBPATHENV="LD_LIBRARY_PATH"
MYPOSTCMD="true"


# Messages
printf '#================================================================\n'
printf '# Configuring EJDB Java Binding version %s.\n' "$PACKAGE_VERSION"
printf '#================================================================\n'


#================================================================
# Checking Commands and Libraries
#================================================================

# C compiler
AC_PROG_CC


# Checks for libraries.
AC_CHECK_LIB([c], main)
AC_CHECK_LIB([m], main)
AC_CHECK_LIB([pthread], main)
AC_CHECK_LIB([rt], main)
AC_CHECK_LIB([tcejdb], main)
AC_CHECK_LIB([z], main)


#================================================================
# Options
#================================================================


# Internal variables
enables=""

# Disable shared object
AC_ARG_ENABLE(shared,
  AC_HELP_STRING([--disable-shared], [avoid to build shared libraries]))
if test "$enable_shared" = "no"
then
  enables="$enables (no-shared)"
fi

# Shared libraries
if test "$enable_shared" != "no"
then
  MYLIBRARYFILES="$MYLIBRARYFILES libjejdb.so.$MYLIBVER.$MYLIBREV.0"
  MYLIBRARYFILES="$MYLIBRARYFILES libjejdb.so.$MYLIBVER"
  MYLIBRARYFILES="$MYLIBRARYFILES libjejdb.so"
fi

#================================================================
# Generic Settings
#================================================================

# Export variables
AC_SUBST(MYLIBVER)
AC_SUBST(MYLIBREV)
AC_SUBST(MYFORMATVER)

AC_SUBST(MYLIBRARYFILES)

AC_SUBST(MYCFLAGS)
AC_SUBST(MYCPPFLAGS)
AC_SUBST(MYLDFLAGS)
AC_SUBST(MYCMDLDFLAGS)
AC_SUBST(MYRUNPATH)
AC_SUBST(MYLDLIBPATHENV)
AC_SUBST(MYPOSTCMD)
AC_SUBST(PACKAGE_NAME)
AC_SUBST(PACKAGE_VERSION)

# Targets
AC_OUTPUT(Makefile)

# Messages
printf '#================================================================\n'
printf '# Ready to make.\n'
printf '#================================================================\n'



# END OF FILE
