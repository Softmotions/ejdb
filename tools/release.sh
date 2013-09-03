#!/bin/bash

DEBUG=true
EJDB_HOME=$(cd $(dirname ${0})/..; pwd -P)
PPA=ppa:adamansky/ejdb

function error() { echo "ERROR $*"; }

function usage() {
cat << EOF
    Usage: release.sh old|new|<version> [command, ...]

    Commands:
        dput - Perform PPA dput
        npm - Publish package to npmjs.org
        w32 - Perform w32 build
        w64 - Perform w64 build

EOF
    exit 1;
}

function _npm() {
    echo "Building npm package";
    cd ${EJDB_HOME}
    sed -i 's/\"version\" *\: *.*\,/"version" : \"'"$1"'\"\,/' ./package.json
    sed -r -i 's/tcejdb-[[:digit:]]+\.[[:digit:]]+\.[[:digit:]]+-/tcejdb-'"$1"'-/' ./package.json || exit $?
    make clean
    ##npm publish || exit $?
    exit 0;
}

function _dput() {
   echo "Building debian packages";
   cd "${EJDB_HOME}"
   rm -f *.upload
   rm -f libtcejdb*.tar.gz libtcejdb*.tgz libtcejdb*.deb libtcejdb*.changes libtcejdb*.build libtcejdb*.dsc
   make -C "${EJDB_HOME}" deb-source-packages-tcejdb
   #dput "${VERSION}"
   echo "Uploading into RARING"
   dput -c tools/dput-raring.cf "${PPA}" "./libtcejdb_${1}_source.changes" || exit $?
   rm -f *.upload
   echo "Uploading into QUANTAL"
   dput -c tools/dput-quantal.cf "${PPA}" "./libtcejdb_${1}_source.changes" || exit $?
   exit 0;
}

function _win() {
  echo "Building ${1} binaries"
  cd ${EJDB_HOME}
  ${EJDB_HOME}/tcejdb/mxe/mxe-build.sh ../../mxe $1 || exit $?
  #tcejdb-1.1.22-mingw32-i686.zip
  if [ "${1}" == "w32" ]; then
    cp ${EJDB_HOME}/tcejdb/tcejdb-${2}-mingw32-i686.zip ${HOME}/Dropbox/Public/ejdb
  elif [ "${1}" == "w64" ]; then
    cp ${EJDB_HOME}/tcejdb/tcejdb-${2}-mingw32-x86_64.zip ${HOME}/Dropbox/Public/ejdb
  fi
  exit 0;
}

function tcejdb() {
    TCEJDB_HOME="${EJDB_HOME}/tcejdb"
    VERSION="$1"
    shift;
    CMDS="$1 $2 $3 $4"

    if [[ -z ${VERSION} ]]; then
       VERSION="new";
    fi

    if [[ -z ${CMDS} ]]; then
       VERSION="new dput w32 w64 npm";
    fi

    cd ${TCEJDB_HOME}
    autoconf
    CPKG=$(cat configure.ac | grep AC_INIT| sed 's/.*(\(.*\)).*/\1/'| awk -F, '{print $1}')
    CVERSION=$(cat configure.ac | grep AC_INIT| sed 's/.*(\(.*\)).*/\1/'| awk -F, '{print $2}');

    if [[ ${VERSION} == "new" ]]; then
        which "semver" > /dev/null || {
           echo "Missing 'semver', npm -g install semver";
           exit 1;
        }
        VERSION=`semver -i ${CVERSION}` || {
            echo "Unable to increment version: ${CVERSION}";
            exit 1;
        }
        echo "Version changed: ${CVERSION} => ${VERSION}";

    elif [[ ${VERSION} == "old" ]]; then
        echo "Using old version ${CVERSION}";
        VERSION=${CVERSION};
    fi

    (echo "$VERSION" | grep -E '[[:digit:]]+\.[[:digit:]]+.[[:digit:]]+') > /dev/null || {
        echo "Invalid version: $VERSION";
        exit 1;
    }

    if [[ ${VERSION} != ${CVERSION} ]]; then
        dch -Dtesting -v"${VERSION}" || exit $?
        cp ./debian/changelog ./Changelog || exit $?
        cp ./debian/changelog ../Changelog || exit $?
        echo "Updating configure.ac version"
        sed -i 's/AC_INIT( *'"$CPKG"' *, *'"$CVERSION"' */AC_INIT('"$CPKG"', '"$VERSION"'/' ./configure.ac || exit $?
        touch ./configure.ac
        rm -rf ./configure ./config.status ./config.log ./autom4te.cache
        autoconf -o ./configure ./configure.ac
        (./configure && \
            make clean version && \
            make && \
            make -C ./testejdb all check && \
            make clean) || exit $?
    else
        echo "The configure.ac version up-to-date"
    fi

    for c in ${CMDS}
    do
        case "$c" in
            "dput" )
                (_dput ${VERSION}) || exit $?
            ;;
            "npm" )
                (_npm ${VERSION}) || exit $?
            ;;
            "w32" )
                (_win "w32" ${VERSION}) || exit $?
             ;;
            "w64" )
                (_win "w64" ${VERSION}) || exit $?
            ;;
            * )
                echo "Invalid command";
                usage
            ;;
        esac
    done
}

tcejdb $*
