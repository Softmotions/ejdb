#include "platform.h"
#include "../tcutil.h"
#include "myconf.h"

static int file_attr_to_st_mode(DWORD attr) {
    int fMode = S_IREAD;
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
        fMode |= S_IFDIR;
    else
        fMode |= S_IFREG;
    if (!(attr & FILE_ATTRIBUTE_READONLY))
        fMode |= S_IWRITE;
    return fMode;
}

static long long filetime_to_hnsec(const FILETIME *ft) {
    long long winTime = ((long long) ft->dwHighDateTime << 32) + ft->dwLowDateTime;
    /* Windows to Unix Epoch conversion */
    return winTime - 116444736000000000LL;
}

static time_t filetime_to_time_t(const FILETIME *ft) {
    return (time_t) (filetime_to_hnsec(ft) / 10000000);
}

int win_fstat(HANDLE fh, struct stat *buf) {
    BY_HANDLE_FILE_INFORMATION fdata;
    if (fh == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    if (GetFileType(fh) != FILE_TYPE_DISK) {
        errno = ENODEV;
        return -1;
    }
    if (GetFileInformationByHandle(fh, &fdata)) {
        buf->st_ino = 0;
        buf->st_gid = 0;
        buf->st_uid = 0;
        buf->st_nlink = 1;
        buf->st_mode = file_attr_to_st_mode(fdata.dwFileAttributes);
        buf->st_size = (off_t) ((((int64_t) fdata.nFileSizeHigh) << 32) | fdata.nFileSizeLow);
        buf->st_dev = buf->st_rdev = 0; /* not used by Git */
        buf->st_atime = filetime_to_time_t(&(fdata.ftLastAccessTime));
        buf->st_mtime = filetime_to_time_t(&(fdata.ftLastWriteTime));
        buf->st_ctime = filetime_to_time_t(&(fdata.ftCreationTime));
        return 0;
    }
    errno = EBADF;
    return -1;
}

ssize_t win_pwrite(HANDLE fd, const void *buf, size_t count, off_t off) {
    DWORD written;
    OVERLAPPED offset;
    memset(&offset, 0, sizeof (OVERLAPPED));
    ULARGE_INTEGER bigint;
    bigint.QuadPart = off;
    offset.Offset = bigint.LowPart;
    offset.OffsetHigh = bigint.HighPart;
    if (!WriteFile(fd, buf, count, &written, &offset)) {
        return -1;
    }
    return written;
}

ssize_t win_pread(HANDLE fd, void *buf, size_t size, off_t off) {
    DWORD bread;
    OVERLAPPED offset;
    memset(&offset, 0, sizeof (OVERLAPPED));
    ULARGE_INTEGER bigint;
    bigint.QuadPart = off;
    offset.Offset = bigint.LowPart;
    offset.OffsetHigh = bigint.HighPart;
    if (!ReadFile(fd, buf, size, &bread, &offset)) {
        return -1;
    }
    return bread;
}

void globfree(glob_t *_pglob) {
    size_t i;
    if (!_pglob->gl_pathv)
        return;
    for (i = 0; i < _pglob->gl_pathc; i++)
        if (_pglob->gl_pathv[i])
            free(_pglob->gl_pathv[i]);
    free(_pglob->gl_pathv);
    _pglob->gl_offs = _pglob->gl_pathc = 0;
    _pglob->gl_pathv = NULL;
}

int glob(const char *pattern, int flags,
        int (*errfunc) (const char *epath, int eerrno),
        glob_t *pglob) {
    WIN32_FIND_DATAA stat;
    char *directory = NULL;
    int dirlen = 0;
    /* init pglob */
    pglob->gl_pathc = 0;
    pglob->gl_offs = 10;
    if ((pglob->gl_pathv = calloc(10, sizeof (char *))) == NULL)
        return GLOB_NOSPACE;
    char *separator = strrchr(pattern, MYPATHCHR);

    if (separator != NULL) {
        directory = tccalloc(1, separator - pattern + 2);
        strncpy(directory, pattern, separator - pattern + 1);
        dirlen = strlen(directory);
    }
    HANDLE h = FindFirstFileA(pattern, &stat);
    if (h == INVALID_HANDLE_VALUE) {
        globfree(pglob);
        if (directory != NULL)
            TCFREE(directory);
        return GLOB_ABORTED;
    }
    do {
        /* add new path to list */
        char *entry = tccalloc(1, strlen(stat.cFileName) + dirlen + 1);
        if (directory != NULL) {
            strcpy(entry, directory);
        }
        strcat(entry, stat.cFileName);
        pglob->gl_pathv[pglob->gl_pathc++] = entry;
        if (pglob->gl_pathc == pglob->gl_offs) {
            /* increase array size */
            void *tmp = realloc(pglob->gl_pathv, sizeof (char *) * (pglob->gl_offs + 10));
            if (tmp == NULL) {
                globfree(pglob);
                if (directory != NULL)
                    TCFREE(directory);
                return GLOB_NOSPACE;
            }
            pglob->gl_offs += 10;
            pglob->gl_pathv = tmp;
        }
    } while (FindNextFileA(h, &stat));
    FindClose(h);
    if (directory != NULL)
        TCFREE(directory);
    return 0;
}

#if defined(EJDB_DLL) && defined(PTW32_VERSION)
BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved) {
    BOOL result = PTW32_TRUE;
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            result = pthread_win32_process_attach_np();
            break;

        case DLL_THREAD_ATTACH:
            /*
             * A thread is being created
             */
            result = pthread_win32_thread_attach_np();
            break;

        case DLL_THREAD_DETACH:
            /*
             * A thread is exiting cleanly
             */
            result = pthread_win32_thread_detach_np();
            break;

        case DLL_PROCESS_DETACH:
            (void) pthread_win32_thread_detach_np();
            result = pthread_win32_process_detach_np();
            break;
    }
    return (result);

} /* DllMain */
#endif

