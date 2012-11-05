#include "ejdb_logging.h"
#include "ejdb_thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
static void __flockfile(FILE *stream) {
    HANDLE hf;
    int fd;
    LARGE_INTEGER li;
    fd = _fileno(stream);
    hf = (HANDLE) _get_osfhandle(fd);
    li.QuadPart = _filelengthi64(fd);
    LockFile(hf, 0, 0, li.LowPart, li.HighPart);
}

static void __funlockfile(FILE *stream) {
    HANDLE hf;
    int fd;
    LARGE_INTEGER li;
    fd = _fileno(stream);
    hf = (HANDLE) _get_osfhandle(fd);
    li.QuadPart = _filelengthi64(fd);
    UnlockFile(hf, 0, 0, li.LowPart, li.HighPart);
}
#endif
#ifdef __unix

static void __flockfile(FILE *stream) {
    flockfile(stream);
}

static void __funlockfile(FILE *stream) {
    funlockfile(stream);
}
#endif

namespace ejdb {

    EJMutex g_outputMtx;

    const char* ej_errno_msg(int errno_rv) {
        return strerror(errno_rv);
    }

    void ej_log_intern(int priority, const char* file, int line, const char* fmt, ...) {
#ifndef _DEBUG
        if (priority == LOG_DEBUG) {
            return;
        }
#endif
        //Timestamp
        char tbuff[64];
        time_t rawtime;
        struct tm* timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(tbuff, 64, "%d %b %H:%M:%S -", timeinfo);

        __flockfile(stderr);
        __flockfile(stdout);
        try {
            switch (priority) {
                case LOG_ERROR:
                    if (file && line > 0) {
                        fprintf(stderr, "%s ERROR %s:%d ", tbuff, file, line);
                    } else {
                        fprintf(stderr, "%s ERROR ", tbuff);
                    }
                    break;
                case LOG_DEBUG:
                    if (file && line > 0) {
                        fprintf(stderr, "%s DEBUG %s:%d ", tbuff, file, line);
                    } else {
                        fprintf(stderr, "%s DEBUG ", tbuff);
                    }
                    break;
                case LOG_INFO:
                    if (file && line > 0) {
                        fprintf(stderr, "%s INFO %s:%d ", tbuff, file, line);
                    } else {
                        fprintf(stderr, "%s INFO ", tbuff);
                    }
                    break;
                case LOG_WARNING:
                    if (file && line > 0) {
                        fprintf(stderr, "%s WARN %s:%d ", tbuff, file, line);
                    } else {
                        fprintf(stderr, "%s WARN ", tbuff);
                    }
                    break;
                default:
                    if (file && line > 0) {
                        fprintf(stderr, "%s %s:%d ", tbuff, file, line);
                    } else {
                        fprintf(stderr, "%s ", tbuff);
                    }
            }
            va_list vl;
            va_start(vl, fmt);
            vfprintf(stderr, fmt, vl);
            va_end(vl);
            fprintf(stderr, "\n");
        } catch (...) {
            __funlockfile(stderr);
            __funlockfile(stdout);
            throw;
        }
        __funlockfile(stderr);
        __funlockfile(stdout);
    }

    void ej_log_errno_status(const char* file, int line, int errno_rv, const std::string& msg) {
        ej_log_errno_status(file, line, errno_rv, msg.c_str());
    }

    void ej_log_errno_status(const char* file, int line, int errno_rv, const char* msg) {
        if (msg) {
            ej_log_intern(LOG_WARNING, file, line, "%s, %s", msg, strerror(errno_rv));
        } else {
            ej_log_intern(LOG_WARNING, file, line, "%s", strerror(errno_rv));
        }
    }

    void ej_log_ejerror(const char* file, int line, const EJError& err) {
        ej_log_errno_status(file, line, err.errno_code, err.msg);
    }

    void EJError::errPrint() {
        ej_log_intern(LOG_ERROR, this->location.c_str(), this->line, "%s, %s",
                this->msg.c_str(),
                strerror(this->errno_code));
    }

} //eof ejdb namespace




