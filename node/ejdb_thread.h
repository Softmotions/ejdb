/**************************************************************************************************
 *  EJDB database library http://ejdb.org
 *  Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
 *
 *  This file is part of EJDB.
 *  EJDB is free software; you can redistribute it and/or modify it under the terms of
 *  the GNU Lesser General Public License as published by the Free Software Foundation; either
 *  version 2.1 of the License or any later version.  EJDB is distributed in the hope
 *  that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *  You should have received a copy of the GNU Lesser General Public License along with EJDB;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA.
 *************************************************************************************************/

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

