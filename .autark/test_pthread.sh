#!/bin/sh

cat > './test_pthread.c' << 'EOF'
#include <pthread.h>
int main(void) {
  pthread_t t;
  return 0;
}
EOF

CC=${CC:-cc}

if ${CC} ./test_pthread.c -pthread; then
  autark set "PTHREAD_LFLAG=-pthread"
elif ${CC} ./test_pthread.c -lpthread; then
  autark set "PTHREAD_LFLAG=-lpthread"
fi

autark env CC