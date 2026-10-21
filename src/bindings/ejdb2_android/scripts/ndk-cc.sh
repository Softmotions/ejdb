#!/bin/sh
# Android NDK cross compiler front-end used by the ejdb2 Autark cross build.
#
# The ejdb2 build is driven by Autark, which has no host/target split: the
# `system.sh` sanity check of every Autark project compiles a small program and
# *executes* it on the build machine to detect word size and endianness, and
# some components (the PEG parser generator used to generate the JQL parser)
# are build host tools.  This front-end handles both cases:
#
#   * The `system.sh` probe is answered for the target (a target binary cannot
#     run on the build machine).
#   * Compilation invoked from a host tool directory (ANDROID_HOST_DIRS) is
#     forwarded to the build host compiler.
#   * Everything else is forwarded to the NDK compiler of the target ABI.
#
# Environment:
#   ANDROID_NDK_CC          Target NDK clang wrapper (required)
#   ANDROID_TARGET_BITS     Target pointer size: 32 or 64 (required)
#   ANDROID_HOST_DIRS       Space separated list of host tool build directories
#   ANDROID_HOST_CC         Build host C compiler (default: cc)
#   ANDROID_EXTRA_CFLAGS    Extra flags for target invocations (optional)

set -e

if [ -z "${ANDROID_NDK_CC}" ]; then
  echo "ndk-cc.sh: ANDROID_NDK_CC is not set" >&2
  exit 1
fi

case " $* " in
  *test_system.c*)
    # Autark system.sh probe: `${CC} ./test_system.c -o ./test_system` and then
    # `./test_system` is executed.  Describe the target instead of compiling
    # the probe for it.
    case "${ANDROID_TARGET_BITS}" in
      64)
        BITNESS=SYSTEM_BITNESS_64
        ;;
      32)
        BITNESS=SYSTEM_BITNESS_32
        ;;
      *)
        echo "ndk-cc.sh: ANDROID_TARGET_BITS must be 32 or 64" >&2
        exit 1
        ;;
    esac
    # All supported Android ABIs are little endian.
    cat > ./test_system <<EOF
#!/bin/sh
echo "${BITNESS}=1"
echo "SYSTEM_LITTLE_ENDIAN=1"
EOF
    chmod +x ./test_system
    exit 0
    ;;
esac

# Host tools (eg the PEG `leg` generator) must be executable on the build host.
if [ -n "${ANDROID_HOST_DIRS}" ]; then
  for dir in ${ANDROID_HOST_DIRS}; do
    case "$PWD/" in
      "${dir%/}"/*)
        exec "${ANDROID_HOST_CC:-cc}" "$@"
        ;;
    esac
  done
fi

if [ -n "${ANDROID_EXTRA_CFLAGS}" ]; then
  # shellcheck disable=SC2086
  exec "${ANDROID_NDK_CC}" ${ANDROID_EXTRA_CFLAGS} "$@"
fi

exec "${ANDROID_NDK_CC}" "$@"
