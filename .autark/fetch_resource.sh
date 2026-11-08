#!/bin/sh
set -e

usage() {
  echo "Usage: $0 <project_url> <target_dir> [target_dir_var] [n_strip_dirs]" >&2
  exit 1
}

[ -z "$1" ] && usage
[ -z "$2" ] && usage

PROJECT_URL="$1"
TARGET_DIR="$2"
TARGET_VAR="$3"
NSTRIP="$4"

# Resolves the actual fetch dir. Autark keeps a build wide registry of already
# fetched resources, so a resource requested by several parts of the build is
# fetched exactly once. The registry dir is also shared by nested builds.
FETCH_PENDING=3
FETCH_REJECTED=4
PLAN_RC=0
PLAN_DIR="$(autark fetch-plan "$PROJECT_URL" "$TARGET_DIR")" || PLAN_RC=$?
case "$PLAN_RC" in
  0)
    # Same resource was fetched by another part of the build: just reuse it.
    TARGET_DIR="$PLAN_DIR"
    echo "Using already fetched $PROJECT_URL at $TARGET_DIR" >&2
    if [ -n "$TARGET_VAR" ]; then
      autark set "$TARGET_VAR=$TARGET_DIR"
    fi
    autark fetched "$PROJECT_URL" "$TARGET_DIR"
    exit 0
    ;;
  $FETCH_PENDING)
    TARGET_DIR="$PLAN_DIR"
    ;;
  $FETCH_REJECTED)
    exit "$PLAN_RC"
    ;;
  *)
    # Autark binary does not support 'fetch-plan': use the legacy behavior
    echo "Warning: autark fetch-plan is not supported, fetching $PROJECT_URL as is" >&2
    TARGET_DIR="$2"
    ;;
esac

rm -rf "$TARGET_DIR"
mkdir -p "$TARGET_DIR"

download_file() {
  URL="$1"
  DEST="$2"
  FILE=${URL#file://}

  if [ "$FILE" != "$URL" ]; then
    cp -f $FILE $DEST
  elif command -v curl >/dev/null 2>&1; then
    curl -L "$URL" -o "$DEST"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$DEST" "$URL"
  else
    echo "Error: neither wget nor curl is available on the system" >&2
    exit 1
  fi
}

case "$PROJECT_URL" in
  https://* | http://* | file://*)
    echo "Downloading an archive..."
    TMP_DIR="$(mktemp -d)"
    ARCHIVE="$TMP_DIR/archive"

    # Determine and extension
    case "$PROJECT_URL" in
      *.tar.gz|*.tgz)
        EXT="tar.gz"
        ;;
      *.tar.xz)
        EXT="tar.xz"
        ;;
      *.zip)
        EXT="zip"
        ;;
      *)
        echo "Unsupported archive format: $PROJECT_URL" >&2
        exit 1
        ;;
    esac

    download_file "$PROJECT_URL" "$ARCHIVE.$EXT"

    [ -z "$NSTRIP" ] && NSTRIP=1

    case "$EXT" in
      tar.gz)
        tar -xzf "$ARCHIVE.$EXT" -C "$TARGET_DIR" --strip-components=$NSTRIP
        ;;
      tar.xz)
        tar -xJf "$ARCHIVE.$EXT" -C "$TARGET_DIR" --strip-components=$NSTRIP
        ;;
      zip)
        unzip -q "$ARCHIVE.$EXT" -d "$TMP_DIR/unzipped"
        if [ "$NSTRIP" != "0" ]; then
          # Flatten if single top-level directory
          SRC_DIR="$TMP_DIR/unzipped"
          if [ "$(find "$SRC_DIR" -mindepth 1 -maxdepth 1 | wc -l)" -eq 1 ]; then
            FIRST_CHILD="$(find "$SRC_DIR" -mindepth 1 -maxdepth 1)"
            if [ -d "$FIRST_CHILD" ]; then
              SRC_DIR="$FIRST_CHILD"
            fi
          fi
        fi
        cp -a "$SRC_DIR"/. "$TARGET_DIR"/
        ;;
    esac

    rm -rf "$TMP_DIR"
    ;;

  dir://*)
    SRC_DIR="${PROJECT_URL#dir://}"
    if [ ! -d "$SRC_DIR" ]; then
      echo "Error: Source directory does not exist: $SRC_DIR" >&2
      exit 1
    fi
    echo "Copying directory from file system..."
    mkdir -p "$TARGET_DIR"
    cp -a "$SRC_DIR"/. "$TARGET_DIR"/
    ;;

  *)
    echo "Cloning Git repository..."
    git clone --depth=1 "$PROJECT_URL" "$TARGET_DIR"
    ;;
esac

if [ -n "$TARGET_VAR" ]; then
  autark set "$TARGET_VAR=$TARGET_DIR"
fi

# A fresh fetch owns the dir: drop the marker of the sources it was made from
rm -f "$TARGET_DIR/.autark-fetch-dep"

autark fetched "$PROJECT_URL" "$TARGET_DIR"