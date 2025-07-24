#!/bin/sh
# MIT License
# Autark (https://autark.dev) build system script wrapper.
# Copyright (c) 2012-2025 Softmotions Ltd <info@softmotions.com>
#
# https://github.com/Softmotions/autark

META_VERSION=0.9.0
META_REVISION=9593c87
cd "$(cd "$(dirname "$0")"; pwd -P)"

prev_arg=""
for arg in "$@"; do
  case "$prev_arg" in
    -H)
      AUTARK_HOME="${arg}/.autark-dist"
      prev_arg="" # сброс
      continue
      ;;
  esac

  case "$arg" in
    -H)
      prev_arg="-H"
      ;;
    -H*)
      AUTARK_HOME="${arg#-H}/.autark-dist"
      ;;
    --cache=*)
      AUTARK_HOME="${arg#--cache=}/.autark-dist"
      ;;
    *)
      prev_arg=""
      ;;
  esac
done

export AUTARK_HOME=${AUTARK_HOME:-autark-cache/.autark-dist}
AUTARK=${AUTARK_HOME}/autark

if [ "${META_VERSION}.${META_REVISION}" = "$(${AUTARK} -v)" ]; then
  ${AUTARK} "$@"
  exit $?
fi

set -e
echo "Building Autark executable in ${AUTARK_HOME}"

if [ -n "$CC" ] && command -v "$CC" >/dev/null 2>&1; then
  COMPILER="$CC"
elif command -v clang >/dev/null 2>&1; then
  COMPILER=clang
elif command -v gcc >/dev/null 2>&1; then
  COMPILER=gcc
else
  echo "No suitable C compiler found" >&2
  exit 1
fi

mkdir -p ${AUTARK_HOME}
cat <<'a292effa503b' > ${AUTARK_HOME}/autark.c
#ifndef CONFIG_H
#define CONFIG_H
#define META_VERSION "0.9.0"
#define META_REVISION "9593c87"
#endif
#define _AMALGAMATE_
#define _XOPEN_SOURCE 600
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <getopt.h>
#include <glob.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef BASEDEFS_H
#define BASEDEFS_H
#define QSTR(x__) #x__
#define Q(x__)    QSTR(x__)
#define AK_LLEN(l__) (sizeof(l__) - 1)
#if __GNUC__ >= 4
#define AUR      __attribute__((warn_unused_result))
#define AK_ALLOC __attribute__((malloc)) __attribute__((warn_unused_result))
#define AK_NORET __attribute__((noreturn))
#else
#define AUR
#define AK_ALLOC
#define AK_NORET
#endif
#define AK_CONSTRUCTOR __attribute__((constructor))
#define AK_DESTRUCTOR  __attribute__((destructor))
#define LLEN(l__) (sizeof(l__) - 1)
/* Align x_ with v_. v_ must be simple power for 2 value. */
#define ROUNDUP(x_, v_) (((x_) + (v_) - 1) & ~((v_) - 1))
/* Round down align x_ with v_. v_ must be simple power for 2 value. */
#define ROUNDOWN(x_, v_) ((x_) - ((x_) & ((v_) - 1)))
#ifdef __GNUC__
#define RCGO(rc__, label__) if (__builtin_expect((!!(rc__)), 0)) goto label__
#else
#define RCGO(rc__, label__) if (rc__) goto label__
#endif
#define RCHECK(rc__, label__, expr__) \
        rc__ = expr__;                \
        RCGO(rc__, label__)
#define RCC(rc__, label__, expr__) RCHECK(rc__, label__, expr__)
#define RCN(label__, expr__) \
        rc = (expr__);       \
        if (rc) {            \
          akerror(rc, 0, 0); \
          goto label__;      \
        }
#ifndef MIN
#define MIN(a_, b_) ((a_) < (b_) ? (a_) : (b_))
#endif
#ifndef MAX
#define MAX(a_, b_) ((a_) > (b_) ? (a_) : (b_))
#endif
#ifndef _AMALGAMATE_
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#endif
struct value {
  void  *buf;
  size_t len;
  int    error;
};
static inline int value_destroy(struct value *v) {
  if (v->buf) {
    free(v->buf);
    v->buf = 0;
  }
  return v->error;
}
#endif
#ifndef LOG_H
#define LOG_H
#ifndef _AMALGAMATE_
#include "basedefs.h"
#include <stdarg.h>
#endif
enum akecode {
  AK_ERROR_OK                    = 0,
  AK_ERROR_FAIL                  = -1,
  AK_ERROR_UNIMPLEMETED          = -2,
  AK_ERROR_INVALID_ARGS          = -3,
  AK_ERROR_ASSERTION             = -4,
  AK_ERROR_OVERFLOW              = -5,
  AK_ERROR_IO                    = -6,
  AK_ERROR_SCRIPT_SYNTAX         = -10,
  AK_ERROR_SCRIPT                = -11,
  AK_ERROR_CYCLIC_BUILD_DEPS     = -12,
  AK_ERROR_SCRIPT_ERROR          = -13,
  AK_ERROR_NOT_IMPLEMENTED       = -14,
  AK_ERROR_EXTERNAL_COMMAND      = -15,
  AK_ERROR_DEPENDENCY_UNRESOLVED = -16,
};
__attribute__((noreturn))
void akfatal2(const char *msg);
__attribute__((noreturn))
void _akfatal(const char *file, int line, int code, const char *fmt, ...);
__attribute__((noreturn))
void _akfatal_va(const char *file, int line, int code, const char *fmt, va_list);
int _akerror(const char *file, int line, int code, const char *fmt, ...);
void _akerror_va(const char *file, int line, int code, const char *fmt, va_list);
void _akverbose(const char *file, int line, const char *fmt, ...);
void akinfo(const char *fmt, ...);
void akwarn(const char *fmt, ...);
#define akfatal(code__, fmt__, ...) \
        _akfatal(__FILE__, __LINE__, (code__), (fmt__), __VA_ARGS__)
#define akfatal_va(code__, fmt__, va__) \
        _akfatal_va(__FILE__, __LINE__, (code__), (fmt__), (va__))
#define akerror(code__, fmt__, ...) \
        _akerror(__FILE__, __LINE__, (code__), (fmt__), __VA_ARGS__)
#define akerror_va(code__, fmt__, va__) \
        _akerror_va(__FILE__, __LINE__, (code__), (fmt__), (va__))
#define akverbose(fmt__, ...) \
        _akverbose(__FILE__, __LINE__, (fmt__), __VA_ARGS__)
#define akassert(exp__)                               \
        do {                                          \
          if (!(exp__)) {                             \
            akfatal(AK_ERROR_ASSERTION, Q(exp__), 0); \
          }                                           \
        } while (0)
#define akcheck(exp__)                    \
        do {                              \
          int e = (exp__);                \
          if (e) akfatal(e, Q(exp__), 0); \
        } while (0)
#endif
#ifndef ALLOC_H
#define ALLOC_H
#ifndef _AMALGAMATE_
#include "basedefs.h"
#include "log.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#endif
AK_ALLOC static inline char* xstrdup(const char *str) {
  char *ret = strdup(str);
  if (!ret) {
    akfatal2("Allocation failed");
  }
  return ret;
}
AK_ALLOC static inline void* xmalloc(size_t size) {
  void *ptr = malloc(size);
  if (!ptr) {
    akfatal2("Allocation failed");
  }
  return ptr;
}
AK_ALLOC static inline void* xrealloc(void *p, size_t size) {
  void *ptr = realloc(p, size);
  if (!ptr) {
    akfatal2("Allocation failed");
  }
  return ptr;
}
AK_ALLOC static inline void* xcalloc(size_t nmemb, size_t size) {
  void *ptr = calloc(nmemb, size);
  if (!ptr) {
     akfatal2("Allocation failed");
  }
  return ptr;
}
#endif
#ifndef POOL_H
#define POOL_H
#ifndef _AMALGAMATE_
#include <stddef.h>
#include <stdarg.h>
#endif
struct  pool_unit {
  void *heap;
  struct pool_unit *next;
};
struct pool {
  size_t usiz;                            /// Used size
  size_t asiz;                            /// Allocated size
  struct pool_unit *unit;                 /// Current heap unit
  void *user_data;                        /// Associated user data
  char *heap;                             /// Current pool heap ptr
  void  (*on_pool_destroy)(struct pool*); /// Called when pool destroyed
};
struct pool* pool_create_empty(void);
struct pool* pool_create_preallocated(size_t);
struct pool* pool_create(void (*)(struct pool*));
void pool_destroy(struct pool*);
void* pool_alloc(struct pool*, size_t siz);
void* pool_calloc(struct pool*, size_t siz);
char* pool_strdup(struct pool*, const char*);
char* pool_strndup(struct pool*, const char*, size_t len);
char* pool_printf_va(struct pool*, const char *format, va_list va);
char* pool_printf(struct pool*, const char*, ...) __attribute__((format(__printf__, 2, 3)));
const char** pool_split_string(
  struct pool *pool,
  const char  *haystack,
  const char  *split_chars,
  int          ignore_whitespace);
const char** pool_split(
  struct pool*, const char *split_chars, int ignore_ws, const char *fmt,
  ...) __attribute__((format(__printf__, 4, 5)));
#endif
#ifndef XSTR_H
#define XSTR_H
#ifndef _AMALGAMATE_
#include <stdarg.h>
#include <stddef.h>
#endif
struct xstr {
  char  *ptr;
  size_t size;
  size_t asize;
};
struct xstr* xstr_create(size_t siz);
struct  xstr* xstr_create_empty(void);
void xstr_cat2(struct xstr*, const void *buf, size_t size);
void xstr_cat(struct xstr*, const char *buf);
void xstr_shift(struct xstr*, size_t shift_size);
void xstr_unshift(struct xstr*, const void *buf, size_t size);
void xstr_pop(struct xstr*, size_t pop_size);
void xstr_insert(struct xstr*, size_t pos, const void *buf);
int xstr_printf_va(struct xstr*, const char *format, va_list va);
int xstr_printf(struct xstr*, const char *format, ...) __attribute__((format(__printf__, 2, 3)));
struct xstr* xstr_clear(struct xstr*);
char* xstr_ptr(struct xstr*);
size_t xstr_size(struct xstr*);
void xstr_destroy(struct xstr*);
char* xstr_destroy_keep_ptr(struct xstr*);
void xstr_cat_repeat(struct xstr*, char ch, int count);
#endif
#ifndef ULIST_H
#define ULIST_H
#ifndef _AMALGAMATE_
#include "basedefs.h"
#endif
struct ulist {
  char    *array;
  unsigned usize;
  unsigned num;
  unsigned anum;
  unsigned start;
};
struct ulist* ulist_create(unsigned initial_len, unsigned unit_size);
void ulist_init(struct ulist*, unsigned ini_length, unsigned unit_size);
void ulist_destroy(struct ulist**);
void ulist_destroy_keep(struct ulist*);
void ulist_clear(struct ulist*);
void ulist_reset(struct ulist*);
void* ulist_get(const struct ulist*, unsigned idx);
void* ulist_peek(const struct ulist*);
void ulist_insert(struct ulist*, unsigned idx, const void *data);
void ulist_set(struct ulist*, unsigned idx, const void *data);
void ulist_remove(struct ulist*, unsigned idx);
void ulist_push(struct ulist*, const void *data);
void ulist_pop(struct ulist*);
void ulist_unshift(struct ulist*, const void *data);
void ulist_shift(struct ulist*);
struct ulist* ulist_clone(const struct ulist*);
int ulist_find(const struct ulist*, const void *data);
int ulist_remove_by(struct ulist*, const void *data);
AK_ALLOC char* ulist_to_vlist(const struct ulist *list);
#endif
#ifndef MAP_H
#define MAP_H
#ifndef _AMALGAMATE_
#include <stdint.h>
#endif
struct map;
struct map_iter;
struct map_iter {
  struct map *hm;
  const void *key;
  const void *val;
  uint32_t    bucket;
  int32_t     entry;
};
void map_kv_free(void *key, void *val);
void map_k_free(void *key, void *val);
struct map* map_create(
  int (*cmp_fn)(const void*, const void*),
  uint32_t (*hash_fn)(const void*),
  void (*kv_free_fn)(void*, void*));
void map_destroy(struct map*);
struct map* map_create_u64(void (*kv_free_fn)(void*, void*));
struct map* map_create_u32(void (*kv_free_fn)(void*, void*));
struct map* map_create_str(void (*kv_free_fn)(void*, void*));
void map_put(struct map*, void *key, void *val);
void map_put_u32(struct map*, uint32_t key, void *val);
void map_put_u64(struct map*, uint64_t key, void *val);
void map_put_str(struct map*, const char *key, void *val);
void map_put_str_no_copy(struct map*, const char *key, void *val);
int map_remove(struct map*, const void *key);
int map_remove_u64(struct map*, uint64_t key);
int map_remove_u32(struct map*, uint32_t key);
void* map_get(struct map*, const void *key);
void* map_get_u64(struct map*, uint64_t key);
void* map_get_u32(struct map*, uint32_t key);
uint32_t map_count(const struct map*);
void map_clear(struct map*);
void map_iter_init(struct map*, struct map_iter*);
int map_iter_next(struct map_iter*);
#endif
#ifndef UTILS_H
#define UTILS_H
#ifndef _AMALGAMATE_
#include "basedefs.h"
#include "xstr.h"
#include <limits.h>
#include <string.h>
#endif
static inline bool utils_char_is_space(char c) {
  return c == 32 || (c >= 9 && c <= 13);
}
static inline void utils_chars_replace(char *buf, char c, char r) {
  for ( ; *buf != '\0'; ++buf) {
    if (*buf == c) {
      *buf = r;
    }
  }
}
static inline long utils_lround(double x) {
  if (x >= 0.0) {
    x += 0.5;
  } else {
    x -= 0.5;
  }
  if (x > (double) LONG_MAX) {
    return LONG_MAX;
  } else if (x < (double) LONG_MIN) {
    return LONG_MIN;
  }
  return (long) x;
}
static inline int utils_toupper_ascii(int c) {
  if (c >= 'a' && c <= 'z') {
    return c - ('a' - 'A');     // or: c - 32
  }
  return c;
}
static inline char* utils_strncpy(char *dst, const char *src, size_t dst_sz) {
  if (dst_sz > 1) {
    size_t len = strnlen(src, dst_sz - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
  } else if (dst_sz) {
    dst[0] = '\0';
  }
  return dst;
}
static inline char* utils_strnncpy(char *dst, const char *src, size_t src_len, size_t dst_sz) {
  if (dst_sz > 1 && src_len) {
    size_t len = MIN(src_len, dst_sz - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
  } else if (dst_sz) {
    dst[0] = '\0';
  }
  return dst;
}
static inline bool utils_startswith(const char *str, const char *prefix) {
  if (!str || !prefix) {
    return false;
  }
  size_t str_len = strlen(str);
  size_t prefix_len = strlen(prefix);
  if (prefix_len > str_len) {
    return false;
  }
  return strncmp(str, prefix, prefix_len) == 0;
}
static inline bool utils_endswith(const char *str, const char *suffix) {
  if (!str || !suffix) {
    return false;
  }
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > str_len) {
    return false;
  }
  return strcmp(str + str_len - suffix_len, suffix) == 0;
}
long int utils_strtol(const char *v, int base, int *rcp);
long long utils_strtoll(const char *v, int base, int *rcp);
struct value utils_file_as_buf(const char *path, ssize_t buflen_max);
int utils_file_write_buf(const char *path, const char *buf, size_t len, bool append);
int utils_exec_path(char buf[PATH_MAX]);
int utils_copy_file(const char *src, const char *dst);
int utils_rename_file(const char *src, const char *dst);
void utils_split_values_add(const char *v, struct xstr *xstr);
int utils_fd_make_non_blocking(int fd);
//----------------------- Vlist
struct vlist_iter {
  const char *item;
  size_t      len;
};
static inline bool is_vlist(const char *val) {
  return (val && *val == '\1');
}
static inline void vlist_iter_init(const char *vlist, struct vlist_iter *iter) {
  iter->item = vlist;
  iter->len = 0;
}
static inline bool vlist_iter_next(struct vlist_iter *iter) {
  iter->item += iter->len;
  while (*iter->item == '\1') {
    iter->item++;
  }
  if (*iter->item == '\0') {
    return false;
  }
  const char *p = iter->item;
  while (*p != '\0' && *p != '\1') {
    ++p;
  }
  iter->len = p - iter->item;
  return true;
}
#endif
#ifndef SPAWN_H
#define SPAWN_H
#ifndef _AMALGAMATE_
#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>
#endif
struct spawn;
struct spawn* spawn_create(const char *exec, void *user_data);
void* spawn_user_data(struct spawn*);
pid_t spawn_pid(struct spawn*);
int spawn_exit_code(struct spawn*);
const char* spawn_arg_add(struct spawn*, const char *arg);
bool spawn_arg_exists(struct spawn*, const char *arg);
bool spawn_arg_starts_with(struct spawn*, const char *arg);
void spawn_env_set(struct spawn*, const char *key, const char *val);
void spawn_env_path_prepend(struct spawn*, const char *path);
void spawn_set_stdin_provider(struct spawn*, size_t (*provider)(char *buf, size_t buflen, struct spawn*));
void spawn_set_stdout_handler(struct spawn*, void (*handler)(char *buf, size_t buflen, struct spawn*));
void spawn_set_stderr_handler(struct spawn*, void (*handler)(char *buf, size_t buflen, struct spawn*));
void spawn_set_nowait(struct spawn*, bool nowait);
void spawn_set_wstatus(struct spawn*, int wstatus);
int spawn_do(struct spawn*);
void spawn_destroy(struct spawn*);
#endif
#ifndef PATHS_H
#define PATHS_H
#ifndef _AMALGAMATE_
#include "pool.h"
#include "basedefs.h"
#include <limits.h>
#include <stdio.h>
#endif
enum akpath_access {
  AKPATH_READ  = 0x01,
  AKPATH_WRITE = 0x02,
  AKPATH_EXEC  = 0x04,
};
int path_access(const char *path, enum akpath_access access);
enum akpath_ftype {
  AKPATH_NOT_EXISTS,
  AKPATH_TYPE_FILE,
  AKPATH_TYPE_DIR,
  AKPATH_LINK,
  AKPATH_OTHER,
};
struct akpath_stat {
  uint64_t size;
  uint64_t atime;
  uint64_t ctime;
  uint64_t mtime;
  enum akpath_ftype ftype;
};
bool path_is_absolute(const char *path);
/// Uses stdlib `realpath` implementation.
/// NOTE: Path argument must exists on file system.
/// Foo other cases see: path_normalize()
const char* path_real(const char *path, char buf[PATH_MAX]);
const char* path_real_pool(const char *path, struct pool*);
/// Normalize a path buffer to an absolute path without resolving symlinks.
/// It operates purely on the path string, and works for non-existing paths.
char* path_normalize(const char *path, char buf[PATH_MAX]);
char* path_normalize_cwd(const char *path, const char *cwd, char buf[PATH_MAX]);
char* path_normalize_pool(const char *path, struct pool*);
char* path_normalize_cwd_pool(const char *path, const char *cwd, struct pool*);
int path_mkdirs(const char *path);
int path_mkdirs_for(const char *path);
int path_rm_cache(const char *path);
int path_stat(const char *path, struct akpath_stat *stat);
int path_stat_fd(int fd, struct akpath_stat *stat);
int path_stat_file(FILE *file, struct akpath_stat *stat);
uint64_t path_mtime(const char *path);
bool path_is_accesible(const char *path, enum akpath_access a);
bool path_is_accesible_read(const char *path);
bool path_is_accesible_write(const char *path);
bool path_is_accesible_exec(const char *path);
bool path_is_dir(const char *path);
bool path_is_file(const char *path);
bool path_is_exist(const char *path);
const char* path_is_prefix_for(const char *path, const char *haystack, const char *cwd);
AK_ALLOC char* path_relativize(const char *from, const char *to);
AK_ALLOC char* path_relativize_cwd(const char *from, const char *to, const char *cwd);
// Modifies its argument
char* path_dirname(char *path);
// Modifies its argument
char* path_basename(char *path);
#endif
#ifndef ENV_H
#define ENV_H
#ifndef _AMALGAMATE_
#include "basedefs.h"
#include "pool.h"
#include "map.h"
#include "ulist.h"
#include <stdbool.h>
#endif
#define AUTARK_CACHE  "autark-cache"
#define AUTARK_SCRIPT "Autark"
#define AUTARK_ROOT_DIR  "AUTARK_ROOT_DIR"  // Project root directory
#define AUTARK_CACHE_DIR "AUTARK_CACHE_DIR" // Project cache directory
#define AUTARK_UNIT      "AUTARK_UNIT"      // Path relative to AUTARK_ROOT_DIR of build process unit executed
                                            // currently.
#define AUTARK_VERBOSE "AUTARK_VERBOSE"     // Autark verbose env key
#define UNIT_FLG_ROOT    0x01U // Project root unit
#define UNIT_FLG_SRC_CWD 0x02U // Set project source dir as unit CWD
#define UNIT_FLG_NO_CWD  0x04U // Do not change CWD for unit
struct unit_env_item {
  const char  *val;
  struct node *n;
};
/// Current execution unit.
struct unit {
  struct map  *env;         // Environment associated with unit. struct unit_env_item.
  const char  *rel_path;    // Path to unit relative to the project root and project cache.
  const char  *basename;    // Basename of unit path.
  const char  *dir;         // Absolute path to unit dir.
  const char  *source_path; // Absolute unit source path.
  const char  *cache_path;  // Absolute path to the unit in cache dir.
  const char  *cache_dir;   // Absolute path to the cache directory where unit file is located.
  struct pool *pool;        // Pool used to allocate unit
  unsigned     flags;       // Unit flags
  struct node *n;           // Node this unit associated. May be zero.
  void *impl;               // Arbirary implementation data
};
/// Unit context in units stack.
struct unit_ctx {
  struct unit *unit;
  unsigned     flags;
};
/// Global env
struct env {
  const char  *cwd;
  struct pool *pool;
  int verbose;
  int max_parallel_jobs;            // Max number of allowed parallel jobs.
  struct {
    const char *root_dir;           // Project root source dir.
    const char *cache_dir;          // Project artifacts cache dir.
    bool cleanup;                   // Clean project cache before build
    bool prepared;                  // Autark build prepared
    struct xstr *options;           // Ask option values
  } project;
  struct {
    const char *prefix_dir;  // Absolute path to install prefix dir.
    const char *bin_dir;     // Path to the bin dir relative to prefix.
    const char *lib_dir;     // Path to lib dir relative to prefix.
    const char *data_dir; // Path to lib data dir relative to prefix.
    const char *include_dir; // Path to include headers dir relative to prefix.
    const char *pkgconf_dir; // Path to pkgconfig dir.
    const char *man_dir;     // Path to man pages dir.
    bool enabled;            // True if install operation should be performed
  } install;
  struct {
    const char *extra_env_paths; // Extra PATH environment for any program spawn
  } spawn;
  struct map  *map_path_to_unit; // Path to unit mapping
  struct ulist stack_units;      // Stack of nested unit contexts (struct unit_ctx)
  struct ulist units;            // All created units. (struct unit*)
  struct {
    struct xstr *log;
  } check;
};
extern struct env g_env;
void on_unit_pool_destroy(struct pool*);
struct unit* unit_create(const char *unit_path, unsigned flags, struct pool *pool);
struct unit* unit_for_path(const char *path);
void unit_push(struct unit*, struct node*);
struct unit* unit_pop(void);
struct unit* unit_peek(void);
struct unit_ctx unit_peek_ctx(void);
struct unit* unit_root(void);
struct unit* unit_parent(struct node *n);
void unit_ch_dir(struct unit_ctx*, char *prevcwd);
void unit_ch_cache_dir(struct unit*, char *prevcwd);
void unit_ch_src_dir(struct unit*, char *prevcwd);
void unit_env_set_val(struct unit*, const char *key, const char *val);
void unit_env_set_node(struct unit*, const char *key, struct node *n);
struct node* unit_env_get_node(struct unit *u, const char *key);
const char* unit_env_get(struct node *n, const char *key);
const char* unit_env_get_raw(struct unit *u, const char *key);
void unit_env_remove(struct unit*, const char *key);
const char* env_libdir(void);
#endif
#ifndef DEPS_H
#define DEPS_H
#ifndef _AMALGAMATE_
#include "env.h"
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#endif
#define DEPS_TYPE_FILE          102 // f
#define DEPS_TYPE_FILE_OUTDATED 111 // x
#define DEPS_TYPE_NODE_VALUE    118 // v
#define DEPS_TYPE_ENV           101 // e
#define DEPS_TYPE_SYS_ENV       115 // s
#define DEPS_TYPE_ALIAS         97  // a
#define DEPS_TYPE_OUTDATED      120 // o
#define DEPS_OPEN_TRUNCATE 0x01U
#define DEPS_OPEN_READONLY 0x02U
#define DEPS_BUF_SZ 262144
struct deps {
  char    type;
  char    flags;
  int     num_registered;   /// Number of deps registerd in the current session
  int64_t serial;
  const char *resource;
  const char *alias;
  FILE       *file;
  char buf[DEPS_BUF_SZ];
};
int deps_open(const char *path, int omode, struct deps *init);
bool deps_cur_next(struct deps*);
bool deps_cur_is_outdated(struct node *n, struct deps*);
int deps_add(struct deps*, char type, char flags, const char *resource, int64_t serial);
int deps_add_alias(struct deps*, char flags, const char *resource, const char *alias);
int deps_add_env(struct deps *d, char flags, const char *key, const char *value);
int deps_add_sys_env(struct deps *d, char flags, const char *key, const char *value);
void deps_close(struct deps*);
void deps_prune_all(const char *path);
#endif
#ifndef NODES_H
#define NODES_H
#ifndef _AMALGAMATE_
#include "script.h"
#endif
int node_script_setup(struct node*);
int node_meta_setup(struct node*);
int node_check_setup(struct node*);
int node_set_setup(struct node*);
int node_include_setup(struct node*);
int node_if_setup(struct node*);
int node_subst_setup(struct node*);
int node_run_setup(struct node*);
int node_echo_setup(struct node*);
int node_join_setup(struct node*);
int node_cc_setup(struct node*);
int node_configure_setup(struct node*);
int node_basename_setup(struct node*);
int node_foreach_setup(struct node*);
int node_in_sources_setup(struct node*);
int node_dir_setup(struct node*);
int node_option_setup(struct node*);
int node_error_setup(struct node*);
int node_echo_setup(struct node*);
int node_install_setup(struct node*);
int node_find_setup(struct node*);
#endif
#ifndef AUTARK_H
#define AUTARK_H
#ifndef _AMALGAMATE_
#include <stdbool.h>
#endif
void autark_init(void);
void autark_run(int argc, const char **argv);
void autark_dispose(void);
void autark_build_prepare(const char *script_path);
#endif
#ifndef SCRIPT_H
#define SCRIPT_H
#ifndef _AMALGAMATE_
#include "ulist.h"
#include "xstr.h"
#include "deps.h"
#include "map.h"
#include <stdbool.h>
#endif
// value types
#define NODE_TYPE_VALUE    0x01U
#define NODE_TYPE_SUBST    0x02U
#define NODE_TYPE_SET      0x04U
#define NODE_TYPE_JOIN     0x08U
#define NODE_TYPE_BASENAME 0x10U
#define NODE_TYPE_DIR      0x20U
#define NODE_TYPE_FIND     0x40U
// eof value types
#define NODE_TYPE_SCRIPT     0x100U
#define NODE_TYPE_BAG        0x200U
#define NODE_TYPE_META       0x400U
#define NODE_TYPE_CHECK      0x800U
#define NODE_TYPE_INCLUDE    0x1000U
#define NODE_TYPE_IF         0x2000U
#define NODE_TYPE_RUN        0x4000U
#define NODE_TYPE_CC         0x8000U
#define NODE_TYPE_CONFIGURE  0x10000U
#define NODE_TYPE_FOREACH    0x20000U
#define NODE_TYPE_IN_SOURCES 0x40000U
#define NODE_TYPE_OPTION     0x80000U
#define NODE_TYPE_ERROR      0x100000U
#define NODE_TYPE_ECHO       0x200000U
#define NODE_TYPE_INSTALL    0x400000U
#define NODE_FLG_BOUND      0x01U
#define NODE_FLG_INIT       0x02U
#define NODE_FLG_SETUP      0x04U
#define NODE_FLG_UPDATED    0x08U // Node product updated as result of build
#define NODE_FLG_BUILT      0x10U // Node built
#define NODE_FLG_POST_BUILT 0x20U // Node post-built
#define NODE_FLG_IN_CACHE   0x40U
#define NODE_FLG_IN_SRC     0x80U
#define NODE_FLG_NO_CWD     0x100U
#define NODE_FLG_NEGATE     0x200U
#define NODE_FLG_IN_ANY (NODE_FLG_IN_SRC | NODE_FLG_IN_CACHE | NODE_FLG_NO_CWD)
#define node_is_init(n__)         (((n__)->flags & NODE_FLG_INIT) != 0)
#define node_is_setup(n__)        (((n__)->flags & NODE_FLG_SETUP) != 0)
#define node_is_built(n__)        (((n__)->flags & NODE_FLG_BUILT) != 0)
#define node_is_post_built(n__)   (((n__)->flags & NODE_FLG_POST_BUILT) != 0)
#define node_is_value(n__)        ((n__)->type == NODE_TYPE_VALUE)
#define node_is_can_be_value(n__) ((n__)->type >= NODE_TYPE_VALUE && (n__)->type <= NODE_TYPE_FIND)
#define node_is_rule(n__) !node_is_value(n__)
#define NODE_PRINT_INDENT 2
struct node_foreach {
  char    *name;
  char    *value;
  char    *items;
  unsigned access_cnt;
};
struct node {
  unsigned type;
  unsigned flags;
  unsigned flags_owner; /// Extra flaghs set by owner node
  unsigned index;       /// Own index in env::nodes
  unsigned lnum;        /// Node line number
  const char *name;   /// Internal node name
  const char *vfile;  /// Node virtual file
  const char *value;  /// Key or value
  struct node *child;
  struct node *next;
  struct node *parent;
  // Recursive set
  struct {
    struct node *n;
    bool active;
  } recur_next;
  struct sctx *ctx;
  struct unit *unit;
  const char* (*value_get)(struct node*);
  void (*init)(struct node*);
  void (*setup)(struct node*);
  void (*build)(struct node*);
  void (*post_build)(struct node*);
  void (*dispose)(struct node*);
  void *impl;
};
struct sctx {
  struct node *root;     /// Project root script node (Autark)
  struct ulist nodes;    /// ulist<struct node*>
  struct map  *products; /// Products of nodes  (product name -> node)
};
int script_open(const char *file, struct sctx **out);
int script_include(struct node *parent, const char *file, struct node **out);
void script_build(struct sctx*);
void script_close(struct sctx**);
void script_dump(struct sctx*, struct xstr *out);
const char* node_env_get(struct node*, const char *key);
void node_env_set(struct node*, const char *key, const char *val);
void node_env_set_node(struct node*, const char *key);
struct node* node_by_product(struct node*, const char *prod, char pathbuf[PATH_MAX]);
struct node* node_by_product_raw(struct node*, const char *prod);
void node_product_add(struct node*, const char *prod, char pathbuf[PATH_MAX]);
void node_product_add_raw(struct node*, const char *prod);
void node_reset(struct node *n);
const char* node_value(struct node *n);
void node_module_setup(struct node *n, unsigned flags);
void node_init(struct node *n);
void node_setup(struct node *n);
void node_build(struct node *n);
void node_post_build(struct node *n);
struct node* node_find_direct_child(struct node *n, int type, const char *val);
struct node* node_find_prev_sibling(struct node *n);
struct  node* node_find_parent_of_type(struct node *n, int type);
struct node_foreach* node_find_parent_foreach(struct node *n);
bool node_is_value_may_be_dep_saved(struct node *n);
struct node* node_consumes_resolve(
  struct node *n,
  struct node *npaths,
  struct ulist *paths,
  void (*on_resolved)(const char *path, void *opq),
  void *opq);
void node_add_unit_deps(struct node *n, struct deps*);
struct node_resolve {
  struct node *n;
  const char  *path;
  void *user_data;
  void  (*on_init)(struct node_resolve*);
  void  (*on_env_value)(struct node_resolve*, const char *key, const char *val);
  void  (*on_resolve)(struct node_resolve*);
  const char  *deps_path_tmp;
  const char  *env_path_tmp;
  struct ulist resolve_outdated; // struct resolve_outdated
  struct ulist node_val_deps;    // struct node*
  struct pool *pool;
  unsigned     mode;
  int  num_deps;    // Number of dependencies
  bool force_outdated;
};
struct resolve_outdated {
  char  type;
  char  flags;
  char *path;
};
#define NODE_RESOLVE_ENV_ALWAYS 0x01U
void node_resolve(struct node_resolve*);
__attribute__((noreturn))
void node_fatal(int rc, struct node *n, const char *fmt, ...);
void node_info(struct node *n, const char *fmt, ...);
void node_warn(struct node *n, const char *fmt, ...);
int node_error(int rc, struct node *n, const char *fmt, ...);
#endif
#ifndef _AMALGAMATE_
#include "xstr.h"
#include "alloc.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#endif
struct xstr* xstr_create(size_t siz) {
  if (!siz) {
    siz = 16;
  }
  struct xstr *xstr;
  xstr = xmalloc(sizeof(*xstr));
  xstr->ptr = xmalloc(siz);
  xstr->size = 0;
  xstr->asize = siz;
  xstr->ptr[0] = '\0';
  return xstr;
}
struct  xstr* xstr_create_empty(void) {
  return xstr_create(0);
}
void xstr_cat2(struct xstr *xstr, const void *buf, size_t size) {
  size_t nsize = xstr->size + size + 1;
  if (xstr->asize < nsize) {
    while (xstr->asize < nsize) {
      xstr->asize <<= 1;
      if (xstr->asize < nsize) {
        xstr->asize = nsize;
      }
    }
    char *ptr = xrealloc(xstr->ptr, xstr->asize);
    xstr->ptr = ptr;
  }
  memcpy(xstr->ptr + xstr->size, buf, size);
  xstr->size += size;
  xstr->ptr[xstr->size] = '\0';
}
void xstr_cat(struct xstr *xstr, const char *buf) {
  size_t size = strlen(buf);
  xstr_cat2(xstr, buf, size);
}
void xstr_shift(struct xstr *xstr, size_t shift_size) {
  if (shift_size == 0) {
    return;
  }
  if (shift_size > xstr->size) {
    shift_size = xstr->size;
  }
  if (xstr->size > shift_size) {
    memmove(xstr->ptr, xstr->ptr + shift_size, xstr->size - shift_size);
  }
  xstr->size -= shift_size;
  xstr->ptr[xstr->size] = '\0';
}
void xstr_unshift(struct xstr *xstr, const void *buf, size_t size) {
  unsigned nsize = xstr->size + size + 1;
  if (xstr->asize < nsize) {
    while (xstr->asize < nsize) {
      xstr->asize <<= 1;
      if (xstr->asize < nsize) {
        xstr->asize = nsize;
      }
    }
    char *ptr = xrealloc(xstr->ptr, xstr->asize);
    xstr->ptr = ptr;
  }
  if (xstr->size) {
    // shift to right
    memmove(xstr->ptr + size, xstr->ptr, xstr->size);
  }
  memcpy(xstr->ptr, buf, size);
  xstr->size += size;
  xstr->ptr[xstr->size] = '\0';
}
void xstr_pop(struct xstr *xstr, size_t pop_size) {
  if (pop_size == 0) {
    return;
  }
  if (pop_size > xstr->size) {
    pop_size = xstr->size;
  }
  xstr->size -= pop_size;
  xstr->ptr[xstr->size] = '\0';
}
void xstr_insert(struct xstr *xstr, size_t pos, const void *buf) {
  if (pos > xstr->size) {
    return;
  }
  unsigned size = strlen(buf);
  if (size == 0) {
    return;
  }
  size_t nsize = xstr->size + size + 1;
  if (xstr->asize < nsize) {
    while (xstr->asize < nsize) {
      xstr->asize <<= 1;
      if (xstr->asize < nsize) {
        xstr->asize = nsize;
      }
    }
    char *ptr = xrealloc(xstr->ptr, xstr->asize);
    xstr->ptr = ptr;
  }
  memmove(xstr->ptr + pos + size, xstr->ptr + pos, xstr->size - pos + 1 /* \0 */);
  memcpy(xstr->ptr + pos, buf, size);
  xstr->size += size;
}
int xstr_printf_va(struct xstr *xstr, const char *format, va_list va) {
  int rc = 0;
  char buf[1024];
  va_list cva;
  va_copy(cva, va);
  char *wp = buf;
  int len = vsnprintf(wp, sizeof(buf), format, va);
  if (len >= sizeof(buf)) {
    wp = xmalloc(len + 1);
    len = vsnprintf(wp, len + 1, format, cva);
    if (len < 0) {
      rc = EINVAL;
      goto finish;
    }
  }
  xstr_cat2(xstr, wp, len);
finish:
  va_end(cva);
  if (wp != buf) {
    free(wp);
  }
  return rc;
}
int xstr_printf(struct xstr *xstr, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int rc = xstr_printf_va(xstr, format, ap);
  va_end(ap);
  return rc;
}
struct xstr* xstr_clear(struct xstr *xstr) {
  xstr->size = 0;
  xstr->ptr[0] = '\0';
  return xstr;
}
char* xstr_ptr(struct xstr *xstr) {
  return xstr->ptr;
}
size_t xstr_size(struct xstr *xstr) {
  return xstr->size;
}
void xstr_destroy(struct xstr *xstr) {
  if (!xstr) {
    return;
  }
  free(xstr->ptr);
  free(xstr);
}
char* xstr_destroy_keep_ptr(struct xstr *xstr) {
  if (!xstr) {
    return 0;
  }
  char *ptr = xstr->ptr;
  free(xstr);
  return ptr;
}
void xstr_cat_repeat(struct xstr *xstr, char ch, int count) {
  for (int i = count; i > 0; --i) {
    char buf[] = { ch };
    xstr_cat2(xstr, buf, 1);
  }
}
#ifndef _AMALGAMATE_
#include "ulist.h"
#include "alloc.h"
#include "xstr.h"
#include <string.h>
#endif
#define _ALLOC_UNIT 32
struct ulist* ulist_create(unsigned initial_len, unsigned unit_size) {
  struct ulist *list = xmalloc(sizeof(*list));
  ulist_init(list, initial_len, unit_size);
  return list;
}
void ulist_init(struct ulist *list, unsigned ini_length, unsigned unit_size) {
  list->usize = unit_size;
  list->num = 0;
  list->start = 0;
  if (!ini_length) {
    ini_length = _ALLOC_UNIT;
  }
  list->anum = ini_length;
  list->array = xmalloc((size_t) unit_size * ini_length);
}
void ulist_destroy(struct ulist **lp) {
  if (lp && *lp) {
    ulist_destroy_keep(*lp);
    free(*lp);
    *lp = 0;
  }
}
void ulist_destroy_keep(struct ulist *list) {
  if (list) {
    free(list->array);
    memset(list, 0, sizeof(*list));
  }
}
void ulist_clear(struct ulist *list) {
  if (list) {
    free(list->array);
    ulist_init(list, _ALLOC_UNIT, list->usize);
  }
}
void ulist_reset(struct ulist *list) {
  if (list) {
    list->num = 0;
    list->start = 0;
  }
}
void* ulist_get(const struct ulist *list, unsigned idx) {
  if (idx >= list->num) {
    return 0;
  }
  idx += list->start;
  return list->array + (size_t) idx * list->usize;
}
void* ulist_peek(const struct ulist *list) {
  return list->num ? ulist_get(list, list->num - 1) : 0;
}
void ulist_insert(struct ulist *list, unsigned idx, const void *data) {
  if (idx > list->num) {
    return;
  }
  idx += list->start;
  if (list->start + list->num >= list->anum) {
    size_t anum = list->anum + list->num + 1;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
  memmove(list->array + ((size_t) idx + 1) * list->usize,
          list->array + (size_t) idx * list->usize,
          (list->start + (size_t) list->num - idx) * list->usize);
  memcpy(list->array + (size_t) idx * list->usize, data, list->usize);
  ++list->num;
}
void ulist_set(struct ulist *list, unsigned idx, const void *data) {
  if (idx >= list->num) {
    return;
  }
  idx += list->start;
  memcpy(list->array + (size_t) idx * list->usize, data, list->usize);
}
void ulist_remove(struct ulist *list, unsigned idx) {
  if (idx >= list->num) {
    return;
  }
  idx += list->start;
  --list->num;
  memmove(list->array + (size_t) idx * list->usize, list->array + ((size_t) idx + 1) * list->usize,
          (list->start + (size_t) list->num - idx) * list->usize);
  if ((list->anum > _ALLOC_UNIT) && (list->anum >= list->num * 2)) {
    if (list->start) {
      memmove(list->array, list->array + (size_t) list->start * list->usize, (size_t) list->num * list->usize);
      list->start = 0;
    }
    size_t anum = list->num > _ALLOC_UNIT ? list->num : _ALLOC_UNIT;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
}
void ulist_push(struct ulist *list, const void *data) {
  size_t idx = list->start + list->num;
  if (idx >= list->anum) {
    size_t anum = list->anum + list->num + 1;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
  memcpy(list->array + idx * list->usize, data, list->usize);
  ++list->num;
}
void ulist_pop(struct ulist *list) {
  if (!list->num) {
    return;
  }
  size_t num = list->num - 1;
  if ((list->anum > _ALLOC_UNIT) && (list->anum >= num * 2)) {
    if (list->start) {
      memmove(list->array, list->array + (size_t) list->start * list->usize, num * list->usize);
      list->start = 0;
    }
    size_t anum = num > _ALLOC_UNIT ? num : _ALLOC_UNIT;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
  list->num = num;
}
void ulist_unshift(struct ulist *list, const void *data) {
  if (!list->start) {
    if (list->num >= list->anum) {
      size_t anum = list->anum + list->num + 1;
      void *nptr = xrealloc(list->array, anum * list->usize);
      list->anum = anum;
      list->array = nptr;
    }
    list->start = list->anum - list->num;
    memmove(list->array + (size_t) list->start * list->usize, list->array, (size_t) list->num * list->usize);
  }
  memcpy(list->array + ((size_t) list->start - 1) * list->usize, data, list->usize);
  --list->start;
  ++list->num;
}
void ulist_shift(struct ulist *list) {
  if (!list->num) {
    return;
  }
  size_t num = list->num - 1;
  size_t start = list->start + 1;
  if ((list->anum > _ALLOC_UNIT) && (list->anum >= num * 2)) {
    if (start) {
      memmove(list->array, list->array + start * list->usize, num * list->usize);
      start = 0;
    }
    size_t anum = num > _ALLOC_UNIT ? num : _ALLOC_UNIT;
    void *nptr = xrealloc(list->array, anum * list->usize);
    list->anum = anum;
    list->array = nptr;
  }
  list->start = start;
  list->num = num;
}
struct ulist* ulist_clone(const struct ulist *list) {
  if (!list->num) {
    return ulist_create(list->anum, list->usize);
  }
  struct ulist *nlist = xmalloc(sizeof(*nlist));
  unsigned anum = list->num > _ALLOC_UNIT ? list->num : _ALLOC_UNIT;
  nlist->array = xmalloc(anum * list->usize);
  memcpy(nlist->array, list->array + list->start, list->num * list->usize);
  nlist->usize = list->usize;
  nlist->num = list->num;
  nlist->anum = anum;
  nlist->start = 0;
  return nlist;
}
int ulist_find(const struct ulist *list, const void *data) {
  for (unsigned i = list->start; i < list->start + list->num; ++i) {
    void *ptr = list->array + i * list->usize;
    if (memcmp(data, ptr, list->usize) == 0) {
      return i - list->start;
    }
  }
  return -1;
}
int ulist_remove_by(struct ulist *list, const void *data) {
  for (unsigned i = list->start; i < list->start + list->num; ++i) {
    void *ptr = list->array + i * list->usize;
    if (memcmp(data, ptr, list->usize) == 0) {
      ulist_remove(list, i - list->start);
      return i;
    }
  }
  return -1;
}
char* ulist_to_vlist(const struct ulist *list) {
  akassert(list && list->usize == sizeof(char*));
  struct xstr *xstr = xstr_create_empty();
  for (int i = 0; i < list->num; ++i) {
    const char *c = *(const char**) ulist_get(list, i);
    xstr_cat(xstr, "\1");
    xstr_cat(xstr, c);
  }
  return xstr_destroy_keep_ptr(xstr);
}
#ifndef _AMALGAMATE_
#include "basedefs.h"
#include "pool.h"
#include "utils.h"
#include "alloc.h"
#include <string.h>
#include <stdio.h>
#endif
#define _UNIT_ALIGN_SIZE 8UL
static int _extend(struct pool *pool, size_t siz);
struct pool* pool_create_empty(void) {
  return xcalloc(1, sizeof(struct pool));
}
struct pool* pool_create_preallocated(size_t sz) {
  struct pool *pool = pool_create_empty();
  _extend(pool, sz);
  return pool;
}
struct pool* pool_create(void (*on_pool_destroy)(struct pool*)) {
  struct pool *pool = pool_create_empty();
  pool->on_pool_destroy = on_pool_destroy;
  return pool;
}
void pool_destroy(struct pool *pool) {
  if (!pool) {
    return;
  }
  if (pool->on_pool_destroy) {
    pool->on_pool_destroy(pool);
  }
  for (struct pool_unit *u = pool->unit, *next; u; u = next) {
    next = u->next;
    free(u->heap);
    free(u);
  }
  free(pool);
}
static int _extend(struct pool *pool, size_t siz) {
  struct pool_unit *nunit = xmalloc(sizeof(*nunit));
  siz = ROUNDUP(siz, _UNIT_ALIGN_SIZE);
  nunit->heap = xmalloc(siz);
  nunit->next = pool->unit;
  pool->heap = nunit->heap;
  pool->unit = nunit;
  pool->usiz = 0;
  pool->asiz = siz;
  return 1;
}
void* pool_alloc(struct pool *pool, size_t siz) {
  siz = ROUNDUP(siz, _UNIT_ALIGN_SIZE);
  size_t usiz = pool->usiz + siz;
  void *h = pool->heap;
  if (usiz > pool->asiz) {
    usiz = usiz + pool->asiz;
    if (!_extend(pool, usiz)) {
      return 0;
    }
    h = pool->heap;
  }
  pool->usiz += siz;
  pool->heap += siz;
  return h;
}
void* pool_calloc(struct pool *pool, size_t siz) {
  void *p = pool_alloc(pool, siz);
  memset(p, 0, siz);
  return p;
}
char* pool_strdup(struct pool *pool, const char *str) {
  if (str) {
    size_t len = strlen(str);
    char *ret = pool_alloc(pool, len + 1);
    memcpy(ret, str, len);
    ret[len] = '\0';
    return ret;
  } else {
    return 0;
  }
}
char* pool_strndup(struct pool *pool, const char *str, size_t len) {
  if (str) {
    len = strnlen(str, len);
    char *ret = pool_alloc(pool, len + 1);
    memcpy(ret, str, len);
    ret[len] = '\0';
    return ret;
  } else {
    return 0;
  }
}
static inline int _printf_estimate_size(const char *format, va_list ap) {
  char buf[1];
  return vsnprintf(buf, sizeof(buf), format, ap) + 1;
}
static char* _printf_va(struct pool *pool, unsigned size, const char *format, va_list ap) {
  char *wbuf = pool_alloc(pool, size);
  if (!wbuf) {
    return 0;
  }
  vsnprintf(wbuf, size, format, ap);
  return wbuf;
}
char* pool_printf_va(struct pool *pool, const char *format, va_list va) {
  va_list cva;
  va_copy(cva, va);
  int size = _printf_estimate_size(format, va);
  va_end(va);
  char *res = _printf_va(pool, size, format, cva);
  va_end(cva);
  return res;
}
char* pool_printf(struct pool *pool, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int size = _printf_estimate_size(fmt, ap);
  va_end(ap);
  va_start(ap, fmt);
  char *res = _printf_va(pool, size, fmt, ap);
  va_end(ap);
  return res;
}
const char** pool_split_string(
  struct pool *pool,
  const char  *haystack,
  const char  *split_chars,
  int          ignore_whitespace) {
  size_t hsz = strlen(haystack);
  const char **ret = (const char**) pool_alloc(pool, (hsz + 1) * sizeof(char*));
  const char *sp = haystack;
  const char *ep = sp;
  int j = 0;
  for (int i = 0; *ep; ++i, ++ep) {
    const char ch = haystack[i];
    const char *sch = strchr(split_chars, ch);
    if ((ep >= sp) && (sch || (*(ep + 1) == '\0'))) {
      if (!sch && (*(ep + 1) == '\0')) {
        ++ep;
      }
      if (ignore_whitespace) {
        while (utils_char_is_space(*sp)) ++sp;
        while (utils_char_is_space(*(ep - 1))) --ep;
      }
      if (ep >= sp) {
        char *s = pool_alloc(pool, ep - sp + 1);
        memcpy(s, sp, ep - sp);
        s[ep - sp] = '\0';
        ret[j++] = s;
        ep = haystack + i;
      }
      sp = haystack + i + 1;
    }
  }
  ret[j] = 0;
  return ret;
}
#ifndef _AMALGAMATE_
#include "basedefs.h"
#include "log.h"
#include "xstr.h"
#include "alloc.h"
#include "utils.h"
#include "env.h"
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#endif
#define LEVEL_VERBOSE 0
#define LEVEL_INFO    1
#define LEVEL_WARN    2
#define LEVEL_ERROR   3
#define LEVEL_FATAL   4
static const char* _error_get(int code) {
  switch (code) {
    case AK_ERROR_OVERFLOW:
      return "Value/argument overflow. (AK_ERROR_OVERFLOW)";
    case AK_ERROR_ASSERTION:
      return "Assertion failed (AK_ERROR_ASSERTION)";
    case AK_ERROR_INVALID_ARGS:
      return "Invalid function arguments (AKL_ERROR_INVALID_ARGS)";
    case AK_ERROR_FAIL:
      return "Fail (AK_ERROR_FAIL)";
    case AK_ERROR_IO:
      return "IO Error (AK_ERROR_IO)";
    case AK_ERROR_UNIMPLEMETED:
      return "Not implemented (AK_ERROR_UNIMPLEMETED)";
    case AK_ERROR_SCRIPT_SYNTAX:
      return "Invalid autark config syntax (AK_ERROR_SCRIPT_SYNTAX)";
    case AK_ERROR_SCRIPT:
      return "Autark script error (AK_ERROR_SCRIPT)";
    case AK_ERROR_CYCLIC_BUILD_DEPS:
      return "Detected cyclic build dependency (AK_ERROR_CYCLIC_BUILD_DEPS)";
    case AK_ERROR_SCRIPT_ERROR:
      return "Build script error (AK_ERROR_SCRIPT_ERROR)";
    case AK_ERROR_NOT_IMPLEMENTED:
      return "Not implemented (AK_ERROR_NOT_IMPLEMENTED)";
    case AK_ERROR_EXTERNAL_COMMAND:
      return "External command failed (AK_ERROR_EXTERNAL_COMMAND)";
    case AK_ERROR_DEPENDENCY_UNRESOLVED:
      return "I don't know how to build dependency (AK_ERROR_DEPENDENCY_UNRESOLVED)";
    case AK_ERROR_OK:
      return "OK";
    default:
      return strerror(code);
  }
}
static char* _log_basename(char *path) {
  size_t i;
  if (!path || *path == '\0') {
    return ".";
  }
  i = strlen(path) - 1;
  for ( ; i && path[i] == '/'; i--) path[i] = 0;
  for ( ; i && path[i - 1] != '/'; i--) ;
  return path + i;
}
static void _event_va(
  int         level,
  const char *file,
  int         line,
  int         code,
  const char *format,
  va_list     va) {
  struct xstr *xstr = 0;
  xstr = xstr_create_empty();
  switch (level) {
    case LEVEL_FATAL:
      xstr_cat2(xstr, "FATAL   ", LLEN("FATAL   "));
      break;
    case LEVEL_INFO:
      xstr_cat2(xstr, "INFO    ", LLEN("INFO    "));
      break;
    case LEVEL_ERROR:
      xstr_cat2(xstr, "ERROR    ", LLEN("ERROR    "));
      break;
    case LEVEL_WARN:
      xstr_cat2(xstr, "WARN     ", LLEN("WARN    "));
      break;
    case LEVEL_VERBOSE:
      xstr_cat2(xstr, "VERBOSE ", LLEN("VERBOSE "));
      break;
    default:
      xstr_cat2(xstr, "XXX     ", LLEN("XXX     "));
      break;
  }
  if (file) {
    char *cfile = xstrdup(file);
    if (cfile) {
      char *p = _log_basename(cfile);
      xstr_printf(xstr, "%s:%d ", p, line);
      free(cfile);
    }
  }
  if (code) {
    const char *err = _error_get(code);
    xstr_printf(xstr, "%d|", code);
    if (err) {
      xstr_cat(xstr, err);
    }
  }
  if (format) {
    if (code) {
      xstr_cat2(xstr, "|", 1);
    }
    xstr_printf_va(xstr, format, va);
  }
  xstr_cat2(xstr, "\n", 1);
  utils_chars_replace(xstr_ptr(xstr), '\1', '^');
  fputs(xstr_ptr(xstr), stderr);
  xstr_destroy(xstr);
}
void akfatal2(const char *msg) {
  if (msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
  }
  _exit(254);
}
void _akfatal(const char *file, int line, int code, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_FATAL, file, line, code, fmt, ap);
  va_end(ap);
  akfatal2(0);
}
int _akerror(const char *file, int line, int code, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_ERROR, file, line, code, fmt, ap);
  va_end(ap);
  return code;
}
void _akverbose(const char *file, int line, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_VERBOSE, file, line, 0, fmt, ap);
  va_end(ap);
}
void akinfo(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_INFO, 0, 0, 0, fmt, ap);
  va_end(ap);
}
void akwarn(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_WARN, 0, 0, 0, fmt, ap);
  va_end(ap);
}
void _akerror_va(const char *file, int line, int code, const char *fmt, va_list ap) {
  _event_va(LEVEL_ERROR, file, line, code, fmt, ap);
}
void _akfatal_va(const char *file, int line, int code, const char *fmt, va_list ap) {
  _event_va(LEVEL_FATAL, file, line, code, fmt, ap);
  akfatal2(0);
}
#ifndef _AMALGAMATE_
#include "map.h"
#include "log.h"
#include "alloc.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#endif
#define MIN_BUCKETS 64
#define STEPS       4
struct _map_entry {
  void    *key;
  void    *val;
  uint32_t hash;
};
struct _map_bucket {
  struct _map_entry *entries;
  uint32_t      used;
  uint32_t      total;
};
struct map {
  uint32_t       count;
  uint32_t       buckets_mask;
  struct _map_bucket *buckets;
  int      (*cmp_fn)(const void*, const void*);
  uint32_t (*hash_key_fn)(const void*);
  void     (*kv_free_fn)(void*, void*);
  int int_key_as_pointer_value;
};
static void _map_noop_kv_free(void *key, void *val) {
}
static void _map_noop_uint64_kv_free(void *key, void *val) {
  if (key) {
    free(key);
  }
}
static inline uint32_t _map_n_buckets(struct map *hm) {
  return hm->buckets_mask + 1;
}
static int _map_ptr_cmp(const void *v1, const void *v2) {
  return v1 > v2 ? 1 : v1 < v2 ? -1 : 0;
}
static int _map_uint32_cmp(const void *v1, const void *v2) {
  intptr_t p1 = (intptr_t) v1;
  intptr_t p2 = (intptr_t) v2;
  return p1 > p2 ? 1 : p1 < p2 ? -1 : 0;
}
static int _map_uint64_cmp(const void *v1, const void *v2) {
  if (sizeof(uintptr_t) >= sizeof(uint64_t)) {
    intptr_t p1 = (intptr_t) v1;
    intptr_t p2 = (intptr_t) v2;
    return p1 > p2 ? 1 : p1 < p2 ? -1 : 0;
  } else {
    uint64_t l1, l2;
    memcpy(&l1, v1, sizeof(l1));
    memcpy(&l2, v2, sizeof(l2));
    return l1 > l2 ? 1 : l1 < l2 ? -1 : 0;
  }
}
static inline uint32_t _map_hash_uint32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x85ebca6bU;
  x ^= x >> 13;
  x *= 0xc2b2ae35U;
  x ^= x >> 16;
  return x;
}
static inline uint32_t _map_hash_uint64(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}
static inline uint32_t _map_hash_uint64_key(const void *key) {
  uint64_t lv = 0;
  memcpy(&lv, key, sizeof(void*));
  return _map_hash_uint64(lv);
}
static inline uint32_t _map_hash_uint32_key(const void *key) {
  return _map_hash_uint32((uintptr_t) key);
}
static inline uint32_t _map_hash_buf_key(const void *key) {
  const char *str = key;
  uint32_t hash = 2166136261U;
  while (*str) {
    hash ^= (uint8_t) (*str);
    hash *= 16777619U;
    str++;
  }
  return hash;
}
static struct _map_entry* _map_entry_add(struct map *hm, void *key, uint32_t hash) {
  struct _map_entry *entry;
  struct _map_bucket *bucket = hm->buckets + (hash & hm->buckets_mask);
  if (bucket->used + 1 >= bucket->total) {
    if (UINT32_MAX - bucket->total < STEPS) {
      akfatal(AK_ERROR_OVERFLOW, 0, 0, 0);
    }
    uint32_t new_total = bucket->total + STEPS;
    struct _map_entry *new_entries = xrealloc(bucket->entries, new_total * sizeof(new_entries[0]));
    bucket->entries = new_entries;
    bucket->total = new_total;
  }
  entry = bucket->entries;
  for (struct _map_entry *end = entry + bucket->used; entry < end; ++entry) {
    if ((hash == entry->hash) && (hm->cmp_fn(key, entry->key) == 0)) {
      return entry;
    }
  }
  ++bucket->used;
  ++hm->count;
  entry->hash = hash;
  entry->key = 0;
  entry->val = 0;
  return entry;
}
static struct _map_entry* _map_entry_find(struct map *hm, const void *key, uint32_t hash) {
  struct _map_bucket *bucket = hm->buckets + (hash & hm->buckets_mask);
  struct _map_entry *entry = bucket->entries;
  for (struct _map_entry *end = entry + bucket->used; entry < end; ++entry) {
    if (hash == entry->hash && hm->cmp_fn(key, entry->key) == 0) {
      return entry;
    }
  }
  return 0;
}
static void _map_rehash(struct map *hm, uint32_t num_buckets) {
  struct _map_bucket *buckets = xcalloc(num_buckets, sizeof(*buckets));
  struct _map_bucket *bucket,
                *bucket_end = hm->buckets + _map_n_buckets(hm);
  struct map hm_copy = *hm;
  hm_copy.count = 0;
  hm_copy.buckets_mask = num_buckets - 1;
  hm_copy.buckets = buckets;
  for (bucket = hm->buckets; bucket < bucket_end; ++bucket) {
    struct _map_entry *entry_old = bucket->entries;
    if (entry_old) {
      struct _map_entry *entry_old_end = entry_old + bucket->used;
      for ( ; entry_old < entry_old_end; ++entry_old) {
        struct _map_entry *entry_new = _map_entry_add(&hm_copy, entry_old->key, entry_old->hash);
        entry_new->key = entry_old->key;
        entry_new->val = entry_old->val;
      }
    }
  }
  for (bucket = hm->buckets; bucket < bucket_end; ++bucket) {
    free(bucket->entries);
  }
  free(hm->buckets);
  hm->buckets = buckets;
  hm->buckets_mask = num_buckets - 1;
  return;
}
static void _map_entry_remove(struct map *hm, struct _map_bucket *bucket, struct _map_entry *entry) {
  hm->kv_free_fn(hm->int_key_as_pointer_value ? 0 : entry->key, entry->val);
  if (bucket->used > 1) {
    struct _map_entry *entry_last = bucket->entries + bucket->used - 1;
    if (entry != entry_last) {
      memcpy(entry, entry_last, sizeof(*entry));
    }
  }
  --bucket->used;
  --hm->count;
  if ((hm->buckets_mask > MIN_BUCKETS - 1) && (hm->count < hm->buckets_mask / 2)) {
    _map_rehash(hm, _map_n_buckets(hm) / 2);
  } else {
    uint32_t steps_used = bucket->used / STEPS;
    uint32_t steps_total = bucket->total / STEPS;
    if (steps_used + 1 < steps_total) {
      struct _map_entry *entries_new = realloc(bucket->entries, ((size_t) steps_used + 1) * STEPS * sizeof(entries_new[0]));
      if (entries_new) {
        bucket->entries = entries_new;
        bucket->total = (steps_used + 1) * STEPS;
      }
    }
  }
}
void map_kv_free(void *key, void *val) {
  free(key);
  free(val);
}
void map_k_free(void *key, void *val) {
  free(key);
}
struct map* map_create(
  int (*cmp_fn)(const void*, const void*),
  uint32_t (*hash_key_fn)(const void*),
  void (*kv_free_fn)(void*, void*)) {
  if (!hash_key_fn) {
    return 0;
  }
  if (!cmp_fn) {
    cmp_fn = _map_ptr_cmp;
  }
  if (!kv_free_fn) {
    kv_free_fn = _map_noop_kv_free;
  }
  struct map *hm = xmalloc(sizeof(*hm));
  hm->buckets = xcalloc(MIN_BUCKETS, sizeof(hm->buckets[0]));
  hm->cmp_fn = cmp_fn;
  hm->hash_key_fn = hash_key_fn;
  hm->kv_free_fn = kv_free_fn;
  hm->buckets_mask = MIN_BUCKETS - 1;
  hm->count = 0;
  hm->int_key_as_pointer_value = 0;
  return hm;
}
void map_destroy(struct map *hm) {
  if (hm) {
    for (struct _map_bucket *b = hm->buckets, *be = hm->buckets + _map_n_buckets(hm); b < be; ++b) {
      if (b->entries) {
        for (struct _map_entry *e = b->entries, *ee = b->entries + b->used; e < ee; ++e) {
          hm->kv_free_fn(hm->int_key_as_pointer_value ? 0 : e->key, e->val);
        }
        free(b->entries);
      }
    }
    free(hm->buckets);
    free(hm);
  }
}
struct map* map_create_u64(void (*kv_free_fn)(void*, void*)) {
  if (!kv_free_fn) {
    kv_free_fn = _map_noop_uint64_kv_free;
  }
  struct map *hm = map_create(_map_uint64_cmp, _map_hash_uint64_key, kv_free_fn);
  if (hm) {
    if (sizeof(uintptr_t) >= sizeof(uint64_t)) {
      hm->int_key_as_pointer_value = 1;
    }
  }
  return hm;
}
struct map* map_create_u32(void (*kv_free_fn)(void*, void*)) {
  struct map *hm = map_create(_map_uint32_cmp, _map_hash_uint32_key, kv_free_fn);
  if (hm) {
    hm->int_key_as_pointer_value = 1;
  }
  return hm;
}
struct map* map_create_str(void (*kv_free_fn)(void*, void*)) {
  return map_create((int (*)(const void*, const void*)) strcmp, _map_hash_buf_key, kv_free_fn);
}
void map_put(struct map *hm, void *key, void *val) {
  uint32_t hash = hm->hash_key_fn(key);
  struct _map_entry *entry = _map_entry_add(hm, key, hash);
  hm->kv_free_fn(hm->int_key_as_pointer_value ? 0 : entry->key, entry->val);
  entry->key = key;
  entry->val = val;
  if (hm->count > hm->buckets_mask) {
    _map_rehash(hm, _map_n_buckets(hm) * 2);
  }
}
void map_put_u32(struct map *hm, uint32_t key, void *val) {
  map_put(hm, (void*) (uintptr_t) key, val);
}
void map_put_u64(struct map *hm, uint64_t key, void *val) {
  if (hm->int_key_as_pointer_value) {
    map_put(hm, (void*) (uintptr_t) key, val);
  } else {
    uint64_t *kv = xmalloc(sizeof(*kv));
    memcpy(kv, &key, sizeof(*kv));
    map_put(hm, kv, val);
  }
}
void map_put_str(struct map *hm, const char *key_, void *val) {
  char *key = xstrdup(key_);
  map_put(hm, key, val);
}
void map_put_str_no_copy(struct map *hm, const char *key, void *val) {
  map_put(hm, (void*) key, val);
}
int map_remove(struct map *hm, const void *key) {
  uint32_t hash = hm->hash_key_fn(key);
  struct _map_bucket *bucket = hm->buckets + (hash & hm->buckets_mask);
  struct _map_entry *entry = _map_entry_find(hm, key, hash);
  if (entry) {
    _map_entry_remove(hm, bucket, entry);
    return 1;
  } else {
    return 0;
  }
}
int map_remove_u64(struct map *hm, uint64_t key) {
  if (hm->int_key_as_pointer_value) {
    return map_remove(hm, (void*) (uintptr_t) key);
  } else {
    return map_remove(hm, &key);
  }
}
int map_remove_u32(struct map *hm, uint32_t key) {
  return map_remove(hm, (void*) (uintptr_t) key);
}
void* map_get(struct map *hm, const void *key) {
  uint32_t hash = hm->hash_key_fn(key);
  struct _map_entry *entry = _map_entry_find(hm, key, hash);
  if (entry) {
    return entry->val;
  } else {
    return 0;
  }
}
void* map_get_u64(struct map *hm, uint64_t key) {
  if (hm->int_key_as_pointer_value) {
    return map_get(hm, (void*) (intptr_t) key);
  } else {
    return map_get(hm, &key);
  }
}
void* map_get_u32(struct map *hm, uint32_t key) {
  return map_get(hm, (void*) (intptr_t) key);
}
uint32_t map_count(const struct map *hm) {
  return hm->count;
}
void map_clear(struct map *hm) {
  if (!hm) {
    return;
  }
  for (struct _map_bucket *b = hm->buckets, *be = hm->buckets + _map_n_buckets(hm); b < be; ++b) {
    for (struct _map_entry *e = b->entries, *ee = b->entries + b->used; e < ee; ++e) {
      hm->kv_free_fn(hm->int_key_as_pointer_value ? 0 : e->key, e->val);
    }
    free(b->entries);
    b->used = 0;
    b->total = 0;
    b->entries = 0;
  }
  if (_map_n_buckets(hm) > MIN_BUCKETS) {
    struct _map_bucket *buckets_new = realloc(hm->buckets, sizeof(buckets_new[0]) * MIN_BUCKETS);
    if (buckets_new) {
      memset(buckets_new, 0, sizeof(buckets_new[0]) * MIN_BUCKETS);
      hm->buckets = buckets_new;
      hm->buckets_mask = MIN_BUCKETS - 1;
    }
  }
  hm->count = 0;
}
void map_iter_init(struct map *hm, struct map_iter *iter) {
  iter->hm = hm;
  iter->entry = -1;
  iter->bucket = 0;
  iter->key = 0;
  iter->val = 0;
}
int map_iter_next(struct map_iter *iter) {
  if (!iter->hm) {
    return 0;
  }
  struct _map_entry *entry;
  struct _map_bucket *bucket = iter->hm->buckets + iter->bucket;
  ++iter->entry;
  if ((uint32_t) iter->entry >= bucket->used) {
    uint32_t n = _map_n_buckets(iter->hm);
    iter->entry = 0;
    for (++iter->bucket; iter->bucket < n; ++iter->bucket) {
      bucket = iter->hm->buckets + iter->bucket;
      if (bucket->used > 0) {
        break;
      }
    }
    if (iter->bucket >= n) {
      return 0;
    }
  }
  entry = bucket->entries + iter->entry;
  iter->key = entry->key;
  iter->val = entry->val;
  return 1;
}
#ifndef _AMALGAMATE_
#include "utils.h"
#include "xstr.h"
#include "log.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#endif
struct value utils_file_as_buf(const char *path, ssize_t buflen_max) {
  struct value ret = { 0 };
  struct xstr *xstr = xstr_create_empty();
  char buf[8192];
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    ret.error = errno;
    xstr_destroy(xstr);
    return ret;
  }
  while (buflen_max != 0 && ret.error == 0) {
    ssize_t rb = read(fd, buf, sizeof(buf));
    if (rb > 0) {
      if (buflen_max > -1) {
        if (rb > buflen_max) {
          rb = buflen_max;
          ret.error = AK_ERROR_OVERFLOW;
        }
        buflen_max -= rb;
      }
      xstr_cat2(xstr, buf, rb);
    } else if (rb == -1) {
      if (errno != EINTR && errno != EAGAIN) {
        ret.error = errno;
        break;
      }
    } else {
      break;
    }
  }
  ret.len = xstr_size(xstr);
  ret.buf = xstr_destroy_keep_ptr(xstr);
  return ret;
}
int utils_file_write_buf(const char *path, const char *buf, size_t len, bool append) {
  int flags = O_WRONLY | O_CREAT;
  if (append) {
    flags |= O_APPEND;
  } else {
    flags |= O_TRUNC;
  }
  int fd = open(path, flags);
  if (fd == -1) {
    return errno;
  }
  for (ssize_t w, tow = len; tow > 0; ) {
    w = write(fd, buf + len - tow, tow);
    if (w >= 0) {
      tow -= w;
    } else if (w < 0) {
      if (errno == EAGAIN) {
        continue;
      }
      int ret = errno;
      close(fd);
      return ret;
    }
  }
  close(fd);
  return 0;
}
int utils_copy_file(const char *src, const char *dst) {
  int rc = 0;
  char buf[8192];
  FILE *sf = fopen(src, "rb");
  if (!sf) {
    return errno;
  }
  FILE *df = fopen(dst, "wb");
  if (!df) {
    rc = errno;
    fclose(sf);
    return rc;
  }
  size_t nr = 0;
  while (1) {
    nr = fread(buf, 1, sizeof(buf), sf);
    if (nr) {
      nr = fwrite(buf, 1, nr, df);
      if (!nr) {
        rc = AK_ERROR_IO;
        break;
      }
    } else if (feof(sf)) {
      break;
    } else if (ferror(sf)) {
      rc = AK_ERROR_IO;
      break;
    }
  }
  fclose(sf);
  fclose(df);
  return rc;
}
int utils_rename_file(const char *src, const char *dst) {
  if (rename(src, dst) == -1) {
    if (errno == EXDEV) {
      int rc = utils_copy_file(src, dst);
      if (!rc) {
        unlink(src);
      }
      return rc;
    } else {
      return errno;
    }
  }
  return 0;
}
long int utils_strtol(const char *v, int base, int *rcp) {
  *rcp = 0;
  char *ep = 0;
  long int ret = strtol(v, &ep, base);
  if (*ep != '\0' || errno == ERANGE) {
    *rcp = AK_ERROR_INVALID_ARGS;
    return 0;
  }
  return ret;
}
long long utils_strtoll(const char *v, int base, int *rcp) {
  *rcp = 0;
  char *ep = 0;
  long long ret = strtoll(v, &ep, base);
  if ((*ep != '\0' && *ep != '\n') || errno == ERANGE) {
    *rcp = AK_ERROR_INVALID_ARGS;
    return 0;
  }
  return ret;
}
void utils_split_values_add(const char *v, struct xstr *xstr) {
  if (is_vlist(v)) {
    xstr_cat(xstr, v);
    return;
  }
  char buf[strlen(v) + 1];
  const char *p = v;
  while (*p) {
    while (utils_char_is_space(*p)) ++p;
    if (*p == '\0') {
      break;
    }
    char *w = buf;
    char q = 0;
    while (*p && (q || !utils_char_is_space(*p))) {
      if (*p == '\\') {
        ++p;
        if (*p) {
          *w++ = *p++;
        }
      } else if (q) {
        if (*p == q) {
          q = 0;
          ++p;
        } else {
          *w++ = *p++;
        }
      } else if (*p == '\'' || *p == '"') {
        q = *p++;
      } else {
        *w++ = *p++;
      }
    }
    *w = '\0';
    xstr_cat(xstr, "\1");
    xstr_cat(xstr, buf);
  }
}
int utils_fd_make_non_blocking(int fd) {
  int rci, flags;
  while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR) ;
  if (flags == -1) {
    return errno;
  }
  while ((rci = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR) ;
  if (rci == -1) {
    return errno;
  }
  return 0;
}
#ifndef _AMALGAMATE_
#include "paths.h"
#include "xstr.h"
#include "alloc.h"
#include "utils.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>
#include <dirent.h>
#endif
#ifdef __APPLE__
#define st_atim st_atimespec
#define st_ctim st_ctimespec
#define st_mtim st_mtimespec
#endif
#define _TIMESPEC2MS(ts__) (((ts__).tv_sec * 1000ULL) + utils_lround((ts__).tv_nsec / 1.0e6))
bool path_is_absolute(const char *path) {
  if (!path) {
    return 0;
  }
  return *path == '/';
}
bool path_is_accesible(const char *path, enum akpath_access a) {
  if (!path || *path == '\0') {
    return 0;
  }
  int mode = 0;
  if (a & AKPATH_READ) {
    mode |= R_OK;
  }
  if (a & AKPATH_WRITE) {
    mode |= W_OK;
  }
  if (a & AKPATH_EXEC) {
    mode |= X_OK;
  }
  if (mode == 0) {
    mode = F_OK;
  }
  return access(path, mode) == 0;
}
bool path_is_accesible_read(const char *path) {
  return path_is_accesible(path, AKPATH_READ);
}
bool path_is_accesible_write(const char *path) {
  return path_is_accesible(path, AKPATH_WRITE);
}
bool path_is_accesible_exec(const char *path) {
  return path_is_accesible(path, AKPATH_EXEC);
}
const char* path_real(const char *path, char buf[PATH_MAX]) {
  return realpath(path, buf);
}
const char* path_real_pool(const char *path, struct pool *pool) {
  char buf[PATH_MAX];
  if (path_real(path, buf)) {
    return pool_strdup(pool, buf);
  } else {
    return 0;
  }
}
char* path_normalize(const char *path, char buf[PATH_MAX]) {
  char cwd[PATH_MAX];
  if (!getcwd(cwd, PATH_MAX)) {
    akfatal(errno, "Cannot get CWD", 0);
  }
  return path_normalize_cwd(path, cwd, buf);
}
char* path_normalize_cwd(const char *path, const char *cwd, char buf[PATH_MAX]) {
  akassert(cwd);
  if (path[0] == '/') {
    utils_strncpy(buf, path, PATH_MAX);
    char *p = strchr(buf, '.');
    if (  p == 0
       || !(*(p - 1) == '/' && (p[1] == '/' || p[1] == '.' || p[1] == '\0'))) {
      return buf;
    }
  } else {
    utils_strncpy(buf, cwd, PATH_MAX);
    size_t len = strlen(buf);
    if (len < PATH_MAX - 1) {
      buf[len] = '/';
      buf[len + 1] = '\0';
      strncat(buf, path, PATH_MAX - len - 2);
    } else {
      errno = ENAMETOOLONG;
      akfatal(errno, "Failed to normalize path, name is too long: %s", path);
    }
  }
  // Normalize: split and process each segment
  const char *rp = buf;
  char *segments[PATH_MAX];
  int top = 0;
  while (*rp) {
    while (*rp == '/') ++rp;
    if (!*rp) {
      break;
    }
    const char *sp = rp;
    while (*rp && *rp != '/') ++rp;
    size_t len = rp - sp;
    if (len == 0) {
      continue;
    }
    char segment[PATH_MAX];
    memcpy(segment, sp, len);
    segment[len] = '\0';
    if (strcmp(segment, ".") == 0) {
      continue;
    } else if (strcmp(segment, "..") == 0) {
      if (top > 0) {
        free(segments[--top]);
      }
    } else {
      segments[top++] = xstrdup(segment);
    }
  }
  if (top == 0) {
    buf[0] = '/';
    buf[1] = '\0';
  } else {
    buf[0] = '\0';
    for (int i = 0; i < top; ++i) {
      strcat(buf, "/");
      strcat(buf, segments[i]);
      free(segments[i]);
    }
  }
  return buf;
}
char* path_normalize_pool(const char *path, struct pool *pool) {
  char cwd[PATH_MAX];
  if (!getcwd(cwd, PATH_MAX)) {
    akfatal(errno, "Cannot get CWD", 0);
  }
  return path_normalize_cwd_pool(path, cwd, pool);
}
char* path_normalize_cwd_pool(const char *path, const char *cwd, struct pool *pool) {
  char buf[PATH_MAX];
  path_normalize_cwd(path, cwd, buf);
  return pool_strdup(pool, buf);
}
int path_mkdirs(const char *path) {
  if ((path[0] == '.' && path[1] == '\0') || path_is_exist(path)) {
    return 0;
  }
  int rc = 0;
  const size_t len = strlen(path);
  char buf[len + 1];
  char *rp = buf;
  memcpy(rp, path, len + 1);
  for (char *p = rp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(rp, S_IRWXU) != 0) {
        if (errno != EEXIST) {
          rc = errno;
          goto finish;
        }
      }
      *p = '/';
    }
  }
  if (mkdir(rp, S_IRWXU) != 0) {
    if (errno != EEXIST) {
      rc = errno;
      goto finish;
    }
  }
finish:
  return rc;
}
int path_mkdirs_for(const char *path) {
  char buf[PATH_MAX];
  utils_strncpy(buf, path, sizeof(buf));
  char *dir = path_dirname(buf);
  return path_mkdirs(dir);
}
static inline bool _is_autark_dist_root(const char *path) {
  return utils_endswith(path, "/.autark-dist");
}
static void _rm_dir_recursive(const char *path) {
  struct stat st;
  if (lstat(path, &st)) {
    perror(path);
    return;
  }
  if (!S_ISDIR(st.st_mode)) {
    if (unlink(path) != 0) {
      perror(path);
      return;
    }
    return;
  }
  DIR *dir = opendir(path);
  if (!dir) {
    perror(path);
    return;
  }
  // Use dynamic allocation to avoid stack overflow early
  char *child = xmalloc(PATH_MAX);
  for (struct dirent *entry; (entry = readdir(dir)) != 0; ) {
    const char *name = entry->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }
    snprintf(child, PATH_MAX, "%s/%s", path, name);
    _rm_dir_recursive(child);
  }
  closedir(dir);
  free(child);
  if (rmdir(path)) {
    perror(path);
  }
}
int path_rm_cache(const char *path) {
  char resolved[PATH_MAX];
  char child[PATH_MAX];
  if (!path_is_exist(path)) {
    return 0;
  }
  if (!realpath(path, resolved)) {
    return errno;
  }
  DIR *dir = opendir(resolved);
  if (!dir) {
    return errno;
  }
  for (struct dirent *entry; (entry = readdir(dir)) != 0; ) {
    const char *name = entry->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }
    snprintf(child, sizeof(child), "%s/%s", path, name);
    if (!_is_autark_dist_root(child)) {
      _rm_dir_recursive(child);
    }
  }
  closedir(dir);
  return 0;
}
static int _stat(const char *path, int fd, struct akpath_stat *fs) {
  struct stat st = { 0 };
  memset(fs, 0, sizeof(*fs));
  if (path) {
    if (stat(path, &st)) {
      if (errno == ENOENT) {
        fs->ftype = AKPATH_NOT_EXISTS;
        return 0;
      } else {
        return errno;
      }
    }
  } else {
    if (fstat(fd, &st)) {
      if (errno == ENOENT) {
        fs->ftype = AKPATH_NOT_EXISTS;
        return 0;
      } else {
        return errno;
      }
    }
  }
  fs->atime = _TIMESPEC2MS(st.st_atim);
  fs->mtime = _TIMESPEC2MS(st.st_mtim);
  fs->ctime = _TIMESPEC2MS(st.st_ctim);
  fs->size = (uint64_t) st.st_size;
  if (S_ISREG(st.st_mode)) {
    fs->ftype = AKPATH_TYPE_FILE;
  } else if (S_ISDIR(st.st_mode)) {
    fs->ftype = AKPATH_TYPE_DIR;
  } else if (S_ISLNK(st.st_mode)) {
    fs->ftype = AKPATH_LINK;
  } else {
    fs->ftype = AKPATH_OTHER;
  }
  return 0;
}
int path_stat(const char *path, struct akpath_stat *stat) {
  return _stat(path, -1, stat);
}
int path_stat_fd(int fd, struct akpath_stat *stat) {
  return _stat(0, fd, stat);
}
int path_stat_file(FILE *file, struct akpath_stat *stat) {
  return _stat(0, fileno(file), stat);
}
uint64_t path_mtime(const char *path) {
  struct akpath_stat st;
  if (path_stat(path, &st)) {
    return 0;
  }
  return st.mtime;
}
bool path_is_dir(const char *path) {
  struct akpath_stat st;
  if (path_stat(path, &st)) {
    return 0;
  }
  return st.ftype == AKPATH_TYPE_DIR;
}
bool path_is_file(const char *path) {
  struct akpath_stat st;
  if (path_stat(path, &st)) {
    return 0;
  }
  return st.ftype == AKPATH_TYPE_FILE;
}
bool path_is_exist(const char *path) {
  struct akpath_stat st;
  if (path_stat(path, &st)) {
    return 0;
  }
  return st.ftype != AKPATH_NOT_EXISTS;
}
static inline int _path_num_segments(const char *path) {
  int c = 0;
  for (const char *rp = path; *rp != '\0'; ++rp) {
    if (*rp == '/' || *rp == '\0') {
      ++c;
    }
  }
  return c;
}
char* path_relativize(const char *from, const char *to) {
  char cwd[PATH_MAX];
  if (!getcwd(cwd, PATH_MAX)) {
    akfatal(errno, "Cannot get CWD", 0);
  }
  return path_relativize_cwd(from, to, cwd);
}
char* path_relativize_cwd(const char *from_, const char *to_, const char *cwd) {
  char from[PATH_MAX], to[PATH_MAX];
  if (from_ == 0) {
    from_ = cwd;
  }
  path_normalize_cwd(from_, cwd, from);
  path_normalize_cwd(to_, cwd, to);
  akassert(*from == '/' && *to == '/');
  int sf, sc;
  struct xstr *xstr = xstr_create_empty();
  const char *frp = from, *trp = to, *srp = to;
  for (++frp, ++trp, sc = 0, sf = _path_num_segments(frp - 1); *frp == *trp; ++frp, ++trp) {
    if (*frp == '/' || *frp == '\0') {
      ++sc;
      srp = trp;
      if (*frp == '\0') {
        break;
      }
    }
  }
  if ((*frp == '\0' && *trp == '/') || (*frp == '/' && *trp == '\0')) {
    ++sc;
    srp = trp;
  }
  for (int i = 0, l = sf - sc; i < l; ++i) {
    xstr_cat2(xstr, "../", AK_LLEN("../"));
  }
  if (*srp != '\0') {
    xstr_cat(xstr, srp + 1);
  }
  return xstr_destroy_keep_ptr(xstr);
}
char* path_dirname(char *path) {
  return dirname(path);
}
char* path_basename(char *path) {
  size_t i;
  if (!path || *path == '\0') {
    return ".";
  }
  i = strlen(path) - 1;
  for ( ; i && path[i] == '/'; i--) path[i] = 0;
  for ( ; i && path[i - 1] != '/'; i--) ;
  return path + i;
}
const char* path_is_prefix_for(const char *prefix, const char *path, const char *cwd) {
  akassert(path_is_absolute(prefix));
  char buf[PATH_MAX];
  if (!path_is_absolute(path)) {
    char cwdbuf[PATH_MAX];
    if (!cwd) {
      cwd = getcwd(cwdbuf, sizeof(cwdbuf));
    }
    path = path_normalize_cwd(path, cwd, buf);
  }
  int len = strlen(prefix);
  if (len < 1 || !utils_startswith(path, prefix)) {
    return 0;
  }
  const char *p = path + len;
  if (*p == '/') {
    while (*p == '/') ++p;
    return p;
  } else if (prefix[len - 1] == '/') {
    return p;
  } else {
    return 0;
  }
}
#ifndef _AMALGAMATE_
#include "spawn.h"
#include "log.h"
#include "ulist.h"
#include "pool.h"
#include "xstr.h"
#include "env.h"
#include "utils.h"
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
extern char **environ;
struct spawn {
  pid_t pid;
  int   wstatus;
  void *user_data;
  struct pool *pool;
  struct ulist args;
  struct ulist env;
  const char  *exec;
  char *path_overriden;
  bool  nowait;
  size_t (*stdin_provider)(char *buf, size_t buflen, struct spawn*);
  void   (*stdout_handler)(char *buf, size_t buflen, struct spawn*);
  void   (*stderr_handler)(char *buf, size_t buflen, struct spawn*);
};
struct spawn* spawn_create(const char *exec, void *user_data) {
  akassert(exec);
  struct pool *pool = pool_create_empty();
  struct spawn *s = pool_alloc(pool, sizeof(*s));
  *s = (struct spawn) {
    .pool = pool,
    .pid = -1,
    .args = {
      .usize = sizeof(char*)
    },
    .env = {
      .usize = sizeof(char*)
    },
    .user_data = user_data,
  };
  s->exec = spawn_arg_add(s, exec);
  if (g_env.project.cache_dir) {
    spawn_env_path_prepend(s, g_env.project.cache_dir);
  }
  if (g_env.spawn.extra_env_paths) {
    spawn_env_path_prepend(s, g_env.spawn.extra_env_paths);
  }
  return s;
}
static const char* _spawn_arg_add(struct spawn *s, const char *arg, int len) {
  if (!arg || *arg == '\0') {
    return "";
  }
  if (is_vlist(arg)) {
    struct vlist_iter iter;
    vlist_iter_init(arg, &iter);
    while (vlist_iter_next(&iter)) {
      _spawn_arg_add(s, iter.item, iter.len);
    }
    return arg;
  } else {
    const char *v = (len < 0) ? pool_strdup(s->pool, arg) : pool_strndup(s->pool, arg, len);
    ulist_push(&s->args, &v);
    return v;
  }
}
const char* spawn_arg_add(struct spawn *s, const char *arg) {
  return _spawn_arg_add(s, arg, -1);
}
bool spawn_arg_exists(struct spawn *s, const char *arg) {
  for (int i = 0; i < s->args.num; ++i) {
    const char *v = *(char**) ulist_get(&s->args, i);
    if (strcmp(v, arg) == 0) {
      return true;
    }
  }
  return false;
}
bool spawn_arg_starts_with(struct spawn *s, const char *arg) {
  for (int i = 0; i < s->args.num; ++i) {
    const char *v = *(char**) ulist_get(&s->args, i);
    if (utils_startswith(v, arg)) {
      return true;
    }
  }
  return false;
}
void spawn_env_set(struct spawn *s, const char *key, const char *val) {
  akassert(key);
  if (!val) {
    val = "";
  }
  const char *v = pool_printf(s->pool, "%s=%s", key, val);
  ulist_push(&s->env, &v);
}
void spawn_env_path_prepend(struct spawn *s, const char *path) {
  struct xstr *xstr = xstr_create_empty();
  const char *old = s->path_overriden;
  if (!old) {
    old = getenv("PATH");
  }
  if (old) {
    xstr_printf(xstr, "%s:%s", path, old);
  } else {
    xstr_cat(xstr, path);
  }
  free(s->path_overriden);
  s->path_overriden = xstr_destroy_keep_ptr(xstr);
}
static char* _file_resolve_in_path(struct spawn *s, const char *file, char pathbuf[PATH_MAX]) {
  char buf[PATH_MAX];
  const char *sp = s->path_overriden;
  if (!sp) {
    sp = getenv("PATH");
  }
  const char *ep = sp;
  while (sp) {
    for ( ; *ep != '\0' && *ep != ':'; ++ep) ;
    if (ep - sp > 0 && ep - sp < PATH_MAX) {
      memcpy(buf, sp, ep - sp);
      buf[ep - sp] = '\0';
      if (buf[ep - sp - 1] != '/') {
        snprintf(pathbuf, PATH_MAX, "%s/%s", buf, file);
      } else {
        snprintf(pathbuf, PATH_MAX, "%s%s", buf, file);
      }
      if (!access(pathbuf, F_OK)) {
        return pathbuf;
      }
    }
    while (*ep == ':') ++ep;
    if (*ep == '\0') {
      break;
    }
    sp = ep;
  }
  return 0;
}
static char* _env_find(struct spawn *s, const char *key, size_t keylen) {
  if (keylen == 0) {
    return 0;
  }
  for (int i = 0; i < s->env.num; ++i) {
    char *v = *(char**) ulist_get(&s->env, i);
    if (strncmp(v, key, keylen) == 0 && v[keylen] == '=') {
      ulist_remove(&s->env, i); // Entry is not needed anymore.
      return v;
    }
  }
  return 0;
}
static char** _env_create(struct spawn *s) {
  int i = 0, c = 0;
  if (s->path_overriden) {
    spawn_env_set(s, "PATH", s->path_overriden);
  }
  for (char **ep = environ; *ep; ++ep, ++c) ;
  c += s->env.num;
  char **nenv = pool_alloc(s->pool, sizeof(*nenv) * (c + 1));
  nenv[c] = 0;
  for (char **it = environ; *it && i < c; ++it, ++i) {
    const char *entry = *it;
    const char *ep = strchr(entry, '=');
    if (ep == 0) {
      continue;
    }
    size_t keylen = ep - entry;
    char *my = _env_find(s, entry, keylen);
    if (my) {
      nenv[i] = my;
    } else {
      nenv[i] = (char*) pool_strdup(s->pool, entry);
    }
  }
  for (c = 0; c < s->env.num; ++c, ++i) {
    nenv[i] = *(char**) ulist_get(&s->env, c);
  }
  nenv[i] = 0;
  return nenv;
}
static char** _args_create(struct spawn *s) {
  char **args = pool_alloc(s->pool, sizeof(*args) * (s->args.num + 1));
  for (int i = 0; i < s->args.num; ++i) {
    char *v = *(char**) ulist_get(&s->args, i);
    args[i] = v;
  }
  args[s->args.num] = 0;
  return args;
}
void* spawn_user_data(struct spawn *s) {
  return s->user_data;
}
pid_t spawn_pid(struct spawn *s) {
  return s->pid;
}
int spawn_exit_code(struct spawn *s) {
  if (WIFEXITED(s->wstatus)) {
    return WEXITSTATUS(s->wstatus);
  }
  return -1;
}
void spawn_set_stdin_provider(
  struct spawn *s,
  size_t (*provider)(char *buf, size_t buflen, struct spawn*)) {
  s->stdin_provider = provider;
}
void spawn_set_stdout_handler(
  struct spawn *s,
  void (*handler)(char *buf, size_t buflen, struct spawn*)) {
  s->stdout_handler = handler;
}
static void _default_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stdout, "%s: %s", s->exec, buf);
}
static void _default_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s: %s", s->exec, buf);
}
void spawn_set_stderr_handler(
  struct spawn *s,
  void (*handler)(char *buf, size_t buflen, struct spawn*)) {
  s->stderr_handler = handler;
}
void spawn_set_nowait(struct spawn *s, bool nowait) {
  s->nowait = nowait;
}
void spawn_set_wstatus(struct spawn *s, int wstatus) {
  s->wstatus = wstatus;
}
int spawn_do(struct spawn *s) {
  int rc = 0;
  bool nowait = s->nowait;
  if (!s->stderr_handler) {
    s->stderr_handler = _default_stderr_handler;
  }
  if (!s->stdout_handler) {
    s->stdout_handler = _default_stdout_handler;
  }
  int pipe_stdout[2] = { -1, -1 };
  int pipe_stderr[2] = { -1, -1 };
  int pipe_stdin[2] = { -1, -1 };
  char **envp = _env_create(s);
  char **args = _args_create(s);
  const char *file = args[0];
  {
    struct xstr *xstr = xstr_create_empty();
    for (char **a = args; *a; ++a) {
      if (a != args) {
        xstr_cat(xstr, " ");
      }
      xstr_cat(xstr, *a);
    }
    puts(xstr_ptr(xstr));
    xstr_destroy(xstr);
  }
  char buf[1024];
  char pathbuf[PATH_MAX];
  ssize_t len;
  if (!strchr(file, '/')) {
    const char *rfile = _file_resolve_in_path(s, file, pathbuf);
    if (!rfile) {
      akerror(ENOENT, "Failed to find: '%s' in PATH", file);
      errno = ENOENT;
      return errno;
    }
    file = rfile;
  }
  if (!nowait) {
    if (pipe(pipe_stdout) == -1) {
      return errno;
    }
    if (pipe(pipe_stderr) == -1) {
      return errno;
    }
  }
  if (s->stdin_provider) {
    if (pipe(pipe_stdin) == -1) {
      return errno;
    }
  }
  s->pid = fork();
  if (s->pid == -1) {
    return errno;
  }
  if (s->pid == 0) {
    if (pipe_stdin[0] != -1) {
      close(pipe_stdin[1]);
      if (dup2(pipe_stdin[0], STDIN_FILENO) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
      }
      close(pipe_stdin[0]);
    }
    if (!nowait) {
      close(pipe_stdout[0]);
      if (dup2(pipe_stdout[1], STDOUT_FILENO) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
      }
      close(pipe_stdout[1]);
      close(pipe_stderr[0]);
      if (dup2(pipe_stderr[1], STDERR_FILENO) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
      }
      close(pipe_stderr[1]);
    }
    execve(file, args, envp);
    perror("execve");
    _exit(EXIT_FAILURE);
  } else {
    if (!nowait) {
      close(pipe_stdout[1]);
      close(pipe_stderr[1]);
    }
    if (s->stdin_provider) {
      close(pipe_stdin[0]);
      ssize_t tow = 0;
      while ((tow = s->stdin_provider(buf, sizeof(buf), s)) > 0) {
        while (tow > 0) {
          len = write(pipe_stdin[1], buf, tow);
          if (len == -1 && errno == EAGAIN) {
            continue;
          }
          if (len > 0) {
            tow -= len;
          } else {
            break;
          }
        }
      }
      close(pipe_stdin[1]);
    }
    if (!nowait) {
      // Now read both stdout & stderr
      struct pollfd fds[] = {
        { .fd = pipe_stdout[0], .events = POLLIN },
        { .fd = pipe_stderr[0], .events = POLLIN }
      };
      int c = sizeof(fds) / sizeof(fds[0]);
      while (c > 0) {
        int ret = poll(fds, sizeof(fds) / sizeof(fds[0]), -1);
        if (ret == -1) {
          break;
        }
        for (int i = 0; i < sizeof(fds) / sizeof(fds[0]); ++i) {
          if (fds[i].fd == -1) {
            continue;
          }
          short revents = fds[i].revents;
          if (revents & POLLIN) {
            ssize_t n = read(fds[i].fd, buf, sizeof(buf) - 1);
            if (n > 0) {
              buf[n] = '\0';
              if (fds[i].fd == pipe_stdout[0]) {
                s->stdout_handler(buf, n, s);
              } else {
                s->stderr_handler(buf, n, s);
              }
            } else if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
              close(fds[i].fd);
              fds[i].fd = -1;
              --c;
            }
          }
          if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
            close(fds[i].fd);
            fds[i].fd = -1;
            --c;
          }
        }
      }
      if (waitpid(s->pid, &s->wstatus, 0) == -1) {
        perror("waitpid");
      }
    }
  }
  if (rc) {
    akerror(rc, "Failed to spawn: %s", file);
  }
  return rc;
}
void spawn_destroy(struct spawn *s) {
  if (s) {
    free(s->path_overriden);
    ulist_destroy_keep(&s->args);
    ulist_destroy_keep(&s->env);
    pool_destroy(s->pool);
  }
}
#ifndef _AMALGAMATE_
#include "deps.h"
#include "log.h"
#include "utils.h"
#include "paths.h"
#include "env.h"
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#endif
int deps_open(const char *path, int omode, struct deps *d) {
  int rc = 0;
  akassert(path && d);
  memset(d, 0, sizeof(*d));
  d->file = fopen(path, (omode & DEPS_OPEN_TRUNCATE) ? "w+" : ((omode & DEPS_OPEN_READONLY) ? "r" : "a+"));
  if (!d->file) {
    return errno;
  }
  if (fseek(d->file, 0, SEEK_SET) < 0) {
    return errno;
  }
  return rc;
}
bool deps_cur_next(struct deps *d) {
  int rc;
  if (d && d->file) {
    if (!fgets(d->buf, sizeof(d->buf), d->file)) {
      return false;
    }
    char *ls = 0;
    char *rp = d->buf;
    d->type = *rp++;
    d->flags = *rp++;
    if (d->type == DEPS_TYPE_NODE_VALUE || d->type == DEPS_TYPE_ENV || d->type == DEPS_TYPE_SYS_ENV) {
      utils_chars_replace(d->buf, '\2', '\n');
    }
    if (d->type == DEPS_TYPE_ALIAS || d->type == DEPS_TYPE_ENV || d->type == DEPS_TYPE_SYS_ENV) {
      d->alias = rp;
      d->resource = 0;
    } else {
      d->resource = rp;
      d->alias = 0;
    }
    while (*rp) {
      if (*rp == '\1') {
        if (!d->resource) {
          *rp = '\0';
          d->resource = rp + 1;
        }
        ls = rp;
      }
      ++rp;
    }
    if (ls) {
      *ls = '\0';
      if (d->type == DEPS_TYPE_FILE || d->type == DEPS_TYPE_NODE_VALUE || d->type == DEPS_TYPE_ALIAS) {
        ++ls;
        d->serial = utils_strtoll(ls, 10, &rc);
        if (rc) {
          akerror(rc, "Failed to read '%s' as number", ls);
          return false;
        }
      }
    }
    return true;
  }
  return false;
}
bool deps_cur_is_outdated(struct node *n, struct deps *d) {
  if (d) {
    switch (d->type) {
      case DEPS_TYPE_FILE: {
        struct akpath_stat st;
        if (path_stat(d->resource, &st) || st.ftype == AKPATH_NOT_EXISTS || st.mtime > d->serial) {
          return true;
        }
        break;
      }
      case DEPS_TYPE_ALIAS: {
        struct akpath_stat st;
        if (path_stat(d->alias, &st) || st.ftype == AKPATH_NOT_EXISTS || st.mtime > d->serial) {
          return true;
        }
        break;
      }
      case DEPS_TYPE_ENV: {
        const char *val = unit_env_get(n, d->alias);
        if (!val) {
          val = "";
        }
        return strcmp(val, d->resource) != 0;
      }
      case DEPS_TYPE_SYS_ENV: {
        const char *val = getenv(d->alias);
        if (!val) {
          val = "";
        }
        return strcmp(val, d->resource) != 0;
      }
      case DEPS_TYPE_OUTDATED:
        return true;
    }
  }
  return false;
}
static int _deps_add(struct deps *d, char type, char flags, const char *resource, const char *alias, int64_t serial) {
  int rc = 0;
  char buf[2][PATH_MAX];
  char dbuf[DEPS_BUF_SZ];
  if (flags == 0) {
    flags = ' ';
  }
  if (type == DEPS_TYPE_FILE) {
    path_normalize(resource, buf[0]);
    resource = buf[0];
    struct akpath_stat st;
    if (!path_stat(resource, &st) && st.ftype != AKPATH_NOT_EXISTS) {
      serial = st.mtime;
    }
  } else if (type == DEPS_TYPE_ALIAS) {
    path_normalize(resource, buf[0]);
    path_normalize(alias, buf[1]);
    resource = buf[0];
    alias = buf[1];
    struct akpath_stat st;
    if (!path_stat(alias, &st) && st.ftype != AKPATH_NOT_EXISTS) {
      serial = st.mtime;
    }
  } else if (type == DEPS_TYPE_ENV || type == DEPS_TYPE_NODE_VALUE || type == DEPS_TYPE_SYS_ENV) {
    assert(resource);
    utils_strncpy(dbuf, resource, sizeof(dbuf));
    utils_chars_replace(dbuf, '\n', '\2');
    resource = dbuf;
  } else if (type == DEPS_TYPE_FILE_OUTDATED) {
    type = DEPS_TYPE_FILE;
    path_normalize(resource, buf[0]);
    resource = buf[0];
    serial = 0;
  }
  long int off = ftell(d->file);
  if (off < 0) {
    return errno;
  }
  fseek(d->file, 0, SEEK_END);
  if (type != DEPS_TYPE_ALIAS && type != DEPS_TYPE_ENV && type != DEPS_TYPE_SYS_ENV) {
    if (fprintf(d->file, "%c%c%s\1%" PRId64 "\n", type, flags, resource, serial) < 0) {
      rc = errno;
    }
  } else {
    if (fprintf(d->file, "%c%c%s\1%s\1%" PRId64 "\n", type, flags, alias, resource, serial) < 0) {
      rc = errno;
    }
  }
  fseek(d->file, off, SEEK_SET);
  if (!rc) {
    ++d->num_registered;
  }
  return rc;
}
int deps_add(struct deps *d, char type, char flags, const char *resource, int64_t serial) {
  return _deps_add(d, type, flags, resource, 0, serial);
}
int deps_add_alias(struct deps *d, char flags, const char *resource, const char *alias) {
  return _deps_add(d, DEPS_TYPE_ALIAS, flags, resource, alias, 0);
}
int deps_add_env(struct deps *d, char flags, const char *key, const char *value) {
  return _deps_add(d, DEPS_TYPE_ENV, flags, value, key, 0);
}
int deps_add_sys_env(struct deps *d, char flags, const char *key, const char *value) {
  return _deps_add(d, DEPS_TYPE_SYS_ENV, flags, value, key, 0);
}
void deps_close(struct deps *d) {
  if (d && d->file) {
    fclose(d->file);
    d->file = 0;
  }
}
void deps_prune_all(const char *path) {
  unlink(path);
}
#ifndef _AMALGAMATE_
#include "script.h"
#endif
int node_script_setup(struct node *n) {
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "env.h"
#include "log.h"
#include "pool.h"
#include "paths.h"
#include "spawn.h"
#include "deps.h"
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#endif
static void _check_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stdout, "%s", buf);
}
static void _check_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}
static void _check_on_env_value(struct node_resolve *nr, const char *key, const char *val) {
  struct unit *unit = nr->user_data;
  akassert(unit->n);
  if (g_env.verbose) {
    akinfo("%s %s=%s", unit->rel_path, key, val);
  }
  node_env_set(unit->n, key, val);
}
static void _check_on_resolve(struct node_resolve *r) {
  struct unit *unit = r->user_data;
  struct node *n = unit->n;
  const char *path = unit->impl;
  struct spawn *s = spawn_create(path, unit);
  spawn_set_stdout_handler(s, _check_stdout_handler);
  spawn_set_stderr_handler(s, _check_stderr_handler);
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      spawn_arg_add(s, node_value(nn));
    }
  }
  int rc = spawn_do(s);
  if (rc) {
    node_fatal(rc, n, "%s", unit->source_path);
  } else {
    int code = spawn_exit_code(s);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", unit->source_path, code);
    }
  }
  spawn_destroy(s);
  // Good, now add dependency on itself
  struct deps deps;
  rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, unit->n, "Failed to open depencency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(n, &deps);
  deps_close(&deps);
}
static char* _resolve_check_path(struct pool *pool, struct node *n, const char *script, struct unit **out_u) {
  *out_u = 0;
  char buf[PATH_MAX];
  for (int i = (int) g_env.stack_units.num - 1; i >= 0; --i) {
    struct unit_ctx *c = (struct unit_ctx*) ulist_get(&g_env.stack_units, i);
    struct unit *u = c->unit;
    snprintf(buf, sizeof(buf), "%s/.autark/%s", u->dir, script);
    if (path_is_exist(buf)) {
      *out_u = u;
      return pool_strdup(pool, buf);
    }
  }
  node_fatal(AK_ERROR_FAIL, n, "Check script not found: %s", script);
  return 0;
}
static void _check_script(struct node *n) {
  const char *script = node_value(n);
  if (g_env.verbose) {
    node_info(n->parent, "%s", script);
  }
  struct unit *parent = 0;
  struct pool *pool = pool_create(on_unit_pool_destroy);
  char *path = _resolve_check_path(pool, n, script, &parent);
  const char *vpath = pool_printf(pool, "%s/.autark/%s", parent->dir, n->vfile);
  struct unit *unit = unit_create(vpath, 0, pool);
  unit->n = n;
  unit->impl = path;
  unit_push(unit, n);
  struct node_resolve nr = {
    .n = n,
    .mode = NODE_RESOLVE_ENV_ALWAYS,
    .path = n->vfile,
    .user_data = unit,
    .on_env_value = _check_on_env_value,
    .on_resolve = _check_on_resolve,
  };
  node_resolve(&nr);
  unit_pop();
  pool_destroy(pool);
}
static void _check_init(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    _check_script(nn);
  }
}
int node_check_setup(struct node *n) {
  n->init = _check_init;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "utils.h"
#include "alloc.h"
#include "env.h"
#endif
static const char* _set_value_get(struct node *n);
static struct unit* _unit_for_set(struct node *n, struct node *nn, const char **keyp) {
  if (nn->type == NODE_TYPE_BAG) {
    if (strcmp(nn->value, "root") == 0) {
      *keyp = node_value(nn->child);
      return unit_root();
    } else if (strcmp(nn->value, "parent") == 0) {
      *keyp = node_value(nn->child);
      return unit_parent(n);
    }
  } else {
    *keyp = node_value(nn);
  }
  return unit_peek();
}
static void _set_setup(struct node *n) {
  if (n->child && strcmp(n->value, "env") == 0) {
    const char *v = _set_value_get(n);
    if (v) {
      const char *key = 0;
      _unit_for_set(n, n->child, &key);
      if (key) {
        if (g_env.verbose) {
          node_info(n, "%s=%s", key, v);
        }
        setenv(key, v, 1);
      }
    }
  }
}
static void _set_init(struct node *n) {
  const char *key = 0;
  struct unit *unit = n->child ? _unit_for_set(n, n->child, &key) : 0;
  if (!key) {
    node_warn(n, "No name specified for 'set' directive");
    return;
  }
  struct node *nn = unit_env_get_node(unit, key);
  if (nn) {
    n->recur_next.n = nn;
  }
  unit_env_set_node(unit, key, n);
}
static const char* _set_value_get(struct node *n) {
  if (n->recur_next.active && n->recur_next.n) {
    return _set_value_get(n->recur_next.n);
  }
  n->recur_next.active = true;
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    if ((uintptr_t) n->impl != (uintptr_t) -1) {
      free(n->impl);
    }
    n->impl = 0;
  }
  if ((uintptr_t) n->impl == (uintptr_t) -1) {
    n->recur_next.active = false;
    return 0;
  } else if (n->impl) {
    n->recur_next.active = false;
    return n->impl;
  }
  n->impl = (void*) (uintptr_t) -1;
  struct node *nn = n->child->next;
  if (!nn) {
    n->impl = xstrdup("");
    n->recur_next.active = false;
    return n->impl;
  }
  if (!nn->next && nn->value[0] != '.') { // Single value
    const char *v = node_value(nn);
    n->impl = xstrdup(v);
    n->recur_next.active = false;
    return n->impl;
  }
  struct xstr *xstr = xstr_create_empty();
  for ( ; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (!v) {
      v = "";
    }
    if (nn->value[0] == '.' && nn->value[1] == '.') {
      utils_split_values_add(v, xstr);
    } else {
      if (!is_vlist(v)) {
        xstr_cat(xstr, "\1");
      }
      xstr_cat(xstr, v);
    }
  }
  n->impl = xstr_destroy_keep_ptr(xstr);
  n->recur_next.active = false;
  return n->impl;
}
static void _set_dispose(struct node *n) {
  if ((uintptr_t) n->impl != (uintptr_t) -1) {
    free(n->impl);
  }
  n->impl = 0;
}
int node_set_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _set_init;
  n->setup = _set_setup;
  n->value_get = _set_value_get;
  n->dispose = _set_dispose;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "alloc.h"
#include "spawn.h"
#include "xstr.h"
#include "utils.h"
#include <stdlib.h>
#endif
static const char* _subst_setval(struct node *n, const char *vv, const char *dv) {
  if (vv) {
    if (n->impl) {
      if (strcmp(n->impl, vv) == 0) {
        return n->impl;
      }
      free(n->impl);
    }
    n->impl = xstrdup(vv);
    return n->impl;
  } else {
    return dv;
  }
}
static const char* _subst_value(struct node *n) {
  if (n->child) {
    const char *dv_ = node_value(n->child->next);
    const char *dv = dv_;
    if (!dv) {
      dv = "";
    }
    const char *key = node_value(n->child);
    if (!key) {
      node_warn(n, "No key specified");
      return _subst_setval(n, 0, dv);
    }
    struct node_foreach *fe = node_find_parent_foreach(n);
    if (fe && strcmp(fe->name, key) == 0) {
      ++fe->access_cnt;
      return _subst_setval(n, fe->value, dv);
    }
    const char *vv = node_env_get(n, key);
    return _subst_setval(n, vv, dv);
  }
  return "";
}
static void _subst_dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}
struct _subst_ctx {
  struct node *n;
  struct xstr *xstr;
  const char  *cmd;
};
static void _subst_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}
static void _subst_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  struct _subst_ctx *ctx = spawn_user_data(s);
  xstr_cat2(ctx->xstr, buf, buflen);
  fprintf(stdout, "%s", buf);
}
static const char* _subst_value_proc(struct node *n) {
  if (n->impl) {
    return n->impl;
  }
  const char *cmd = node_value(n->child);
  if (cmd == 0 || *cmd == '\0') {
    return 0;
  }
  struct xstr *xstr = xstr_create_empty();
  struct _subst_ctx ctx = {
    .n = n,
    .cmd = cmd,
    .xstr = xstr
  };
  struct spawn *s = spawn_create(cmd, &ctx);
  spawn_set_stdout_handler(s, _subst_stdout_handler);
  spawn_set_stderr_handler(s, _subst_stderr_handler);
  for (struct node *nn = n->child->next; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      spawn_arg_add(s, node_value(nn));
    }
  }
  int rc = spawn_do(s);
  if (rc) {
    node_fatal(rc, n, "%s", cmd);
  } else {
    int code = spawn_exit_code(s);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", cmd, code);
    }
  }
  char *val = xstr_destroy_keep_ptr(xstr);
  for (int i = strlen(val) - 1; i >= 0 && utils_char_is_space(val[i]); --i) {
    val[i] = '\0';
  }
  n->impl = val;
  spawn_destroy(s);
  return n->impl;
}
int node_subst_setup(struct node *n) {
  if (strchr(n->value, '@')) {
    n->value_get = _subst_value_proc;
  } else {
    n->flags |= NODE_FLG_NO_CWD;
    n->value_get = _subst_value;
  }
  n->dispose = _subst_dispose;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "log.h"
#include "env.h"
#include "utils.h"
#include <string.h>
#endif
static bool _if_defined_eval(struct node *mn) {
  struct node *n = mn->parent;
  for (struct node *nn = mn->child; nn; nn = nn->next) {
    const char *val = node_value(mn->child);
    if (val && *val != '\0' && node_env_get(n, val)) {
      return true;
    }
  }
  return false;
}
static bool _if_matched_eval(struct node *mn) {
  const char *val1 = node_value(mn->child);
  const char *val2 = mn->child ? node_value(mn->child->next) : 0;
  if (val1 && val2) {
    return strcmp(val1, val2) == 0;
  } else if (val1 == 0 && val2 == 0) {
    return true;
  } else {
    return false;
  }
}
static bool _if_prefix_eval(struct node *mn) {
  const char *val1 = node_value(mn->child);
  const char *val2 = mn->child ? node_value(mn->child->next) : 0;
  if (val1 && val2) {
    return utils_startswith(val1, val2);
  } else if (val1 == 0 && val2 == 0) {
    return true;
  } else {
    return false;
  }
}
static bool _if_contains_eval(struct node *mn) {
  const char *val1 = node_value(mn->child);
  const char *val2 = mn->child ? node_value(mn->child->next) : 0;
  if (val1 && val2) {
    return strstr(val1, val2);
  } else if (val1 == 0 && val2 == 0) {
    return true;
  } else {
    return false;
  }
}
static bool _if_cond_eval(struct node *n, struct node *mn);
static bool _if_OR_eval(struct node *n, struct node *mn) {
  for (struct node *nn = mn->child; nn; nn = nn->next) {
    if (_if_cond_eval(n, nn)) {
      return true;
    }
  }
  return false;
}
static bool _if_AND_eval(struct node *n, struct node *mn) {
  for (struct node *nn = mn->child; nn; nn = nn->child) {
    if (!_if_cond_eval(n, nn)) {
      return false;
    }
  }
  return mn->child != 0;
}
static bool _if_cond_eval(struct node *n, struct node *mn) {
  if (node_is_value(mn)) {
    return true;
  }
  const char *op = mn->value;
  if (!op) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Matching condition is not set");
  }
  bool eq = false;
  bool neg = false;
  if (mn->flags & NODE_FLG_NEGATE) {
    neg = true;
  }
  if (mn->type == NODE_TYPE_BAG) {
    if (*op == '!') {
      ++op;
    }
    if (strcmp(op, "eq") == 0) {
      eq = _if_matched_eval(mn);
    } else if (strcmp(op, "defined") == 0) {
      eq = _if_defined_eval(mn);
    } else if (strcmp(op, "or") == 0) {
      eq = _if_OR_eval(n, mn);
    } else if (strcmp(op, "and") == 0) {
      eq = _if_AND_eval(n, mn);
    } else if (strcmp(op, "prefix") == 0) {
      eq = _if_prefix_eval(mn);
    } else if (strcmp(op, "contains") == 0) {
      eq = _if_contains_eval(mn);
    } else {
      node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Unknown matching condition: %s", op);
    }
  } else if (node_is_can_be_value(mn)) {
    op = node_value(mn);
    if (is_vlist(op)) {
      struct vlist_iter iter;
      vlist_iter_init(op, &iter);
      if (vlist_iter_next(&iter) && !(iter.len == 1 && iter.item[0] == '0')) {
        eq = iter.len > 0 || vlist_iter_next(&iter);
      }
    } else {
      eq = (op && *op != '\0' && !(op[0] == '0' && op[1] == '\0'));
    }
  } else {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Unknown matching condition: %s", op);
  }
  if (neg) {
    eq = !eq;
  }
  if (g_env.verbose) {
    node_info(n, "Evaluated to: %s", (eq ? "true" : "false"));
  }
  return eq;
}
static inline bool _if_node_is_else(struct node *n) {
  return n && n->value && strcmp(n->value, "else") == 0;
}
static void _if_pull_else(struct node *n) {
  struct node *prev = node_find_prev_sibling(n);
  struct node *next = n->next;
  struct node *nn = next;
  bool elz = false;
  if (_if_node_is_else(nn)) {
    elz = true;
    next = nn->next;
  }
  n->child = 0;
  if (elz && nn->child) {
    nn = nn->child;
    n->next = nn; // Keep upper iterations (if any) be consistent.
    n->child = 0;
    if (prev) {
      prev->next = nn;
    } else {
      n->parent->child = nn;
    }
    for ( ; nn; nn = nn->next) {
      nn->parent = n->parent;
      if (nn->next == 0) {
        nn->next = next;
        break;
      }
    }
  } else if (prev) {
    n->next = next;
    prev->next = next;
  } else {
    n->next = next;
    n->parent->child = next;
  }
}
static void _if_pull_if(struct node *n) {
  struct node *mn = n->child;
  struct node *prev = node_find_prev_sibling(n);
  struct node *next = n->next;
  struct node *nn = mn->next;
  if (_if_node_is_else(next)) {
    next = next->next; // Skip else block
  }
  n->child = 0;
  if (nn) {
    n->next = nn; // Keep upper iterations (if any) be consistent.
    if (prev) {
      prev->next = nn;
    } else {
      n->parent->child = nn;
    }
    for ( ; nn; nn = nn->next) {
      nn->parent = n->parent;
      if (nn->next == 0) {
        nn->next = next;
        break;
      }
    }
  } else if (prev) {
    n->next = next;
    prev->next = next;
  } else {
    n->next = next;
    n->parent->child = next;
  }
}
static void _if_init(struct node *n) {
  struct node *cn = n->child; // Conditional node
  if (!cn) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "'if {...}' must have a condition clause");
  }
  node_init(cn); // Explicitly init conditional node since in script init worflow 'if' initiated first.
  bool matched = _if_cond_eval(n, cn);
  if (matched) {
    _if_pull_if(n);
  } else {
    _if_pull_else(n);
  }
  n->init = 0; // Protect me from second call in any way
}
int node_if_setup(struct node *n) {
  n->init = _if_init;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include <stdlib.h>
#endif
static const char* _join_value(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    free(n->impl);
    n->impl = 0;
  }
  if (n->impl) {
    return n->impl;
  }
  struct xstr *xstr = xstr_create_empty();
  bool list = n->value[0] == '.';
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (list) {
      xstr_cat(xstr, "\1");
    }
    xstr_cat(xstr, node_value(nn));
  }
  n->impl = xstr_destroy_keep_ptr(xstr);
  return n->impl;
}
static void _join_dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}
int node_join_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->value_get = _join_value;
  n->dispose = _join_dispose;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "utils.h"
#include <string.h>
#endif
static void _meta_on_let(struct node *n, struct node *e) {
  const char *name = node_value(e->child);
  if (name == 0) {
    return;
  }
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = e->child->next; nn; nn = nn->next) {
    if (nn != e->child->next) {
      xstr_cat2(xstr, " ", 1);
    }
    const char *nv = node_value(nn);
    if (is_vlist(nv)) {
      struct vlist_iter iter;
      vlist_iter_init(nv, &iter);
      for (int i = 0; vlist_iter_next(&iter); ++i) {
        if (i) {
          xstr_cat2(xstr, " ", 1);
        }
        xstr_cat2(xstr, iter.item, iter.len);
      }
    } else {
      xstr_cat(xstr, nv);
    }
  }
  node_env_set(n, name, xstr_ptr(xstr));
  xstr_destroy(xstr);
}
static void _meta_on_entry(struct node *n, struct node *e) {
  if (e->type != NODE_TYPE_BAG) {
    return;
  }
  const int plen = AK_LLEN("META_");
  char name[plen + strlen(e->value) + 1];
  char *wp = name;
  memcpy(wp, "META_", plen), wp += plen;
  memcpy(wp, e->value, sizeof(name) - plen);
  for (int i = plen; i < sizeof(name) - 1; ++i) {
    name[i] = utils_toupper_ascii(name[i]);
  }
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = e->child; nn; nn = nn->next) {
    if (nn != e->child) {
      xstr_cat2(xstr, " ", 1);
    }
    const char *nv = node_value(nn);
    xstr_cat(xstr, nv);
  }
  node_env_set(n, name, xstr_ptr(xstr));
  xstr_destroy(xstr);
}
static void _meta_init(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (strcmp(nn->value, "let") == 0) {
      _meta_on_let(n, nn);
    } else {
      _meta_on_entry(n, nn);
    }
  }
}
int node_meta_setup(struct node *n) {
  n->init = _meta_init;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "log.h"
#endif
static void _include(struct node *n, struct node *cn) {
  const char *file = node_value(cn);
  if (!file || *file == '\0') {
    node_fatal(AK_ERROR_SCRIPT, n, "No Autark script specified");
  }
  struct node *nn = 0;
  struct node *pn = node_find_prev_sibling(n);
  int rc = script_include(n->parent, file, &nn);
  if (rc) {
    node_fatal(rc, n, "Failed to open Autark script: %s", file);
  }
  akassert(nn);
  if (pn) {
    pn->next = nn;
  } else if (n->parent) {
    n->parent->child = nn;
  }
  nn->next = n->next;
  n->next = nn; // Keep upper iterations (if any) be consistent.
  n->child = 0;
}
static void _include_init(struct node *n) {
  struct node *cn = n->child;
  if (!cn) {
    node_fatal(AK_ERROR_SCRIPT, n, "Missing path in 'include {...}' directive");
  }
  node_init(cn); // Explicitly init conditional node since in script init worflow 'include' initiated first.
  _include(n, cn);
  n->init = 0;
}
int node_include_setup(struct node *n) {
  n->init = _include_init;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "spawn.h"
#include "env.h"
#include "log.h"
#include "spawn.h"
#include "utils.h"
#include "log.h"
#include "paths.h"
#include <unistd.h>
#include <string.h>
#endif
struct _run_spawn_data {
  struct node *n;
  const char  *cmd;
};
static void _run_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stdout, "%s", buf);
}
static void _run_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s", buf);
}
struct _run_on_resolve_ctx {
  struct node_resolve *r;
  struct node_foreach *fe;
  struct ulist consumes;         // sizeof(char*)
  struct ulist consumes_foreach; // sizeof(char*)
  bool fe_consumed;              // Foreach variable is consumed
};
static void _run_on_resolve_shell(struct node_resolve *r, struct node *nn_) {
  struct node *n = r->n;
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = nn_; nn; nn = nn->next) {
    if (nn != nn_) {
      xstr_cat2(xstr, " ", 1);
    }
    const char *v = node_value(nn);
    if (is_vlist(v)) {
      struct vlist_iter iter;
      vlist_iter_init(v, &iter);
      while (vlist_iter_next(&iter)) {
        if (xstr_size(xstr)) {
          xstr_cat2(xstr, " ", 1);
        }
        xstr_cat2(xstr, iter.item, iter.len);
      }
    } else {
      xstr_cat(xstr, v);
    }
  }
  const char *shell = "/bin/sh";
  struct unit *unit = unit_peek();
  struct spawn *s = spawn_create(shell, &(struct _run_spawn_data) {
    .cmd = shell,
    .n = n
  });
  spawn_env_path_prepend(s, unit->dir);
  spawn_env_path_prepend(s, unit->cache_dir);
  spawn_set_stdout_handler(s, _run_stdout_handler);
  spawn_set_stderr_handler(s, _run_stderr_handler);
  spawn_arg_add(s, "-c");
  spawn_arg_add(s, xstr_ptr(xstr));
  if (g_env.check.log) {
    xstr_printf(g_env.check.log, "%s: %s\n", n->name, shell);
  }
  int rc = spawn_do(s);
  if (rc) {
    node_fatal(rc, n, "%s", shell);
  } else {
    int code = spawn_exit_code(s);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", shell, code);
    }
  }
  spawn_destroy(s);
  xstr_destroy(xstr);
}
static void _run_on_resolve_exec(struct node_resolve *r, struct node *ncmd) {
  struct node *n = r->n;
  const char *cmd = node_value(ncmd);
  if (!cmd) {
    node_fatal(AK_ERROR_FAIL, n, "No run command specified");
  }
  struct unit *unit = unit_peek();
  struct spawn *s = spawn_create(cmd, &(struct _run_spawn_data) {
    .cmd = cmd,
    .n = n
  });
  spawn_env_path_prepend(s, unit->dir);
  spawn_env_path_prepend(s, unit->cache_dir);
  spawn_set_stdout_handler(s, _run_stdout_handler);
  spawn_set_stderr_handler(s, _run_stderr_handler);
  for (struct node *nn = ncmd->next; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      spawn_arg_add(s, node_value(nn));
    }
  }
  if (g_env.check.log) {
    xstr_printf(g_env.check.log, "%s: %s\n", n->name, cmd);
  }
  int rc = spawn_do(s);
  if (rc) {
    node_fatal(rc, n, "%s", cmd);
  } else {
    int code = spawn_exit_code(s);
    if (code != 0) {
      node_fatal(AK_ERROR_EXTERNAL_COMMAND, n, "%s: %d", cmd, code);
    }
  }
  spawn_destroy(s);
}
static void _run_on_resolve_do(struct node_resolve *r, struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (strcmp(nn->value, "exec") == 0) {
      _run_on_resolve_exec(r, nn->child);
    } else if (strcmp(nn->value, "shell") == 0) {
      _run_on_resolve_shell(r, nn->child);
    }
  }
}
static void _run_on_resolve(struct node_resolve *r) {
  struct node *n = r->n;
  struct _run_on_resolve_ctx *ctx = r->user_data;
  if (ctx->fe) {
    if (ctx->fe_consumed) { // Foreach variable is consumed by run
      struct ulist flist_ = { .usize = sizeof(char*) };
      struct ulist *flist = &ctx->consumes_foreach;
      if (r->resolve_outdated.num) {
        for (int i = 0; i < r->resolve_outdated.num; ++i) {
          struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
          if (u->flags != 'f') {
            flist = &ctx->consumes_foreach; // Fallback to all consumed
            break;
          }
          char *path = pool_strdup(r->pool, u->path);
          ulist_push(&flist_, &path);
          flist = &flist_;
        }
      }
      struct unit *unit = unit_peek();
      for (int i = 0; i < flist->num; ++i) {
        char *p, *fi = *(char**) ulist_get(flist, i);
        if (n->flags & NODE_FLG_IN_CACHE) {
          p = path_relativize_cwd(unit->cache_dir, fi, unit->cache_dir);
        } else if (n->flags & NODE_FLG_IN_SRC) {
          p = path_relativize_cwd(unit->dir, fi, unit->dir);
        } else {
          p = fi;
        }
        ctx->fe->value = p;
        _run_on_resolve_do(r, n);
        ctx->fe->value = 0;
        if (p != fi) {
          free(p);
        }
      }
      ulist_destroy_keep(&flist_);
    } else {
      struct vlist_iter iter;
      vlist_iter_init(ctx->fe->items, &iter);
      while (vlist_iter_next(&iter)) {
        char buf[iter.len + 1];
        memcpy(buf, iter.item, iter.len);
        buf[iter.len] = '\0';
        ctx->fe->value = buf;
        _run_on_resolve_do(r, n);
        ctx->fe->value = 0;
      }
    }
  } else {
    _run_on_resolve_do(r, n);
  }
  struct deps deps;
  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(n, &deps);
  for (int i = 0; i < r->node_val_deps.num; ++i) {
    struct node *nv = *(struct node**) ulist_get(&r->node_val_deps, i);
    const char *val = node_value(nv);
    if (val) {
      deps_add(&deps, DEPS_TYPE_NODE_VALUE, 0, val, i);
    }
  }
  for (int i = 0; i < ctx->consumes.num; ++i) {
    const char *path = *(const char**) ulist_get(&ctx->consumes, i);
    deps_add(&deps, DEPS_TYPE_FILE, 0, path, 0);
  }
  for (int i = 0; i < ctx->consumes_foreach.num; ++i) {
    const char *path = *(const char**) ulist_get(&ctx->consumes_foreach, i);
    deps_add(&deps, DEPS_TYPE_FILE, 'f', path, 0);
  }
  deps_close(&deps);
}
static bool _run_setup_foreach(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (!fe) {
    return false;
  }
  struct node *pn = node_find_direct_child(n, NODE_TYPE_BAG, "produces");
  if (pn && pn->child) {
    struct vlist_iter iter;
    vlist_iter_init(fe->items, &iter);
    while (vlist_iter_next(&iter)) {
      char buf[iter.len + 1];
      memcpy(buf, iter.item, iter.len);
      buf[iter.len] = '\0';
      fe->value = buf;
      for (struct node *nn = pn->child; nn; nn = nn->next) {
        if (node_is_can_be_value(nn)) {
          const char *value = node_value(nn);
          if (g_env.verbose) {
            node_info(n, "Product: %s", value);
          }
          node_product_add(n, value, 0);
        }
      }
      fe->value = 0;
    }
  }
  return true;
}
static void _run_setup(struct node *n) {
  if (_run_setup_foreach(n)) {
    return;
  }
  struct node *nn = node_find_direct_child(n, NODE_TYPE_BAG, "produces");
  if (nn && nn->child) {
    for (nn = nn->child; nn; nn = nn->next) {
      if (node_is_can_be_value(nn)) {
        const char *value = node_value(nn);
        if (g_env.verbose) {
          node_info(n, "Product: %s", value);
        }
        node_product_add(n, value, 0);
      }
    }
  }
}
static void _run_on_consumed_resolved(const char *path_, void *d) {
  struct _run_on_resolve_ctx *ctx = d;
  const char *path = pool_strdup(ctx->r->pool, path_);
  ulist_push(&ctx->consumes, &path);
}
static void _run_on_consumed_resolved_foreach(const char *path_, void *d) {
  struct _run_on_resolve_ctx *ctx = d;
  const char *path = pool_strdup(ctx->r->pool, path_);
  ulist_push(&ctx->consumes_foreach, &path);
}
static void _run_on_resolve_init(struct node_resolve *r) {
  struct _run_on_resolve_ctx *ctx = r->user_data;
  ctx->r = r;
  struct node *nn = node_find_direct_child(r->n, NODE_TYPE_BAG, "consumes");
  if (ctx->fe) {
    ctx->fe->value = 0;
    ctx->fe->access_cnt = 0;
  }
  if (nn && nn->child) {
    node_consumes_resolve(r->n, nn->child, 0, _run_on_consumed_resolved, ctx);
  }
  if (ctx->fe && ctx->fe->access_cnt > 0) {
    ctx->fe_consumed = true;
    struct vlist_iter iter;
    struct ulist paths = { .usize = sizeof(char*) };
    vlist_iter_init(ctx->fe->items, &iter);
    while (vlist_iter_next(&iter)) {
      char buf[iter.len + 1];
      memcpy(buf, iter.item, iter.len);
      char *p = pool_strndup(r->pool, buf, iter.len);
      ulist_push(&paths, &p);
    }
    node_consumes_resolve(r->n, 0, &paths, _run_on_consumed_resolved_foreach, ctx);
    ulist_destroy_keep(&paths);
  }
}
static void _run_build(struct node *n) {
  struct _run_on_resolve_ctx ctx = {
    .consumes = { .usize = sizeof(char*) },
    .consumes_foreach = { .usize = sizeof(char*) },
    .fe = node_find_parent_foreach(n),
  };
  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .user_data = &ctx,
    .on_init = _run_on_resolve_init,
    .on_resolve = _run_on_resolve,
    .node_val_deps = { .usize = sizeof(struct node*) },
  };
  r.force_outdated = n->post_build != 0
                     || node_find_direct_child(n, NODE_TYPE_VALUE, "always") != 0;
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (strcmp(nn->value, "exec") == 0 || strcmp(nn->value, "shell") == 0) {
      for (struct node *cn = nn->child; cn; cn = cn->next) {
        if (node_is_value_may_be_dep_saved(cn)) {
          ulist_push(&r.node_val_deps, &cn);
        }
      }
    }
  }
  node_resolve(&r);
  ulist_destroy_keep(&ctx.consumes);
  ulist_destroy_keep(&ctx.consumes_foreach);
}
int node_run_setup(struct node *n) {
  if (strcmp("run-on-install", n->value) == 0) {
    if (g_env.install.enabled) {
      n->flags |= NODE_FLG_IN_CACHE;
      n->setup = _run_setup;
      n->post_build = _run_build;
    }
  } else {
    n->flags |= NODE_FLG_IN_CACHE;
    n->setup = _run_setup;
    n->build = _run_build;
  }
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "env.h"
#include "pool.h"
#include "paths.h"
#include "utils.h"
#include "xstr.h"
#include "ulist.h"
#include "alloc.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#endif
struct _configure_ctx {
  struct pool *pool;
  struct node *n;
  struct ulist sources;  // char*
};
static char* _line_replace_def(struct node *n, struct deps *deps, char *line) {
  char key[4096];
  const char *ep = line + AK_LLEN("//autarkdef ");
  while (utils_char_is_space(*ep)) ++ep;
  const char *sp = ep;
  while (*ep != '\0' && !utils_char_is_space(*ep)) ++ep;
  const char *np = ep;
  while (*np != '\0' && utils_char_is_space(*np)) ++np;
  if (*np == '\"') {
    utils_strnncpy(key, sp, ep - sp, sizeof(key));
    const char *val = node_env_get(n, key);
    deps_add_env(deps, 0, key, val ? val : "");
    if (val) {
      struct xstr *xstr = xstr_create_empty();
      xstr_printf(xstr, "#define %s \"%s\"\n", key, val);
      return xstr_destroy_keep_ptr(xstr);
    }
  } else if (*np >= '0' && *np <= '9') {
    utils_strnncpy(key, sp, ep - sp, sizeof(key));
    const char *val = node_env_get(n, key);
    deps_add_env(deps, 0, key, val ? val : "");
    if (val) {
      struct xstr *xstr = xstr_create_empty();
      xstr_printf(xstr, "#define %s %s", key, np);
      return xstr_destroy_keep_ptr(xstr);
    } else {
      struct xstr *xstr = xstr_create_empty();
      xstr_printf(xstr, "#define %s 0\n", key);
      return xstr_destroy_keep_ptr(xstr);
    }
  } else {
    utils_strnncpy(key, sp, ep - sp, sizeof(key));
    const char *val = node_env_get(n, key);
    deps_add_env(deps, 0, key, val ? val : "");
    if (val) {
      struct xstr *xstr = xstr_create_empty();
      xstr_printf(xstr, "#define %s\n", key);
      return xstr_destroy_keep_ptr(xstr);
    }
  }
  return line;
}
static char* _line_replace_subs(struct node *n, struct deps *deps, char *line) {
  char *s = line;
  char *sp = strchr(line, '@');
  if (!sp) {
    return line;
  }
  struct xstr *xstr = xstr_create_empty();
  struct xstr *kstr = xstr_create_empty();
  do {
    xstr_cat2(xstr, s, sp - s);
    xstr_clear(kstr);
    char *p = ++sp;
    sp = 0;
    while (*p) {
      if (*p == '@') {
        const char *key = xstr_ptr(kstr);
        const char *val = node_env_get(n, key);
        deps_add_env(deps, 0, key, val ? val : "");
        if (val && *val != '\0') {
          if (is_vlist(val)) {
            ++val;
            size_t len = strlen(val);
            char buf[len + 1];
            memcpy(buf, val, len + 1);
            utils_chars_replace(buf, '\1', ' ');
            xstr_cat(xstr, buf);
          } else {
            xstr_cat(xstr, val);
          }
        }
        s = ++p;
        sp = strchr(s, '@');
        xstr_clear(kstr);
        break;
      } else {
        xstr_cat2(kstr, p++, 1);
      }
    }
  } while (sp);
  if (xstr_size(kstr)) {
    xstr_cat2(xstr, "@", 1);
    xstr_cat(xstr, xstr_ptr(kstr));
  } else {
    xstr_cat(xstr, s);
  }
  xstr_destroy(kstr);
  return xstr_destroy_keep_ptr(xstr);
}
static void _process_file(struct node *n, const char *src, const char *tgt, struct deps *deps) {
  struct xstr *xstr = 0;
  if (strcmp(src, tgt) == 0) {
    xstr = xstr_create_empty();
    xstr_printf(xstr, "%s.tmp", src);
    tgt = xstr_ptr(xstr);
  }
  path_mkdirs_for(tgt);
  FILE *f = fopen(src, "r");
  if (!f) {
    node_fatal(errno, n, "Failed to open file: %s", src);
  }
  FILE *t = fopen(tgt, "w");
  if (!t) {
    node_fatal(errno, n, "Failed to open file for writing: %s", tgt);
  }
  char *line;
  char buf[16384];
  while ((line = fgets(buf, sizeof(buf), f))) {
    char *rl;
    if (utils_startswith(line, "//autarkdef ")) {
      rl = _line_replace_def(n, deps, line);
    } else {
      rl = _line_replace_subs(n, deps, line);
    }
    if (rl && fputs(rl, t) == EOF) {
      node_fatal(AK_ERROR_IO, n, "File write fail: %s", tgt);
    }
    if (rl != line) {
      free(rl);
    }
  }
  // Transfer file permissions
  struct stat st;
  if (stat(src, &st) == -1) {
    node_fatal(errno, n, "Failed to stat source file: %s", src);
  }
  if (fchmod(fileno(t), st.st_mode & 07777) == -1) {
    node_fatal(errno, n, "Failed to set permissions on target file: %s", tgt);
  }
  fclose(f);
  fclose(t);
  if (xstr) {
    int rc = utils_rename_file(tgt, src);
    if (rc) {
      node_fatal(rc, n, "Rename failed of %s to %s", tgt, src);
    }
    xstr_destroy(xstr);
  }
}
static void _configure_on_resolve(struct node_resolve *r) {
  struct node *n = r->n;
  struct _configure_ctx *ctx = n->impl;
  struct ulist *slist = &ctx->sources;
  struct ulist rlist = { .usize = sizeof(char*) };
  struct unit *unit = unit_peek();
  struct deps deps;
  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(n, &deps);
  if (r->resolve_outdated.num) {
    for (int i = 0; i < r->resolve_outdated.num; ++i) {
      struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
      if (u->flags != 's') {
        slist = &ctx->sources;
        break;
      } else {
        char *path = pool_strdup(r->pool, u->path);
        ulist_push(&rlist, &path);
        slist = &rlist;
      }
    }
  }
  for (int i = 0; i < slist->num; ++i) {
    char buf[PATH_MAX];
    char *tgt, *src = *(char**) ulist_get(slist, i);
    src = path_normalize_cwd(src, unit->cache_dir, buf);
    bool incache = path_is_prefix_for(g_env.project.cache_dir, src, unit->cache_dir);
    if (!incache) {
      tgt = path_relativize_cwd(unit->dir, src, unit->dir);
      src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
    } else {
      tgt = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
      src = xstrdup(tgt);
    }
    if (utils_endswith(tgt, ".in")) {
      size_t len = strlen(tgt);
      tgt[len - AK_LLEN(".in")] = '\0';
    }
    _process_file(n, src, tgt, &deps);
    free(tgt);
    free(src);
  }
  for (int i = 0; i < ctx->sources.num; ++i) {
    char *src = *(char**) ulist_get(&ctx->sources, i);
    deps_add(&deps, DEPS_TYPE_FILE, 's', src, 0);
  }
  deps_close(&deps);
  ulist_destroy_keep(&rlist);
}
static void _configure_on_resolve_init(struct node_resolve *r) {
  struct node *n = r->n;
  struct _configure_ctx *ctx = n->impl;
  node_consumes_resolve(n, 0, &ctx->sources, 0, 0);
}
static void _configure_build(struct node *n) {
  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .on_init = _configure_on_resolve_init,
    .on_resolve = _configure_on_resolve,
  };
  node_resolve(&r);
}
static void _configure_init(struct node *n) {
  struct pool *pool = pool_create_empty();
  struct _configure_ctx *ctx = n->impl = pool_calloc(pool, sizeof(*ctx));
  ctx->pool = pool;
  ulist_init(&ctx->sources, 32, sizeof(char*));
}
static void _setup_item(struct node *n, const char *v) {
  char buf[PATH_MAX];
  struct _configure_ctx *ctx = n->impl;
  struct unit *unit = unit_peek();
  char *tgt = 0, *src = path_normalize_cwd(v, unit->dir, buf);
  bool incache = path_is_prefix_for(g_env.project.cache_dir, src, unit->cache_dir);
  if (!incache) {
    tgt = path_relativize_cwd(unit->dir, src, unit->dir);
    src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
  } else {
    tgt = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
    src = xstrdup(tgt);
  }
  if (utils_endswith(tgt, ".in")) {
    size_t len = strlen(tgt);
    tgt[len - AK_LLEN(".in")] = '\0';
  }
  node_product_add(n, tgt, 0);
  v = pool_strdup(ctx->pool, src);
  ulist_push(&ctx->sources, &v);
  free(tgt);
  free(src);
}
static void _configure_setup(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (is_vlist(v)) {
      struct vlist_iter iter;
      while (vlist_iter_next(&iter)) {
        char buf[iter.len + 1];
        memcpy(buf, iter.item, iter.len);
        buf[iter.len] = '\0';
        _setup_item(n, buf);
      }
    } else {
      _setup_item(n, v);
    }
  }
}
static void _configure_dispose(struct node *n) {
  struct _configure_ctx *ctx = n->impl;
  if (ctx) {
    ulist_destroy_keep(&ctx->sources);
    pool_destroy(ctx->pool);
  }
}
int node_configure_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->dispose = _configure_dispose;
  n->init = _configure_init;
  n->setup = _configure_setup;
  n->build = _configure_build;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "ulist.h"
#include "pool.h"
#include "log.h"
#include "paths.h"
#include "env.h"
#include "utils.h"
#include "xstr.h"
#include "spawn.h"
#include "alloc.h"
#include "map.h"
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#endif
struct _cc_ctx {
  struct pool *pool;
  struct ulist sources;     // char*
  struct ulist objects;     // char*
  struct node *n;
  struct node *n_cflags;
  struct node *n_sources;
  struct node *n_cc;
  struct node *n_consumes;
  struct node *n_objects;
  const char  *cc;
  struct ulist consumes;    // sizeof(char*)
  int num_failed;
};
static void _cc_deps_MMD_item_add(const char *item, struct node *n, struct deps *deps, const char *src) {
  char buf[127];
  char *p = strrchr(item, '.');
  if (!p || p[1] == '\0') {
    return;
  }
  ++p;
  char *ext = utils_strncpy(buf, p, sizeof(buf));
  if (*ext == 'c' || *ext == 'C' || *ext == 'm') {
    // Skip source files
    return;
  }
  deps_add_alias(deps, 's', src, item);
}
static void _cc_deps_MMD_add(struct node *n, struct deps *deps, const char *src, const char *obj) {
  char buf[MAX(2 * PATH_MAX, 8192)];
  size_t len = strlen(obj);
  utils_strncpy(buf, obj, sizeof(buf));
  char *p = strrchr(buf, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'd';
  p[2] = '\0';
  FILE *f = fopen(buf, "r");
  if (!f) {
    node_warn(n, "Failed to open compiler generated (-MMD) dependency file: %s", buf);
    return;
  }
  bool nl = false;
  while ((p = fgets(buf, sizeof(buf), f))) {
    if (!nl) {
      if (!utils_startswith(p, obj)) {
        continue;
      }
      p += len;
      if (*p++ != ':') {
        continue;
      }
    } else {
      nl = false;
    }
    while (*p != '\0') {
      while (*p != '\0' && utils_char_is_space(*p)) {
        ++p;
      }
      if (p[0] == '\\' && p[1] == '\n') {
        nl = true;
        break;
      }
      char *sp = p;
      char *ep = sp;
      while (*p != '\0' && !utils_char_is_space(*p)) {
        ep = p;
        ++p;
      }
      if (ep > sp) {
        if (*p != '\0') {
          *p = '\0';
          ++p;
        }
        _cc_deps_MMD_item_add(sp, n, deps, src);
      }
    }
  }
  fclose(f);
}
struct _cc_task {
  struct spawn *s;
  char *src;
  char *obj;
  pid_t pid;
};
static void _cc_task_destroy(struct _cc_task *t) {
  spawn_destroy(t->s);
  free(t->src);
  free(t->obj);
}
static void _cc_on_build_source(
  struct node     *n,
  struct deps     *deps,
  const char      *src,
  const char      *obj,
  struct _cc_task *task) {
  if (g_env.check.log) {
    xstr_printf(g_env.check.log, "%s: build src=%s obj=%s\n", n->name, src, obj);
  }
  struct _cc_ctx *ctx = n->impl;
  struct spawn *s = spawn_create(ctx->cc, ctx);
  task->s = s;
  spawn_set_nowait(s, true);
  if (ctx->n_cflags) {
    struct xstr *xstr = 0;
    const char *cflags = node_value(ctx->n_cflags);
    if (!is_vlist(cflags)) {
      xstr = xstr_create_empty();
      utils_split_values_add(cflags, xstr);
      cflags = xstr_ptr(xstr);
    }
    spawn_arg_add(s, cflags);
    xstr_destroy(xstr);
  }
  if (!spawn_arg_starts_with(s, "-M")) {
    spawn_arg_add(s, "-MMD");
  }
  spawn_arg_add(s, "-I./"); // Current cache dir
  spawn_arg_add(s, "-c");
  spawn_arg_add(s, src);
  spawn_arg_add(s, "-o");
  spawn_arg_add(s, obj);
  int rc = spawn_do(s);
  if (rc) {
    spawn_destroy(s);
    node_error(rc, ctx->n, "%s", ctx->cc);
    return;
  }
  task->pid = spawn_pid(s);
}
static void _cc_on_resolve(struct node_resolve *r) {
  struct deps deps;
  struct _cc_ctx *ctx = r->user_data;
  struct unit *unit = unit_peek();
  struct ulist *slist = &ctx->sources;
  struct ulist rlist = { .usize = sizeof(char*) };
  struct ulist tasks = { .usize = sizeof(struct _cc_task) };
  struct map *fmap = map_create_str(map_k_free);
  int max_jobs = g_env.max_parallel_jobs;
  if (max_jobs <= 0) {
    max_jobs = 1;
  }
  if (r->resolve_outdated.num) {
    for (int i = 0; i < r->resolve_outdated.num; ++i) {
      struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
      if (u->flags != 's') { // Rebuild all on any outdated non source dependency
        slist = &ctx->sources;
        break;
      } else {
        char *path = pool_strdup(r->pool, u->path);
        ulist_push(&rlist, &path);
        slist = &rlist;
      }
    }
  }
  int rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, ctx->n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(ctx->n, &deps);
  for (int i = 0; i < r->node_val_deps.num; ++i) {
    struct node *nv = *(struct node**) ulist_get(&r->node_val_deps, i);
    const char *val = node_value(nv);
    if (val) {
      deps_add(&deps, DEPS_TYPE_NODE_VALUE, 0, val, i);
    }
  }
  for (int i = 0; i < ctx->consumes.num; ++i) {
    const char *path = *(const char**) ulist_get(&ctx->consumes, i);
    deps_add(&deps, DEPS_TYPE_FILE, 0, path, 0);
  }
  for (int i = 0; i < slist->num || tasks.num > 0; ) {
    while (tasks.num < max_jobs && i < slist->num) {
      char buf[PATH_MAX];
      char *obj, *src = *(char**) ulist_get(slist, i);
      src = path_normalize_cwd(src, unit->cache_dir, buf);
      bool incache = path_is_prefix_for(g_env.project.cache_dir, src, unit->cache_dir);
      if (!incache) {
        obj = path_relativize_cwd(unit->dir, src, unit->dir);
        src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
      } else {
        obj = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
        src = xstrdup(obj);
      }
      char *p = strrchr(obj, '.');
      akassert(p && p[1] != '\0');
      p[1] = 'o';
      p[2] = '\0';
      struct _cc_task task = { .src = src, .obj = obj, .pid = -1 };
      _cc_on_build_source(ctx->n, &deps, src, obj, &task);
      if (task.pid == -1) {
        ++ctx->num_failed;
        map_put_str(fmap, src, (void*) (intptr_t) 1);
        if (slist == &ctx->sources) {
          deps_add(&deps, DEPS_TYPE_FILE_OUTDATED, 's', src, 0);
        }
        _cc_task_destroy(&task);
      } else {
        ulist_push(&tasks, &task);
      }
      ++i;
    }
    int wstatus = 0;
    pid_t pid = wait(&wstatus);
    if (pid == -1) {
      akfatal(errno, "wait() syscall failed", 0);
    }
    for (int j = 0; j < tasks.num; ++j) {
      struct _cc_task *t = (struct _cc_task*) ulist_get(&tasks, j);
      if (t->pid == pid) {
        spawn_set_wstatus(t->s, wstatus);
        int code = spawn_exit_code(t->s);
        if (code != 0) {
          ++ctx->num_failed;
          map_put_str(fmap, t->src, (void*) (intptr_t) 1);
        }
        if (slist == &ctx->sources) {
          deps_add(&deps, code != 0 ? DEPS_TYPE_FILE_OUTDATED : DEPS_TYPE_FILE, 's', t->src, 0);
          if (code == 0) {
            _cc_deps_MMD_add(ctx->n, &deps, t->src, t->obj);
          }
        }
        _cc_task_destroy(t);
        ulist_remove(&tasks, j);
        break;
      }
    }
  }
  if (slist != &ctx->sources) {
    for (int i = 0; i < ctx->sources.num; ++i) {
      char buf[PATH_MAX];
      char *obj, *src = *(char**) ulist_get(&ctx->sources, i);
      src = path_normalize_cwd(src, unit->cache_dir, buf);
      bool incache = path_is_prefix_for(g_env.project.cache_dir, src, unit->cache_dir);
      if (!incache) {
        obj = path_relativize_cwd(unit->dir, src, unit->dir);
        src = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
      } else {
        obj = path_relativize_cwd(unit->cache_dir, src, unit->cache_dir);
        src = xstrdup(obj);
      }
      char *p = strrchr(obj, '.');
      akassert(p && p[1] != '\0');
      p[1] = 'o';
      p[2] = '\0';
      bool failed = map_get(fmap, src) != 0;
      deps_add(&deps, failed ? DEPS_TYPE_FILE_OUTDATED : DEPS_TYPE_FILE, 's', src, 0);
      if (!failed) {
        _cc_deps_MMD_add(ctx->n, &deps, src, obj);
      }
      free(obj);
      free(src);
    }
  }
  deps_close(&deps);
  ulist_destroy_keep(&rlist);
  ulist_destroy_keep(&tasks);
  map_destroy(fmap);
}
static void _cc_on_consumed_resolved(const char *path_, void *d) {
  struct node *n = d;
  struct _cc_ctx *ctx = n->impl;
  const char *path = pool_strdup(ctx->pool, path_);
  ulist_push(&ctx->consumes, &path);
}
static void _cc_on_resolve_init(struct node_resolve *r) {
  struct _cc_ctx *ctx = r->n->impl;
  if (ctx->n_consumes) {
    node_consumes_resolve(r->n, ctx->n_consumes->child, 0, _cc_on_consumed_resolved, r->n);
  }
}
static void _cc_build(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
  for (int i = 0; i < ctx->sources.num; ++i) {
    const char *src = *(char**) ulist_get(&ctx->sources, i);
    if (!path_is_exist(src)) {
      struct node *pn = node_by_product_raw(n, src);
      if (pn) {
        node_build(pn);
      } else {
        node_fatal(AK_ERROR_DEPENDENCY_UNRESOLVED, n, "'%s'", src);
      }
    }
  }
  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .user_data = ctx,
    .on_init = _cc_on_resolve_init,
    .on_resolve = _cc_on_resolve,
    .node_val_deps = { .usize = sizeof(struct node*) }
  };
  if (node_is_value_may_be_dep_saved(ctx->n_cc)) {
    ulist_push(&r.node_val_deps, &ctx->n_cc);
  }
  if (node_is_value_may_be_dep_saved(ctx->n_cflags)) {
    ulist_push(&r.node_val_deps, &ctx->n_cflags);
  }
  if (node_is_value_may_be_dep_saved(ctx->n_sources)) {
    ulist_push(&r.node_val_deps, &ctx->n_sources);
  }
  node_resolve(&r);
  if (ctx->num_failed) {
    akfatal2("Compilation terminated with errors!");
  }
}
static void _cc_source_add(struct node *n, const char *src) {
  char buf[PATH_MAX], *p;
  if (src == 0 || *src == '\0') {
    return;
  }
  p = strrchr(src, '.');
  if (p == 0 || p[1] == '\0') {
    return;
  }
  struct _cc_ctx *ctx = n->impl;
  struct unit *unit = unit_peek();
  const char *npath = src;
  if (!path_is_absolute(src)) {
    npath = path_normalize_cwd(src, unit->dir, buf);
  }
  p = pool_strdup(ctx->pool, npath);
  ulist_push(&ctx->sources, &p);
  bool incache = path_is_prefix_for(g_env.project.cache_dir, p, unit->dir);
  const char *dir = incache ? unit->cache_dir : unit->dir;
  char *obj = path_relativize_cwd(dir, p, dir);
  p = strrchr(obj, '.');
  akassert(p && p[1] != '\0');
  p[1] = 'o';
  p[2] = '\0';
  ulist_push(&ctx->objects, &obj);
  node_product_add(n, obj, 0);
}
static void _cc_setup(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
  const char *val = node_value(ctx->n_sources);
  if (is_vlist(val)) {
    struct vlist_iter iter;
    vlist_iter_init(val, &iter);
    while (vlist_iter_next(&iter)) {
      char buf[PATH_MAX];
      utils_strnncpy(buf, iter.item, iter.len, sizeof(buf));
      _cc_source_add(n, buf);
    }
  } else {
    _cc_source_add(n, val);
  }
  if (ctx->n_cc) {
    const char *cc = node_value(ctx->n_cc);
    if (cc && *cc != '\0') {
      ctx->cc = pool_strdup(ctx->pool, cc);
    }
  }
  if (!ctx->cc) {
    const char *key = strcmp(n->value, "cc") == 0 ? "CC" : "CXX";
    if (key) {
      ctx->cc = pool_strdup(ctx->pool, node_env_get(n, key));
      if (g_env.verbose && ctx->cc) {
        node_info(n, "Found '%s' compiler in ${%s}", ctx->cc, key);
      }
    }
  }
  if (!ctx->cc) {
    if (strcmp(n->value, "cc") == 0) {
      ctx->cc = "cc";
    } else {
      ctx->cc = "c++";
    }
    node_warn(n, "Fallback compiler: %s", ctx->cc);
  } else if (g_env.verbose) {
    node_info(n, "Compiler: %s", ctx->cc);
  }
  const char *objskey = 0;
  if (ctx->n_objects && ctx->n_objects->child) {
    objskey = node_value(ctx->n_objects->child);
  }
  if (!objskey) {
    if (strcmp(n->value, "cc") == 0) {
      objskey = "CC_OBJS";
    } else {
      objskey = "CXX_OBJS";
    }
  }
  if (g_env.verbose) {
    node_info(n, "Objects in ${%s}", objskey);
  }
  char *objs = ulist_to_vlist(&ctx->objects);
  node_env_set(n, objskey, objs);
  free(objs);
}
static void _cc_init(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (strcmp(nn->value, "consumes") == 0) {
      ctx->n_consumes = nn;
      continue;
    } else if (strcmp(nn->value, "objects") == 0) {
      ctx->n_objects = nn;
      continue;
    }
    if (!ctx->n_sources) {
      ctx->n_sources = nn;
      continue;
    }
    if (!ctx->n_cflags) {
      ctx->n_cflags = nn;
      continue;
    }
    if (!ctx->n_cc) {
      ctx->n_cc = nn;
    }
  }
  if (!ctx->n_sources) {
    node_fatal(AK_ERROR_SCRIPT, n, "No sources specified");
  }
}
static void _cc_dispose(struct node *n) {
  struct _cc_ctx *ctx = n->impl;
  if (ctx) {
    for (int i = 0; i < ctx->objects.num; ++i) {
      char *p = *(char**) ulist_get(&ctx->objects, i);
      free(p);
    }
    ulist_destroy_keep(&ctx->sources);
    ulist_destroy_keep(&ctx->objects);
    ulist_destroy_keep(&ctx->consumes);
    pool_destroy(ctx->pool);
  }
}
int node_cc_setup(struct node *n) {
  n->flags |= NODE_FLG_IN_CACHE;
  n->init = _cc_init;
  n->setup = _cc_setup;
  n->build = _cc_build;
  n->dispose = _cc_dispose;
  struct pool *pool = pool_create_empty();
  struct _cc_ctx *ctx = pool_alloc(pool, sizeof(*ctx));
  *ctx = (struct _cc_ctx) {
    .pool = pool,
    .n = n,
    .sources = { .usize = sizeof(char*) },
    .objects = { .usize = sizeof(char*) },
    .consumes = { .usize = sizeof(char*) },
  };
  n->impl = ctx;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "log.h"
#include "xstr.h"
#include <stdlib.h>
#include <string.h>
#endif
static const char* _basename_value(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    free(n->impl);
    n->impl = 0;
  }
  if (n->impl) {
    return n->impl;
  }
  const char *val = 0;
  const char *replace_ext = 0;
  if (!n->child || !(val = node_value(n->child))) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Argument is not set");
  }
  if (n->child->next) {
    replace_ext = node_value(n->child);
  }
  struct xstr *xstr = xstr_create_empty();
  const char *dp = strrchr(val, '.');
  if (dp) {
    xstr_cat2(xstr, val, (dp - val));
  } else {
    xstr_cat(xstr, val);
  }
  if (replace_ext) {
    xstr_cat(xstr, replace_ext);
  }
  n->impl = xstr_destroy_keep_ptr(xstr);
  return n->impl;
}
static void _basename_dispose(struct node *n) {
  free(n->impl);
}
int node_basename_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->value_get = _basename_value;
  n->dispose = _basename_dispose;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "log.h"
#include "alloc.h"
#include "utils.h"
#endif
static void _foreach_setup(struct node *n) {
  if (!n->child) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "Foreach must have a variable name as first parameter");
  }
   struct node_foreach *fe = xcalloc(1, sizeof(*fe));
  n->impl = fe;
  fe->name = xstrdup(node_value(n->child));
  struct xstr *xstr = xstr_create_empty();
  const char *v = node_value(n->child->next);
  if (v && *v != '\0') {
    xstr_cat(xstr, v);
  }
  if (!is_vlist(xstr_ptr(xstr))) {
    xstr_unshift(xstr, "\1", 1);
  }
  fe->items = xstr_destroy_keep_ptr(xstr);
}
static void _foreach_dispose(struct node *n) {
  struct node_foreach *fe = n->impl;
  if (fe) {
    free(fe->items);
    free(fe->name);
    free(fe);
  }
}
static void _foreach_init(struct node *n) {
}
int node_foreach_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _foreach_init;
  n->setup = _foreach_setup;
  n->dispose = _foreach_dispose;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#endif
static void _in_sources_init(struct node *n) {
  // Remove me from the tree
  struct node *nn = n->child;
  struct node *prev = node_find_prev_sibling(n);
  struct node *next = n->next;
  if (nn) {
    n->next = nn;
    n->child = 0;
    if (prev) {
      prev->next = nn;
    } else {
      n->parent->child = nn;
    }
    for ( ; nn; nn = nn->next) {
      nn->parent = n->parent;
      if (nn->next == 0) {
        nn->next = next;
        break;
      }
    }
  } else if (prev) {
    prev->next = next;
  } else {
    n->parent->child = next;
  }
}
int node_in_sources_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _in_sources_init;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "env.h"
#include "paths.h"
#include "utils.h"
#include "alloc.h"
#include <unistd.h>
#endif
static const char* _dir_value(struct node *n) {
  struct node_foreach *fe = node_find_parent_foreach(n);
  if (fe) {
    free(n->impl);
    n->impl = 0;
  }
  if (n->impl) {
    return n->impl;
  }
  char buf[PATH_MAX];
  struct unit *root = unit_root();
  struct unit *unit = unit_peek();
  struct xstr *xstr = xstr_create_empty();
  const char *dir = 0;
  if (n->value[0] == 'S') {
    if (n->value[1] == 'S') {
      dir = unit->dir;
    } else {
      dir = root->dir;
    }
  } else {
    if (n->value[1] == 'C') {
      dir = unit->cache_dir;
    } else {
      dir = root->cache_dir;
    }
  }
  for (struct node *nn = n->child; nn; nn = nn->next) {
    const char *v = node_value(nn);
    if (is_vlist(v)) {
      struct vlist_iter iter;
      vlist_iter_init(v, &iter);
      while (vlist_iter_next(&iter)) {
        if (xstr_size(xstr) && iter.len) {
          if (iter.item[0] != '/' && !utils_endswith(xstr_ptr(xstr), "/")) {
            xstr_cat2(xstr, "/", 1);
          }
        }
        xstr_cat2(xstr, iter.item, iter.len);
      }
    } else {
      xstr_cat(xstr, v);
    }
  }
  if (xstr_size(xstr) == 0) {
    xstr_cat2(xstr, ".", 1);
  }
  char *path = path_normalize_cwd(xstr_ptr(xstr), dir, buf);
  xstr_destroy(xstr);
  n->impl = xstrdup(path);
  return n->impl;
}
static void _dir_dispose(struct node *n) {
  if (n->impl) {
    free(n->impl);
  }
}
int node_dir_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->value_get = _dir_value;
  n->dispose = _dir_dispose;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "env.h"
#endif
int node_option_setup(struct node *n) {
  if (!g_env.project.options || !n->child) {
    return 0;
  }
  const char *name = n->child->value;
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = n->child->next; nn; nn = nn->next) {
    if (xstr_size(xstr)) {
      xstr_cat2(xstr, " ", 1);
    }
    xstr_cat(xstr, nn->value);
  }
  xstr_printf(g_env.project.options, "%s: %s\n", name, xstr_ptr(xstr));
  xstr_destroy(xstr);
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "log.h"
#endif
static void _error_setup(struct node *n) {
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (node_value(nn)) {
      if (xstr_size(xstr)) {
        xstr_cat2(xstr, " ", 1);
      }
      xstr_cat(xstr, node_value(nn));
    }
  }
  node_fatal(AK_ERROR_FAIL, n, xstr_ptr(xstr));
}
int node_error_setup(struct node *n) {
  n->setup = _error_setup;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "alloc.h"
#include "xstr.h"
#include "utils.h"
#include <string.h>
#endif
struct _echo_ctx {
  struct node *n_init;
  struct node *n_setup;
  struct node *n_build;
};
static void _echo_item(struct xstr *xstr, const char *v, size_t len) {
  if (is_vlist(v)) {
    struct vlist_iter iter;
    vlist_iter_init(v, &iter);
    while (vlist_iter_next(&iter)) {
      _echo_item(xstr, iter.item, iter.len);
    }
  } else {
    if (xstr_size(xstr)) {
      xstr_cat2(xstr, " ", 1);
    }
    xstr_cat2(xstr, v, len);
  }
}
static void _echo(struct node *n) {
  struct xstr *xstr = xstr_create_empty();
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      const char *v = node_value(nn);
      if (v) {
        _echo_item(xstr, v, strlen(v));
      }
    }
  }
  node_info(n, "%s", xstr_ptr(xstr));
  xstr_destroy(xstr);
}
static void _echo_init(struct node *n) {
  struct _echo_ctx *ctx = n->impl;
  _echo(ctx->n_init);
}
static void _echo_setup(struct node *n) {
  struct _echo_ctx *ctx = n->impl;
  _echo(ctx->n_setup);
}
static void _echo_build(struct node *n) {
  struct _echo_ctx *ctx = n->impl;
  _echo(ctx->n_build);
}
static void _echo_dispose(struct node *n) {
  free(n->impl);
}
int node_echo_setup(struct node *n) {
  struct _echo_ctx *ctx = xcalloc(1, sizeof(*ctx));
  n->impl = ctx;
  n->dispose = _echo_dispose;
  for (struct node *nn = n->child; nn; nn = nn->next) {
    if (nn->type == NODE_TYPE_BAG) {
      if (!n->build && strcmp(nn->value, "build") == 0) {
        n->build = _echo_build;
        ctx->n_build = nn;
      } else if (!n->setup && strcmp(nn->value, "setup") == 0) {
        n->setup = _echo_setup;
        ctx->n_setup = nn;
      } else if (!n->init && strcmp(nn->value, "init") == 0) {
        n->init = _echo_init;
        ctx->n_init = nn;
      }
    }
  }
  if (!(n->init || n->setup || n->build)) {
    n->build = _echo_build;
    ctx->n_build = n;
  }
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "ulist.h"
#include "env.h"
#include "log.h"
#include "paths.h"
#include "pool.h"
#include "utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#endif
struct _install_on_resolve_ctx {
  struct node_resolve *r;
  struct node *n;
  struct node *n_target;
  struct ulist consumes;  // sizeof(char*)
};
static void _install_symlink(struct _install_on_resolve_ctx *ctx, const char *src, const char *dst, struct stat *st) {
  char buf[PATH_MAX];
  struct node *n = ctx->n;
  node_info(n, "Symlink %s => %s", src, dst);
  ssize_t len = readlink(src, buf, sizeof(buf) - 1);
  if (len == -1) {
    node_fatal(errno, n, "Error reading symlink: %s", src);
  }
  buf[len] = '\0';
  if (symlink(buf, dst) == -1) {
    if (errno == EEXIST) {
      unlink(dst);
      if (symlink(buf, dst) == -1) {
        node_fatal(errno, n, "Error creating symlink: %s", dst);
      }
    } else {
      node_fatal(errno, n, "Error creating symlink: %s", dst);
    }
  }
  struct timespec times[2] = { st->st_atim, st->st_mtim };
  utimensat(AT_FDCWD, dst, times, AT_SYMLINK_NOFOLLOW);
}
static void _install_file(struct _install_on_resolve_ctx *ctx, const char *src, const char *dst, struct stat *st) {
  struct node *n = ctx->n;
  node_info(n, "File %s => %s", src, dst);
  int in_fd = open(src, O_RDONLY);
  if (in_fd == -1) {
    node_fatal(errno, n, "Error opening file: %s", src);
  }
  int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st->st_mode);
  if (out_fd == -1) {
    node_fatal(errno, n, "Error opening file: %s", dst);
  }
  char buf[8192];
  for (ssize_t r; ; ) {
    r = read(in_fd, buf, sizeof(buf));
    if (r < 0) {
      if (errno == EAGAIN) {
        continue;
      } else {
        node_fatal(errno, n, "Error reading file: %s", src);
      }
    } else if (r == 0) {
      break;
    }
    for (ssize_t w, tow = r; tow > 0; ) {
      w = write(out_fd, buf + r - tow, tow);
      if (w >= 0) {
        tow -= w;
      } else if (w < 0) {
        if (errno == EAGAIN) {
          continue;
        }
        node_fatal(errno, n, "Error writing file: %s", dst);
      }
    }
  }
  fchmod(out_fd, st->st_mode);
  struct timespec times[2] = { st->st_atim, st->st_mtim };
  futimens(out_fd, times);
  close(in_fd);
  close(out_fd);
}
static void _install_do(struct node_resolve *r, const char *src, const char *target) {
  char src_buf[PATH_MAX];
  char dst_buf[PATH_MAX];
  struct stat st;
  struct _install_on_resolve_ctx *ctx = r->user_data;
  akcheck(lstat(src, &st));
  if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)) {
    node_fatal(AK_ERROR_FAIL, ctx->n, "Cannot install unsupported file type. File: %s", src);
  }
  utils_strncpy(src_buf, src, sizeof(src_buf));
  snprintf(dst_buf, sizeof(dst_buf), "%s/%s", target, path_basename(src_buf));
  if (S_ISREG(st.st_mode)) {
    _install_file(ctx, src, dst_buf, &st);
  } else if (S_ISLNK(st.st_mode)) {
    _install_symlink(ctx, src, dst_buf, &st);
  }
}
static void _install_dep_add(struct deps *deps, const char *src, const char *target) {
  char src_buf[PATH_MAX];
  char dst_buf[PATH_MAX];
  deps_add(deps, DEPS_TYPE_FILE, 's', src, 0);
  utils_strncpy(src_buf, src, sizeof(src_buf));
  snprintf(dst_buf, sizeof(dst_buf), "%s/%s", target, path_basename(src_buf));
  deps_add_alias(deps, 's', src, dst_buf);
}
static void _install_on_resolve(struct node_resolve *r) {
  struct deps deps;
  struct node *n = r->n;
  struct _install_on_resolve_ctx *ctx = r->user_data;
  const char *target = node_value(ctx->n_target);
  if (!path_is_absolute(target)) {
    target = pool_printf(r->pool, "%s/%s", g_env.install.prefix_dir, target);
    target = path_normalize_pool(target, g_env.pool);
  }
  if (path_is_exist(target) && !path_is_dir(target)) {
    node_fatal(AK_ERROR_FAIL, n, "Target: %s is not a directory", target);
  }
  int rc = path_mkdirs(target);
  if (rc) {
    node_fatal(rc, n, "Failed to create target install directory: %s", target);
  }
  struct ulist rlist = { .usize = sizeof(char*) };
  struct ulist *slist = &ctx->consumes;
  if (r->resolve_outdated.num) {
    for (int i = 0; i < r->resolve_outdated.num; ++i) {
      struct resolve_outdated *u = ulist_get(&r->resolve_outdated, i);
      if (u->flags != 's') { // Rebuild all on any outdated non source dependency
        slist = &ctx->consumes;
        break;
      } else {
        char *path = pool_strdup(r->pool, u->path);
        ulist_push(&rlist, &path);
        slist = &rlist;
      }
    }
  }
  for (int i = 0; i < slist->num; ++i) {
    const char *src = *(const char**) ulist_get(slist, i);
    _install_do(r, src, target);
  }
  rc = deps_open(r->deps_path_tmp, 0, &deps);
  if (rc) {
    node_fatal(rc, n, "Failed to open dependency file: %s", r->deps_path_tmp);
  }
  node_add_unit_deps(n, &deps);
  deps_add_env(&deps, 0, "INSTALL_PREFIX", g_env.install.prefix_dir);
  for (int i = 0; i < r->node_val_deps.num; ++i) {
    struct node *nv = *(struct node**) ulist_get(&r->node_val_deps, i);
    const char *val = node_value(nv);
    if (val) {
      deps_add(&deps, DEPS_TYPE_NODE_VALUE, 0, val, i);
    }
  }
  for (int i = 0; i < ctx->consumes.num; ++i) {
    const char *src = *(const char**) ulist_get(&ctx->consumes, i);
    _install_dep_add(&deps, src, target);
  }
  deps_close(&deps);
  ulist_destroy_keep(&rlist);
}
static void _install_on_consumed_resolved(const char *path_, void *d) {
  struct _install_on_resolve_ctx *ctx = d;
  if (path_is_dir(path_)) {
    node_fatal(AK_ERROR_FAIL, ctx->n, "Installing of directories is not supported. Path: %s", path_);
  }
  const char *path = pool_strdup(ctx->r->pool, path_);
  ulist_push(&ctx->consumes, &path);
}
static void _install_on_resolve_init(struct node_resolve *r) {
  struct _install_on_resolve_ctx *ctx = r->user_data;
  ctx->r = r;
  struct node *nn = ctx->n_target->next;
  if (nn) {
    node_consumes_resolve(r->n, nn, 0, _install_on_consumed_resolved, ctx);
  }
}
static void _install_post_build(struct node *n) {
  struct _install_on_resolve_ctx ctx = {
    .n = n,
    .n_target = n->child,
    .consumes = { .usize = sizeof(char*) },
  };
  if (!ctx.n_target || !node_is_can_be_value(ctx.n_target)) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "No target dir specified");
  }
  if (!ctx.n_target->next || !node_is_can_be_value(ctx.n_target->next)) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "At least one source file/dir should be specified");
  }
  struct node_resolve r = {
    .n = n,
    .path = n->vfile,
    .user_data = &ctx,
    .on_init = _install_on_resolve_init,
    .on_resolve = _install_on_resolve,
    .node_val_deps = { .usize = sizeof(struct node*) }
  };
  for (struct node *nn = ctx.n_target; nn; nn = nn->next) {
    if (node_is_value_may_be_dep_saved(nn)) {
      ulist_push(&r.node_val_deps, &nn);
    }
  }
  node_resolve(&r);
  ulist_destroy_keep(&ctx.consumes);
}
int node_install_setup(struct node *n) {
  if (!g_env.install.enabled || !g_env.install.prefix_dir) {
    return 0;
  }
  n->flags |= NODE_FLG_IN_CACHE;
  n->post_build = _install_post_build;
  return 0;
}
#ifndef _AMALGAMATE_
#include "script.h"
#include "xstr.h"
#include "env.h"
#include "utils.h"
#include "paths.h"
#include "alloc.h"
#include <string.h>
#endif
static void _library_find(struct node *n) {
  struct node *nn = n->child;
  struct unit *unit = unit_peek();
  struct xstr *names = xstr_create_empty();
  for (nn = nn->next; nn; nn = nn->next) {
    if (node_is_can_be_value(nn)) {
      const char *val = node_value(nn);
      if (val && *val != '\0') {
        xstr_printf(names, "\1%s", val);
      }
    }
  }
  struct xstr *dirs = xstr_create_empty();
  const char *ldir = env_libdir();
  bool sld = strcmp(ldir, "lib") != 0;
  xstr_printf(dirs, "\1%s/%s/", unit->cache_dir, ldir);
  if (sld) {
    xstr_printf(dirs, "\1%s/lib/", unit->cache_dir);
  }
  if (strcmp(unit->cache_dir, g_env.project.cache_dir) != 0) {
    xstr_printf(dirs, "\1%s/%s/", g_env.project.cache_dir, ldir);
    if (sld) {
      xstr_printf(dirs, "\1%s/lib/", g_env.project.cache_dir);
    }
  }
  const char *home = getenv("HOME");
  if (home) {
    xstr_printf(dirs, "\1%s/.local/%s/", home, ldir);
    if (sld) {
      xstr_printf(dirs, "\1%s/.local/lib/", home);
    }
  }
  xstr_printf(dirs, "\1/usr/local/%s/", ldir);
  if (sld) {
    xstr_cat(dirs, "\1/usr/local/lib/");
  }
  xstr_printf(dirs, "\1/usr/%s/", ldir);
  if (sld) {
    xstr_cat(dirs, "\1/usr/lib/");
  }
  xstr_printf(dirs, "\1/%s/", ldir);
  if (sld) {
    xstr_cat(dirs, "\1/lib/");
  }
  struct vlist_iter iter_dirs;
  vlist_iter_init(xstr_ptr(dirs), &iter_dirs);
  while (vlist_iter_next(&iter_dirs)) {
    struct vlist_iter iter_names;
    vlist_iter_init(xstr_ptr(names), &iter_names);
    while (vlist_iter_next(&iter_names)) {
      char buf[PATH_MAX];
      snprintf(buf, sizeof(buf), "%.*s%.*s",
               (int) iter_dirs.len, iter_dirs.item, (int) iter_names.len, iter_names.item);
      if (path_is_exist(buf)) {
        n->impl = xstrdup(buf);
        goto finish;
      }
    }
  }
finish:
  xstr_destroy(names);
  xstr_destroy(dirs);
}
static const char* _find_value_get(struct node *n) {
  if (n->impl) {
    if ((uintptr_t) n->impl == (uintptr_t) -1) {
      return "";
    } else {
      return n->impl;
    }
  }
  n->impl = (void*) (intptr_t) -1;
  if (strcmp("library", n->value) == 0) {
    _library_find(n);
  }
  if (!n->impl) {
    n->impl = (void*) (uintptr_t) -1;
  }
  if ((uintptr_t) n->impl == (uintptr_t) -1) {
    return "";
  } else {
    return n->impl;
  }
}
static struct unit* _unit_for_find(struct node *n, struct node *nn, const char **keyp) {
  if (nn->type == NODE_TYPE_BAG) {
    if (strcmp(nn->value, "root") == 0) {
      *keyp = node_value(nn->child);
      return unit_root();
    } else if (strcmp(nn->value, "parent") == 0) {
      *keyp = node_value(nn->child);
      return unit_parent(n);
    }
  } else {
    *keyp = node_value(nn);
  }
  return unit_peek();
}
static void _find_init(struct node *n) {
  const char *key = 0;
  struct unit *unit = n->child ? _unit_for_find(n, n->child, &key) : 0;
  if (!key) {
    node_fatal(AK_ERROR_SCRIPT_SYNTAX, n, "No name specified for '%s' directive", n->value);
    return;
  }
  unit_env_set_node(unit, key, n);
}
static void _find_dispose(struct node *n) {
  if ((uintptr_t) n->impl != (uintptr_t) -1) {
    free(n->impl);
  }
  n->impl = 0;
}
int node_find_setup(struct node *n) {
  n->flags |= NODE_FLG_NO_CWD;
  n->init = _find_init;
  n->value_get = _find_value_get;
  n->dispose = _find_dispose;
  return 0;
}
#ifndef _AMALGAMATE_
#ifndef META_VERSION
#define META_VERSION "dev"
#endif
#ifndef META_REVISION
#define META_REVISION ""
#endif
#include "env.h"
#include "utils.h"
#include "log.h"
#include "paths.h"
#include "script.h"
#include "autark.h"
#include "map.h"
#include "alloc.h"
#include "deps.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <glob.h>
#endif
struct env g_env;
static void _unit_destroy(struct unit *unit) {
  map_destroy(unit->env);
}
void unit_env_set_val(struct unit *u, const char *key, const char *val) {
  size_t len = sizeof(struct unit_env_item);
  if (val) {
    len += strlen(val) + 1;
  }
  struct unit_env_item *item = xmalloc(len);
  item->n = 0;
  if (len > sizeof(struct unit_env_item)) {
    char *wp = ((char*) item) + sizeof(struct unit_env_item);
    memcpy(wp, val, len - sizeof(struct unit_env_item));
    item->val = wp;
  } else {
    item->val = 0;
  }
  map_put_str(u->env, key, item);
}
void unit_env_set_node(struct unit *u, const char *key, struct node *n) {
  struct unit_env_item *item = xmalloc(sizeof(*item));
  item->val = 0;
  item->n = n;
  map_put_str(u->env, key, item);
}
struct node* unit_env_get_node(struct unit *u, const char *key) {
  struct unit_env_item *item = map_get(u->env, key);
  if (item) {
    return item->n;
  }
  return 0;
}
const char* unit_env_get_raw(struct unit *u, const char *key) {
  struct unit_env_item *item = map_get(u->env, key);
  if (item) {
    if (item->val) {
      return item->val;
    } else {
      return node_value(item->n);
    }
  }
  return 0;
}
const char* unit_env_get(struct node *n, const char *key) {
  struct unit *prev = 0;
  for (struct node *nn = n; nn; nn = nn->parent) {
    if (nn->unit && nn->unit != prev) {
      prev = nn->unit;
      const char *ret = unit_env_get_raw(nn->unit, key);
      if (ret) {
        return ret;
      }
    }
  }
  return getenv(key);
}
void unit_env_remove(struct unit *u, const char *key) {
  map_remove(u->env, key);
}
void on_unit_pool_destroy(struct pool *pool) {
  for (int i = 0; i < g_env.units.num; ++i) {
    struct unit *unit = *(struct unit**) ulist_get(&g_env.units, i);
    if (unit->pool == pool) {
      map_remove(g_env.map_path_to_unit, unit->source_path);
      map_remove(g_env.map_path_to_unit, unit->cache_path);
      _unit_destroy(unit);
      ulist_remove(&g_env.units, i);
      --i;
    }
  }
}
struct unit* unit_for_path(const char *path) {
  struct unit *unit = map_get(g_env.map_path_to_unit, path);
  return unit;
}
struct unit* unit_create(const char *unit_path_, unsigned flags, struct pool *pool) {
  char path[PATH_MAX];
  const char *unit_path = unit_path_;
  if (path_is_absolute(unit_path)) {
    const char *p = path_is_prefix_for(g_env.project.cache_dir, unit_path, 0);
    if (p) {
      unit_path = p;
    } else {
      p = path_is_prefix_for(g_env.project.root_dir, unit_path, 0);
      if (!p) {
        akfatal(AK_ERROR_FAIL, "Unit path: %s must be either in project root or cache directory", unit_path_);
      }
      unit_path = p;
    }
  }
  struct unit *unit = pool_calloc(g_env.pool, sizeof(*unit));
  unit->flags = flags;
  unit->pool = pool;
  unit->env = map_create_str(map_kv_free);
  unit->rel_path = pool_strdup(pool, unit_path);
  unit->basename = path_basename((char*) unit->rel_path);
  unit->source_path = pool_printf(pool, "%s/%s", g_env.project.root_dir, unit_path);
  utils_strncpy(path, unit->source_path, sizeof(path));
  path_dirname(path);
  unit->dir = pool_strdup(pool, path);
  unit->cache_path = pool_printf(pool, "%s/%s", g_env.project.cache_dir, unit_path);
  utils_strncpy(path, unit->cache_path, sizeof(path));
  path_dirname(path);
  unit->cache_dir = pool_strdup(pool, path);
  int rc = path_mkdirs(unit->cache_dir);
  if (rc) {
    akfatal(rc, "Failed to create directory: %s", path);
  }
  map_put_str_no_copy(g_env.map_path_to_unit, unit->source_path, unit);
  map_put_str_no_copy(g_env.map_path_to_unit, unit->cache_path, unit);
  ulist_push(&g_env.units, &unit);
  if (g_env.verbose) {
    if (unit->flags & UNIT_FLG_ROOT) {
      akinfo("%s: AUTARK_ROOT_DIR=%s", unit->rel_path, unit->dir);
      akinfo("%s: AUTARK_CACHE_DIR=%s", unit->rel_path, unit->cache_dir);
    } else {
      akinfo("%s: UNIT_DIR=%s", unit->rel_path, unit->dir);
      akinfo("%s: UNIT_CACHE_DIR=%s", unit->rel_path, unit->cache_dir);
    }
  }
  if (unit->flags & UNIT_FLG_ROOT) {
    unit_env_set_val(unit, "AUTARK_ROOT_DIR", unit->dir);
    unit_env_set_val(unit, "AUTARK_CACHE_DIR", unit->cache_dir);
  }
  unit_env_set_val(unit, "UNIT_DIR", unit->dir);
  unit_env_set_val(unit, "UNIT_CACHE_DIR", unit->cache_dir);
  return unit;
}
void unit_push(struct unit *unit, struct node *n) {
  akassert(unit);
  struct unit_ctx ctx = { unit, 0 };
  if (n) {
    ctx.flags = n->flags;
  }
  ulist_push(&g_env.stack_units, &ctx);
  unit_ch_dir(&ctx, 0);
  setenv(AUTARK_UNIT, unit->rel_path, 1);
}
struct unit* unit_pop(void) {
  akassert(g_env.stack_units.num > 0);
  struct unit_ctx *ctx = (struct unit_ctx*) ulist_get(&g_env.stack_units, g_env.stack_units.num - 1);
  ulist_pop(&g_env.stack_units);
  struct unit_ctx peek = unit_peek_ctx();
  if (peek.unit) {
    unit_ch_dir(&peek, 0);
    setenv(AUTARK_UNIT, peek.unit->rel_path, 1);
  } else {
    unsetenv(AUTARK_UNIT);
  }
  return ctx->unit;
}
struct unit* unit_peek(void) {
  struct unit *u = unit_peek_ctx().unit;
  akassert(u);
  return u;
}
struct unit* unit_root(void) {
  akassert(g_env.stack_units.num);
  struct unit_ctx uc = *(struct unit_ctx*) ulist_get(&g_env.stack_units, 0);
  return uc.unit;
}
struct unit* unit_parent(struct node *n) {
  struct unit *u = n->unit;
  for (struct node *nn = n->parent; nn; nn = nn->parent) {
    if (!u) {
      u = nn->unit;
    } else if (nn->unit && n->unit != nn->unit) {
      return nn->unit;
    }
  }
  return unit_root();
}
struct unit_ctx unit_peek_ctx(void) {
  if (g_env.stack_units.num == 0) {
    return (struct unit_ctx) {
             0, 0
    };
  }
  return *(struct unit_ctx*) ulist_get(&g_env.stack_units, g_env.stack_units.num - 1);
}
void unit_ch_dir(struct unit_ctx *ctx, char *prevcwd) {
  if (ctx->flags & NODE_FLG_IN_ANY) {
    if (ctx->flags & NODE_FLG_IN_SRC) {
      unit_ch_src_dir(ctx->unit, prevcwd);
    } else if (ctx->flags & NODE_FLG_IN_CACHE) {
      unit_ch_cache_dir(ctx->unit, prevcwd);
    }
  } else if (ctx->unit->flags & UNIT_FLG_SRC_CWD) {
    unit_ch_src_dir(ctx->unit, prevcwd);
  } else if (!(ctx->unit->flags & UNIT_FLG_NO_CWD)) {
    unit_ch_cache_dir(ctx->unit, prevcwd);
  }
}
void unit_ch_cache_dir(struct unit *unit, char *prevcwd) {
  akassert(unit);
  if (prevcwd) {
    akassert(getcwd(prevcwd, PATH_MAX));
  }
  akcheck(chdir(unit->cache_dir));
}
void unit_ch_src_dir(struct unit *unit, char *prevcwd) {
  akassert(unit);
  if (prevcwd) {
    akassert(getcwd(prevcwd, PATH_MAX));
  }
  akcheck(chdir(unit->dir));
}
static int _usage_va(const char *err, va_list ap) {
  if (err) {
    fprintf(stderr, "\nError:  ");
    vfprintf(stderr, err, ap);
    fprintf(stderr, "\n\n");
  }
  fprintf(stderr, "Usage\n");
  fprintf(stderr, "\nCommon options:\n"
          "    -V, --verbose               Outputs verbose execution info.\n"
          "    -v, --version               Prints version info.\n"
          "    -h, --help                  Prints usage help.\n");
  fprintf(stderr,
          "\nautark [sources_dir/command] [options]\n"
          "  Build project in given sources dir.\n");
  fprintf(stderr,
          "    -H, --cache=<>              Project cache/build dir. Default: ./" AUTARK_CACHE "\n");
  fprintf(stderr,
          "    -c, --clean                 Clean build cache dir.\n");
  fprintf(stderr,
          "    -l, --options               List of all available project options and their description.\n");
  fprintf(stderr,
          "    -J  --jobs=<>               Number of jobs used in c/cxx compilation tasks. Default: 4\n");
  fprintf(stderr,
          "    -D<option>[=<val>]          Set project build option.\n");
  fprintf(stderr,
          "    -I, --install               Install all built artifacts\n");
  fprintf(stderr,
          "    -R, --prefix=<>             Install prefix. Default: $HOME/.local\n");
  fprintf(stderr,
          "        --bindir=<>             Path to 'bin' dir relative to a `prefix` dir. Default: bin\n");
  fprintf(stderr,
          "        --libdir=<>             Path to 'lib' dir relative to a `prefix` dir. Default: lib\n");
  fprintf(stderr,
          "        --datadir=<>            Path to 'data' dir relative to a `prefix` dir. Default: share\n");
  fprintf(stderr,
          "        --includedir=<>         Path to 'include' dir relative to `prefix` dir. Default: include\n");
  fprintf(stderr,
          "        --mandir=<>             Path to 'man' dir relative to `prefix` dir. Default: share/man\n");
#ifdef __FreeBSD__
  fprintf(stderr,
          "        --pkgconfdir=<>         Path to 'pkgconfig' dir relative to prefix dir. Default: libdata/pkgconfig");
#else
  fprintf(stderr,
          "        --pkgconfdir=<>         Path to 'pkgconfig' dir relative to prefix dir. Default: lib/pkgconfig");
#endif
  fprintf(stderr, "\nautark <cmd> [options]\n");
  fprintf(stderr, "  Execute a given command from check script.\n");
  fprintf(stderr,
          "\nautark set <key>=<value>\n"
          "  Sets key/value pair as output for check script.\n");
  fprintf(stderr,
          "\nautark dep <file>\n"
          "  Registers a given file as dependency for check script.\n");
  fprintf(stderr,
          "\nautark env <env>\n"
          "  Registers a given environment variable as dependency for check script.\n");
  fprintf(stderr,
          "\nautark glob <pattern>\n"
          "   -C, --dir                   Current directory for glob list.\n"
          "  Lists files in current directory filtered by glob pattern.\n");
  fprintf(stderr, "\n");
  return AK_ERROR_INVALID_ARGS;
}
__attribute__((noreturn)) static void _usage(const char *err, ...) {
  va_list ap;
  va_start(ap, err);
  _usage_va(err, ap);
  va_end(ap);
  _exit(1);
}
void autark_build_prepare(const char *script_path) {
  if (g_env.project.prepared) {
    return;
  }
  g_env.project.prepared = true;
  char path_buf[PATH_MAX];
  autark_init();
  if (!g_env.project.root_dir) {
    if (script_path) {
      utils_strncpy(path_buf, script_path, PATH_MAX);
      g_env.project.root_dir = pool_strdup(g_env.pool, path_dirname(path_buf));
    } else {
      g_env.project.root_dir = g_env.cwd;
    }
    akassert(g_env.project.root_dir);
  }
  const char *root_dir = path_normalize_pool(g_env.project.root_dir, g_env.pool);
  if (!root_dir) {
    akfatal(AK_ERROR_FAIL, "%s is not a directory", root_dir);
  }
  g_env.project.root_dir = root_dir;
  g_env.cwd = root_dir;
  if (chdir(root_dir)) {
    akfatal(errno, "Failed to change dir to: %s", root_dir);
  }
  if (!g_env.project.cache_dir) {
    g_env.project.cache_dir = AUTARK_CACHE;
  }
  g_env.project.cache_dir = path_normalize_pool(g_env.project.cache_dir, g_env.pool);
  if (!g_env.project.cache_dir) {
    akfatal(errno, "Failed to resolve project CACHE dir: %s", g_env.project.cache_dir);
  }
  if (path_is_prefix_for(g_env.project.cache_dir, g_env.project.root_dir, 0)) {
    akfatal(AK_ERROR_FAIL, "Project cache dir cannot be parent of project root dir", 0);
  }
  if (g_env.project.cleanup) {
    if (path_is_dir(g_env.project.cache_dir)) {
      int rc = path_rm_cache(g_env.project.cache_dir);
      if (rc) {
        akfatal(rc, "Failed to remove cache directory: %s", g_env.project.cache_dir);
      }
    }
  }
  utils_strncpy(path_buf, script_path, PATH_MAX);
  const char *path = path_basename(path_buf);
  struct unit *unit = unit_create(path, UNIT_FLG_SRC_CWD | UNIT_FLG_ROOT, g_env.pool);
  unit_push(unit, 0);
  if (!path_is_dir(g_env.project.cache_dir) || !path_is_accesible_read(g_env.project.cache_dir)) {
    akfatal(AK_ERROR_FAIL, "Failed to access build CACHE directory: %s", g_env.project.cache_dir);
  }
  setenv(AUTARK_ROOT_DIR, g_env.project.root_dir, 1);
  setenv(AUTARK_CACHE_DIR, g_env.project.cache_dir, 1);
  if (g_env.verbose) {
    setenv(AUTARK_VERBOSE, "1", 1);
  }
}
static void _project_env_read(void) {
  autark_init();
  const char *val = getenv(AUTARK_CACHE_DIR);
  if (!val) {
    akfatal(AK_ERROR_FAIL, "Missing required AUTARK_CACHE_DIR env variable", 0);
  }
  g_env.project.cache_dir = pool_strdup(g_env.pool, val);
  val = getenv(AUTARK_ROOT_DIR);
  if (!val) {
    akfatal(AK_ERROR_FAIL, "Missing required AUTARK_ROOT_DIR env variable", 0);
  }
  g_env.project.root_dir = pool_strdup(g_env.pool, val);
  val = getenv(AUTARK_UNIT);
  if (!val) {
    akfatal(AK_ERROR_FAIL, "Missing required AUTARK_UNIT env variable", 0);
  }
  if (path_is_absolute(val)) {
    akfatal(AK_ERROR_FAIL, "AUTARK_UNIT cannot be an absolute path", 0);
  }
  struct unit *unit = unit_create(val, UNIT_FLG_NO_CWD, g_env.pool);
  unit_push(unit, 0);
}
static void _on_command_set(int argc, const char **argv) {
  _project_env_read();
  const char *kv;
  if (optind >= argc) {
    _usage("Missing <key> argument");
  }
  struct unit *unit = unit_peek();
  const char *env_path = pool_printf(g_env.pool, "%s.env.tmp", unit->cache_path);
  FILE *f = fopen(env_path, "a+");
  if (!f) {
    akfatal(errno, "Failed to open file: %s", env_path);
  }
  for ( ; optind < argc; ++optind) {
    kv = argv[optind];
    if (strchr(kv, '=')) {
      if (g_env.verbose) {
        akinfo("autark set %s", kv);
      }
      fprintf(f, "%s\n", kv);
    }
  }
  fclose(f);
}
static void _on_command_dep(int argc, const char **argv) {
  _project_env_read();
  if (optind >= argc) {
    _usage("Missing required dependency option");
  }
  const char *file = argv[optind];
  if (g_env.verbose) {
    akinfo("autark dep %s", file);
  }
  int type = DEPS_TYPE_FILE;
  if (strcmp(file, "-") == 0) {
    type = DEPS_TYPE_OUTDATED;
  }
  struct deps deps;
  struct unit *unit = unit_peek();
  const char *deps_path = pool_printf(g_env.pool, "%s.%s", unit->cache_path, "deps.tmp");
  int rc = deps_open(deps_path, false, &deps);
  if (rc) {
    akfatal(rc, "Failed to open deps file: %s", deps_path);
  }
  rc = deps_add(&deps, type, 0, file, 0);
  if (rc) {
    akfatal(rc, "Failed to write deps file: %s", deps_path);
  }
  deps_close(&deps);
}
static void _on_command_dep_env(int argc, const char **argv) {
  _project_env_read();
  if (optind >= argc) {
    _usage("Missing required dependency option");
  }
  const char *key = argv[optind];
  if (g_env.verbose) {
    akinfo("autark dep env %s", key);
  }
  const char *val = getenv(key);
  if (!val) {
    val = "";
  }
  struct deps deps;
  struct unit *unit = unit_peek();
  const char *deps_path = pool_printf(g_env.pool, "%s.%s", unit->cache_path, "deps.tmp");
  int rc = deps_open(deps_path, false, &deps);
  if (rc) {
    akfatal(rc, "Failed to open deps file: %s", deps_path);
  }
  rc = deps_add_sys_env(&deps, 0, key, val);
  if (rc) {
    akfatal(rc, "Failed to write deps file: %s", deps_path);
  }
  deps_close(&deps);
}
static void _on_command_glob(int argc, const char **argv, const char *cdir) {
  if (cdir) {
    akcheck(chdir(cdir));
  }
  do {
    const char *pattern = "*";
    if (optind < argc) {
      pattern = argv[optind];
    }
    glob_t g;
    int rc = glob(pattern, 0, 0, &g);
    if (rc == 0) {
      for (int i = 0; i < g.gl_pathc; ++i) {
        puts(g.gl_pathv[i]);
      }
    }
    ;
    globfree(&g);
    if (rc && rc != GLOB_NOMATCH) {
      exit(1);
    }
  } while (++optind < argc);
}
#ifdef TESTS
void on_command_set(int argc, const char **argv) {
  _on_command_set(argc, argv);
}
void on_command_dep(int argc, const char **argv) {
  _on_command_dep(argc, argv);
}
void on_command_dep_env(int argc, const char **argv) {
  _on_command_dep_env(argc, argv);
}
#endif
static void _build(struct ulist *options) {
  struct sctx *x;
  int rc = script_open(AUTARK_SCRIPT, &x);
  if (rc) {
    akfatal(rc, "Failed to open script: %s", AUTARK_SCRIPT);
  }
  struct unit *root = unit_root();
  for (int i = 0; i < options->num; ++i) {
    const char *opt = *(char**) ulist_get(options, i);
    char *p = strchr(opt, '=');
    if (p) {
      *p = '\0';
      char *val = p + 1;
      for (int vlen = strlen(val); vlen >= 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r'); --vlen) {
        val[vlen - 1] = '\0';
      }
      unit_env_set_val(root, opt, val);
    } else {
      unit_env_set_val(root, opt, "");
    }
  }
  script_build(x);
  script_close(&x);
  akinfo("[%s] Build successful", g_env.project.root_dir);
}
static void _options(void) {
  struct sctx *x;
  int rc = script_open(AUTARK_SCRIPT, &x);
  if (rc) {
    akfatal(rc, "Failed to open script: %s", AUTARK_SCRIPT);
  }
  node_init(x->root);
  if (xstr_size(g_env.project.options)) {
    fprintf(stderr, "\n%s", xstr_ptr(g_env.project.options));
  }
}
void autark_init(void) {
  if (!g_env.pool) {
    g_env.stack_units.usize = sizeof(struct unit_ctx);
    g_env.units.usize = sizeof(struct unit*);
    g_env.pool = pool_create_empty();
    char buf[PATH_MAX];
    if (!getcwd(buf, PATH_MAX)) {
      akfatal(errno, 0, 0);
    }
    g_env.cwd = pool_strdup(g_env.pool, buf);
    g_env.map_path_to_unit = map_create_str(0);
  }
}
AK_DESTRUCTOR void autark_dispose(void) {
  if (g_env.pool) {
    struct pool *pool = g_env.pool;
    g_env.pool = 0;
    for (int i = 0; i < g_env.units.num; ++i) {
      struct unit *unit = *(struct unit**) ulist_get(&g_env.units, i);
      _unit_destroy(unit);
    }
    ulist_destroy_keep(&g_env.units);
    ulist_destroy_keep(&g_env.stack_units);
    map_destroy(g_env.map_path_to_unit);
    pool_destroy(pool);
    memset(&g_env, 0, sizeof(g_env));
  }
}
const char* env_libdir(void) {
 #if defined(__linux__)
  #if defined(__x86_64__)
  // RPM-style or Debian-style
    #if defined(DEBIAN_MULTIARCH)
  return "lib/x86_64-linux-gnu";      // Debian/Ubuntu
    #else
  return "lib64";                     // RHEL/Fedora
    #endif
  #elif defined(__i386__)
  return "lib";
  #elif defined(__aarch64__)
    #if defined(DEBIAN_MULTIARCH)
  return "aarch64-linux-gnu";
    #else
  return "lib64";
    #endif
  #elif defined(__arm__)
  return "lib";
  #else
  return "lib";
  #endif
#elif defined(__APPLE__)
  return "lib";    // macOS uses universal binaries
#elif defined(_WIN64)
  return "C:\\Windows\\System32";
#elif defined(_WIN32)
  return "C:\\Windows\\SysWOW64";
#else
  return "lib";
#endif
}
void autark_run(int argc, const char **argv) {
  akassert(argc > 0 && argv[0]);
  autark_init();
  static const struct option long_options[] = {
    { "cache", 1, 0, 'H' },
    { "clean", 0, 0, 'c' },
    { "help", 0, 0, 'h' },
    { "verbose", 0, 0, 'V' },
    { "version", 0, 0, 'v' },
    { "options", 0, 0, 'l' },
    { "install", 0, 0, 'I' },
    { "prefix", 1, 0, 'R' },
    { "dir", 1, 0, 'C' },
    { "jobs", 1, 0, 'J' },
    { "bindir", 1, 0, -1 },
    { "libdir", 1, 0, -2 },
    { "includedir", 1, 0, -3 },
    { "pkgconfdir", 1, 0, -4 },
    { "mandir", 1, 0, -5 },
    { "datadir", 1, 0, -6 },
    { 0 }
  };
  bool version = false;
  const char *cdir = 0;
  struct ulist options = { .usize = sizeof(char*) };
  for (int ch; (ch = getopt_long(argc, (void*) argv, "+H:chVvlR:C:D:J:I", long_options, 0)) != -1; ) {
    switch (ch) {
      case 'H':
        g_env.project.cache_dir = pool_strdup(g_env.pool, optarg);
        break;
      case 'V':
        g_env.verbose = 1;
        break;
      case 'c':
        g_env.project.cleanup = true;
        break;
      case 'v':
        version = true;
        break;
      case 'l':
        if (!g_env.project.options) {
          g_env.project.options = xstr_create_empty();
        }
        break;
      case 'D': {
        char *p = pool_strdup(g_env.pool, optarg);
        ulist_push(&options, &p);
        break;
      }
      case 'R':
        g_env.install.enabled = true;
        g_env.install.prefix_dir = path_normalize_cwd_pool(optarg, g_env.cwd, g_env.pool);
        break;
      case 'I':
        g_env.install.enabled = true;
        break;
      case 'C':
        cdir = pool_strdup(g_env.pool, optarg);
        break;
      case -1:
        g_env.install.bin_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -2:
        g_env.install.lib_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -3:
        g_env.install.include_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -4:
        g_env.install.pkgconf_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -5:
        g_env.install.man_dir = pool_strdup(g_env.pool, optarg);
        break;
      case -6:
        g_env.install.data_dir = pool_strdup(g_env.pool, optarg);
        break;
      case 'J': {
        int rc = 0;
        g_env.max_parallel_jobs = utils_strtol(optarg, 10, &rc);
        if (rc) {
          akfatal(AK_ERROR_FAIL, "Command line option -J,--jobs value: %s should be non negative number", optarg);
        }
        break;
      }
      case 'h':
      default:
        _usage(0);
    }
  }
  if (version) {
    printf(META_VERSION "." META_REVISION);
    if (g_env.verbose) {
      puts("\nRevision: " META_REVISION);
    }
    fflush(stdout);
    _exit(0);
  }
  if (!g_env.verbose) {
    const char *v = getenv(AUTARK_VERBOSE);
    if (v && *v == '1') {
      g_env.verbose = true;
    }
  }
  char buf[PATH_MAX];
  const char *autark_home = getenv("AUTARK_HOME");
  if (!autark_home) {
    utils_strncpy(buf, argv[0], sizeof(buf));
    autark_home = path_dirname(buf);
  }
  autark_home = path_normalize_pool(autark_home, g_env.pool);
  if (g_env.verbose) {
    akinfo("AUTARK_HOME=%s", autark_home);
  }
  g_env.spawn.extra_env_paths = autark_home;
  if (!g_env.install.prefix_dir) {
    char *home = getenv("HOME");
    if (home) {
      g_env.install.prefix_dir = pool_printf(g_env.pool, "%s/.local", home);
    } else {
      g_env.install.prefix_dir = "/usr/local";
    }
  }
  if (!g_env.install.bin_dir) {
    g_env.install.bin_dir = "bin";
  }
  if (!g_env.install.lib_dir) {
    g_env.install.lib_dir = env_libdir();
  }
  if (!g_env.install.data_dir) {
    g_env.install.data_dir = "share";
  }
  if (!g_env.install.include_dir) {
    g_env.install.include_dir = "include";
  }
  if (!g_env.install.man_dir) {
    g_env.install.man_dir = "share/man";
  }
  if (!g_env.install.pkgconf_dir) {
#ifdef __FreeBSD__
    g_env.install.pkgconf_dir = "libdata/pkgconfig";
#else
    g_env.install.pkgconf_dir = "lib/pkgconfig";
#endif
  }
  if (g_env.max_parallel_jobs <= 0) {
    g_env.max_parallel_jobs = 4;
  }
  if (g_env.max_parallel_jobs > 16) {
    g_env.max_parallel_jobs = 16;
  }
  if (optind < argc) {
    const char *arg = argv[optind];
    ++optind;
    if (strcmp(arg, "set") == 0) {
      _on_command_set(argc, argv);
      return;
    } else if (strcmp(arg, "dep") == 0) {
      _on_command_dep(argc, argv);
      return;
    } else if (strcmp(arg, "env") == 0) {
      _on_command_dep_env(argc, argv);
      return;
    } else if (strcmp(arg, "glob") == 0) {
      _on_command_glob(argc, argv, cdir);
      return;
    } else { // Root dir expected
      g_env.project.root_dir = pool_strdup(g_env.pool, arg);
    }
  }
  autark_build_prepare(AUTARK_SCRIPT);
  if (g_env.project.options) {
    _options();
  } else {
    _build(&options);
  }
  ulist_destroy_keep(&options);
}
#ifndef _AMALGAMATE_
#include "autark.h"
#endif
int main(int argc, const char **argv) {
  autark_init();
  autark_run(argc, argv);
  autark_dispose();
  return 0;
}
#ifndef _AMALGAMATE_
#include "env.h"
#include "script.h"
#include "utils.h"
#include "log.h"
#include "pool.h"
#include "xstr.h"
#include "alloc.h"
#include "nodes.h"
#include "autark.h"
#include "paths.h"
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#endif
#define XNODE(n__)              ((struct xnode*) (n__))
#define NODE_AT(list__, idx__)  *(struct node**) ulist_get(list__, idx__)
#define NODE_PEEK(list__)       ((list__)->num ? *(struct node**) ulist_peek(list__) : 0)
#define XNODE_AT(list__, idx__) XNODE(NODE_AT(list__, idx__))
#define XNODE_PEEK(list__)      ((list__)->num ? XNODE(*(struct node**) ulist_peek(list__)) : 0)
#define XCTX(n__)               (n__)->base.ctx
#define NODE(x__)               (struct node*) (x__)
struct _yycontext;
struct xparse {
  struct _yycontext *yy;
  struct ulist       stack;   // ulist<struct xnode*>
  struct value       val;
  struct xstr       *xerr;
  int pos;
};
struct xnode {
  struct node    base;
  struct xparse *xp;        // Used only by root script node
  unsigned       bld_calls; // Build calls counter in order to detect cyclic build deps
  unsigned       lnum;
};
//#define YY_DEBUG
#define YY_CTX_LOCAL 1
#define YY_CTX_MEMBERS \
        struct xnode *x;
#define YYSTYPE struct xnode*
#define YY_MALLOC(yy_, sz_)        _x_malloc(yy_, sz_)
#define YY_REALLOC(yy_, ptr_, sz_) _x_realloc(yy_, ptr_, sz_)
#define YY_INPUT(yy_, buf_, result_, max_size_) \
        result_ = _input(yy_, buf_, max_size_);
static void* _x_malloc(struct _yycontext *yy, size_t size) {
  return xmalloc(size);
}
static void* _x_realloc(struct _yycontext *yy, void *ptr, size_t size) {
  return xrealloc(ptr, size);
}
static int _input(struct _yycontext *yy, char *buf, int max_size);
static struct xnode* _push_and_register(struct _yycontext *yy, struct xnode *x);
static struct xnode* _node_text(struct  _yycontext *yy, const char *text);
static struct xnode* _node_text_push(struct  _yycontext *yy, const char *text);
static struct xnode* _node_text_escaped_push(struct  _yycontext *yy, const char *text);
static struct xnode* _rule(struct _yycontext *yy, struct xnode *x);
static void _finish(struct _yycontext *yy);
#ifndef _AMALGAMATE_
#include "scriptx.h"
#endif
/* A recursive-descent parser generated by peg 0.1.18 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define YYRULECOUNT 14
#ifndef YY_MALLOC
#define YY_MALLOC(C, N)		malloc(N)
#endif
#ifndef YY_REALLOC
#define YY_REALLOC(C, P, N)	realloc(P, N)
#endif
#ifndef YY_FREE
#define YY_FREE(C, P)		free(P)
#endif
#ifndef YY_LOCAL
#define YY_LOCAL(T)	static T
#endif
#ifndef YY_ACTION
#define YY_ACTION(T)	static T
#endif
#ifndef YY_RULE
#define YY_RULE(T)	static T
#endif
#ifndef YY_PARSE
#define YY_PARSE(T)	T
#endif
#ifndef YYPARSE
#define YYPARSE		yyparse
#endif
#ifndef YYPARSEFROM
#define YYPARSEFROM	yyparsefrom
#endif
#ifndef YYRELEASE
#define YYRELEASE	yyrelease
#endif
#ifndef YY_BEGIN
#define YY_BEGIN	( yy->__begin= yy->__pos, 1)
#endif
#ifndef YY_END
#define YY_END		( yy->__end= yy->__pos, 1)
#endif
#ifdef YY_DEBUG
# define yyprintf(args)	fprintf args
#else
# define yyprintf(args)
#endif
#ifndef YYSTYPE
#define YYSTYPE	int
#endif
#ifndef YY_STACK_SIZE
#define YY_STACK_SIZE 128
#endif
#ifndef YY_BUFFER_SIZE
#define YY_BUFFER_SIZE 1024
#endif
#ifndef YY_PART
typedef struct _yycontext yycontext;
typedef void (*yyaction)(yycontext *yy, char *yytext, int yyleng);
typedef struct _yythunk { int begin, end;  yyaction  action;  struct _yythunk *next; } yythunk;
struct _yycontext {
  char     *__buf;
  int       __buflen;
  int       __pos;
  int       __limit;
  char     *__text;
  int       __textlen;
  int       __begin;
  int       __end;
  int       __textmax;
  yythunk  *__thunks;
  int       __thunkslen;
  int       __thunkpos;
  YYSTYPE   __;
  YYSTYPE  *__val;
  YYSTYPE  *__vals;
  int       __valslen;
#ifdef YY_CTX_MEMBERS
  YY_CTX_MEMBERS
#endif
};
#ifdef YY_CTX_LOCAL
#define YY_CTX_PARAM_	yycontext *yyctx,
#define YY_CTX_PARAM	yycontext *yyctx
#define YY_CTX_ARG_	yyctx,
#define YY_CTX_ARG	yyctx
#ifndef YY_INPUT
#define YY_INPUT(yy, buf, result, max_size)		\
  {							\
    int yyc= getchar();					\
    result= (EOF == yyc) ? 0 : (*(buf)= yyc, 1);	\
    yyprintf((stderr, "<%c>", yyc));			\
  }
#endif
#else
#define YY_CTX_PARAM_
#define YY_CTX_PARAM
#define YY_CTX_ARG_
#define YY_CTX_ARG
yycontext _yyctx= { 0, 0 };
yycontext *yyctx= &_yyctx;
#ifndef YY_INPUT
#define YY_INPUT(buf, result, max_size)			\
  {							\
    int yyc= getchar();					\
    result= (EOF == yyc) ? 0 : (*(buf)= yyc, 1);	\
    yyprintf((stderr, "<%c>", yyc));			\
  }
#endif
#endif
YY_LOCAL(int) yyrefill(yycontext *yy)
{
  int yyn;
  while (yy->__buflen - yy->__pos < 512)
    {
      yy->__buflen *= 2;
      yy->__buf= (char *)YY_REALLOC(yy, yy->__buf, yy->__buflen);
    }
#ifdef YY_CTX_LOCAL
  YY_INPUT(yy, (yy->__buf + yy->__pos), yyn, (yy->__buflen - yy->__pos));
#else
  YY_INPUT((yy->__buf + yy->__pos), yyn, (yy->__buflen - yy->__pos));
#endif
  if (!yyn) return 0;
  yy->__limit += yyn;
  return 1;
}
YY_LOCAL(int) yymatchDot(yycontext *yy)
{
  if (yy->__pos >= yy->__limit && !yyrefill(yy)) return 0;
  ++yy->__pos;
  return 1;
}
YY_LOCAL(int) yymatchChar(yycontext *yy, int c)
{
  if (yy->__pos >= yy->__limit && !yyrefill(yy)) return 0;
  if ((unsigned char)yy->__buf[yy->__pos] == c)
    {
      ++yy->__pos;
      yyprintf((stderr, "  ok   yymatchChar(yy, %c) @ %s\n", c, yy->__buf+yy->__pos));
      return 1;
    }
  yyprintf((stderr, "  fail yymatchChar(yy, %c) @ %s\n", c, yy->__buf+yy->__pos));
  return 0;
}
YY_LOCAL(int) yymatchString(yycontext *yy, const char *s)
{
  int yysav= yy->__pos;
  while (*s)
    {
      if (yy->__pos >= yy->__limit && !yyrefill(yy)) return 0;
      if (yy->__buf[yy->__pos] != *s)
        {
          yy->__pos= yysav;
          return 0;
        }
      ++s;
      ++yy->__pos;
    }
  return 1;
}
YY_LOCAL(int) yymatchClass(yycontext *yy, unsigned char *bits)
{
  int c;
  if (yy->__pos >= yy->__limit && !yyrefill(yy)) return 0;
  c= (unsigned char)yy->__buf[yy->__pos];
  if (bits[c >> 3] & (1 << (c & 7)))
    {
      ++yy->__pos;
      yyprintf((stderr, "  ok   yymatchClass @ %s\n", yy->__buf+yy->__pos));
      return 1;
    }
  yyprintf((stderr, "  fail yymatchClass @ %s\n", yy->__buf+yy->__pos));
  return 0;
}
YY_LOCAL(void) yyDo(yycontext *yy, yyaction action, int begin, int end)
{
  while (yy->__thunkpos >= yy->__thunkslen)
    {
      yy->__thunkslen *= 2;
      yy->__thunks= (yythunk *)YY_REALLOC(yy, yy->__thunks, sizeof(yythunk) * yy->__thunkslen);
    }
  yy->__thunks[yy->__thunkpos].begin=  begin;
  yy->__thunks[yy->__thunkpos].end=    end;
  yy->__thunks[yy->__thunkpos].action= action;
  ++yy->__thunkpos;
}
YY_LOCAL(int) yyText(yycontext *yy, int begin, int end)
{
  int yyleng= end - begin;
  if (yyleng <= 0)
    yyleng= 0;
  else
    {
      while (yy->__textlen < (yyleng + 1))
	{
	  yy->__textlen *= 2;
	  yy->__text= (char *)YY_REALLOC(yy, yy->__text, yy->__textlen);
	}
      memcpy(yy->__text, yy->__buf + begin, yyleng);
    }
  yy->__text[yyleng]= '\0';
  return yyleng;
}
YY_LOCAL(void) yyDone(yycontext *yy)
{
  int pos;
  for (pos= 0;  pos < yy->__thunkpos;  ++pos)
    {
      yythunk *thunk= &yy->__thunks[pos];
      int yyleng= thunk->end ? yyText(yy, thunk->begin, thunk->end) : thunk->begin;
      yyprintf((stderr, "DO [%d] %p %s\n", pos, thunk->action, yy->__text));
      thunk->action(yy, yy->__text, yyleng);
    }
  yy->__thunkpos= 0;
}
YY_LOCAL(void) yyCommit(yycontext *yy)
{
  if ((yy->__limit -= yy->__pos))
    {
      memmove(yy->__buf, yy->__buf + yy->__pos, yy->__limit);
    }
  yy->__begin -= yy->__pos;
  yy->__end -= yy->__pos;
  yy->__pos= yy->__thunkpos= 0;
}
YY_LOCAL(int) yyAccept(yycontext *yy, int tp0)
{
  if (tp0)
    {
      fprintf(stderr, "accept denied at %d\n", tp0);
      return 0;
    }
  else
    {
      yyDone(yy);
      yyCommit(yy);
    }
  return 1;
}
YY_LOCAL(void) yyPush(yycontext *yy, char *text, int count)
{
  yy->__val += count;
  while (yy->__valslen <= yy->__val - yy->__vals)
    {
      long offset= yy->__val - yy->__vals;
      yy->__valslen *= 2;
      yy->__vals= (YYSTYPE *)YY_REALLOC(yy, yy->__vals, sizeof(YYSTYPE) * yy->__valslen);
      yy->__val= yy->__vals + offset;
    }
}
YY_LOCAL(void) yyPop(yycontext *yy, char *text, int count)   { yy->__val -= count; }
YY_LOCAL(void) yySet(yycontext *yy, char *text, int count)   { yy->__val[count]= yy->__; }
#endif /* YY_PART */
#define	YYACCEPT	yyAccept(yy, yythunkpos0)
YY_RULE(int) yy_EOL(yycontext *yy); /* 14 */
YY_RULE(int) yy_SPACE(yycontext *yy); /* 13 */
YY_RULE(int) yy_CHP2(yycontext *yy); /* 12 */
YY_RULE(int) yy_CHP1(yycontext *yy); /* 11 */
YY_RULE(int) yy_CHP(yycontext *yy); /* 10 */
YY_RULE(int) yy_STRQQ(yycontext *yy); /* 9 */
YY_RULE(int) yy_STRQ(yycontext *yy); /* 8 */
YY_RULE(int) yy___(yycontext *yy); /* 7 */
YY_RULE(int) yy_VALR(yycontext *yy); /* 6 */
YY_RULE(int) yy_STRP(yycontext *yy); /* 5 */
YY_RULE(int) yy_EOF(yycontext *yy); /* 4 */
YY_RULE(int) yy_RULE(yycontext *yy); /* 3 */
YY_RULE(int) yy__(yycontext *yy); /* 2 */
YY_RULE(int) yy_RULES(yycontext *yy); /* 1 */
YY_ACTION(void) yy_1_EOL(yycontext *yy, char *yytext, int yyleng)
{
#define __ yy->__
#define yypos yy->__pos
#define yythunkpos yy->__thunkpos
  yyprintf((stderr, "do yy_1_EOL\n"));
  {
   ++yy->x->lnum; ;
  }
#undef yythunkpos
#undef yypos
#undef yy
}
YY_ACTION(void) yy_1_STRQQ(yycontext *yy, char *yytext, int yyleng)
{
#define __ yy->__
#define yypos yy->__pos
#define yythunkpos yy->__thunkpos
  yyprintf((stderr, "do yy_1_STRQQ\n"));
  {
   __ = _node_text_push(yy, yytext); ;
  }
#undef yythunkpos
#undef yypos
#undef yy
}
YY_ACTION(void) yy_1_STRQ(yycontext *yy, char *yytext, int yyleng)
{
#define __ yy->__
#define yypos yy->__pos
#define yythunkpos yy->__thunkpos
  yyprintf((stderr, "do yy_1_STRQ\n"));
  {
   __ = _node_text_push(yy, yytext); ;
  }
#undef yythunkpos
#undef yypos
#undef yy
}
YY_ACTION(void) yy_1_STRP(yycontext *yy, char *yytext, int yyleng)
{
#define __ yy->__
#define yypos yy->__pos
#define yythunkpos yy->__thunkpos
  yyprintf((stderr, "do yy_1_STRP\n"));
  {
   __ = _node_text_escaped_push(yy, yytext); ;
  }
#undef yythunkpos
#undef yypos
#undef yy
}
YY_ACTION(void) yy_1_RULE(yycontext *yy, char *yytext, int yyleng)
{
#define k yy->__val[-1]
#define __ yy->__
#define yypos yy->__pos
#define yythunkpos yy->__thunkpos
  yyprintf((stderr, "do yy_1_RULE\n"));
  {
   _rule(yy, k); ;
  }
#undef yythunkpos
#undef yypos
#undef yy
#undef k
}
YY_ACTION(void) yy_1_RULES(yycontext *yy, char *yytext, int yyleng)
{
#define __ yy->__
#define yypos yy->__pos
#define yythunkpos yy->__thunkpos
  yyprintf((stderr, "do yy_1_RULES\n"));
  {
   _finish(yy); ;
  }
#undef yythunkpos
#undef yypos
#undef yy
}
YY_RULE(int) yy_EOL(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "EOL"));
  {  int yypos2= yy->__pos, yythunkpos2= yy->__thunkpos;  if (!yymatchString(yy, "\r\n")) goto l3;  goto l2;
  l3:;	  yy->__pos= yypos2; yy->__thunkpos= yythunkpos2;  if (!yymatchChar(yy, '\n')) goto l4;  goto l2;
  l4:;	  yy->__pos= yypos2; yy->__thunkpos= yythunkpos2;  if (!yymatchChar(yy, '\r')) goto l1;
  }
  l2:;	  yyDo(yy, yy_1_EOL, yy->__begin, yy->__end);
  yyprintf((stderr, "  ok   %s @ %s\n", "EOL", yy->__buf+yy->__pos));
  return 1;
  l1:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "EOL", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_SPACE(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "SPACE"));
  {  int yypos6= yy->__pos, yythunkpos6= yy->__thunkpos;  if (!yymatchChar(yy, ' ')) goto l7;  goto l6;
  l7:;	  yy->__pos= yypos6; yy->__thunkpos= yythunkpos6;  if (!yymatchChar(yy, '\t')) goto l8;  goto l6;
  l8:;	  yy->__pos= yypos6; yy->__thunkpos= yythunkpos6;  if (!yy_EOL(yy)) goto l5;
  }
  l6:;	
  yyprintf((stderr, "  ok   %s @ %s\n", "SPACE", yy->__buf+yy->__pos));
  return 1;
  l5:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "SPACE", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_CHP2(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "CHP2"));
  {  int yypos10= yy->__pos, yythunkpos10= yy->__thunkpos;
  {  int yypos11= yy->__pos, yythunkpos11= yy->__thunkpos;  if (!yymatchClass(yy, (unsigned char *)"\000\000\000\000\000\000\000\000\000\000\000\020\000\000\000\050\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000")) goto l12;  goto l11;
  l12:;	  yy->__pos= yypos11; yy->__thunkpos= yythunkpos11;  if (!yy_SPACE(yy)) goto l10;
  }
  l11:;	  goto l9;
  l10:;	  yy->__pos= yypos10; yy->__thunkpos= yythunkpos10;
  }  if (!yymatchDot(yy)) goto l9;
  yyprintf((stderr, "  ok   %s @ %s\n", "CHP2", yy->__buf+yy->__pos));
  return 1;
  l9:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "CHP2", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_CHP1(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "CHP1"));  if (!yymatchChar(yy, '\\')) goto l13;  if (!yymatchClass(yy, (unsigned char *)"\000\000\000\000\000\000\000\000\000\000\000\020\000\100\024\050\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000")) goto l13;
  yyprintf((stderr, "  ok   %s @ %s\n", "CHP1", yy->__buf+yy->__pos));
  return 1;
  l13:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "CHP1", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_CHP(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "CHP"));
  {  int yypos15= yy->__pos, yythunkpos15= yy->__thunkpos;  if (!yy_CHP1(yy)) goto l16;  goto l15;
  l16:;	  yy->__pos= yypos15; yy->__thunkpos= yythunkpos15;  if (!yy_CHP2(yy)) goto l14;
  }
  l15:;	
  yyprintf((stderr, "  ok   %s @ %s\n", "CHP", yy->__buf+yy->__pos));
  return 1;
  l14:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "CHP", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_STRQQ(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "STRQQ"));  if (!yymatchClass(yy, (unsigned char *)"\000\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000")) goto l17;  yyText(yy, yy->__begin, yy->__end);  {
#define yytext yy->__text
#define yyleng yy->__textlen
if (!(YY_BEGIN)) goto l17;
#undef yytext
#undef yyleng
  }
  l18:;	
  {  int yypos19= yy->__pos, yythunkpos19= yy->__thunkpos;
  {  int yypos20= yy->__pos, yythunkpos20= yy->__thunkpos;  if (!yymatchClass(yy, (unsigned char *)"\000\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000")) goto l20;  goto l19;
  l20:;	  yy->__pos= yypos20; yy->__thunkpos= yythunkpos20;
  }  if (!yymatchDot(yy)) goto l19;  goto l18;
  l19:;	  yy->__pos= yypos19; yy->__thunkpos= yythunkpos19;
  }  yyText(yy, yy->__begin, yy->__end);  {
#define yytext yy->__text
#define yyleng yy->__textlen
if (!(YY_END)) goto l17;
#undef yytext
#undef yyleng
  }  if (!yymatchClass(yy, (unsigned char *)"\000\000\000\000\004\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000")) goto l17;  yyDo(yy, yy_1_STRQQ, yy->__begin, yy->__end);
  yyprintf((stderr, "  ok   %s @ %s\n", "STRQQ", yy->__buf+yy->__pos));
  return 1;
  l17:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "STRQQ", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_STRQ(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "STRQ"));  if (!yymatchClass(yy, (unsigned char *)"\000\000\000\000\200\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000")) goto l21;  yyText(yy, yy->__begin, yy->__end);  {
#define yytext yy->__text
#define yyleng yy->__textlen
if (!(YY_BEGIN)) goto l21;
#undef yytext
#undef yyleng
  }
  l22:;	
  {  int yypos23= yy->__pos, yythunkpos23= yy->__thunkpos;
  {  int yypos24= yy->__pos, yythunkpos24= yy->__thunkpos;  if (!yymatchClass(yy, (unsigned char *)"\000\000\000\000\200\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000")) goto l24;  goto l23;
  l24:;	  yy->__pos= yypos24; yy->__thunkpos= yythunkpos24;
  }  if (!yymatchDot(yy)) goto l23;  goto l22;
  l23:;	  yy->__pos= yypos23; yy->__thunkpos= yythunkpos23;
  }  yyText(yy, yy->__begin, yy->__end);  {
#define yytext yy->__text
#define yyleng yy->__textlen
if (!(YY_END)) goto l21;
#undef yytext
#undef yyleng
  }  if (!yymatchClass(yy, (unsigned char *)"\000\000\000\000\200\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000")) goto l21;  yyDo(yy, yy_1_STRQ, yy->__begin, yy->__end);
  yyprintf((stderr, "  ok   %s @ %s\n", "STRQ", yy->__buf+yy->__pos));
  return 1;
  l21:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "STRQ", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy___(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "__"));  if (!yy_SPACE(yy)) goto l25;
  l26:;	
  {  int yypos27= yy->__pos, yythunkpos27= yy->__thunkpos;  if (!yy_SPACE(yy)) goto l27;  goto l26;
  l27:;	  yy->__pos= yypos27; yy->__thunkpos= yythunkpos27;
  }
  yyprintf((stderr, "  ok   %s @ %s\n", "__", yy->__buf+yy->__pos));
  return 1;
  l25:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "__", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_VALR(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "VALR"));
  {  int yypos29= yy->__pos, yythunkpos29= yy->__thunkpos;  if (!yy_STRQ(yy)) goto l30;  goto l29;
  l30:;	  yy->__pos= yypos29; yy->__thunkpos= yythunkpos29;  if (!yy_STRQQ(yy)) goto l31;  goto l29;
  l31:;	  yy->__pos= yypos29; yy->__thunkpos= yythunkpos29;  if (!yy_RULE(yy)) goto l32;  goto l29;
  l32:;	  yy->__pos= yypos29; yy->__thunkpos= yythunkpos29;  if (!yy_STRP(yy)) goto l28;
  }
  l29:;	
  yyprintf((stderr, "  ok   %s @ %s\n", "VALR", yy->__buf+yy->__pos));
  return 1;
  l28:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "VALR", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_STRP(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "STRP"));  yyText(yy, yy->__begin, yy->__end);  {
#define yytext yy->__text
#define yyleng yy->__textlen
if (!(YY_BEGIN)) goto l33;
#undef yytext
#undef yyleng
  }  if (!yy_CHP(yy)) goto l33;
  l34:;	
  {  int yypos35= yy->__pos, yythunkpos35= yy->__thunkpos;  if (!yy_CHP(yy)) goto l35;  goto l34;
  l35:;	  yy->__pos= yypos35; yy->__thunkpos= yythunkpos35;
  }  yyText(yy, yy->__begin, yy->__end);  {
#define yytext yy->__text
#define yyleng yy->__textlen
if (!(YY_END)) goto l33;
#undef yytext
#undef yyleng
  }  yyDo(yy, yy_1_STRP, yy->__begin, yy->__end);
  yyprintf((stderr, "  ok   %s @ %s\n", "STRP", yy->__buf+yy->__pos));
  return 1;
  l33:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "STRP", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_EOF(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "EOF"));
  {  int yypos37= yy->__pos, yythunkpos37= yy->__thunkpos;  if (!yymatchDot(yy)) goto l37;  goto l36;
  l37:;	  yy->__pos= yypos37; yy->__thunkpos= yythunkpos37;
  }
  yyprintf((stderr, "  ok   %s @ %s\n", "EOF", yy->__buf+yy->__pos));
  return 1;
  l36:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "EOF", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy_RULE(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;  yyDo(yy, yyPush, 1, 0);
  yyprintf((stderr, "%s\n", "RULE"));  if (!yy_STRP(yy)) goto l38;  yyDo(yy, yySet, -1, 0);  if (!yy__(yy)) goto l38;  if (!yymatchChar(yy, '{')) goto l38;  if (!yy__(yy)) goto l38;
  {  int yypos39= yy->__pos, yythunkpos39= yy->__thunkpos;  if (!yy_VALR(yy)) goto l39;
  l41:;	
  {  int yypos42= yy->__pos, yythunkpos42= yy->__thunkpos;  if (!yy___(yy)) goto l42;  if (!yy_VALR(yy)) goto l42;  goto l41;
  l42:;	  yy->__pos= yypos42; yy->__thunkpos= yythunkpos42;
  }  goto l40;
  l39:;	  yy->__pos= yypos39; yy->__thunkpos= yythunkpos39;
  }
  l40:;	  if (!yy__(yy)) goto l38;  if (!yymatchChar(yy, '}')) goto l38;  yyDo(yy, yy_1_RULE, yy->__begin, yy->__end);
  yyprintf((stderr, "  ok   %s @ %s\n", "RULE", yy->__buf+yy->__pos));  yyDo(yy, yyPop, 1, 0);
  return 1;
  l38:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "RULE", yy->__buf+yy->__pos));
  return 0;
}
YY_RULE(int) yy__(yycontext *yy)
{
  yyprintf((stderr, "%s\n", "_"));
  l44:;	
  {  int yypos45= yy->__pos, yythunkpos45= yy->__thunkpos;  if (!yy_SPACE(yy)) goto l45;  goto l44;
  l45:;	  yy->__pos= yypos45; yy->__thunkpos= yythunkpos45;
  }
  yyprintf((stderr, "  ok   %s @ %s\n", "_", yy->__buf+yy->__pos));
  return 1;
}
YY_RULE(int) yy_RULES(yycontext *yy)
{  int yypos0= yy->__pos, yythunkpos0= yy->__thunkpos;
  yyprintf((stderr, "%s\n", "RULES"));  if (!yy__(yy)) goto l46;
  l47:;	
  {  int yypos48= yy->__pos, yythunkpos48= yy->__thunkpos;  if (!yy_RULE(yy)) goto l48;  if (!yy__(yy)) goto l48;  goto l47;
  l48:;	  yy->__pos= yypos48; yy->__thunkpos= yythunkpos48;
  }  if (!yy__(yy)) goto l46;  if (!yy_EOF(yy)) goto l46;  yyDo(yy, yy_1_RULES, yy->__begin, yy->__end);
  yyprintf((stderr, "  ok   %s @ %s\n", "RULES", yy->__buf+yy->__pos));
  return 1;
  l46:;	  yy->__pos= yypos0; yy->__thunkpos= yythunkpos0;
  yyprintf((stderr, "  fail %s @ %s\n", "RULES", yy->__buf+yy->__pos));
  return 0;
}
#ifndef YY_PART
typedef int (*yyrule)(yycontext *yy);
YY_PARSE(int) YYPARSEFROM(YY_CTX_PARAM_ yyrule yystart)
{
  int yyok;
  if (!yyctx->__buflen)
    {
      yyctx->__buflen= YY_BUFFER_SIZE;
      yyctx->__buf= (char *)YY_MALLOC(yyctx, yyctx->__buflen);
      yyctx->__textlen= YY_BUFFER_SIZE;
      yyctx->__text= (char *)YY_MALLOC(yyctx, yyctx->__textlen);
      yyctx->__thunkslen= YY_STACK_SIZE;
      yyctx->__thunks= (yythunk *)YY_MALLOC(yyctx, sizeof(yythunk) * yyctx->__thunkslen);
      yyctx->__valslen= YY_STACK_SIZE;
      yyctx->__vals= (YYSTYPE *)YY_MALLOC(yyctx, sizeof(YYSTYPE) * yyctx->__valslen);
      yyctx->__begin= yyctx->__end= yyctx->__pos= yyctx->__limit= yyctx->__thunkpos= 0;
    }
  yyctx->__begin= yyctx->__end= yyctx->__pos;
  yyctx->__thunkpos= 0;
  yyctx->__val= yyctx->__vals;
  yyok= yystart(yyctx);
  if (yyok) yyDone(yyctx);
  yyCommit(yyctx);
  return yyok;
}
YY_PARSE(int) YYPARSE(YY_CTX_PARAM)
{
  return YYPARSEFROM(YY_CTX_ARG_ yy_RULES);
}
YY_PARSE(yycontext *) YYRELEASE(yycontext *yyctx)
{
  if (yyctx->__buflen)
    {
      yyctx->__buflen= 0;
      YY_FREE(yyctx, yyctx->__buf);
      YY_FREE(yyctx, yyctx->__text);
      YY_FREE(yyctx, yyctx->__thunks);
      YY_FREE(yyctx, yyctx->__vals);
    }
  return yyctx;
}
#endif
#ifndef _AMALGAMATE_
#include "script_impl.h"
#endif
static int _node_bind(struct node *n);
static void _yyerror(yycontext *yy) {
  struct xnode *x = yy->x;
  struct xstr *xerr = x->xp->xerr;
  if (yy->__pos && yy->__text[0]) {
    xstr_cat(xerr, "near token: '");
    xstr_cat(xerr, yy->__text);
    xstr_cat(xerr, "'\n");
  }
  if (yy->__pos < yy->__limit) {
    char buf[2] = { 0 };
    yy->__buf[yy->__limit] = '\0';
    xstr_cat(xerr, "\n");
    while (yy->__pos < yy->__limit) {
      buf[0] = yy->__buf[yy->__pos++];
      xstr_cat2(xerr, buf, 1);
    }
  }
  xstr_cat(xerr, " <--- \n");
}
static void _xparse_destroy(struct xparse *xp) {
  if (xp) {
    xstr_destroy(xp->xerr);
    ulist_destroy_keep(&xp->stack);
    if (xp->yy) {
      yyrelease(xp->yy);
      free(xp->yy);
      xp->yy = 0;
    }
    free(xp);
  }
}
static int _input(struct _yycontext *yy, char *buf, int max_size) {
  struct xparse *xp = yy->x->xp;
  int cnt = 0;
  while (cnt < max_size && xp->pos < xp->val.len) {
    char ch = *((char*) xp->val.buf + xp->pos);
    *(buf + cnt) = ch;
    ++xp->pos;
    ++cnt;
  }
  return cnt;
}
static void _xnode_destroy(struct xnode *x) {
  struct node *n = &x->base;
  if (n->unit && n->unit->n == n) {
    n->unit->n = 0;
  }
  if (x->base.dispose) {
    x->base.dispose(n);
  }
  _xparse_destroy(x->xp);
  x->xp = 0;
}
static unsigned _rule_type(const char *key, unsigned *flags) {
  *flags = 0;
  if (key[0] == '.' && key[1] == '.') {
    key += 2;
  }
  if (key[0] == '!') {
    *flags = NODE_FLG_NEGATE;
    key += 1;
  }
  if (strcmp(key, "$") == 0 || strcmp(key, "@") == 0) {
    return NODE_TYPE_SUBST;
  } else if (strcmp(key, "^") == 0) {
    return NODE_TYPE_JOIN;
  } else if (strcmp(key, "set") == 0 || strcmp(key, "env") == 0) {
    return NODE_TYPE_SET;
  } else if (strcmp(key, "check") == 0) {
    return NODE_TYPE_CHECK;
  } else if (strcmp(key, "include") == 0) {
    return NODE_TYPE_INCLUDE;
  } else if (strcmp(key, "if") == 0) {
    return NODE_TYPE_IF;
  } else if (strcmp(key, "run") == 0 || strcmp(key, "run-on-install") == 0) {
    return NODE_TYPE_RUN;
  } else if (strcmp(key, "meta") == 0) {
    return NODE_TYPE_META;
  } else if (strcmp(key, "cc") == 0 || strcmp(key, "cxx") == 0) {
    return NODE_TYPE_CC;
  } else if (strcmp(key, "configure") == 0) {
    return NODE_TYPE_CONFIGURE;
  } else if (strcmp(key, "%") == 0) {
    return NODE_TYPE_BASENAME;
  } else if (strcmp(key, "foreach") == 0) {
    return NODE_TYPE_FOREACH;
  } else if (strcmp(key, "in-sources") == 0) {
    return NODE_TYPE_IN_SOURCES;
  } else if (  strcmp(key, "S") == 0 || strcmp(key, "C") == 0
            || strcmp(key, "SS") == 0 || strcmp(key, "CC") == 0) {
    return NODE_TYPE_DIR;
  } else if (strcmp(key, "option") == 0) {
    return NODE_TYPE_OPTION;
  } else if (strcmp(key, "error") == 0) {
    return NODE_TYPE_ERROR;
  } else if (strcmp(key, "echo") == 0) {
    return NODE_TYPE_ECHO;
  } else if (strcmp(key, "install") == 0) {
    return NODE_TYPE_INSTALL;
  } else if (strcmp(key, "library") == 0) {
    return NODE_TYPE_FIND;
  } else {
    return NODE_TYPE_BAG;
  }
}
static const char* _node_file(struct node *n) {
  for ( ; n; n = n->parent) {
    if (n->type == NODE_TYPE_SCRIPT) {
      return n->value;
    }
  }
  return "script";
}
void node_info(struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akinfo("%s: %s", n->name, xstr_ptr(xstr));
  } else {
    akinfo(n->name, 0);
  }
  xstr_destroy(xstr);
}
void node_warn(struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akwarn("%s: %s", n->name, xstr_ptr(xstr));
  } else {
    akinfo(n->name, 0);
  }
  xstr_destroy(xstr);
}
void node_fatal(int rc, struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akfatal(rc, "%s %s", n->name, xstr_ptr(xstr));
  } else {
    akfatal(rc, n->name, 0);
  }
  xstr_destroy(xstr);
}
int node_error(int rc, struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akerror(rc, "%s %s:0x%x %s", _node_file(n), n->value, n->type, xstr_ptr(xstr));
  } else {
    akerror(rc, "%s %s:0x%x", _node_file(n), n->value, n->type);
  }
  xstr_destroy(xstr);
  return rc;
}
static void _node_register(struct sctx *p, struct xnode *x) {
  x->base.index = p->nodes.num;
  ulist_push(&p->nodes, &x);
}
static struct xnode* _push_and_register(struct _yycontext *yy, struct xnode *x) {
  struct ulist *s = &yy->x->xp->stack;
  ulist_push(s, &x);
  _node_register(yy->x->base.ctx, x);
  return x;
}
static struct xnode* _node_text(struct  _yycontext *yy, const char *text) {
  struct sctx *ctx = XCTX(yy->x);
  struct xnode *x = pool_calloc(g_env.pool, sizeof(*x));
  x->base.value = pool_strdup(g_env.pool, text);
  x->base.ctx = ctx;
  x->base.type = NODE_TYPE_VALUE;
  x->base.lnum = yy->x->lnum + 1;
  return x;
}
static struct xnode* _node_text_push(struct  _yycontext *yy, const char *text) {
  return _push_and_register(yy, _node_text(yy, text));
}
static char* _text_escaped(char *wp, const char *rp) {
  // Replace: \} \{ \n \r \t
  int esc = 0;
  char *ret = wp;
  while (*rp != '\0') {
    if (esc) {
      switch (*rp) {
        case 'n':
          *wp = '\n';
          break;
        case 'r':
          *wp = '\r';
          break;
        case 't':
          *wp = '\t';
          break;
        default:
          *wp = *rp;
          break;
      }
      esc = 0;
    } else if (*rp == '\\') {
      esc = 1;
      ++rp;
      continue;
    } else {
      *wp = *rp;
    }
    ++wp;
    ++rp;
  }
  *wp = '\0';
  return ret;
}
static struct xnode* _node_text_escaped_push(struct  _yycontext *yy, const char *text) {
  char buf[strlen(text) + 1];
  return _push_and_register(yy, _node_text(yy, _text_escaped(buf, text)));
}
static struct xnode* _rule(struct _yycontext *yy, struct xnode *key) {
  struct xparse *xp = yy->x->xp;
  struct ulist *s = &xp->stack;
  unsigned flags = 0;
  key->base.type = _rule_type(key->base.value, &flags);
  key->base.flags |= flags;
  while (s->num) {
    struct xnode *x = XNODE_PEEK(s);
    if (x != key) {
      x->base.next = key->base.child;
      x->base.parent = &key->base;
      key->base.child = &x->base;
      ulist_pop(s);
    } else {
      // Keep rule on the stack
      break;
    }
  }
  return key;
}
static void _finish(struct _yycontext *yy) {
  struct xnode *root = yy->x;
  struct xparse *xp = root->xp;
  struct ulist *s = &xp->stack;
  while (s->num) {
    struct xnode *x = XNODE_PEEK(s);
    x->base.next = root->base.child;
    x->base.parent = &root->base;
    root->base.child = &x->base;
    ulist_pop(s);
  }
}
static int _node_visit(struct node *n, int lvl, void *ctx, int (*visitor)(struct node*, int, void*)) {
  int ret = visitor(n, lvl, ctx);
  if (ret) {
    return ret;
  }
  for (struct node *c = n->child; c; c = c->next) {
    ret = _node_visit(c, lvl + 1, ctx, visitor);
    if (ret) {
      return ret;
    }
  }
  return visitor(n, -lvl, ctx);
}
static void _preprocess_script(struct value *v) {
  const char *p = v->buf;
  char *nv = xmalloc(v->len + 1), *w = nv;
  bool comment = false, eol = true;
  while (*p) {
    if (*p == '\n') {
      eol = true;
      comment = false;
    } else if (eol) {
      const char *sp = p;
      while (utils_char_is_space(*sp)) {
        ++sp;
      }
      if (*sp == '#') {
        comment = true;
      }
      eol = false;
    }
    if (!comment) {
      *w++ = *p++;
    } else {
      p++;
    }
  }
  *w = '\0';
  free(v->buf);
  v->buf = nv;
  v->len = w - nv;
}
static int _script_from_value(
  struct node  *parent,
  const char   *file,
  struct value *val,
  struct node **out) {
  int rc = 0;
  struct xnode *x = 0;
  struct pool *pool = g_env.pool;
  char prevcwd_[PATH_MAX];
  char *prevcwd = 0;
  _preprocess_script(val);
  if (!parent) {
    struct sctx *ctx = pool_calloc(pool, sizeof(*ctx));
    ulist_init(&ctx->nodes, 64, sizeof(struct node*));
    ctx->products = map_create_str(map_k_free);
    x = pool_calloc(pool, sizeof(*x));
    x->base.ctx = ctx;
    ctx->root = (struct node*) x;
  } else {
    x = pool_calloc(pool, sizeof(*x));
    x->base.parent = parent;
    x->base.ctx = parent->ctx;
  }
  if (file) {
    struct unit *unit;
    char *f = path_relativize(g_env.project.root_dir, file);
    if (parent == 0 && g_env.units.num == 1) {
      unit = unit_peek();
      akassert(unit->n == 0); // We are the root script
    } else {
      unit = unit_create(f, UNIT_FLG_SRC_CWD, g_env.pool);
    }
    unit->n = &x->base;
    x->base.unit = unit;
    unit_ch_dir(&(struct unit_ctx) { unit, 0 }, prevcwd_);
    prevcwd = prevcwd_;
    x->base.value = pool_strdup(pool, f);
    free(f);
  } else {
    x->base.value = "<script>";
  }
  x->base.type = NODE_TYPE_SCRIPT;
  _node_register(x->base.ctx, x);
  _node_bind(&x->base);
  x->xp = xmalloc(sizeof(*x->xp));
  *x->xp = (struct xparse) {
    .stack = { .usize = sizeof(struct xnode*) },
    .xerr = xstr_create_empty(),
    .val = *val,
  };
  yycontext *yy = x->xp->yy = xmalloc(sizeof(yycontext));
  *yy = (yycontext) {
    .x = x
  };
  if (!yyparse(yy)) {
    rc = AK_ERROR_SCRIPT_SYNTAX;
    _yyerror(yy);
    if (x->xp->xerr->size) {
      akerror(rc, "%s\n", x->xp->xerr->ptr);
    } else {
      akerror(rc, 0, 0);
    }
    goto finish;
  }
finish:
  _xparse_destroy(x->xp);
  x->xp = 0;
  if (rc) {
    _xnode_destroy(x);
  } else {
    *out = &x->base;
  }
  if (prevcwd) {
    akcheck(chdir(prevcwd));
  }
  return rc;
}
static int _script_from_file(struct node *parent, const char *file, struct node **out) {
  *out = 0;
  struct value buf = utils_file_as_buf(file, (1024 * 1024));
  if (buf.error) {
    return value_destroy(&buf);
  }
  int ret = _script_from_value(parent, file, &buf, out);
  value_destroy(&buf);
  return ret;
}
static void _script_destroy(struct sctx *s) {
  if (s) {
    for (int i = 0; i < s->nodes.num; ++i) {
      struct xnode *x = XNODE_AT(&s->nodes, i);
      _xnode_destroy(x);
    }
    map_destroy(s->products);
    ulist_destroy_keep(&s->nodes);
  }
}
static int _node_bind(struct node *n) {
  int rc = 0;
  // Tree has been built since its safe to compute node name
  if (n->type != NODE_TYPE_SCRIPT) {
    n->name = pool_printf(g_env.pool, "%s:%-3u %5s", _node_file(n), n->lnum, n->value);
  } else {
    n->name = pool_printf(g_env.pool, "%s     %5s", _node_file(n), "");
  }
  n->vfile = pool_printf(g_env.pool, ".%u", n->index);
  if (!(n->flags & NODE_FLG_BOUND)) {
    n->flags |= NODE_FLG_BOUND;
    switch (n->type) {
      case NODE_TYPE_SCRIPT:
        rc = node_script_setup(n);
        break;
      case NODE_TYPE_META:
        rc = node_meta_setup(n);
        break;
      case NODE_TYPE_CHECK:
        rc = node_check_setup(n);
        break;
      case NODE_TYPE_SET:
        rc = node_set_setup(n);
        break;
      case NODE_TYPE_INCLUDE:
        rc = node_include_setup(n);
        break;
      case NODE_TYPE_IF:
        rc = node_if_setup(n);
        break;
      case NODE_TYPE_SUBST:
        rc = node_subst_setup(n);
        break;
      case NODE_TYPE_RUN:
        rc = node_run_setup(n);
        break;
      case NODE_TYPE_JOIN:
        rc = node_join_setup(n);
        break;
      case NODE_TYPE_CC:
        rc = node_cc_setup(n);
        break;
      case NODE_TYPE_CONFIGURE:
        rc = node_configure_setup(n);
        break;
      case NODE_TYPE_BASENAME:
        rc = node_basename_setup(n);
        break;
      case NODE_TYPE_FOREACH:
        rc = node_foreach_setup(n);
        break;
      case NODE_TYPE_IN_SOURCES:
        rc = node_in_sources_setup(n);
        break;
      case NODE_TYPE_DIR:
        rc = node_dir_setup(n);
        break;
      case NODE_TYPE_OPTION:
        rc = node_option_setup(n);
        break;
      case NODE_TYPE_ERROR:
        rc = node_error_setup(n);
        break;
      case NODE_TYPE_ECHO:
        rc = node_echo_setup(n);
        break;
      case NODE_TYPE_INSTALL:
        rc = node_install_setup(n);
        break;
      case NODE_TYPE_FIND:
        rc = node_find_setup(n);
        break;
    }
    switch (n->type) {
      case NODE_TYPE_RUN:
      case NODE_TYPE_SUBST: {
        struct node *nn = node_find_parent_of_type(n, NODE_TYPE_IN_SOURCES);
        if (nn) {
          if (n->flags & NODE_FLG_IN_CACHE) {
            n->flags &= ~(NODE_FLG_IN_CACHE);
          }
          n->flags |= NODE_FLG_IN_SRC;
        }
      }
    }
  }
  return rc;
}
static struct node* _unit_node_find(struct node *n) {
  for ( ; n; n = n->parent) {
    if (n->unit) {
      return n;
    }
  }
  akassert(0);
}
static void _node_context_push(struct node *n) {
  struct node *s = _unit_node_find(n);
  unit_push(s->unit, n);
}
static void _node_context_pop(struct node *n) {
  unit_pop();
}
const char* node_value(struct node *n) {
  if (n) {
    node_setup(n);
    if (n->value_get) {
      _node_context_push(n);
      const char *ret = n->value_get(n);
      _node_context_pop(n);
      return ret;
    } else {
      return n->value;
    }
  } else {
    return 0;
  }
}
void node_reset(struct node *n) {
  n->flags &= ~(NODE_FLG_UPDATED | NODE_FLG_BUILT | NODE_FLG_SETUP | NODE_FLG_INIT);
}
static void _init_subnodes(struct node *n) {
  int c;
  do {
    c = 0;
    for (struct node *nn = n->child; nn; nn = nn->next) {
      if (!node_is_init(nn)) {
        ++c;
        node_init(nn);
      }
    }
  } while (c > 0);
}
static void _setup_subnodes(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_setup(nn);
  }
}
static void _build_subnodes(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_build(nn);
  }
}
static void _post_build_subnodes(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_post_build(nn);
  }
}
void node_init(struct node *n) {
  if (!node_is_init(n)) {
    n->flags |= NODE_FLG_INIT;
    switch (n->type) {
      case NODE_TYPE_IF:
      case NODE_TYPE_INCLUDE:
      case NODE_TYPE_FOREACH:
      case NODE_TYPE_IN_SOURCES:
        _node_context_push(n);
        n->init(n);
        _init_subnodes(n);
        _node_context_pop(n);
        break;
      default:
        if (n->init || n->child) {
          _node_context_push(n);
          _init_subnodes(n);
          if (n->init) {
            n->init(n);
          }
          _node_context_pop(n);
        }
    }
  }
}
void node_setup(struct node *n) {
  if (!node_is_setup(n)) {
    n->flags |= NODE_FLG_SETUP;
    if (n->type != NODE_TYPE_FOREACH) {
      if (n->child || n->setup) {
        _node_context_push(n);
        _setup_subnodes(n);
        if (n->setup) {
          n->setup(n);
        }
        _node_context_pop(n);
      }
    } else {
      _node_context_push(n);
      n->setup(n);
      _setup_subnodes(n);
      _node_context_pop(n);
    }
  }
}
void node_build(struct node *n) {
  if (!node_is_built(n)) {
    _build_subnodes(n);
    if (n->build) {
      if (g_env.verbose) {
        node_info(n, "Build");
      }
      struct xnode *x = (void*) n;
      x->bld_calls++;
      if (x->bld_calls > 1) {
        node_fatal(AK_ERROR_CYCLIC_BUILD_DEPS, n, 0);
      }
      _node_context_push(n);
      n->build(n);
      _node_context_pop(n);
      x->bld_calls--;
    }
    n->flags |= NODE_FLG_BUILT;
  }
}
void node_post_build(struct node *n) {
  if (!node_is_post_built(n)) {
    _post_build_subnodes(n);
    if (n->post_build) {
      if (g_env.verbose) {
        node_info(n, "Post-build");
      }
      struct xnode *x = (void*) n;
      x->bld_calls++;
      if (x->bld_calls > 1) {
        node_fatal(AK_ERROR_CYCLIC_BUILD_DEPS, n, 0);
      }
      _node_context_push(n);
      n->post_build(n);
      _node_context_pop(n);
      x->bld_calls--;
    }
    n->flags |= NODE_FLG_POST_BUILT;
  }
}
static int _script_bind(struct sctx *s) {
  int rc = 0;
  for (int i = 0; i < s->nodes.num; ++i) {
    struct node *n = NODE_AT(&s->nodes, i);
    if (!(n->flags & NODE_FLG_BOUND)) {
      RCC(rc, finish, _node_bind(n));
    }
  }
finish:
  return rc;
}
static int _script_open(struct node *parent, const char *file, struct node **out) {
  int rc = 0;
  struct node *n;
  char buf[PATH_MAX];
  autark_build_prepare(path_normalize(file, buf));
  RCC(rc, finish, _script_from_file(parent, buf, &n));
  RCC(rc, finish, _script_bind(n->ctx));
  if (out) {
    *out = n;
  }
finish:
  return rc;
}
int script_open(const char *file, struct sctx **out) {
  if (out) {
    *out = 0;
  }
  struct node *n = 0;
  int rc = _script_open(0, file, &n);
  if (!rc) {
    struct unit *root = unit_root();
    if (g_env.install.enabled) {
      unit_env_set_val(root, "INSTALL_ENABLED", "1");
    }
    if (g_env.install.prefix_dir || g_env.install.bin_dir) {
      unit_env_set_val(root, "INSTALL_PREFIX", g_env.install.prefix_dir);
      unit_env_set_val(root, "INSTALL_BIN_DIR", g_env.install.bin_dir);
      unit_env_set_val(root, "INSTALL_LIB_DIR", g_env.install.lib_dir);
      unit_env_set_val(root, "INSTALL_DATA_DIR", g_env.install.data_dir);
      unit_env_set_val(root, "INSTALL_INCLUDE_DIR", g_env.install.include_dir);
      unit_env_set_val(root, "INSTALL_PKGCONFIG_DIR", g_env.install.pkgconf_dir);
      unit_env_set_val(root, "INSTALL_MAN_DIR", g_env.install.man_dir);
      if (g_env.verbose) {
        akinfo("%s: INSTALL_PREFIX=%s", root->rel_path, g_env.install.prefix_dir);
        akinfo("%s: INSTALL_BIN_DIR=%s", root->rel_path, g_env.install.bin_dir);
        akinfo("%s: INSTALL_LIB_DIR=%s", root->rel_path, g_env.install.lib_dir);
        akinfo("%s: INSTALL_DATA_DIR=%s", root->rel_path, g_env.install.data_dir);
        akinfo("%s: INSTALL_INCLUDE_DIR=%s", root->rel_path, g_env.install.include_dir);
        akinfo("%s: INSTALL_PKGCONFIG_DIR=%s", root->rel_path, g_env.install.pkgconf_dir);
        akinfo("%s: INSTALL_MAN_DIR=%s", root->rel_path, g_env.install.man_dir);
      }
    }
    if (out && n) {
      *out = n->ctx;
    }
  }
  return rc;
}
int script_include(struct node *parent, const char *file, struct node **out) {
  return _script_open(parent, file, out);
}
void script_build(struct sctx *s) {
  akassert(s->root);
  node_init(s->root);
  node_setup(s->root);
  node_build(s->root);
  node_post_build(s->root);
}
void script_close(struct sctx **sp) {
  if (sp && *sp) {
    _script_destroy(*sp);
    *sp = 0;
  }
}
struct node_print_ctx {
  struct xstr *xstr;
};
static int _node_dump_visitor(struct node *n, int lvl, void *d) {
  struct node_print_ctx *ctx = d;
  struct xstr *xstr = ctx->xstr;
  if (lvl > 0) {
    xstr_cat_repeat(xstr, ' ', (lvl - 1) * NODE_PRINT_INDENT);
    const char *v = n->value ? n->value : "";
    if (node_is_value(n)) {
      xstr_printf(xstr, "%s:0x%x\n", v, n->type);
    } else {
      xstr_printf(xstr, "%s:0x%x {\n", v, n->type);
    }
  } else if (lvl < 0 && node_is_rule(n)) {
    xstr_cat_repeat(xstr, ' ', (-lvl - 1) * NODE_PRINT_INDENT);
    xstr_cat2(xstr, "}\n", 2);
  }
  return 0;
}
void script_dump(struct sctx *p, struct xstr *xstr) {
  if (!p || !p->root) {
    xstr_cat(xstr, "null");
    return;
  }
  struct node_print_ctx ctx = {
    .xstr = xstr,
  };
  _node_visit(p->root, 1, &ctx, _node_dump_visitor);
}
const char* node_env_get(struct node *n, const char *key) {
  const char *ret = 0;
  for ( ; n; n = n->parent) {
    if (n->unit) {
      ret = unit_env_get_raw(n->unit, key);
      if (ret) {
        return ret;
      }
    }
  }
  return getenv(key);
}
void node_env_set(struct node *n, const char *key, const char *val) {
  if (g_env.verbose) {
    if (val) {
      node_info(n, "%s=%s", key, val);
    } else {
      node_info(n, "UNSET %s", key);
    }
  }
  if (key[0] == '_' && key[1] == '\0') {
    // Skip on special '_' key
    return;
  }
  for ( ; n; n = n->parent) {
    if (n->unit) {
      if (val) {
        unit_env_set_val(n->unit, key, val);
      } else {
        unit_env_remove(n->unit, key);
      }
      return;
    }
  }
}
void node_env_set_node(struct node *n_, const char *key) {
  if (key[0] == '_' && key[1] == '\0') {
    // Skip on special '_' key
    return;
  }
  for (struct node *n = n_; n; n = n->parent) {
    if (n->unit) {
      unit_env_set_node(n->unit, key, n_);
      return;
    }
  }
}
void node_product_add(struct node *n, const char *prod, char pathbuf[PATH_MAX]) {
  char buf[PATH_MAX];
  if (!pathbuf) {
    pathbuf = buf;
  }
  if (is_vlist(prod)) {
    struct vlist_iter iter;
    vlist_iter_init(prod, &iter);
    while (vlist_iter_next(&iter)) {
      char path[PATH_MAX];
      utils_strnncpy(path, iter.item, iter.len, sizeof(path));
      prod = path_normalize(prod, pathbuf);
      node_product_add_raw(n, prod);
    }
  } else {
    prod = path_normalize(prod, pathbuf);
    node_product_add_raw(n, prod);
  }
}
void node_product_add_raw(struct node *n, const char *prod) {
  char buf[PATH_MAX];
  struct sctx *s = n->ctx;
  struct node *nn = map_get(s->products, prod);
  if (nn) {
    if (nn == n) {
      return;
    }
    node_fatal(AK_ERROR_FAIL, n, "Product: '%s' was registered by other rule: %s", prod, nn->name);
  }
  utils_strncpy(buf, prod, sizeof(buf));
  char *dir = path_dirname(buf);
  if (dir) {
    path_mkdirs(dir);
  }
  map_put_str(s->products, prod, n);
}
struct node* node_by_product(struct node *n, const char *prod, char pathbuf[PATH_MAX]) {
  struct sctx *s = n->ctx;
  char buf[PATH_MAX];
  if (!pathbuf) {
    pathbuf = buf;
  }
  prod = path_normalize(prod, pathbuf);
  if (!prod) {
    node_fatal(errno, n, 0);
  }
  struct node *nn = map_get(s->products, prod);
  return nn;
}
struct node* node_by_product_raw(struct node *n, const char *prod) {
  struct sctx *s = n->ctx;
  return map_get(s->products, prod);
}
struct node* node_find_direct_child(struct node *n, int type, const char *val) {
  if (n) {
    for (struct node *nn = n->child; nn; nn = nn->next) {
      if (type == 0 || nn->type == type) {
        if (val == 0 || strcmp(nn->value, val) == 0) {
          return nn;
        }
      }
    }
  }
  return 0;
}
struct node* node_find_prev_sibling(struct node *n) {
  if (n) {
    struct node *p = n->parent;
    if (p) {
      for (struct node *prev = 0, *c = p->child; c; prev = c, c = c->next) {
        if (c == n) {
          return prev;
        }
      }
    }
  }
  return 0;
}
struct  node* node_find_parent_of_type(struct node *n, int type) {
  for (struct node *nn = n->parent; nn; nn = nn->parent) {
    if (type == 0 || nn->type == type) {
      return nn;
    }
  }
  return 0;
}
struct node_foreach* node_find_parent_foreach(struct node *n) {
  struct node *nn = node_find_parent_of_type(n, NODE_TYPE_FOREACH);
  if (nn) {
    return nn->impl;
  } else {
    return 0;
  }
}
bool node_is_value_may_be_dep_saved(struct node *n) {
  if (!n || n->type == NODE_TYPE_VALUE) { // Hardcoded value
    return false;
  }
  if (node_is_can_be_value(n)) {
    struct node_foreach *fe = node_find_parent_foreach(n);
    if (fe) {
      fe->access_cnt = 0;
      node_value(n);
      if (fe->access_cnt > 0) {
        return false;
      }
    }
    return true;
  }
  return false;
}
struct node* node_consumes_resolve(
  struct node *n,
  struct node *nn,
  struct ulist *paths,
  void (*on_resolved)(const char *path, void*),
  void *opq) {
  char prevcwd[PATH_MAX];
  char pathbuf[PATH_MAX];
  struct unit *unit = unit_peek();
  struct ulist rlist = { .usize = sizeof(char*) };
  struct pool *pool = pool_create_empty();
  if (nn || paths) {
    for ( ; nn; nn = nn->next) {
      const char *cv = node_value(nn);
      if (is_vlist(cv)) {
        struct vlist_iter iter;
        vlist_iter_init(cv, &iter);
        while (vlist_iter_next(&iter)) {
          cv = pool_strndup(pool, iter.item, iter.len);
          ulist_push(&rlist, &cv);
        }
      } else if (cv && *cv != '\0') {
        ulist_push(&rlist, &cv);
      }
    }
    if (paths) {
      for (int i = 0; i < paths->num; ++i) {
        const char *p = *(char**) ulist_get(paths, i);
        if (p && *p != '\0') {
          ulist_push(&rlist, &p);
        }
      }
    }
    unit_ch_cache_dir(unit, prevcwd);
    for (int i = 0; i < rlist.num; ++i) {
      const char *cv = *(char**) ulist_get(&rlist, i);
      struct node *pn = node_by_product(n, cv, pathbuf);
      if (pn) {
        node_build(pn);
      }
      if (path_is_exist(pathbuf)) {
        if (on_resolved) {
          on_resolved(pathbuf, opq);
        }
        ulist_remove(&rlist, i--);
      }
    }
    akcheck(chdir(prevcwd));
    if (rlist.num) {
      unit_ch_src_dir(unit, prevcwd);
      for (int i = 0; i < rlist.num; ++i) {
        const char *cv = *(char**) ulist_get(&rlist, i);
        akassert(path_normalize(cv, pathbuf));
        if (path_is_exist(pathbuf)) {
          if (on_resolved) {
            on_resolved(pathbuf, opq);
          }
        } else {
          node_fatal(AK_ERROR_DEPENDENCY_UNRESOLVED, n, "'%s'", cv);
        }
      }
      akcheck(chdir(prevcwd));
    }
  }
  ulist_destroy_keep(&rlist);
  pool_destroy(pool);
  return nn;
}
void node_add_unit_deps(struct node *n, struct deps *deps) {
  struct unit *prev = 0;
  for (struct node *nn = n; nn; nn = nn->parent) {
    if (nn->unit && nn->unit != prev) {
      prev = nn->unit;
      deps_add(deps, DEPS_TYPE_FILE, 0, nn->unit->source_path, 0);
    }
  }
}
void node_resolve(struct node_resolve *r) {
  akassert(r && r->path && r->n);
  int rc;
  struct deps deps = { 0 };
  struct pool *pool = pool_create_empty();
  struct unit *unit = unit_peek();
  const char *deps_path = pool_printf(pool, "%s/%s.deps", unit->cache_dir, r->path);
  const char *env_path = pool_printf(pool, "%s/%s.env", unit->cache_dir, r->path);
  const char *deps_path_tmp = pool_printf(pool, "%s/%s.deps.tmp", unit->cache_dir, r->path);
  const char *env_path_tmp = pool_printf(pool, "%s/%s.env.tmp", unit->cache_dir, r->path);
  unlink(deps_path_tmp);
  unlink(env_path_tmp);
  r->pool = pool;
  r->num_deps = 0;
  r->deps_path_tmp = deps_path_tmp;
  r->env_path_tmp = env_path_tmp;
  ulist_init(&r->resolve_outdated, 0, sizeof(struct resolve_outdated));
  if (r->on_init) {
    r->on_init(r);
  }
  if (!r->force_outdated && !deps_open(deps_path, DEPS_OPEN_READONLY, &deps)) {
    char *prev_outdated = 0;
    while (deps_cur_next(&deps)) {
      ++r->num_deps;
      bool outdated = false;
      if (prev_outdated && strcmp(prev_outdated, deps.resource) == 0) {
        outdated = false;
      } else if (deps.type == DEPS_TYPE_NODE_VALUE) {
        if (deps.serial >= 0 && deps.serial < r->node_val_deps.num) {
          struct node *nv = *(struct node**) ulist_get(&r->node_val_deps, (unsigned int) deps.serial);
          const char *val = node_value(nv);
          outdated = val == 0 || strcmp(val, deps.resource) != 0;
        } else {
          outdated = true;
        }
      } else {
        outdated = deps_cur_is_outdated(r->n, &deps);
      }
      if (outdated) {
        prev_outdated = pool_strdup(pool, deps.resource);
        if (g_env.check.log && r->n) {
          char buf[PATH_MAX];
          utils_strncpy(buf, prev_outdated, sizeof(buf));
          xstr_printf(g_env.check.log, "%s: outdated %s t=%c f=%c\n",
                      r->n->name, path_basename(buf), deps.type, deps.flags);
        }
        ulist_push(&r->resolve_outdated, &(struct resolve_outdated) {
          .type = deps.type,
          .flags = deps.flags,
          .path = prev_outdated
        });
      }
    }
  }
  if (deps.file) {
    deps_close(&deps);
  }
  bool env_created = false;
  if (r->on_resolve && (r->num_deps == 0 || r->resolve_outdated.num)) {
    if (g_env.check.log && r->n) {
      xstr_printf(g_env.check.log, "%s: resolved outdated outdated=%d\n", r->n->name, r->resolve_outdated.num);
    }
    r->on_resolve(r);
    if (access(deps_path_tmp, F_OK) == 0) {
      rc = utils_rename_file(deps_path_tmp, deps_path);
      if (rc) {
        akfatal(rc, "Rename failed of %s to %s", deps_path_tmp, deps_path);
      }
    }
    if (r->on_env_value && !access(env_path_tmp, F_OK)) {
      rc = utils_rename_file(env_path_tmp, env_path);
      if (rc) {
        akfatal(rc, "Rename failed of %s to %s", env_path_tmp, env_path);
      }
      env_created = true;
    }
  }
  if (r->on_env_value && (r->mode & NODE_RESOLVE_ENV_ALWAYS) && (env_created || !access(env_path, R_OK))) {
    char buf[4096];
    FILE *f = fopen(env_path, "r");
    if (f) {
      while (fgets(buf, sizeof(buf), f)) {
        char *p = strchr(buf, '=');
        if (p) {
          *p = '\0';
          char *val = p + 1;
          for (int vlen = strlen(val); vlen >= 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r'); --vlen) {
            val[vlen - 1] = '\0';
          }
          r->on_env_value(r, buf, val);
        }
      }
      fclose(f);
    } else {
      akfatal(rc, "Failed to open env file: %s", env_path);
    }
  }
  ulist_destroy_keep(&r->resolve_outdated);
  ulist_destroy_keep(&r->node_val_deps);
  pool_destroy(pool);
}
#ifdef TESTS
int test_script_parse(const char *script_path, struct sctx **out) {
  *out = 0;
  char buf[PATH_MAX];
  akassert(getcwd(buf, sizeof(buf)));
  int rc = script_open(script_path, out);
  chdir(buf);
  return rc;
}
#endif
a292effa503b
if grep -E '^ID(_LIKE)?=.*debian' /etc/os-release >/dev/null; then
  CFLAGS="-DDEBIAN_MULTIARCH"
fi

(set -x; ${COMPILER} ${AUTARK_HOME}/autark.c --std=c99 -O1 -march=native ${CFLAGS} -o ${AUTARK_HOME}/autark)
cp $0 ${AUTARK_HOME}/build.sh
echo "Done"

set +e
${AUTARK} "$@"
exit $?
