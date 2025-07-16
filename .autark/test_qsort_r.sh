#!/bin/sh

cat > './test_qsort_r.c' << 'EOF'
#include <stdlib.h>
int main(int argc, char **argv) {
  qsort_r(0, 0, 0, 0, 0);
  return 0;
}
EOF

${CC:=cc} ./test_qsort_r.c

if [ "$?" -eq "0" ]; then
  autark set "HAVE_QSORT_R=1"
else
  echo "No qsort_r found"
fi