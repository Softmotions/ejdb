/*************************************************************************************************
 * System-dependent configurations of Tokyo Cabinet
 *                                                               Copyright (C) 2006-2012 FAL Labs
 * This file is part of Tokyo Cabinet.
 * Tokyo Cabinet is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Cabinet is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Cabinet; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#ifndef _MYCONF_H                        // duplication check
#define _MYCONF_H



/*************************************************************************************************
 * system discrimination
 *************************************************************************************************/


#if defined(__linux__)

#define _SYS_LINUX_
#define TCSYSNAME   "Linux"

#elif defined(__FreeBSD__)

#define _SYS_FREEBSD_
#define TCSYSNAME   "FreeBSD"

#elif defined(__NetBSD__)

#define _SYS_NETBSD_
#define TCSYSNAME   "NetBSD"

#elif defined(__OpenBSD__)

#define _SYS_OPENBSD_
#define TCSYSNAME   "OpenBSD"

#elif defined(__sun__) || defined(__sun)

#define _SYS_SUNOS_
#define TCSYSNAME   "SunOS"

#elif defined(__hpux)

#define _SYS_HPUX_
#define TCSYSNAME   "HP-UX"

#elif defined(__osf)

#define _SYS_TRU64_
#define TCSYSNAME   "Tru64"

#elif defined(_AIX)

#define _SYS_AIX_
#define TCSYSNAME   "AIX"

#elif defined(__APPLE__) && defined(__MACH__)

#define _SYS_MACOSX_
#define TCSYSNAME   "Mac OS X"

#elif defined(_MSC_VER)

#define _SYS_MSVC_
#define TCSYSNAME   "Windows (VC++)"

#elif defined(_WIN32)

#define _SYS_MINGW_
#define TCSYSNAME   "Windows (MinGW)"

#elif defined(__CYGWIN__)

#define _SYS_CYGWIN_
#define TCSYSNAME   "Windows (Cygwin)"

#else

#define _SYS_GENERIC_
#define TCSYSNAME   "Generic"

#endif



/*************************************************************************************************
 * common settings
 *************************************************************************************************/

#ifdef __cplusplus
#define EJDB_EXTERN_C_START extern "C" {
#define EJDB_EXTERN_C_END }
#else
#define EJDB_EXTERN_C_START
#define EJDB_EXTERN_C_END
#endif

#ifdef __GNUC__
#define EJDB_INLINE static __inline__
#define EJDB_EXPORT
#else
#define EJDB_INLINE static
#ifdef EJDB_STATIC_BUILD
#define EJDB_EXPORT
#elif defined(MONGO_DLL_BUILD)
#define EJDB_EXPORT __declspec(dllexport)
#else
#define EJDB_EXPORT __declspec(dllimport)
#endif
#endif


#if defined(NDEBUG)
#define TCDODEBUG(TC_expr) \
  do { \
  } while(false)
#else
#define TCDODEBUG(TC_expr) \
  do { \
    TC_expr; \
  } while(false)
#endif

#define TCSWAB16(TC_num) \
  ( \
   ((TC_num & 0x00ffU) << 8) | \
   ((TC_num & 0xff00U) >> 8) \
  )

#define TCSWAB32(TC_num) \
  ( \
   ((TC_num & 0x000000ffUL) << 24) | \
   ((TC_num & 0x0000ff00UL) << 8) | \
   ((TC_num & 0x00ff0000UL) >> 8) | \
   ((TC_num & 0xff000000UL) >> 24) \
  )

#define TCSWAB64(TC_num) \
  ( \
   ((TC_num & 0x00000000000000ffULL) << 56) | \
   ((TC_num & 0x000000000000ff00ULL) << 40) | \
   ((TC_num & 0x0000000000ff0000ULL) << 24) | \
   ((TC_num & 0x00000000ff000000ULL) << 8) | \
   ((TC_num & 0x000000ff00000000ULL) >> 8) | \
   ((TC_num & 0x0000ff0000000000ULL) >> 24) | \
   ((TC_num & 0x00ff000000000000ULL) >> 40) | \
   ((TC_num & 0xff00000000000000ULL) >> 56) \
  )

#if defined(_MYBIGEND) || defined(_MYSWAB)
#define TCBIGEND       1
#define TCHTOIS(TC_num)   TCSWAB16(TC_num)
#define TCHTOIL(TC_num)   TCSWAB32(TC_num)
#define TCHTOILL(TC_num)  TCSWAB64(TC_num)
#define TCITOHS(TC_num)   TCSWAB16(TC_num)
#define TCITOHL(TC_num)   TCSWAB32(TC_num)
#define TCITOHLL(TC_num)  TCSWAB64(TC_num)
#else
#define TCBIGEND       0
#define TCHTOIS(TC_num)   (TC_num)
#define TCHTOIL(TC_num)   (TC_num)
#define TCHTOILL(TC_num)  (TC_num)
#define TCITOHS(TC_num)   (TC_num)
#define TCITOHL(TC_num)   (TC_num)
#define TCITOHLL(TC_num)  (TC_num)
#endif

#if defined(_MYNOUBC) || defined(__hppa__)
#define TCUBCACHE      0
#elif defined(_SYS_LINUX_) || defined(_SYS_FREEBSD_) || defined(_SYS_NETBSD_) || \
  defined(_SYS_MACOSX_) || defined(_SYS_SUNOS_)
#define TCUBCACHE      1
#else
#define TCUBCACHE      0
#endif

#if defined(_MYNOZLIB)
#define TCUSEZLIB      0
#else
#define TCUSEZLIB      1
#endif

#if defined(_MYBZIP)
#define TCUSEBZIP      1
#else
#define TCUSEBZIP      0
#endif

#if defined(_MYEXLZMA)
#define TCUSEEXLZMA    1
#else
#define TCUSEEXLZMA    0
#endif

#if defined(_MYEXLZO)
#define TCUSEEXLZO     1
#else
#define TCUSEEXLZO     0
#endif

#if defined(_MYNOPTHREAD)
#define TCUSEPTHREAD   0
#else
#define TCUSEPTHREAD   1
#endif

#if defined(_MYMICROYIELD)
#define TCMICROYIELD   1
#else
#define TCMICROYIELD   0
#endif

#define MYMALLOC       malloc
#define MYCALLOC       calloc
#define MYREALLOC      realloc
#define MYFREE         free



/*************************************************************************************************
 * general headers
 *************************************************************************************************/


#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <dirent.h>
#include <regex.h>
#include <glob.h>

#if TCUSEPTHREAD
#include <pthread.h>
#if defined(_POSIX_PRIORITY_SCHEDULING)
#include <sched.h>
#endif
#endif



/*************************************************************************************************
 * miscellaneous hacks
 *************************************************************************************************/


#if defined(__GNUC__)
#define _alignof(TC_a) ((size_t)__alignof__(TC_a))
#else
#define _alignof(TC_a) sizeof(TC_a)
#endif
#define _issigned(TC_a)  ((TC_a)-1 < 1 ? true : false)
#define _maxof(TC_a) \
 ((TC_a)(sizeof(TC_a) == sizeof(int64_t) ? _issigned(TC_a) ? INT64_MAX : UINT64_MAX : \
   sizeof(TC_a) == sizeof(int32_t) ? _issigned(TC_a) ? INT32_MAX : UINT32_MAX : \
   sizeof(TC_a) == sizeof(int16_t) ? _issigned(TC_a) ? INT16_MAX : UINT16_MAX : \
   _issigned(TC_a) ? INT8_MAX : UINT8_MAX))

#if defined(_SYS_FREEBSD_) || defined(_SYS_NETBSD_) || defined(_SYS_OPENBSD_)
#define nan(TC_a)      strtod("nan", NULL)
#define nanl(TC_a)     ((long double)strtod("nan", NULL))
#endif

#if ! defined(PATH_MAX)
#if defined(MAXPATHLEN)
#define PATH_MAX       MAXPATHLEN
#else
#define PATH_MAX       4096
#endif
#endif
#if ! defined(NAME_MAX)
#define NAME_MAX       255
#endif

extern int _tc_dummy_cnt;

int _tc_dummyfunc(void);

int _tc_dummyfuncv(int a, ...);

/* MAX and MIN are defined in a really funky place in Solaris. */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/*************************************************************************************************
 * notation of filesystems
 *************************************************************************************************/


#define MYPATHCHR       '/'
#define MYPATHSTR       "/"
#define MYEXTCHR        '.'
#define MYEXTSTR        "."
#define MYCDIRSTR       "."
#define MYPDIRSTR       ".."



/*************************************************************************************************
 * for ZLIB
 *************************************************************************************************/


enum {
  _TCZMZLIB,
  _TCZMRAW,
  _TCZMGZIP
};


extern char *(*_tc_deflate)(const char *, int, int *, int);

extern char *(*_tc_inflate)(const char *, int, int *, int);

extern unsigned int (*_tc_getcrc)(const char *, int);



/*************************************************************************************************
 * for BZIP2
 *************************************************************************************************/


extern char *(*_tc_bzcompress)(const char *, int, int *);

extern char *(*_tc_bzdecompress)(const char *, int, int *);



/*************************************************************************************************
 * for test of custom codec functions
 *************************************************************************************************/


void *_tc_recencode(const void *ptr, int size, int *sp, void *op);

void *_tc_recdecode(const void *ptr, int size, int *sp, void *op);



/*************************************************************************************************
 * for POSIX thread disability
 *************************************************************************************************/


#if ! TCUSEPTHREAD

#define pthread_t                        intptr_t

#define pthread_once_t                   intptr_t
#undef PTHREAD_ONCE_INIT
#define PTHREAD_ONCE_INIT                0
#define pthread_once(TC_a, TC_b)         _tc_dummyfuncv((intptr_t)(TC_a), (TC_b))

#define pthread_mutexattr_t              intptr_t
#undef PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_RECURSIVE          0
#define pthread_mutexattr_init(TC_a)     _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_mutexattr_destroy(TC_a)  _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_mutexattr_settype(TC_a, TC_b)  _tc_dummyfuncv((intptr_t)(TC_a), (TC_b))

#define pthread_mutex_t                  intptr_t
#undef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER        0
#define pthread_mutex_init(TC_a, TC_b)   _tc_dummyfuncv((intptr_t)(TC_a), (TC_b))
#define pthread_mutex_destroy(TC_a)      _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_mutex_lock(TC_a)         _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_mutex_unlock(TC_a)       _tc_dummyfuncv((intptr_t)(TC_a))

#define pthread_rwlock_t                 intptr_t
#undef PTHREAD_RWLOCK_INITIALIZER
#define PTHREAD_RWLOCK_INITIALIZER       0
#define pthread_rwlock_init(TC_a, TC_b)  _tc_dummyfuncv((intptr_t)(TC_a), (TC_b))
#define pthread_rwlock_destroy(TC_a)     _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_rwlock_rdlock(TC_a)      _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_rwlock_wrlock(TC_a)      _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_rwlock_unlock(TC_a)      _tc_dummyfuncv((intptr_t)(TC_a))

#define pthread_key_t                    intptr_t
#define pthread_key_create(TC_a, TC_b)   _tc_dummyfuncv((intptr_t)(TC_a), (TC_b))
#define pthread_key_delete(TC_a)         _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_setspecific(TC_a, TC_b)  _tc_dummyfuncv((intptr_t)(TC_a))
#define pthread_getspecific(TC_a)        _tc_dummyfuncv((intptr_t)(TC_a))

#define pthread_create(TC_th, TC_attr, TC_func, TC_arg) \
  (*(TC_th) = 0, (TC_func)(TC_arg), 0)
#define pthread_join(TC_th, TC_rv)       (*(TC_rv) = NULL, 0)
#define pthread_detach(TC_th)            0
#define sched_yield()                    _tc_dummyfunc()

#endif

#if TCUSEPTHREAD && TCMICROYIELD
#define TCTESTYIELD() \
  do { \
    if(((++_tc_dummy_cnt) & (0x20 - 1)) == 0){ \
      sched_yield(); \
      if(_tc_dummy_cnt > 0x1000) _tc_dummy_cnt = (uint32_t)time(NULL) % 0x1000; \
    } \
  } while(false)
#undef assert
#define assert(TC_expr) \
  do { \
    if(!(TC_expr)){ \
      fprintf(stderr, "assertion failed: %s\n", #TC_expr); \
      abort(); \
    } \
    TCTESTYIELD(); \
  } while(false)
#define if(TC_cond) \
  if((((++_tc_dummy_cnt) & (0x100 - 1)) != 0 || (sched_yield() * 0) == 0) && (TC_cond))
#define while(TC_cond) \
  while((((++_tc_dummy_cnt) & (0x100 - 1)) != 0 || (sched_yield() * 0) == 0) && (TC_cond))
#else
#define TCTESTYIELD() \
  do { \
  } while(false)
#endif

#if !defined(_POSIX_PRIORITY_SCHEDULING) && TCUSEPTHREAD
#define sched_yield()                    usleep(1000 * 20)
#endif



/*************************************************************************************************
 * utilities for implementation
 *************************************************************************************************/


#define TCNUMBUFSIZ    32                // size of a buffer for a number

/* set a buffer for a variable length number */
#define TCSETVNUMBUF(TC_len, TC_buf, TC_num) \
  do { \
    int _TC_num = (TC_num); \
    if(_TC_num == 0){ \
      ((signed char *)(TC_buf))[0] = 0; \
      (TC_len) = 1; \
    } else { \
      (TC_len) = 0; \
      while(_TC_num > 0){ \
        int _TC_rem = _TC_num & 0x7f; \
        _TC_num >>= 7; \
        if(_TC_num > 0){ \
          ((signed char *)(TC_buf))[(TC_len)] = -_TC_rem - 1; \
        } else { \
          ((signed char *)(TC_buf))[(TC_len)] = _TC_rem; \
        } \
        (TC_len)++; \
      } \
    } \
  } while(false)

/* set a buffer for a variable length number of 64-bit */
#define TCSETVNUMBUF64(TC_len, TC_buf, TC_num) \
  do { \
    long long int _TC_num = (TC_num); \
    if(_TC_num == 0){ \
      ((signed char *)(TC_buf))[0] = 0; \
      (TC_len) = 1; \
    } else { \
      (TC_len) = 0; \
      while(_TC_num > 0){ \
        int _TC_rem = _TC_num & 0x7f; \
        _TC_num >>= 7; \
        if(_TC_num > 0){ \
          ((signed char *)(TC_buf))[(TC_len)] = -_TC_rem - 1; \
        } else { \
          ((signed char *)(TC_buf))[(TC_len)] = _TC_rem; \
        } \
        (TC_len)++; \
      } \
    } \
  } while(false)

/* read a variable length buffer */
#define TCREADVNUMBUF(TC_buf, TC_num, TC_step) \
  do { \
    TC_num = 0; \
    int _TC_base = 1; \
    int _TC_i = 0; \
    while(true){ \
      if(((signed char *)(TC_buf))[_TC_i] >= 0){ \
        TC_num += ((signed char *)(TC_buf))[_TC_i] * _TC_base; \
        break; \
      } \
      TC_num += _TC_base * (((signed char *)(TC_buf))[_TC_i] + 1) * -1; \
      _TC_base <<= 7; \
      _TC_i++; \
    } \
    (TC_step) = _TC_i + 1; \
  } while(false)

/* read a variable length buffer */
#define TCREADVNUMBUF64(TC_buf, TC_num, TC_step) \
  do { \
    TC_num = 0; \
    long long int _TC_base = 1; \
    int _TC_i = 0; \
    while(true){ \
      if(((signed char *)(TC_buf))[_TC_i] >= 0){ \
        TC_num += ((signed char *)(TC_buf))[_TC_i] * _TC_base; \
        break; \
      } \
      TC_num += _TC_base * (((signed char *)(TC_buf))[_TC_i] + 1) * -1; \
      _TC_base <<= 7; \
      _TC_i++; \
    } \
    (TC_step) = _TC_i + 1; \
  } while(false)

/* calculate the size of a buffer for a variable length number */
#define TCCALCVNUMSIZE(TC_num) \
  ((TC_num) < 0x80 ? 1 : (TC_num) < 0x4000 ? 2 : (TC_num) < 0x200000 ? 3 : \
   (TC_num) < 0x10000000 ? 4 : 5)

#define TCCMPLEXICAL(TC_rv, TC_aptr, TC_asiz, TC_bptr, TC_bsiz) \
 do { \
    (TC_rv) = 0; \
    int _TC_min = (TC_asiz) < (TC_bsiz) ? (TC_asiz) : (TC_bsiz); \
    for(int _TC_i = 0; _TC_i < _TC_min; _TC_i++){ \
      if(((unsigned char *)(TC_aptr))[_TC_i] != ((unsigned char *)(TC_bptr))[_TC_i]){ \
        (TC_rv) = ((unsigned char *)(TC_aptr))[_TC_i] - ((unsigned char *)(TC_bptr))[_TC_i]; \
        break; \
      } \
    } \
    if((TC_rv) == 0) (TC_rv) = (TC_asiz) - (TC_bsiz); \
  } while(false)

#endif                                   // duplication check


//EJDB Shared

#define JDBIDKEYNAME "_id"  /**> Name of PK _id field in BSONs */
#define JDBIDKEYNAMEL 3


// END OF FILE
