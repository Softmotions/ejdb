#!/bin/sh

# Sets the follwoing Autark variables if JDK is found
# - JDK_HOME
# - JNI_LIBDIR
# - JNI_LDFLAGS
# - JNI_CFLAGS
# - JAVAC_EXEC
# - JAVA_EXEC
# - JAR_EXEC

set -e

OS="$(uname -s)"
JAVA_HOME_CANDIDATE="$JAVA_HOME"

if [ -z "$JAVA_HOME_CANDIDATE" ]; then
  JAVA_HOME_CANDIDATE="$JDK_HOME";
fi

if [ -n "$JAVA_HOME_CANDIDATE" ] && [ ! -x "$JAVA_HOME_CANDIDATE/bin/java" ]; then
  JAVA_HOME_CANDIDATE=""
fi

if [ -z "$JAVA_HOME_CANDIDATE" ] && [ command -v java >/dev/null 2>&1 ]; then
  JAVA_REAL_PATH="$(readlink -f "$(command -v java)" 2>/dev/null || true)"

  # On OpenBSD readlink -f is not available
  if [ -z "$JAVA_REAL_PATH" ] && [ "$OS" = "OpenBSD" ]; then
    JAVA_REAL_PATH="$(cd "$(dirname "$(command -v java)")" && pwd -P)/java"
  fi

  JAVA_BIN_DIR="$(dirname "$JAVA_REAL_PATH")"
  JAVA_HOME_CANDIDATE="$(dirname "$JAVA_BIN_DIR")"

  # Skip if it's just /usr or /usr/bin
  case "$JAVA_HOME_CANDIDATE" in
    /usr|/usr/bin|/usr/java)
      JAVA_HOME_CANDIDATE=""
      ;;
  esac
fi

if [ -z "$JAVA_HOME_CANDIDATE" ] && [ "$OS" = "Darwin" ]; then
  JAVA_HOME_CANDIDATE="$(/usr/libexec/java_home 2>/dev/null || true)"
fi

if [ -z "$JAVA_HOME_CANDIDATE" ] && [ "$OS" = "OpenBSD" ]; then
  JAVA_HOME_CANDIDATE="$(find /usr/local -maxdepth 1 -type d -name 'jdk-*' | sort -Vr | head -n 1)"
fi

if [ -z "$JAVA_HOME_CANDIDATE" ] && [ "$OS" = "FreeBSD" ]; then
  JAVA_HOME_CANDIDATE="$(find /usr/local -maxdepth 1 -type d -name 'openjdk*' | sort -Vr | head -n 1)"
fi

# Linux fallback
if [ -z "$JAVA_HOME_CANDIDATE" ]; then
  for dir in /usr/lib/jvm /usr/java /opt/java; do
    [ -d "$dir" ] || continue
    if [ -d "$dir/default-jdk" ]; then
      JAVA_HOME_CANDIDATE="$dir/default-jdk"
      break;
    fi
    JAVA_HOME_CANDIDATE="$(find "$dir" -maxdepth 1 -type d -name '*jdk*' | sort -Vr | head -n 1)"
    [ -n "$JAVA_HOME_CANDIDATE" ] && break
  done
fi

if [ -n "$JAVA_HOME_CANDIDATE" ] && [ -x "$JAVA_HOME_CANDIDATE/bin/java" ]; then
  JAVA_HOME="$JAVA_HOME_CANDIDATE"
  JDK_HOME="$JAVA_HOME"

  JNI_CFLAGS="-I$JAVA_HOME/include"
  JNI_LIB_DIR=""

  # OS-specific JNI headers
  case "$OS" in
    Linux)   JNI_OS_DIR="linux" ;;
    Darwin)  JNI_OS_DIR="darwin" ;;
    FreeBSD) JNI_OS_DIR="freebsd" ;;
    OpenBSD) JNI_OS_DIR="openbsd" ;;
    SunOS)   JNI_OS_DIR="solaris" ;;
    *)       JNI_OS_DIR="" ;;
  esac

  for candidate in \
    "$JAVA_HOME/lib/server" \
    "$JAVA_HOME/jre/lib/server" \
    "$JAVA_HOME/lib/amd64/server" \
    "$JAVA_HOME/jre/lib/amd64/server" \
    "$JAVA_HOME/lib/sparcv9/server" \
    "$JAVA_HOME/jre/lib/sparcv9/server" \
    "$JAVA_HOME/lib/j9vm" \
    "$JAVA_HOME/lib/default" \
    "$JAVA_HOME/jre/lib/amd64/j9vm" \
    "$JAVA_HOME/jre/lib/amd64/default" \
    "$JAVA_HOME/lib" \
    "$JAVA_HOME/jre/lib"
  do
    if [ -f "$candidate/libjvm.so" ] || [ -f "$candidate/libjvm.dylib" ]; then
      JNI_LIB_DIR="$candidate"
      break
    fi
  done

  echo "JDK_HOME: $JAVA_HOME"

  if [ -n "$JNI_LIB_DIR" ]; then
    JNI_LDFLAGS="-L$JNI_LIB_DIR -ljvm"
    echo "JNI_LDFLAGS: $JNI_LDFLAGS"
    autark set "JNI_LDFLAGS=$JNI_LDFLAGS"
    autark set "JNI_LIBDIR=$JNI_LIB_DIR"
  else
    echo "Warning: libjvm.so not found in known locations" >&2
  fi

  if [ -n "$JNI_OS_DIR" ] && [ -d "$JAVA_HOME/include/$JNI_OS_DIR" ]; then
    JNI_CFLAGS="$JNI_CFLAGS -I$JAVA_HOME/include/$JNI_OS_DIR"
  fi
  
  autark set "JDK_HOME=$JDK_HOME"
  autark set "JNI_CFLAGS=$JNI_CFLAGS"
  autark set "JAVA_EXEC=$JDK_HOME/bin/java"
  autark set "JAVAC_EXEC=$JDK_HOME/bin/javac"
  autark set "JAR_EXEC=$JDK_HOME/bin/jar"

else
  echo "Warning: No valid JDK found." >&2
fi