#!/bin/sh

cat > './test_pthread.c' << 'EOF'
#include <pthread.h>
int main(void) {
  pthread_t t;
  return 0;
}
EOF

cat > './test_pthread_spinlock.c' << 'EOF'
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
int main(void) {
  pthread_spinlock_t lock;
  if (pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE) != 0) {
    return 1;
  }
  pthread_spin_destroy(&lock);
  return 0;
}
EOF

CC=${CC:-cc}

if ${CC} ./test_pthread.c -pthread; then
  PTHREAD_LFLAG=-pthread
  autark set "PTHREAD_LFLAG=$PTHREAD_LFLAG"
elif ${CC} ./test_pthread.c -lpthread; then
  PTHREAD_LFLAG=-lpthread
  autark set "PTHREAD_LFLAG=$PTHREAD_LFLAG"
fi

if [ -n "${PTHREAD_LFLAG}" ]; then
  if ${CC} ./test_pthread_spinlock.c ${PTHREAD_LFLAG}; then
    autark set "PTHREAD_HAS_SPINLOCK=1"
  fi
fi

autark env CC