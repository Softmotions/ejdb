#!/bin/sh

set -e

SYSTEM_NAME="$(uname -s)"
ARCH_RAW="$(uname -m)"

case "$SYSTEM_NAME" in
  CYGWIN*|MINGW*|MSYS*) SYSTEM_NAME="Windows" ;;
esac

if uname -o 2>/dev/null | grep -qi android || uname -a | grep -qi android || [ -f /system/build.prop ]; then
  SYSTEM_NAME=Android
fi

case "$ARCH_RAW" in
  i386|i486|i586|i686)
    SYSTEM_ARCH="x86"
    ;;
  x86_64|amd64)
    SYSTEM_ARCH="x86_64"
    ;;
  armv6l|armv7l|armv7hl)
    SYSTEM_ARCH="armv7"
    ;;
  aarch64|arm64)
    SYSTEM_ARCH="aarch64"
    ;;
  riscv32)
    SYSTEM_ARCH="riscv32"
    ;;
  riscv64)
    SYSTEM_ARCH="riscv64"
    ;;
  mips|mipsel|mips64|mips64el)
    SYSTEM_ARCH="mips"
    ;;
  ppc|ppc64|ppc64le)
    SYSTEM_ARCH="powerpc"
    ;;
  s390|s390x)
    SYSTEM_ARCH="s390"
    ;;
  *)
    SYSTEM_ARCH="unknown"
    ;;
esac

check_tool() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: required tool '$1' not found in PATH" >&2
    exit 1
  }
}

if command -v pkgconf >/dev/null 2>&1; then
  PKGCONF=pkgconf
elif command -v pkg-config >/dev/null 2>&1; then
  PKGCONF=pkg-config
else
  echo "error: neither 'pkgconf' nor 'pkg-config' found in PATH" >&2
  exit 1
fi

CC=${CC:-cc}
AR=${AR:-ar}

autark set "PKGCONF=$PKGCONF"
check_tool $CC && autark set "CC=$CC"
check_tool $AR && autark set "AR=$AR"
check_tool install

cat > './test_system.c' << 'EOF'
#include <stdio.h>
#include <stdlib.h>
int main(void) {

  if (sizeof(void*) == 8) {
    puts("SYSTEM_BITNESS_64=1");
  } else if (sizeof(void*) == 4) {
    puts("SYSTEM_BITNESS_32=1");
  } else {
    puts("Unknown bitness");
    exit(1);
  }

  unsigned x = 1;
  if (*(char*)&x == 0) {
    puts("SYSTEM_BIGENDIAN=1");
  } else {
    puts("SYSTEM_LITTLE_ENDIAN=1");
  }

  return 0;
}
EOF

${CC} ./test_system.c -o ./test_system || exit 1

./test_system | xargs autark set
eval "$(./test_system)"

autark env CC
autark env CFLAGS
autark set "SYSTEM_NAME=$SYSTEM_NAME"
autark set "SYSTEM_$(echo -n "$SYSTEM_NAME" | tr '[:lower:]' '[:upper:]')=1"
autark set "SYSTEM_ARCH=$SYSTEM_ARCH"

