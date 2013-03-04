
test -n "$CFLAGS" && MYCFLAGS="$CFLAGS $MYCFLAGS"
test -n "$CPPFLAGS" && MYCPPFLAGS="$CPPFLAGS $MYCPPFLAGS"
test -n "$LDFLAGS" && MYLDFLAGS="$LDFLAGS $MYLDFLAGS"

# Package name
AC_INIT(luaejdb, 1.0.1)

luaver=`lua -e 'v = string.gsub(_VERSION, ".* ", ""); print(v)'`

MYLIBRARYFILES=""
MYLIBOBJFILES="luaejdb.o"
MYCFLAGS="-std=c99 -Wall -fPIC -fsigned-char -O2 $MYCFLAGS"
MYCPPFLAGS="-DNDEBUG -D_GNU_SOURCE=1 $MYCPPFLAGS"
MYLDLIBPATHENV="LD_LIBRARY_PATH"
MYLDFLAGS="-L. -L\$(LIBDIR)"

PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin:$PATH"
LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
CPATH="/usr/local/include:/usr/include:$CPATH"
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
export PATH LIBRARY_PATH LD_LIBRARY_PATH CPATH PKG_CONFIG_PATH

if type pkg-config > /dev/null 2>&1
then
  LIBRARY_PATH="`pkg-config --variable=libdir tcejdb`:`pkg-config --variable=libdir lua$luaver`:$LIBRARY_PATH"
  LD_LIBRARY_PATH="`pkg-config --variable=libdir tcejdb`:`pkg-config --variable=libdir lua$luaver`:$LD_LIBRARY_PATH"
  CPATH="`pkg-config --variable=includedir tcejdb`:`pkg-config --variable=includedir lua$luaver`/lua$luaver:$CPATH"
  export PATH LIBRARY_PATH LD_LIBRARY_PATH CPATH PKG_CONFIG_PATH
  MYCPPFLAGS="$MYCPPFLAGS `pkg-config --cflags tcejdb lua$luaver`"
fi

printf "$LIBRARY_PATH"

# Shared libraries
if uname | grep Darwin >/dev/null
then
  MYLIBRARYFILES="$MYLIBRARYFILES luaejdb.dylib"
  MYLDLIBPATHENV="DYLD_LIBRARY_PATH"
else
  MYLIBRARYFILES="$MYLIBRARYFILES luaejdb.so"
fi

# Lua library path
MYLUALIBDIR=`lua tools/libpath.lua`
if test -z "$MYLUALIBDIR"
then
  MYLUALIBDIR="$libdir/lua/$luaver"
fi

# C compiler
AC_PROG_CC

AC_CHECK_LIB(tcejdb, main)

AC_CHECK_HEADER(tcejdb/ejdb.h, true, AC_MSG_ERROR(tcejdb/ejdb.h is required))
AC_CHECK_HEADER(lua.h, true, AC_MSG_ERROR(lua.h is required))

AC_SUBST(MYLIBOBJFILES)
AC_SUBST(MYCFLAGS)
AC_SUBST(MYCPPFLAGS)
AC_SUBST(MYLDLIBPATHENV)
AC_SUBST(MYLDFLAGS)
AC_SUBST(MYLIBRARYFILES)
AC_SUBST(MYLUALIBDIR)

# Targets
AC_OUTPUT(Makefile)
AC_OUTPUT(luaejdb.rockspec)

