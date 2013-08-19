#!/bin/bash

DEBUG=true
EJDB_HOME=$(cd $(dirname ${0})/..; pwd -P)
PPA=ppa:adamansky/ejdb

function error() { echo "ERROR $*"; }

function tcejdb() {
    TCEJDB_HOME="${EJDB_HOME}/tcejdb"
    VERSION="$1"
    if [ -z ${VERSION} ]; then
       error "$VERSION";
    fi
    cd ${TCEJDB_HOME}
    autoconf
    CPKG=$(cat configure.ac | grep AC_INIT| sed 's/.*(\(.*\)).*/\1/'| awk -F, '{print $1}')
    CVERSION=$(cat configure.ac | grep AC_INIT| sed 's/.*(\(.*\)).*/\1/'| awk -F, '{print $2}');
    if [[ ${VERSION} != ${CVERSION} ]]; then
        echo "Updating configure.ac version"
        sed -i 's/AC_INIT( *'"$CPKG"' *, *'"$CVERSION"' */AC_INIT('"$CPKG"','"$VERSION"'/' configure.ac
        autoconf
        dch -Dtesting -v"${VERSION}" || exit $?
        make -C "${EJDB_HOME}" deb-source-packages-tcejdb
        cd ${EJDB_HOME}
	dput -c tools/dput-raring.cf "${PPA}" "./libtcejdb_${VERSION}_source.changes"
    else
        echo "The configure.ac version up-to-date"
        exit 0;
    fi

}


tcejdb $*
