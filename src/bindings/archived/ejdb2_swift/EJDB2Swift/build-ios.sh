#!/bin/bash
set -e
set -x

SCRIPTPATH="$(
  cd "$(dirname "$0")"
  pwd -P
)"
cd $SCRIPTPATH
INSTALLPATH="${SCRIPTPATH}"

if [[ ! -z "${PODS_ROOT}" ]]; then
  INSTALLPATH="${PODS_ROOT}/EJDB2"
fi

OWNSRC=""
if [[ -z "${EJDB_ROOT}" ]]; then
  OWNSRC="yes"
  EJDB_ROOT=`mktemp -d`
  echo "Source root: ${EJDB_ROOT}"
  echo "Checkout https://github.com/Softmotions/ejdb"
  git clone "https://github.com/Softmotions/ejdb" "${EJDB_ROOT}"
  (cd "${EJDB_ROOT}" && git submodule update --init)
else
  echo "Source root: ${EJDB_ROOT}"
fi

LIBDIR="${INSTALLPATH}/lib"
INCDIR="${INSTALLPATH}/include"
PLATFORMS="SIMULATOR64 OS64"
BUILD_ROOT="${EJDB_ROOT}/build-xcode"
INSTALL_ROOT="${EJDB_ROOT}/install-xcode"

for PLATFORM in ${PLATFORMS}; do
  rm -rf "${BUILD_ROOT}"
  mkdir -p "${BUILD_ROOT}"
  INSTALL_PREFIX="${INSTALL_ROOT}/${PLATFORM}"
  rm -rf "${INSTALL_PREFIX}"
  cd "${BUILD_ROOT}"
  cmake .. \
        -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_HTTP=OFF \
        -DCMAKE_TOOLCHAIN_FILE="${EJDB_ROOT}/ios-tc.cmake" \
        -DPLATFORM="${PLATFORM}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"
  cmake --build . --target install;
done

rm -rf "${LIBDIR}"
rm -rf "${INCDIR}"

mkdir -p "${INCDIR}"
mkdir -p "${LIBDIR}/IOS"

lipo -create "${INSTALL_ROOT}/SIMULATOR64/lib/libejdb2-2.a" \
             "${INSTALL_ROOT}/OS64/lib/libejdb2-2.a" \
              -o "${LIBDIR}/IOS/libejdb2-2.a"

lipo -create "${INSTALL_ROOT}/SIMULATOR64/lib/libiowow-1.a" \
             "${INSTALL_ROOT}/OS64/lib/libiowow-1.a" \
              -o "${LIBDIR}/IOS/libiowow-1.a"

cp -R "${INSTALL_ROOT}/OS64/include" "${INSTALLPATH}"

rm -rf "${BUILD_ROOT}"
rm -rf "${INSTALL_ROOT}"
test -z "${OWNSRC}" || rm -rf "${EJDB_ROOT}"
