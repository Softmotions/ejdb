#!/bin/sh

DEFINE_VAR=$1
CC=${CC:-cc}

usage() {
  echo "Usage test_blocks.sh <DEFINE_VAR>"
  exit 1
}

[ -n "$DEFINE_VAR" ] || usage

cat > "test_blocks.c" <<'EOF'
int main() {
  void (^block)(void) = ^{ };
  block();
  return 0;
}
EOF

if ${CC} -Werror -fblocks -lBlocksRuntime ./test_blocks.c -o ./test_blocks; then
  autark set "$DEFINE_VAR=1"
fi

autark env CC
