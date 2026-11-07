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

  CHANGELOG=./CHANGELOG.md
  VERSION=$(grep -m1 -o '\[v[0-9][^]]*\]' "$CHANGELOG" | sed 's/\[v//;s/\]//')
  TAG="v${VERSION}"

  # Release notes: body of the Markdown section started by the "[${TAG}]" header
  CHANGESET=$(awk -v tag="$TAG" '
  /^[[:space:]]*#[#]?[[:space:]]+/ && index($0, "[" tag "]") {
    inside = 1
    next
  }
  inside && /^[[:space:]]*#[#]?[[:space:]]+/ {
    exit
  }
  inside {
    lines[++n] = $0
  }
  END {
    while (n > 0 && lines[n] ~ /^[[:space:]]*$/) {
      n--
    }
    i = 1
    while (i <= n && lines[i] ~ /^[[:space:]]*$/) {
      i++
    }
    for (; i <= n; i++) {
      print lines[i]
    }
  }
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