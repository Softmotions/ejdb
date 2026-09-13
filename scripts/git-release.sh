#!/bin/bash

set -e
# set -x

cd "$(dirname "$(readlink -f "$0")")/.."

readme() {
  echo "Generating README.md";
  cat "./BASE.md" > "./README.md"
  echo -e "\n\n" >> "./README.md"
  cat "./src/jql/README.md" >> "./README.md"
  echo -e "\n\n" >> "./README.md"
  cat "./src/jbr/README.md" >> "./README.md"
  echo -e "\n\n" >> "./README.md"
  cat "./CAPI.md" >> "./README.md"
  echo -e '\n# License\n```\n' >> "./README.md"
  cat "./LICENSE" >> "./README.md"
  echo -e '\n```\n' >> "./README.md"
}

release_tag() {
  echo "Creating EJDB2 release"
  readme

  CHANGELOG=./Changelog
  VERSION=$(grep -m1 -o '\[v[0-9][^]]*\]' "$CHANGELOG" | sed 's/\[v//;s/\]//')
  TAG="v${VERSION}"

  CHANGESET=$(awk -v tag="$TAG" '
  $0 ~ tag { skip = 1; next }
  skip && NF == 0 { exit }
  skip { print }
  ' "$CHANGELOG" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' )

  git add "$CHANGELOG"

  if ! git diff-index --quiet HEAD --; then
    git commit -m"${TAG} landed"
    git push origin master
  fi

  echo "$CHANGESET" | git tag -f -a -F - "$TAG"
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