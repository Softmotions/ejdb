# Source of configuration for Tokyo Cabinet



#================================================================
# Generic Settings
#================================================================

test -n "$CFLAGS" && MYCFLAGS="$CFLAGS $MYCFLAGS"
test -n "$CPPFLAGS" && MYCPPFLAGS="$CPPFLAGS $MYCPPFLAGS"
test -n "$LDFLAGS" && MYLDFLAGS="$LDFLAGS $MYLDFLAGS"

# Package name
AC_INIT(tcejdb, 1.0.65)

# Package information
MYLIBVER=9
MYLIBREV=11
MYFORMATVER="1.0"

# Targets
MYHEADERFILES="tcutil.h tchdb.h tcbdb.h tcfdb.h tctdb.h tcadb.h ejdb.h ejdb_private.h bson.h myconf.h"
MYLIBRARYFILES="libtcejdb.a"
MYLIBOBJFILES="tcutil.o tchdb.o tcbdb.o tcfdb.o tctdb.o tcadb.o myconf.o md5.o ejdb.o \
bson.o numbers.o encoding.o utf8proc.o ejdbutl.o"
MYCOMMANDFILES="tcutest tcumttest tcucodec tchtest tchmttest tchmgr"
MYCOMMANDFILES="$MYCOMMANDFILES tcbtest tcbmttest tcbmgr tcftest tcfmttest tcfmgr"
MYCOMMANDFILES="$MYCOMMANDFILES tcttest tctmttest tctmgr tcatest tcamttest tcamgr"
MYCGIFILES="tcawmgr.cgi"
MYMAN1FILES=""
MYMAN3FILES="libtcejdb.3"
#MYDOCUMENTFILES="COPYING ChangeLog doc tokyocabinet.idl"
MYDOCUMENTFILES="COPYING Changelog"
MYPCFILES="tcejdb.pc"

# Building flags
MYCFLAGS="$MYCFLAGS -std=c99 -Wall -fPIC -fsigned-char -O2"
MYCPPFLAGS="-I. -I\$(INCLUDEDIR) -I$HOME/include -I/usr/local/include"
MYCPPFLAGS="$MYCPPFLAGS -D_UNICODE -DNDEBUG -D_GNU_SOURCE=1 -D_REENTRANT -D__EXTENSIONS__"
MYLDFLAGS="-L. -L\$(LIBDIR) -L$HOME/lib -L/usr/local/lib"
MYCMDLDFLAGS=""
MYRUNPATH="\$(LIBDIR)"
MYLDLIBPATHENV="LD_LIBRARY_PATH"
MYPOSTCMD="true"

# Building paths
PATH="$PATH:$HOME/bin:/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin"
PATH="$PATH:/opt/SUNWspro/bin:/usr/ccs/bin:/usr/xpg4/bin:/usr/xpg6/bin:/usr/ucb"
CPATH="$HOME/include:/usr/local/include:$CPATH"
LIBRARY_PATH="$HOME/lib:/usr/local/lib:$LIBRARY_PATH"
LD_LIBRARY_PATH="$HOME/lib:/usr/local/lib:$LD_LIBRARY_PATH"
PKG_CONFIG_PATH="$HOME/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
export PATH CPATH LIBRARY_PATH LD_LIBRARY_PATH PKG_CONFIG_PATH


#================================================================
# Options
#================================================================


# Internal variables
enables=""

# Debug mode
AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--enable-debug], [build for debugging]))
if test "$enable_debug" = "yes"
then
  MYCFLAGS="-std=c99 -Wall -fPIC -pedantic -fsigned-char -g -O0"
  MYCPPFLAGS="$MYCPPFLAGS -UNDEBUG"
  MYCMDLDFLAGS="$MYCMDLDFLAGS -static"
  enables="$enables (debug)"
fi

# Developping mode
AC_ARG_ENABLE(devel,
  AC_HELP_STRING([--enable-devel], [build for development]))
if test "$enable_devel" = "yes"
then
  MYCFLAGS="-std=c99 -Wall -fPIC -pedantic -fsigned-char -g -O2 -pipe"
  MYCPPFLAGS="$MYCPPFLAGS -UNDEBUG"
  enables="$enables (devel)"
fi

# Profiling mode
AC_ARG_ENABLE(profile,
  AC_HELP_STRING([--enable-profile], [build for profiling]))
if test "$enable_profile" = "yes"
then
  MYCFLAGS="-std=c99 -Wall -fPIC -pedantic -fsigned-char -g -pg -O2 -pipe"
  enables="$enables (profile)"
fi

# Static mode
AC_ARG_ENABLE(static,
  AC_HELP_STRING([--enable-static], [build by static linking]))
if test "$enable_static" = "yes"
then
  MYCMDLDFLAGS="$MYCMDLDFLAGS -static"
  enables="$enables (static)"
fi

# Fastest mode
AC_ARG_ENABLE(fastest,
  AC_HELP_STRING([--enable-fastest], [build for fastest run]))
if test "$enable_fastest" = "yes"
then
  MYLIBOBJFILES="tokyocabinet_all.o"
  MYCFLAGS="-std=c99 -Wall -fPIC -pedantic -fsigned-char -O3"
  MYCFLAGS="$MYCFLAGS -fomit-frame-pointer -fforce-addr -minline-all-stringops"
  MYCPPFLAGS="$MYCPPFLAGS -D_MYFASTEST"
  enables="$enables (fastest)"
fi

# 64-bit offset mode
AC_ARG_ENABLE(off64,
  AC_HELP_STRING([--enable-off64], [build with 64-bit file offset on 32-bit system]))
if test "$enable_off64" = "yes"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_FILE_OFFSET_BITS=64"
  enables="$enables (off64)"
fi

# Swapping byte-orders mode
AC_ARG_ENABLE(swab,
  AC_HELP_STRING([--enable-swab], [build for swapping byte-orders]))
if test "$enable_swab" = "yes"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYSWAB"
  enables="$enables (swab)"
fi

# Micro yield mode
AC_ARG_ENABLE(uyield,
  AC_HELP_STRING([--enable-uyield], [build for detecting race conditions]))
if test "$enable_uyield" = "yes"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYMICROYIELD"
  enables="$enables (uyield)"
fi

# Enable test cases
AC_ARG_ENABLE(tests,
  AC_HELP_STRING([--enable-tests], [build with testcases]))
if test "$enable_tests" = "yes"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYTESTS"
  enables="$enables (tests)"
fi

# Enable BZIP2 compression
AC_ARG_ENABLE(bzip,
  AC_HELP_STRING([--enable-bzip], [build with BZIP2 compression]))
if test "$enable_bzip" = "yes"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYBZIP"
  enables="$enables (bzip)"
fi

# Disable the unified buffer cache assumption
AC_ARG_ENABLE(ubc,
  AC_HELP_STRING([--disable-ubc], [build without the unified buffer cache assumption]))
if test "$enable_ubc" = "no"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYNOUBC"
  enables="$enables (no-ubc)"
fi

# Disable ZLIB compression
AC_ARG_ENABLE(zlib,
  AC_HELP_STRING([--disable-zlib], [build without ZLIB compression]))
if test "$enable_zlib" = "no"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYNOZLIB"
  enables="$enables (no-zlib)"
fi

# Disable POSIX thread
AC_ARG_ENABLE(pthread,
  AC_HELP_STRING([--disable-pthread], [build without POSIX thread support]))
if test "$enable_pthread" = "no"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYNOPTHREAD"
  enables="$enables (no-pthread)"
fi

# Disable shared object
AC_ARG_ENABLE(shared,
  AC_HELP_STRING([--disable-shared], [avoid to build shared libraries]))
if test "$enable_shared" = "no"
then
  enables="$enables (no-shared)"
fi

# Enable custom codec functions of LZMA
AC_ARG_ENABLE(exlzma,
  AC_HELP_STRING([--disable-exlzma], [build with the custom codec of LZMA]))
if test "$enable_exlzma" = "yes"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYEXLZMA"
  enables="$enables (exlzma)"
fi

# Enable custom codec functions of LZO
AC_ARG_ENABLE(exlzo,
  AC_HELP_STRING([--disable-exlzo], [build with the custom codec of LZO]))
if test "$enable_exlzo" = "yes"
then
  MYCPPFLAGS="$MYCPPFLAGS -D_MYEXLZO"
  enables="$enables (exlzo)"
fi

# Specify the installation path of ZLIB
AC_ARG_WITH(zlib,
  AC_HELP_STRING([--with-zlib=DIR], [search DIR/include and DIR/lib for ZLIB]))
if test -n "$with_zlib"
then
  MYCPPFLAGS="$MYCPPFLAGS -I$with_zlib/include"
  MYLDFLAGS="$MYLDFLAGS -L$with_zlib/lib"
  MYRUNPATH="$MYRUNPATH:$with_zlib/lib"
  CPATH="$CPATH:$with_zlib/include"
  LIBRARY_PATH="$LIBRARY_PATH:$with_zlib/lib"
  LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$with_zlib/lib"
fi

# Specify the installation path of BZIP2
AC_ARG_WITH(bzip,
  AC_HELP_STRING([--with-bzip=DIR], [search DIR/include and DIR/lib for BZIP2]))
if test -n "$with_bzip"
then
  MYCPPFLAGS="$MYCPPFLAGS -I$with_bzip/include"
  MYLDFLAGS="$MYLDFLAGS -L$with_bzip/lib"
  MYRUNPATH="$MYRUNPATH:$with_bzip/lib"
  CPATH="$CPATH:$with_bzip/include"
  LIBRARY_PATH="$LIBRARY_PATH:$with_bzip/lib"
  LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$with_bzip/lib"
fi

# Messages
printf '#================================================================\n'
printf '# Configuring EJDB version %s%s.\n' "$PACKAGE_VERSION" "$enables"
printf '#================================================================\n'



#================================================================
# Checking Commands and Libraries
#================================================================

# C compiler
AC_PROG_CC

# Reset variables
if test "$GCC" != "yes"
then
  AC_MSG_WARN([another compiler except for GCC was detected])
  MYCFLAGS=""
fi

# Byte order
AC_C_BIGENDIAN(MYCPPFLAGS="$MYCPPFLAGS -D_MYBIGEND")

# Underlying libraries
AC_CHECK_LIB(c, main)
AC_CHECK_LIB(m, main)
if test "$enable_pthread" != "no"
then
  AC_CHECK_LIB(pthread, main)
  AC_CHECK_LIB(rt, main)
fi
if test "$enable_zlib" != "no"
then
  AC_CHECK_LIB(z, main)
fi
if test "$enable_bzip" = "yes"
then
  AC_CHECK_LIB(bz2, main)
fi
if test "$enable_exlzma" = "yes"
then
  AC_CHECK_LIB(lzma, main)
fi
if test "$enable_exlzo" = "yes"
then
  AC_CHECK_LIB(lzo2, main)
fi
AC_CHECK_LIB(tcejdb, main, AC_MSG_WARN([old version of EJDB was found]))

if test "$enable_tests" = "yes"
then
  AC_CHECK_LIB(cunit, main, true, AC_MSG_ERROR([CUnit lubrary is required]))
  AC_CHECK_HEADER(CUnit/Basic.h, true, AC_MSG_ERROR([CUnit/Basic.h is required]))
fi

# Necessary headers
AC_CHECK_HEADER(stdlib.h, true, AC_MSG_ERROR([stdlib.h is required]))
AC_CHECK_HEADER(stdint.h, true, AC_MSG_ERROR([stdint.h is required]))
AC_CHECK_HEADER(unistd.h, true, AC_MSG_ERROR([unistd.h is required]))
AC_CHECK_HEADER(dirent.h, true, AC_MSG_ERROR([dirent.h is required]))
AC_CHECK_HEADER(regex.h, true, AC_MSG_ERROR([regex.h is required]))
AC_CHECK_HEADER(glob.h, true, AC_MSG_ERROR([glob.h is required]))
if test "$enable_pthread" != "no"
then
  AC_CHECK_HEADER(pthread.h, true, AC_MSG_ERROR([pthread.h is required]))
fi
if test "$enable_zlib" != "no"
then
  AC_CHECK_HEADER(zlib.h, true, AC_MSG_ERROR([zlib.h is required]))
fi
if test "$enable_bzip" = "yes"
then
  AC_CHECK_HEADER(bzlib.h, true, AC_MSG_ERROR([bzlib.h is required]))
fi
if test "$enable_exlzma" = "yes"
then
  AC_CHECK_HEADER(lzmalib.h, true, AC_MSG_ERROR([lzmalib.h is required]))
fi
if test "$enable_exlzo" = "yes"
then
  AC_CHECK_HEADER(lzo/lzo1x.h, true, AC_MSG_ERROR([lzo/lzo1x.h is required]))
fi

# Shared libraries
if test "$enable_shared" != "no" && test "$enable_profile" != "yes"
then
  if uname | grep Darwin >/dev/null
  then
    MYLIBRARYFILES="$MYLIBRARYFILES libtcejdb.$MYLIBVER.$MYLIBREV.0.dylib"
    MYLIBRARYFILES="$MYLIBRARYFILES libtcejdb.$MYLIBVER.dylib"
    MYLIBRARYFILES="$MYLIBRARYFILES libtcejdb.dylib"
    MYLDLIBPATHENV="DYLD_LIBRARY_PATH"
  else
    MYLIBRARYFILES="$MYLIBRARYFILES libtcejdb.so.$MYLIBVER.$MYLIBREV.0"
    MYLIBRARYFILES="$MYLIBRARYFILES libtcejdb.so.$MYLIBVER"
    MYLIBRARYFILES="$MYLIBRARYFILES libtcejdb.so"
  fi
fi



#================================================================
# Generic Settings
#================================================================

# Export variables
AC_SUBST(MYLIBVER)
AC_SUBST(MYLIBREV)
AC_SUBST(MYFORMATVER)
AC_SUBST(MYHEADERFILES)
AC_SUBST(MYLIBRARYFILES)
AC_SUBST(MYLIBOBJFILES)
AC_SUBST(MYCOMMANDFILES)
AC_SUBST(MYCGIFILES)
AC_SUBST(MYMAN1FILES)
AC_SUBST(MYMAN3FILES)
AC_SUBST(MYDOCUMENTFILES)
AC_SUBST(MYPCFILES)
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
AC_OUTPUT(Makefile tcejdb.pc)

# Messages
printf '#================================================================\n'
printf '# Ready to make.\n'
printf '#================================================================\n'



# END OF FILE
