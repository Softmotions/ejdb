#!/bin/sh

TCEJDB_HOME=$(cd $(dirname ${0})/..; pwd -P)
MXE_HOME=$(cd ${1}; pwd -P)

(test -d ${MXE_HOME}/usr/bin) || { echo "Invalid MXE_HOME: ${1}"; exit 1; }
shift

if [ "${1}" = "w32" ]; then
    HOST="i686-w64-mingw32"
elif [ "${1}" = "w64" ]; then
    HOST="x86_64-w64-mingw32"
else
    echo "Unknown target: ${1}. Must be one of: w32|w64";
    exit 1;
fi
shift
CONFLAGS="${1}"

echo "TCEJDB_HOME=${TCEJDB_HOME}"
echo "MXE_HOME=${MXE_HOME}"
echo "HOST=${HOST}"

PREFIX=${TCEJDB_HOME}/${HOST}
export PATH=${MXE_HOME}/usr/bin:${MXE_HOME}/usr/${HOST}/bin:${PATH};
unset CC CFLAGS CPPFLAGS LDFLAGS LD_LIBRARY_PATH

cd ${TCEJDB_HOME}

WINTOOLS=${TCEJDB_HOME}/tools/win32
if [ ! -d ${WINTOOLS} ] || [ ! -r  ${WINTOOLS}/lib.exe ]; then
  which wine || { echo "'wine' is not found! Please install 'wine'"; exit 1; }
  which wget || { echo "'wget' is not found! Please install 'wget'"; exit 1; }
  mkdir -p ${WINTOOLS} || exit 1
  wget -O ${WINTOOLS}/lib.exe https://dl.dropboxusercontent.com/u/4709222/windev/lib.exe || exit 1
  wget -O ${WINTOOLS}/link.exe https://dl.dropboxusercontent.com/u/4709222/windev/link.exe || exit 1
  wget -O ${WINTOOLS}/mspdb100.dll https://dl.dropboxusercontent.com/u/4709222/windev/mspdb100.dll || exit 1
fi

export WINEDEBUG=fixme-all

set -x
autoconf
./configure --host="${HOST}" --prefix=${PREFIX} ${CONFLAGS}
make clean
make
make MXE=${MXE_HOME} win-archive
make -C testejdb/ all

