#!/bin/sh

set -e

CROSS=0
if [ -n "${SYSTEM_CROSS:-}" ]; then
  CROSS=1
fi

# In a cross build the toolchain descriptor is a real dependency of this check:
# editing the descriptor (or switching the target) reruns it.
if [ "$CROSS" -eq 1 ] && [ -n "${SYSTEM_TOOLCHAIN:-}" ]; then
  autark dep "$SYSTEM_TOOLCHAIN"
fi

# Target facts declared by the toolchain descriptor (if any).
DECLARED_SYSTEM_NAME="${SYSTEM_NAME:-}"
DECLARED_SYSTEM_ARCH="${SYSTEM_ARCH:-}"
DECLARED_SYSTEM_ARCH_ALT="${SYSTEM_ARCH_ALT:-}"

SYSTEM_NAME="$(uname -s)"
ARCH_RAW="$(uname -m)"

case "$SYSTEM_NAME" in
  CYGWIN*|MINGW*|MSYS*) SYSTEM_NAME="Windows" ;;
esac

if uname -o 2>/dev/null | grep -qi android || uname -a | grep -qi android || [ -f /system/build.prop ]; then
  SYSTEM_NAME=Android
fi

if [ "$CROSS" -eq 1 ] && [ -n "$DECLARED_SYSTEM_NAME" ]; then
  SYSTEM_NAME="$DECLARED_SYSTEM_NAME"
fi

case "$ARCH_RAW" in
  i386|i486|i586|i686)
    SYSTEM_ARCH="x86"
    SYSTEM_ARCH_ALT="ia32"
    ;;
  x86_64|amd64)
    SYSTEM_ARCH="x86_64"
    SYSTEM_ARCH_ALT="x64"
    ;;
  armv6l|armv7l|armv7hl|arm)
    SYSTEM_ARCH="armv7"
    SYSTEM_ARCH_ALT="arm"
    ;;
  aarch64|arm64)
    SYSTEM_ARCH="aarch64"
    SYSTEM_ARCH_ALT="arm64"
    ;;
  riscv32)
    SYSTEM_ARCH="riscv32"
    SYSTEM_ARCH_ALT="riscv32"
    ;;
  riscv64)
    SYSTEM_ARCH="riscv64"
    SYSTEM_ARCH_ALT="riscv64"
    ;;
  mips|mipsel|mips64|mips64el)
    SYSTEM_ARCH="mips"
    SYSTEM_ARCH_ALT="mips"
    ;;
  ppc)
    SYSTEM_ARCH="powerpc"
    SYSTEM_ARCH_ALT="ppc"
    ;;
  ppc64|ppc64le)
    SYSTEM_ARCH="powerpc"
    SYSTEM_ARCH_ALT="ppc64"
    ;;
  s390)
    SYSTEM_ARCH="s390"
    SYSTEM_ARCH_ALT="s390"
    ;;
  s390x)
    SYSTEM_ARCH="s390"
    SYSTEM_ARCH_ALT="s390x"
    ;;
  *)
    SYSTEM_ARCH="unknown"
    SYSTEM_ARCH_ALT="unknown"
    ;;
esac

if [ "$CROSS" -eq 1 ]; then
  if [ -n "$DECLARED_SYSTEM_ARCH" ]; then
    SYSTEM_ARCH="$DECLARED_SYSTEM_ARCH"
  fi
  if [ -n "$DECLARED_SYSTEM_ARCH_ALT" ]; then
    SYSTEM_ARCH_ALT="$DECLARED_SYSTEM_ARCH_ALT"
  fi
fi

check_tool() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: required tool '$1' not found in PATH" >&2
    exit 1
  }
}

if [ -n "${PKGCONF:-}" ] && command -v "$PKGCONF" >/dev/null 2>&1; then
  : # pkg-config tool declared by the toolchain descriptor
elif command -v pkgconf >/dev/null 2>&1; then
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

if [ "$CROSS" -eq 1 ]; then
  # ---------------------------------------------------------------------------
  # Cross compilation: target facts are declared by the toolchain descriptor
  # (or by SYSTEM_* environment variables) instead of being probed on the host.
  # ---------------------------------------------------------------------------

  if [ -z "${SYSTEM_NAME:-}" ]; then
    echo "error: cross build for target ${SYSTEM_TARGET:-unknown}: target system is not declared" >&2
    exit 1
  fi

  BITS="${SYSTEM_BITNESS:-}"
  ENDIAN="${SYSTEM_ENDIAN:-}"
  RUNNER="${SYSTEM_RUNNER:-${RUNNER:-}}"

  if [ -n "${SYSTEM_TARGET:-}" ]; then
    echo "Cross compiling for target: ${SYSTEM_TARGET} (${SYSTEM_NAME}/${SYSTEM_ARCH})"
  else
    echo "Cross compiling for target: ${SYSTEM_NAME}/${SYSTEM_ARCH}"
  fi

  # If an emulator/device runner is declared, the target probe can be executed,
  # which turns declared capabilities into measured ones.
  if [ -n "$RUNNER" ]; then
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
    if ${RUNNER} ./test_system > ./test_system.out 2>/dev/null; then
      if grep -q '^SYSTEM_' ./test_system.out; then
        grep -E '^SYSTEM_(BITNESS_[0-9]+|[A-Z]*ENDIAN)=' ./test_system.out | xargs autark set
      fi
      measured_bits="$(sed -n 's/^SYSTEM_BITNESS_\([0-9][0-9]*\)=1$/\1/p' ./test_system.out | head -n 1)"
      if [ -n "$measured_bits" ]; then
        BITS="$measured_bits"
      fi
      if grep -q '^SYSTEM_LITTLE_ENDIAN=1$' ./test_system.out; then
        ENDIAN="little"
      elif grep -q '^SYSTEM_BIGENDIAN=1$' ./test_system.out; then
        ENDIAN="big"
      fi
    else
      echo "warning: target probe failed to run through: ${RUNNER}" >&2
    fi
  fi

  autark set "SYSTEM_CROSS=1"
  if [ -n "${SYSTEM_TARGET:-}" ]; then
    autark set "SYSTEM_TARGET=$SYSTEM_TARGET"
  fi
  if [ -n "${SYSTEM_TOOLCHAIN:-}" ]; then
    autark set "SYSTEM_TOOLCHAIN=$SYSTEM_TOOLCHAIN"
  fi
  autark set "SYSTEM_NAME=$(printf '%s' "$SYSTEM_NAME" | tr '[:upper:]' '[:lower:]')"
  autark set "SYSTEM_$(printf '%s' "$SYSTEM_NAME" | tr '[:lower:]' '[:upper:]')=1"
  autark set "SYSTEM_ARCH=$SYSTEM_ARCH"
  autark set "SYSTEM_ARCH_ALT=$SYSTEM_ARCH_ALT"

  if [ -n "$BITS" ]; then
    autark set "SYSTEM_BITNESS=$BITS"
    autark set "SYSTEM_BITNESS_$BITS=1"
  fi

  case "$ENDIAN" in
    little|le)
      autark set "SYSTEM_ENDIAN=little"
      autark set "SYSTEM_LITTLE_ENDIAN=1"
      ;;
    big|be)
      autark set "SYSTEM_ENDIAN=big"
      autark set "SYSTEM_BIGENDIAN=1"
      ;;
    '')
      echo "warning: endianness of target ${SYSTEM_TARGET:-unknown} is not declared" >&2
      ;;
    *)
      echo "error: unknown target endianness: $ENDIAN" >&2
      exit 1
      ;;
  esac

  if [ -n "$RUNNER" ]; then
    autark set "RUNNER=$RUNNER"
    autark set "SYSTEM_RUNNER=$RUNNER"
  fi

  autark env CC
  autark env AR
  autark env CFLAGS
  autark env SYSTEM_CROSS
  autark env SYSTEM_TARGET
  autark env SYSTEM_TOOLCHAIN
  exit 0
fi

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
autark set "SYSTEM_NAME=$(printf '%s' "$SYSTEM_NAME" | tr '[:upper:]' '[:lower:]')"
autark set "SYSTEM_$(printf '%s' "$SYSTEM_NAME" | tr '[:lower:]' '[:upper:]')=1"
autark set "SYSTEM_ARCH=$SYSTEM_ARCH"
autark set "SYSTEM_ARCH_ALT=$SYSTEM_ARCH_ALT"
