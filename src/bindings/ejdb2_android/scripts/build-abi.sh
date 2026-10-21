#!/bin/sh
# Cross builds the ejdb2 JNI binding for a single Android ABI.
#
# The build itself is the regular ejdb2 Autark build, executed as a cross build
# in a per-ABI cache directory with the NDK toolchain descriptor
# `toolchains/android-<abi>.autark` (Autark `--toolchain` option).  Target facts
# (SYSTEM_*, CC, AR, sysroot) are declared by that descriptor instead of being
# probed on the build host, and build tools (the PEG `leg` generator) are built
# by the build host toolchain.  HTTP/WebSocket server support is disabled for the
# Android build (src/jbr is not built).
#
# Usage: build-abi.sh <abi> <api-level>
#
#   abi        Android ABI: armeabi-v7a, arm64-v8a, x86, x86_64
#   api-level  Android API level, must be >= 24 (default in the root Autark: 24)
#
# Environment:
#   ANDROID_NDK_HOME         Android NDK root, or ANDROID_HOME/ANDROID_SDK_ROOT
#                            pointing to an SDK with an installed NDK
#   ANDROID_BUILD_TYPE       Build type of the native code (default: Release)
#   EJDB_ANDROID_JNILIBS_DIR Directory the per-ABI libejdb2jni.so is staged in
#   AUTARK_ROOT_DIR          ejdb2 source tree root (set by the Autark build)
#   AUTARK_CACHE_DIR         ejdb2 build cache (set by the Autark build)
#   AUTARK                   Autark executable (default: autark)
#
# The built library is staged as
#   <EJDB_ANDROID_JNILIBS_DIR>/<abi>/libejdb2jni.so
# which is the standard `jniLibs` location of the Gradle test project.

set -e

ABI="$1"
API="$2"

usage() {
  echo "Usage: build-abi.sh <abi> <api-level>" >&2
  echo "ABIs: armeabi-v7a arm64-v8a x86 x86_64" >&2
  exit 1
}

[ -n "$ABI" ] || usage
[ -n "$API" ] || usage

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
EJDB_ROOT="${AUTARK_ROOT_DIR:-$(cd "$SCRIPT_DIR/../../.." && pwd -P)}"
EJDB_CACHE="${AUTARK_CACHE_DIR:-$EJDB_ROOT/autark-cache}"
AUTARK="${AUTARK:-autark}"
BUILD_TYPE="${ANDROID_BUILD_TYPE:-Release}"
JNILIBS_DIR="${EJDB_ANDROID_JNILIBS_DIR:-$EJDB_ROOT/src/bindings/ejdb2_android/test/ejdb2/src/main/jniLibs}"
TOOLCHAIN="$SCRIPT_DIR/../toolchains/android-$ABI.autark"

case "$ABI" in
  armeabi-v7a)
    TRIPLE=armv7a-linux-androideabi
    ;;
  arm64-v8a)
    TRIPLE=aarch64-linux-android
    ;;
  x86)
    TRIPLE=i686-linux-android
    ;;
  x86_64)
    TRIPLE=x86_64-linux-android
    ;;
  *)
    echo "build-abi.sh: unsupported Android ABI '$ABI'" >&2
    usage
    ;;
esac

if [ ! -f "$TOOLCHAIN" ]; then
  echo "build-abi.sh: toolchain descriptor not found: $TOOLCHAIN" >&2
  exit 1
fi

case "$API" in
  ''|*[!0-9]*)
    echo "build-abi.sh: API level must be a number, got '$API'" >&2
    exit 1
    ;;
esac

# iowow relies on bionic pthread spinlocks, they are available since API 24.
if [ "$API" -lt 24 ]; then
  echo "build-abi.sh: Android API level must be >= 24, got '$API'" >&2
  exit 1
fi

NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT}}"

if [ -z "$NDK" ]; then
  for sdk in "$ANDROID_HOME" "$ANDROID_SDK_ROOT"; do
    [ -n "$sdk" ] && [ -d "$sdk/ndk" ] || continue
    NDK="$(find "$sdk/ndk" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -n 1)"
    [ -n "$NDK" ] && break
  done
fi

if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
  echo "build-abi.sh: Android NDK not found." >&2
  echo "build-abi.sh: install it with sdkmanager or set ANDROID_NDK_HOME" >&2
  exit 1
fi

NDK="$(cd "$NDK" && pwd -P)"

case "$(uname -s)-$(uname -m)" in
  Linux-x86_64)          HOST_TAG=linux-x86_64 ;;
  Darwin-x86_64)         HOST_TAG=darwin-x86_64 ;;
  Darwin-arm64)          HOST_TAG=darwin-arm64 ;;
  MINGW*|MSYS*|CYGWIN*)  HOST_TAG=windows-x86_64 ;;
  *)
    echo "build-abi.sh: unsupported build host '$(uname -s) $(uname -m)'" >&2
    exit 1
    ;;
esac

NDK_PREBUILT="$NDK/toolchains/llvm/prebuilt/$HOST_TAG"
ANDROID_NDK_BIN="$NDK_PREBUILT/bin"
ANDROID_NDK_SYSROOT="$NDK_PREBUILT/sysroot"
NDK_CC="$ANDROID_NDK_BIN/$TRIPLE$API-clang"

if [ ! -x "$NDK_CC" ]; then
  echo "build-abi.sh: target compiler not found: $NDK_CC" >&2
  echo "build-abi.sh: API level $API may not be supported by this NDK" >&2
  exit 1
fi

CACHE="$EJDB_CACHE/android-$ABI"

echo "Android $ABI (API $API), build type: $BUILD_TYPE"
echo "  NDK:       $NDK"
echo "  toolchain: $TOOLCHAIN"
echo "  cache:     $CACHE"

# NDK layout facts for the toolchain descriptor.  Autark variables are not
# exported to check scripts, the descriptor reads them from the environment.
export ANDROID_NDK_BIN ANDROID_NDK_SYSROOT
ANDROID_NDK_API_LEVEL="$API"
export ANDROID_NDK_API_LEVEL

"$AUTARK" -H "$CACHE" \
  --toolchain "$TOOLCHAIN" \
  -DBUILD_TYPE="$BUILD_TYPE" \
  -DEJDB_ENABLE_HTTP=0 \
  -DEJDB_BUILD_TESTS=0 \
  -DEJDB_RUN_TESTS=0 \
  -DBUILD_BINDING_JNI=1 \
  -DBUILD_BINDING_ANDROID=0 \
  -DBUILD_BINDING_NODEJS=0 \
  "$EJDB_ROOT"

SO="$CACHE/src/bindings/ejdb2_jni/libejdb2jni.so"

if [ ! -e "$SO" ]; then
  echo "build-abi.sh: expected build artifact not found: $SO" >&2
  exit 1
fi

mkdir -p "$JNILIBS_DIR/$ABI"
cp -f "$SO" "$JNILIBS_DIR/$ABI/libejdb2jni.so"

echo "Staged: $JNILIBS_DIR/$ABI/libejdb2jni.so"
