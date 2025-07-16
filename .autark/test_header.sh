#!/bin/sh

HEADER=$1
DEFINE_VAR=$2

usage() {
  echo "Usage test_header.sh <HEADER> <DEFINE_VAR>"
  exit 1
}

[ -n "$HEADER" ] || usage
[ -n "$DEFINE_VAR" ] || usage

cat > "test_header.c" <<EOF
#include <$HEADER>
int main() {
  return 0;
}
EOF

if ${CC:-cc} -Werror ./test_header.c; then
  autark set "$DEFINE_VAR=1"
fi

autark env CC
