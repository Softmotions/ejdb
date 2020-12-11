#!/bin/sh

set -e

SCRIPTPATH="$(
  cd "$(dirname "$0")"
  pwd -P
)"
cd $SCRIPTPATH

VERSION="@EJDB2_JNI_VERSION@"
DEPLOY_JAR_FILE="@DEPLOY_JAR_FILE@"
DEPLOY_SOURCES_JAR_FILE="@SOURCES_JAR_FILE@"

mvn org.apache.maven.plugins:maven-deploy-plugin:3.0.0-M1:deploy-file \
  -DgroupId=softmotions \
  -DartifactId=ejdb2 \
  -Dversion=${VERSION} \
  -Dpackaging=jar \
  -Dfile=${DEPLOY_JAR_FILE} \
  -Dsources=${DEPLOY_SOURCES_JAR_FILE} \
  -DrepositoryId=repsy_repository \
  -DgeneratePom=true \
  -Durl=https://repo.repsy.io/mvn/adamansky/softmotions
