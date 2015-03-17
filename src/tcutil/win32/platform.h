/*
 * File:   platform.h
 * Author: adam
 *
 * Created on March 31, 2013, 9:34 PM
 */

#ifndef PLATFORM_H
#define	PLATFORM_H

#include "basedefs.h"
#include <stddef.h>
#include <time.h>
#include <direct.h>
#include <sys/stat.h>

typedef struct {
    size_t gl_pathc; /* Count of paths matched so far  */
    char **gl_pathv; /* List of matched pathnames.  */
    size_t gl_offs; /* Slots to reserve in gl_pathv.  */
} glob_t;

#define GLOB_ERR 1
#define GLOB_MARK 0
#define GLOB_NOSORT 2
#define GLOB_DOOFFS 0
#define GLOB_NOCHECK 0
#define GLOB_APPEND 0
#define GLOB_NOESCAPE 0
#define GLOB_PERIOD 0
#define GLOB_ALTDIRFUNC 0
#define GLOB_BRACE 0
#define GLOB_NOMAGIC 0
#define GLOB_TILDE 0
#define GLOB_TILDE_CHECK 0
#define GLOB_ONLYDIR 4

#define GLOB_NOSPACE -1
#define GLOB_ABORTED -2
#define GLOB_NOMATCH -3

int glob(const char *pattern, int flags, int (*errfunc) (const char *epath, int eerrno), glob_t *pglob);
void globfree(glob_t *pglob);
int win_fstat(HANDLE fh, struct stat *buf);
ssize_t win_pwrite(HANDLE fd, const void *buf, size_t count, off_t offset);
ssize_t win_pread(HANDLE fd, void *buf, size_t size, off_t off);

#define mkdir(a, b) _mkdir(a)
#define fstat win_fstat
#define lstat stat
#define sysconf_SC_CLK_TCK 64
#define fsync(a) !FlushFileBuffers(a)
#define pwrite win_pwrite
#define pread win_pread


#ifndef WIFEXITED
#define WIFEXITED(S) 1
#endif

#ifndef WEXITSTATUS
#define WEXITSTATUS(S) (S)
#endif

#endif	/* PLATFORM_H */

