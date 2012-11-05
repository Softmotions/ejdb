#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef EJDB_THREAD_H
#define	EJDB_THREAD_H

#ifdef _WIN32
#include "win32/pthread_mutex.h"
#else
#include <pthread.h>
#endif

namespace ejdb {
    /**
     * Mutex
     */
    class EJMutex {
        pthread_mutex_t     cs;
        volatile bool       initialized;
      public:
        EJMutex() {
            pthread_mutex_init(&cs, NULL);
            initialized = true;
        }
        ~EJMutex() {
            pthread_mutex_destroy(&cs);
            initialized = false;
        }
        bool IsInitialized() {
            return initialized;
        }
        void Lock() {
            if (initialized) {
                pthread_mutex_lock(&cs);
            }
        }
        void Unlock() {
            if (initialized) {
                pthread_mutex_unlock(&cs);
            }
        }
    };

    /**
     * Stack based mutex locking
     */
    class EJCriticalSection {
      private:
        EJMutex& mutex;
      public:
        EJCriticalSection(EJMutex& guard) : mutex(guard) {
            mutex.Lock();
        }
        ~EJCriticalSection() {
            mutex.Unlock();
        }
    };
}



#endif	/* EJDB_THREAD_H */

