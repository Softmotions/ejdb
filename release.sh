#!/bin/sh

set -e
set -x

cd `readlink -f "$0" | xargs dirname`
git pull
dch --distribution testing --no-force-save-on-release --release "" -c ./Changelog
VERSION=`dpkg-parsechangelog -l./Changelog -SVersion`
TAG="v${VERSION}"
CHANGESET=`dpkg-parsechangelog -l./Changelog -SChanges | sed '/^ejdb.*/d' | sed '/^\s*$/d'`
git add ./Changelog

if ! git diff-index --quiet HEAD --; then
  git commit -m"${TAG} landed"
  git push origin master
fi

echo "${CHANGESET}" | git tag -f -a -F - "${TAG}"
git push -f --tags

