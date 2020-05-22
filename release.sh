#!/bin/bash

set -e
# set -x

SCRIPTPATH="$(
  cd "$(dirname "$0")"
  pwd -P
)"
cd $SCRIPTPATH


readme() {
  echo "Generating README.md";
  cat "./BASE.md" > "./README.md"
  echo -e "\n\n" >> "./README.md"
  cat "./src/bindings/ejdb2_android/README.md" >> "./README.md"
  echo -e "\n\n" >> "./README.md"
  # cat "./src/bindings/ejdb2_swift/EJDB2Swift/README.md" >> "./README.md"
  # echo -e "\n\n" >> "./README.md"
  cat "./src/jql/README.md" >> "./README.md"
  echo -e "\n\n" >> "./README.md"
  cat "./src/jbr/README.md" >> "./README.md"
  echo -e "\n\n" >> "./README.md"
  cat "./docker/README.md" >> "./README.md"
  echo -e "\n\n" >> "./README.md"
  cat "./CAPI.md" >> "./README.md"
  echo -e '\n# License\n```\n' >> "./README.md"
  cat "./LICENSE" >> "./README.md"
  echo -e '\n```\n' >> "./README.md"
}

release_tag() {
  echo "Creating EJDB2 release"
  readme

  git pull origin master
  dch --distribution testing --no-force-save-on-release --release "" -c ./Changelog
  VERSION=`dpkg-parsechangelog -l./Changelog -SVersion`
  TAG="v${VERSION}"
  CHANGESET=`dpkg-parsechangelog -l./Changelog -SChanges | sed '/^ejdb2.*/d' | sed '/^\s*$/d'`
  git add ./Changelog
  git add ./README.md

  if ! git diff-index --quiet HEAD --; then
    git commit -a -m"${TAG} landed"
    git push origin master
  fi

  echo "${CHANGESET}" | git tag -f -a -F - "${TAG}"
  git push origin -f --tags
}

while [ "$1" != "" ]; do
  case $1 in
    "-d"  )  readme
             exit
             ;;
    "-r" )   release_tag
             exit
             ;;
  esac
  shift
done


