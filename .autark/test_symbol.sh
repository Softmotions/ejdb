#!/bin/sh

SYMBOL=$1
HEADER=$2
DEFINE_VAR=$3

usage() {
  echo "Usage test_symbol.sh <SYMBOL> <HEADER> <DEFINE_VAR>"
  exit 1
}

[ -n "$SYMBOL" ] || usage
[ -n "$HEADER" ] || usage
[ -n "$DEFINE_VAR" ] || usage

cat > "test_symbol.c" <<EOF
#include <$HEADER>
int main() {
#ifdef $SYMBOL
  return 0; // Symbol is a macro
#else
  (void)$SYMBOL; // Symbol might be a function or variable
  return 0;
#endif
}
EOF

if ${CC:-cc} -Werror ./test_symbol.c; then
  autark set "$DEFINE_VAR=1"
fi

autark env CC
