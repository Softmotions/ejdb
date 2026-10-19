#pragma once
#ifndef IW_IWTESTS_H
#define IW_IWTESTS_H

/**************************************************************************************************
 * IOWOW library
 *
 * MIT License
 *
 * Copyright (c) 2012-2026 Softmotions Ltd <info@softmotions.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *************************************************************************************************/

/** @file
 *  @brief Minimal dependency free test harness used by the iowow test cases.
 *
 *  <strong>Usage</strong>
 *
 *  A test executable declares a set of case functions and a single
 *  ::iw_test_run() call in `main`:
 *
 *  @code{.c}
 *  #include "iwtests.h"
 *
 *  static void test_foo(void) {
 *    IW_TEST_ASSERT_EQUAL(1 + 1, 2);
 *    IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(some_object());
 *    IW_TEST_ASSERT_STR_EQUAL("bar", "bar");
 *  }
 *
 *  static int suite_init(void) { return 0; }
 *  static void suite_cleanup(void) { }
 *
 *  int main(int argc, char *argv[]) {
 *    static const struct iw_test_case cases[] = {
 *      IW_TEST_CASE(test_foo),
 *    };
 *    return iw_test_run(argc, argv, IW_TEST_SUITE("my_test", cases, suite_init, suite_cleanup));
 *  }
 *  @endcode
 *
 *  The harness has no dependencies besides the C11 standard library and can
 *  be used by other projects as well.
 *
 *  <strong>Selected cases</strong>
 *
 *  Any number of case name patterns can be passed on the command line
 *  (positional arguments or `-f/--filter`). The `IW_TEST_FILTER` environment
 *  variable provides the same filter for a run driven by a build system:
 *
 *  @code{.sh}
 *  ./my_test reopen                 # run cases whose name contains "reopen"
 *  ./my_test -f reopen,wal -v       # run matching cases, list the skipped ones
 *  ./my_test --list                 # just show what can be run
 *  IW_TEST_FILTER=reopen ./run_tests.sh
 *  @endcode
 *
 *  Patterns are case-insensitive substrings, a case is run when it matches at
 *  least one pattern. A suite without matching cases produces no output and
 *  exits with `0`.
 *
 *  Everything after a `--` argument is not parsed by the harness and is
 *  available to the test code as ::iw_test_argc and ::iw_test_argv:
 *
 *  @code{.sh}
 *  ./my_test -f reopen -- --listen localhost:8080
 *  @endcode
 *
 *  <strong>Test code output</strong>
 *
 *  The `stdout`/`stderr` written by the test cases (and by the suite hooks) is
 *  redirected into `<program name>.log` to keep the report readable. Use
 *  `-o/--output FILE` (or `IW_TEST_OUTPUT`) for another file and
 *  `--no-log` (or `IW_TEST_NO_LOG`) to keep the output on the console. The
 *  redirection is implemented with `dup(2)`/`dup2(2)` and is available on the
 *  `POSIX` systems only.
 *
 *  Colored output is used only when the report is written to a terminal (or is
 *  forced). `--color`/`IW_TEST_COLOR`/`AUTARK_INTERACTIVE` force the
 *  colors, `--no-color`/`IW_TEST_NO_COLOR` disable them. The output
 *  relayed by a build system is not a terminal, so such a build has to pass
 *  `--color` (or set `IW_TEST_COLOR`) to keep the colors, see the color notes
 *  in `TESTS.md`.
 *
 *  <strong>Fatal assertions and skipped cases</strong>
 *
 *  Fatal assertions (`*_FATAL`) report the failure and abort the current test
 *  case, the remaining cases are still executed. A fatal assertion raised from
 *  a thread other than the test case thread cannot be unwound, such a failure
 *  aborts the whole process.
 *
 *  ::IW_TEST_SKIP() reports the current case as skipped without failing the
 *  run, it is intended for the opt-in (fuzzer/slow) test cases.
 *
 *  <strong>Standalone use</strong>
 *
 *  The assertion macros and the global state do not require ::iw_test_run(),
 *  so a test fixture (for example a HTTP server serving the requests of an
 *  external test script) can use the same assertions and report its failures
 *  with ::iw_test_failures():
 *
 *  @code{.c}
 *  int main(int argc, char *argv[]) {
 *    serve_requests();
 *    return iw_test_failures() ? EXIT_FAILURE : EXIT_SUCCESS;
 *  }
 *  @endcode
 *
 *  A fatal assertion raised outside of a test case terminates the process with
 *  the exit code `1`.
 */

#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* POSIX systems: isatty(3) and the output redirection support. */
#if defined(unix) || defined(__unix__) || defined(__unix) || defined(__APPLE__) \
  || defined(__MACH__) || defined(__FreeBSD__) || defined(__NetBSD__)           \
  || defined(__OpenBSD__) || defined(__DragonFly__)
#include <unistd.h>
#define IW_TEST_HAVE_ISATTY   1
#define IW_TEST_HAVE_REDIRECT 1
#endif

/** Number of failure details retained and reported for a single test case. */
#define IW_TEST_MAX_FAILURES 10

/** Maximum length of a single failure detail message. */
#define IW_TEST_MSG_SZ 512

/** Maximum number of name filter patterns. */
#define IW_TEST_MAX_PATTERNS 32

/** Name of the harness shown in the report header, define it before including
 *  `iwtests.h` to use your own project name. */
#ifndef IW_TEST_HARNESS
#define IW_TEST_HARNESS "iowow"
#endif

/** Single test case: a case name and a void function performing it. */
struct iw_test_case {
  const char *name;
  void (*fn)(void);
};

/** Test case set executed by a test executable. */
struct iw_test_suite {
  const char *name;                  ///< Suite name shown in the report.
  const struct iw_test_case *cases;  ///< Test cases.
  unsigned int ncases;               ///< Number of test cases.
  int  (*init)(void);                ///< Optional suite initializer, `0` to skip.
  void (*cleanup)(void);             ///< Optional suite cleanup, `0` to skip.
};

/** Declares a test case entry. */
#define IW_TEST_CASE(fn_) { .name =#fn_, .fn = (fn_) }

/** Declares a test case entry with an explicit case name. */
#define IW_TEST_CASE_AS(name_, fn_) { .name = (name_), .fn = (fn_) }

/** Builds a ::iw_test_suite value, must be used as an argument in place. */
#define IW_TEST_SUITE(name_, cases_, init_, cleanup_)                \
        (&(const struct iw_test_suite) {                             \
    .name = (name_),                                                 \
    .cases = (cases_),                                               \
    .ncases = (unsigned int) (sizeof(cases_) / sizeof((cases_)[0])), \
    .init = (init_),                                                 \
    .cleanup = (cleanup_),                                           \
  })

/** Runs the test suite and returns the process exit code: `0` on success. */
static int iw_test_run(int argc, char *argv[], const struct iw_test_suite *suite);

/** Number of arguments given after `--`, `0` when there were none. */
static int iw_test_argc;

/** Arguments given after `--`, `0` when there were none. */
static char **iw_test_argv;

/* -------------------------------------------------------------------------- */
/*                                 Internals                                  */
/* -------------------------------------------------------------------------- */

/// Silences the expected signed/unsigned and float equality warnings inside a macro block.
#if defined(__GNUC__) || defined(__clang__)
#define IW_TEST_DIAG_PUSH                                    \
        _Pragma("GCC diagnostic push")                       \
        _Pragma("GCC diagnostic ignored \"-Wsign-compare\"") \
        _Pragma("GCC diagnostic ignored \"-Wfloat-equal\"")
#define IW_TEST_DIAG_POP _Pragma("GCC diagnostic pop")
#else
#define IW_TEST_DIAG_PUSH
#define IW_TEST_DIAG_POP
#endif

/// Zero for signed integer types, `1` for unsigned ones, `2` for pointers.
#define IW_TEST_KIND(v_) _Generic((v_),                                                 \
                                  char*: 2, const char*: 2, void*: 2, const void*: 2,   \
                                  _Bool: 1,                                             \
                                  unsigned char: 1, unsigned short: 1, unsigned int: 1, \
                                  unsigned long: 1, unsigned long long: 1,              \
                                    default:                                            \
                                      0)

struct iw_test_state {
  jmp_buf jmp;                          ///< Unwind point of the current case.
  bool    armed;                        ///< `jmp` can be used.
  bool    aborted;                      ///< Case was aborted by a fatal assertion.
  bool    skipped;                      ///< Case is skipped by IW_TEST_SKIP().
  char    skip_reason[IW_TEST_MSG_SZ];  ///< Reason of the skip.
  _Atomic unsigned long assertions;     ///< Assertions performed in the current case.
  _Atomic unsigned long failures;       ///< Failures of the current case.
  unsigned long dumped;                 ///< Failure details stored in `messages`.
  char messages[IW_TEST_MAX_FAILURES][IW_TEST_MSG_SZ];
  unsigned long total_assertions;       ///< Assertions of all executed cases.
  unsigned long passed;                 ///< Passed cases.
  unsigned long failed;                 ///< Failed cases.
  unsigned long skipped_cases;          ///< Skipped cases.
};

static struct iw_test_state iw_test_state;
static _Thread_local bool iw_test_case_thread;
static atomic_flag iw_test_msg_lock = ATOMIC_FLAG_INIT;
static FILE *iw_test_out;

static void iw_test_lock(void) {
  while (atomic_flag_test_and_set(&iw_test_msg_lock)) {
  }
}

static void iw_test_unlock(void) {
  atomic_flag_clear(&iw_test_msg_lock);
}

/// Returns the last path component of `path`.
static const char* iw_test_basename(const char *path) {
  const char *p = strrchr(path, '/');
  if (p) {
    return p + 1;
  }
  p = strrchr(path, '\\');
  return p ? p + 1 : path;
}

static const char* iw_test_str(const char *s) {
  return s ? s : "(null)";
}

static char iw_test_lower(char c) {
  return (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
}

#define IW_TEST_TC_RESET "\x1b[0m"
#define IW_TEST_TC_BOLD  "\x1b[1m"
#define IW_TEST_TC_DIM   "\x1b[2m"
#define IW_TEST_TC_RED   "\x1b[31m"
#define IW_TEST_TC_GREEN "\x1b[32m"
#define IW_TEST_TC_GREY  "\x1b[90m"

/// Formats a signed integer value, the `0x` form is added for non trivial values.
static void iw_test_fmt_signed(char *buf, size_t sz, long long v) {
  if (v > 255 || v < -255) {
    unsigned long long uv = v < 0 ? (unsigned long long) (-(v + 1)) + 1 : (unsigned long long) v;
    snprintf(buf, sz, "%lld (0x%llx)", v, uv);
  } else {
    snprintf(buf, sz, "%lld", v);
  }
}

/// Formats an unsigned integer value, the `0x` form is added for non trivial values.
static void iw_test_fmt_unsigned(char *buf, size_t sz, unsigned long long v) {
  if (v > 255) {
    snprintf(buf, sz, "%llu (0x%llx)", v, v);
  } else {
    snprintf(buf, sz, "%llu", v);
  }
}

/// Records an assertion failure of the current test case.
static void iw_test_vfail(const char *file, int line, const char *fmt, va_list ap) {
  iw_test_state.failures += 1;
  iw_test_lock();
  if (iw_test_state.dumped < IW_TEST_MAX_FAILURES) {
    char *buf = iw_test_state.messages[iw_test_state.dumped];
    int off = snprintf(buf, IW_TEST_MSG_SZ, "%s:%d: ", iw_test_basename(file), line);
    if (off > 0 && off < IW_TEST_MSG_SZ) {
      vsnprintf(buf + off, IW_TEST_MSG_SZ - (size_t) off, fmt, ap);
    }
    ++iw_test_state.dumped;
  }
  iw_test_unlock();
}

/// Records an assertion failure of the current test case.
static void iw_test_fail(const char *file, int line, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  iw_test_vfail(file, line, fmt, ap);
  va_end(ap);
}

/// Terminates the current test case, called by the fatal assertions.
static _Noreturn void iw_test_abort_case(void) {
  if (iw_test_state.armed && iw_test_case_thread) {
    longjmp(iw_test_state.jmp, 1);
  }
  fflush(0);
  if (iw_test_state.armed) {
    /* A fatal assertion reported by a foreign thread cannot be unwound. */
    abort();
  }
  /* No case is running: the harness is used by a standalone test program. */
  exit(1);
}

/** Returns the number of failed assertions recorded so far.
 *
 *  It is the number of failures of the current case, or the number of failures
 *  of the whole program for a standalone test program which does not call
 *  ::iw_test_run().
 */
static unsigned long iw_test_failures(void) {
  return atomic_load(&iw_test_state.failures);
}

/// Marks the current test case as skipped and terminates it.
static _Noreturn void iw_test_skip(const char *file, int line, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  iw_test_lock();
  int off = snprintf(iw_test_state.skip_reason, IW_TEST_MSG_SZ, "%s:%d: ",
                     iw_test_basename(file), line);
  if (off > 0 && off < IW_TEST_MSG_SZ) {
    vsnprintf(iw_test_state.skip_reason + off, IW_TEST_MSG_SZ - (size_t) off, fmt, ap);
  }
  iw_test_unlock();
  va_end(ap);
  iw_test_state.skipped = true;
  iw_test_abort_case();
}

static void iw_test_fail_eq_signed(
  const char *file, int line, const char *aexpr, const char *eexpr,
  long long a, long long e) {
  char ab[48], eb[48];
  iw_test_fmt_signed(ab, sizeof(ab), a);
  iw_test_fmt_signed(eb, sizeof(eb), e);
  iw_test_fail(file, line, "`%s` != `%s`: %s vs %s", aexpr, eexpr, ab, eb);
}

static void iw_test_fail_eq_unsigned(
  const char *file, int line, const char *aexpr, const char *eexpr,
  unsigned long long a, unsigned long long e) {
  char ab[48], eb[48];
  iw_test_fmt_unsigned(ab, sizeof(ab), a);
  iw_test_fmt_unsigned(eb, sizeof(eb), e);
  iw_test_fail(file, line, "`%s` != `%s`: %s vs %s", aexpr, eexpr, ab, eb);
}

static void iw_test_fail_eq_ptr(
  const char *file, int line, const char *aexpr, const char *eexpr,
  const void *a, const void *e) {
  iw_test_fail(file, line, "`%s` != `%s`: %p vs %p", aexpr, eexpr, a, e);
}

/// Returns `true` if `pat` is a case-insensitive substring of `name`.
static bool iw_test_match_one(const char *pat, size_t len, const char *name) {
  size_t nlen = strlen(name);
  if (!len || len > nlen) {
    return false;
  }
  for (size_t i = 0; i + len <= nlen; ++i) {
    size_t j = 0;
    while (j < len && iw_test_lower(name[i + j]) == iw_test_lower(pat[j])) {
      ++j;
    }
    if (j == len) {
      return true;
    }
  }
  return false;
}

/// Returns `true` if `name` matches any of the `,` separated patterns of `filter`.
static bool iw_test_match(const char *filter, const char *name) {
  const char *p = filter;
  while (p && *p) {
    const char *e = strchr(p, ',');
    size_t len = e ? (size_t) (e - p) : strlen(p);
    while (len && (*p == ' ' || *p == '\t')) {
      ++p;
      --len;
    }
    while (len && (p[len - 1] == ' ' || p[len - 1] == '\t')) {
      --len;
    }
    if (iw_test_match_one(p, len, name)) {
      return true;
    }
    p = e ? e + 1 : 0;
  }
  return false;
}

static double iw_test_now(void) {
  struct timespec ts;
#if defined(CLOCK_MONOTONIC)
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  timespec_get(&ts, TIME_UTC);
#endif
  return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

/// Returns `true` if the output can render unicode decorations.
static bool iw_test_utf8(void) {
  if (getenv("IW_TEST_ASCII")) {
    return false;
  }
  static const char *vars[] = { "LC_ALL", "LC_CTYPE", "LANG" };
  for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); ++i) {
    const char *v = getenv(vars[i]);
    if (!v || !*v) {
      continue;
    }
    /* The first defined locale variable wins. */
    return !!strstr(v, "UTF-8") || !!strstr(v, "utf8") || !!strstr(v, "UTF8") || !!strstr(v, "utf-8");
  }
  return false;
}

struct iw_test_ui {
  bool color;   ///< ANSI colors are enabled.
  bool utf8;    ///< Unicode decorations are enabled.
};

/** Redirects the `stdout`/`stderr` of the test code into a file. */
struct iw_test_redir {
  FILE *file;      ///< Opened output file, `0` when redirection is disabled.
  int   fd;        ///< Descriptor of the output file.
  int   saved_out; ///< Saved `STDOUT_FILENO`.
  int   saved_err; ///< Saved `STDERR_FILENO`.
  bool  active;    ///< The descriptors are currently redirected.
};

/// Starts redirecting the test code output into the `r->file`.
static void iw_test_redir_push(struct iw_test_redir *r) {
#if defined(IW_TEST_HAVE_REDIRECT)
  if (r->fd < 0 || r->active) {
    return;
  }
  fflush(stdout);
  fflush(stderr);
  r->saved_out = dup(STDOUT_FILENO);
  r->saved_err = dup(STDERR_FILENO);
  if (  r->saved_out < 0 || r->saved_err < 0
     || dup2(r->fd, STDOUT_FILENO) < 0 || dup2(r->fd, STDERR_FILENO) < 0) {
    if (r->saved_out >= 0) {
      dup2(r->saved_out, STDOUT_FILENO);
      close(r->saved_out);
    }
    if (r->saved_err >= 0) {
      dup2(r->saved_err, STDERR_FILENO);
      close(r->saved_err);
    }
    r->fd = -1; /* Give up silently, the report is more important. */
    return;
  }
  r->active = true;
#else
  (void) r;
#endif
}

/// Stops redirecting the test code output.
static void iw_test_redir_pop(struct iw_test_redir *r) {
#if defined(IW_TEST_HAVE_REDIRECT)
  if (!r->active) {
    return;
  }
  fflush(stdout);
  fflush(stderr);
  dup2(r->saved_out, STDOUT_FILENO);
  dup2(r->saved_err, STDERR_FILENO);
  close(r->saved_out);
  close(r->saved_err);
  r->saved_out = -1;
  r->saved_err = -1;
  r->active = false;
#else
  (void) r;
#endif
}

/// Returns `true` if the environment variable `name` is set to a non empty value.
static bool iw_test_env_set(const char *name) {
  const char *v = getenv(name);
  return v && *v;
}

/**
 * Decides whether the colored output should be used.
 *
 * @param cmdline `1` for `--color`, `-1` for `--no-color`, `0` to auto detect.
 */
static bool iw_test_color(int cmdline) {
  if (cmdline > 0) {
    return true;
  } else if (cmdline < 0) {
    return false;
  }
  if (iw_test_env_set("IW_TEST_NO_COLOR")) {
    return false;
  }
  const char *force = getenv("IW_TEST_COLOR");
  if (!force || !*force) {
    force = getenv("AUTARK_INTERACTIVE");
  }
  if (force && *force) {
    return strcmp(force, "0") != 0;
  }
#if defined(IW_TEST_HAVE_ISATTY)
  return isatty(STDOUT_FILENO) != 0;
#else
  return false;
#endif
}

static const char* iw_test_c(const struct iw_test_ui *ui, const char *seq) {
  return ui->color ? seq : "";
}

static void iw_test_hr(const struct iw_test_ui *ui, unsigned int width) {
  fputc(' ', iw_test_out);
  fputs(iw_test_c(ui, IW_TEST_TC_GREY), iw_test_out);
  for (unsigned int i = 2; i < width; ++i) {
    /* U+2500 BOX DRAWINGS LIGHT HORIZONTAL */
    fputs(ui->utf8 ? "\xe2\x94\x80" : "-", iw_test_out);
  }
  fputs(iw_test_c(ui, IW_TEST_TC_RESET), iw_test_out);
  fputc('\n', iw_test_out);
}

static void iw_test_dot(const struct iw_test_ui *ui) {
  fputs(iw_test_c(ui, IW_TEST_TC_GREY), iw_test_out);
  /* U+00B7 MIDDLE DOT */
  fputs(ui->utf8 ? "\xc2\xb7" : "-", iw_test_out);
  fputs(iw_test_c(ui, IW_TEST_TC_RESET), iw_test_out);
}

/// Emits the case state glyph: `0` - passed, `1` - failed, other - skipped.
static void iw_test_glyph(const struct iw_test_ui *ui, int state) {
  switch (state) {
    case 0:
      fputs(iw_test_c(ui, IW_TEST_TC_GREEN IW_TEST_TC_BOLD), iw_test_out);
      /* U+2713 CHECK MARK */
      fputs(ui->utf8 ? "\xe2\x9c\x93" : "ok  ", iw_test_out);
      break;
    case 1:
      fputs(iw_test_c(ui, IW_TEST_TC_RED IW_TEST_TC_BOLD), iw_test_out);
      /* U+2717 BALLOT X */
      fputs(ui->utf8 ? "\xe2\x9c\x97" : "FAIL", iw_test_out);
      break;
    default:
      fputs(iw_test_c(ui, IW_TEST_TC_GREY), iw_test_out);
      /* U+2022 BULLET */
      fputs(ui->utf8 ? "\xe2\x80\xa2" : "skip", iw_test_out);
      break;
  }
  fputs(iw_test_c(ui, IW_TEST_TC_RESET), iw_test_out);
}

/// Prints the collected failure details of the current test case.
static void iw_test_report_failures(const struct iw_test_ui *ui) {
  unsigned long failures = atomic_load(&iw_test_state.failures);
  for (unsigned long i = 0; i < iw_test_state.dumped; ++i) {
    fputs("      ", iw_test_out);
    fputs(iw_test_c(ui, IW_TEST_TC_RED), iw_test_out);
    fputs(iw_test_state.messages[i], iw_test_out);
    fputs(iw_test_c(ui, IW_TEST_TC_RESET), iw_test_out);
    fputc('\n', iw_test_out);
  }
  if (failures > iw_test_state.dumped) {
    fputs("      ", iw_test_out);
    fputs(iw_test_c(ui, IW_TEST_TC_RED), iw_test_out);
    fprintf(iw_test_out, "... %lu more failed assertion(s)", failures - iw_test_state.dumped);
    fputs(iw_test_c(ui, IW_TEST_TC_RESET), iw_test_out);
    fputc('\n', iw_test_out);
  }
}

static void iw_test_usage(FILE *out, const char *prog, const struct iw_test_suite *suite) {
  fprintf(out,
          "Usage: %s [options] [pattern ...]\n"
          "\n"
          "Runs the test cases of the '%s' suite.\n"
          "\n"
          "Options:\n"
          "  -f, --filter PATTERNS   Run only cases matching the comma separated\n"
          "                          case-insensitive substring list.\n"
          "  -o, --output FILE       Redirect the stdout/stderr of the test code\n"
          "                          into FILE to keep the report clean.\n"
          "                          Default: <program name>.log\n"
          "      --no-log            Do not redirect the test code output.\n"
          "      -- [ARG ...]        Stop option parsing, ARG(s) are passed to the\n"
          "                          test code (see iw_test_argc, iw_test_argv).\n"
          "  -l, --list              List the test cases, do not run them.\n"
          "  -v, --verbose           Show the cases skipped by the filter.\n"
          "      --color             Force colored output.\n"
          "      --no-color          Disable colored output.\n"
          "  -h, --help              Show this help.\n"
          "\n"
          "Environment:\n"
          "  IW_TEST_FILTER          Same as --filter, ignored when patterns are\n"
          "                          given on the command line.\n"
          "  IW_TEST_OUTPUT          Same as --output.\n"
          "  IW_TEST_NO_LOG          Same as --no-log.\n"
          "  IW_TEST_COLOR           Force colored output, the value `0` disables it.\n"
          "  IW_TEST_NO_COLOR        Disable colored output.\n"
          "  NO_COLOR                Disable colored output.\n"
          "  FORCE_COLOR             Force colored output, the value `0` disables it.\n"
          "  CLICOLOR_FORCE          Force colored output, the value `0` disables it.\n"
          "  IW_TEST_ASCII           Disable unicode decorations.\n"
          "\n"
          "Examples:\n"
          "  %s                  run every case\n"
          "  %s reopen           run cases whose name contains 'reopen'\n"
          "  %s -f a,b -v        run cases matching 'a' or 'b', show skipped ones\n"
          "  %s -o out.log       write the test code output into out.log\n"
          "  %s --no-log         keep the test code output on the console\n"
          "  %s --list           list the cases\n",
          prog, suite->name, prog, prog, prog, prog, prog, prog);
}

/// Executes a single test case, fatal assertions and skips are unwound here.
static void iw_test_exec(void (*fn)(void)) {
  iw_test_state.dumped = 0;
  iw_test_state.aborted = false;
  iw_test_state.skipped = false;
  iw_test_state.skip_reason[0] = '\0';
  atomic_store(&iw_test_state.assertions, 0);
  atomic_store(&iw_test_state.failures, 0);
  iw_test_case_thread = true;
  if (setjmp(iw_test_state.jmp) == 0) {
    iw_test_state.armed = true;
    fn();
  } else {
    iw_test_state.aborted = true;
  }
  iw_test_state.armed = false;
  iw_test_case_thread = false;
}

static void iw_test_print_header(
  const struct iw_test_ui *ui, const struct iw_test_suite *suite,
  unsigned int nsel, const char *filter, const char *output) {
  fputc('\n', iw_test_out);
  fputc(' ', iw_test_out);
  fputs(iw_test_c(ui, IW_TEST_TC_DIM), iw_test_out);
  fputs(IW_TEST_HARNESS, iw_test_out);
  fputs(iw_test_c(ui, IW_TEST_TC_RESET), iw_test_out);
  fputc(' ', iw_test_out);
  iw_test_dot(ui);
  fprintf(iw_test_out, " %s%s%s ", iw_test_c(ui, IW_TEST_TC_BOLD), suite->name,
          iw_test_c(ui, IW_TEST_TC_RESET));
  iw_test_dot(ui);
  if (filter) {
    fprintf(iw_test_out, " %u of %u case(s) ", nsel, suite->ncases);
    iw_test_dot(ui);
    fprintf(iw_test_out, " %s\"%s\"%s", iw_test_c(ui, IW_TEST_TC_DIM), filter,
            iw_test_c(ui, IW_TEST_TC_RESET));
  } else {
    fprintf(iw_test_out, " %u case(s)", nsel);
  }
  if (output) {
    fprintf(iw_test_out, " ");
    iw_test_dot(ui);
    fprintf(iw_test_out, " %soutput: %s%s", iw_test_c(ui, IW_TEST_TC_DIM), output,
            iw_test_c(ui, IW_TEST_TC_RESET));
  }
  fputc('\n', iw_test_out);
}

static int iw_test_run(int argc, char *argv[], const struct iw_test_suite *suite) {
  const char *patterns[IW_TEST_MAX_PATTERNS];
  char *filter_buf = 0;
  iw_test_argc = 0;
  iw_test_argv = 0;
  const char *output = 0;
  char output_auto[512];
  int npatterns = 0;
  int color = 0; /* 0 - auto, 1 - force on, -1 - force off */
  bool list = false;
  bool verbose = false;
  bool no_log = false;
  const char *prog = iw_test_basename(argv[0]);

  iw_test_out = stdout;
  output_auto[0] = '\0';

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    const char *value = 0;
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      iw_test_usage(stdout, prog, suite);
      return 0;
    } else if (!strcmp(arg, "-l") || !strcmp(arg, "--list")) {
      list = true;
    } else if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose")) {
      verbose = true;
    } else if (!strcmp(arg, "--color")) {
      color = 1;
    } else if (!strcmp(arg, "--no-color")) {
      color = -1;
    } else if (!strcmp(arg, "--no-log")) {
      no_log = true;
    } else if (!strcmp(arg, "--")) {
      /* The rest of the arguments belong to the test code. */
      iw_test_argc = argc - i - 1;
      iw_test_argv = argv + i + 1;
      break;
    } else if (!strcmp(arg, "-f") || !strcmp(arg, "--filter")) {
      if (++i >= argc) {
        fprintf(stderr, "%s: option '%s' requires an argument\n", prog, arg);
        return 2;
      }
      value = argv[i];
    } else if (!strncmp(arg, "--filter=", sizeof("--filter=") - 1)) {
      value = arg + sizeof("--filter=") - 1;
    } else if (!strcmp(arg, "-o") || !strcmp(arg, "--output")) {
      if (++i >= argc) {
        fprintf(stderr, "%s: option '%s' requires an argument\n", prog, arg);
        return 2;
      }
      output = argv[i];
    } else if (!strncmp(arg, "--output=", sizeof("--output=") - 1)) {
      output = arg + sizeof("--output=") - 1;
    } else if (arg[0] == '-' && arg[1]) {
      fprintf(stderr, "%s: unknown option '%s'\n", prog, arg);
      iw_test_usage(stderr, prog, suite);
      return 2;
    } else {
      value = arg;
    }
    if (value && *value) {
      if (npatterns == IW_TEST_MAX_PATTERNS) {
        fprintf(stderr, "%s: too many filter patterns (max %d)\n", prog, IW_TEST_MAX_PATTERNS);
        return 2;
      }
      patterns[npatterns++] = value;
    }
  }

  if (no_log) {
    /* --no-log wins over any --output option. */
    output = 0;
  }
  if (!output && !no_log) {
    const char *env = getenv("IW_TEST_NO_LOG");
    if (env && *env && strcmp(env, "0") != 0) {
      no_log = true;
    }
  }
  if (!output && !no_log) {
    const char *env = getenv("IW_TEST_OUTPUT");
    if (env && *env) {
      output = env;
    }
  }
  if (!output && !no_log) {
    /* By default the test code output is written next to the build objects. */
    snprintf(output_auto, sizeof(output_auto), "%s.log", prog);
    output = output_auto;
  }

  if (!npatterns) {
    const char *env = getenv("IW_TEST_FILTER");
    if (env && *env) {
      size_t sz = strlen(env);
      char *p = malloc(sz + 1);
      if (!p) {
        return 1;
      }
      memcpy(p, env, sz + 1);
      /* Patterns may be separated by commas, spaces or semicolons. */
      for (char *s = p; *s; ++s) {
        if (*s == ' ' || *s == '\t' || *s == ';') {
          *s = ',';
        }
      }
      filter_buf = p;
      patterns[npatterns++] = filter_buf;
    }
  }
  char filter[1024] = { 0 };
  for (int i = 0; i < npatterns; ++i) {
    if (i) {
      strncat(filter, ",", sizeof(filter) - strlen(filter) - 1);
    }
    strncat(filter, patterns[i], sizeof(filter) - strlen(filter) - 1);
  }
  const char *filtered = filter[0] ? filter : 0;

  struct iw_test_ui ui = { .utf8 = iw_test_utf8(), .color = iw_test_color(color) };

  /* Select the cases to run and compute the report layout. */
  unsigned int nsel = 0;
  size_t namew = 12;
  for (unsigned int i = 0; i < suite->ncases; ++i) {
    const char *name = suite->cases[i].name;
    if (filtered && !iw_test_match(filtered, name)) {
      continue;
    }
    ++nsel;
    size_t len = strlen(name);
    if (len > namew) {
      namew = len;
    }
  }
  if (namew > 46) {
    namew = 46;
  }
  unsigned int width = (unsigned int) namew + 29;
  if (width < 66) {
    width = 66;
  }

  if (list) {
    iw_test_print_header(&ui, suite, nsel, filtered, 0);
    for (unsigned int i = 0; i < suite->ncases; ++i) {
      const char *name = suite->cases[i].name;
      if (filtered && !iw_test_match(filtered, name)) {
        continue;
      }
      fprintf(iw_test_out, "  %s\n", name);
    }
    fputc('\n', iw_test_out);
    free(filter_buf);
    return 0;
  }

  if (!nsel) {
    if (!filtered) {
      fprintf(stderr, "%s: the suite '%s' has no test cases\n", prog, suite->name);
      free(filter_buf);
      return 1;
    }
    /* Nothing to do for this suite, keep the run output clean. */
    free(filter_buf);
    return 0;
  }

  /* Open the file receiving the stdout/stderr of the test code. */
  struct iw_test_redir redir = { .fd = -1, .saved_out = -1, .saved_err = -1 };
  if (output) {
#if defined(IW_TEST_HAVE_REDIRECT)
    redir.file = fopen(output, "w");
    if (!redir.file) {
      fprintf(stderr, "%s: unable to open the output file '%s': %s\n",
              prog, output, strerror(errno));
      output = 0;
    } else {
      redir.fd = fileno(redir.file);
    }
#else
    output = 0;
#endif
  }

  iw_test_print_header(&ui, suite, nsel, filtered, output);
  iw_test_hr(&ui, width);

  /* Suite initialization. */
  if (suite->init) {
    int rc = 0;
    iw_test_state.dumped = 0;
    atomic_store(&iw_test_state.assertions, 0);
    atomic_store(&iw_test_state.failures, 0);
    iw_test_case_thread = true;
    iw_test_redir_push(&redir);
    if (setjmp(iw_test_state.jmp) == 0) {
      iw_test_state.armed = true;
      rc = suite->init();
    } else {
      rc = -1;
    }
    iw_test_state.armed = false;
    iw_test_redir_pop(&redir);
    iw_test_case_thread = false;
    if (rc) {
      fputs("  ", iw_test_out);
      iw_test_glyph(&ui, 1);
      fprintf(iw_test_out, " %s%s suite initialization failed%s\n",
              iw_test_c(&ui, IW_TEST_TC_RED IW_TEST_TC_BOLD), suite->name,
              iw_test_c(&ui, IW_TEST_TC_RESET));
      iw_test_report_failures(&ui);
      if (suite->cleanup) {
        iw_test_redir_push(&redir);
        suite->cleanup();
        iw_test_redir_pop(&redir);
      }
      if (redir.file) {
        fclose(redir.file);
      }
      free(filter_buf);
      return 1;
    }
  }

  char failed_names[512] = { 0 };
  unsigned long failed_rest = 0;
  unsigned int failed_shown = 0;
  double t_start = iw_test_now();

  for (unsigned int i = 0; i < suite->ncases; ++i) {
    const struct iw_test_case *tc = &suite->cases[i];
    if (filtered && !iw_test_match(filtered, tc->name)) {
      if (verbose) {
        fputs("  ", iw_test_out);
        iw_test_glyph(&ui, -1);
        fprintf(iw_test_out, " %s%s%s\n", iw_test_c(&ui, IW_TEST_TC_DIM), tc->name,
                iw_test_c(&ui, IW_TEST_TC_RESET));
      }
      continue;
    }
    double t0 = iw_test_now();
    iw_test_redir_push(&redir);
    iw_test_exec(tc->fn);
    iw_test_redir_pop(&redir);
    double secs = iw_test_now() - t0;
    unsigned long assertions = atomic_load(&iw_test_state.assertions);
    unsigned long failures = atomic_load(&iw_test_state.failures);
    bool skipped = iw_test_state.skipped;

    iw_test_state.total_assertions += assertions;
    if (skipped) {
      ++iw_test_state.skipped_cases;
    } else if (failures) {
      ++iw_test_state.failed;
      if (failed_shown < 8) {
        if (failed_shown) {
          strncat(failed_names, ", ", sizeof(failed_names) - strlen(failed_names) - 1);
        }
        strncat(failed_names, tc->name, sizeof(failed_names) - strlen(failed_names) - 1);
        ++failed_shown;
      } else {
        ++failed_rest;
      }
    } else {
      ++iw_test_state.passed;
    }

    fputs("  ", iw_test_out);
    if (skipped) {
      iw_test_glyph(&ui, 2);
      fprintf(iw_test_out, " %-*s %s%7s%s %s%8.3fs%s\n",
              (int) namew, tc->name,
              iw_test_c(&ui, IW_TEST_TC_DIM), "skipped", iw_test_c(&ui, IW_TEST_TC_RESET),
              iw_test_c(&ui, IW_TEST_TC_DIM), secs, iw_test_c(&ui, IW_TEST_TC_RESET));
      fputs("      ", iw_test_out);
      fputs(iw_test_c(&ui, IW_TEST_TC_GREY), iw_test_out);
      fputs(iw_test_state.skip_reason, iw_test_out);
      fputs(iw_test_c(&ui, IW_TEST_TC_RESET), iw_test_out);
      fputc('\n', iw_test_out);
    } else {
      iw_test_glyph(&ui, failures ? 1 : 0);
      fprintf(iw_test_out, " %-*s %s%7lu assert(s)%s %s%8.3fs%s\n",
              (int) namew, tc->name,
              iw_test_c(&ui, IW_TEST_TC_DIM), assertions, iw_test_c(&ui, IW_TEST_TC_RESET),
              iw_test_c(&ui, IW_TEST_TC_DIM), secs, iw_test_c(&ui, IW_TEST_TC_RESET));
      if (failures) {
        iw_test_report_failures(&ui);
      }
      if (iw_test_state.aborted) {
        fputs("      ", iw_test_out);
        fputs(iw_test_c(&ui, IW_TEST_TC_RED), iw_test_out);
        fputs("aborted by a fatal assertion", iw_test_out);
        fputs(iw_test_c(&ui, IW_TEST_TC_RESET), iw_test_out);
        fputc('\n', iw_test_out);
      }
    }
    fflush(iw_test_out);
  }
  double total_secs = iw_test_now() - t_start;

  if (suite->cleanup) {
    iw_test_redir_push(&redir);
    suite->cleanup();
    iw_test_redir_pop(&redir);
  }
  if (redir.file) {
    fclose(redir.file);
    redir.file = 0;
  }

  /* Summary */
  iw_test_hr(&ui, width);
  unsigned long failed = iw_test_state.failed;
  const char *verdict = failed ? "FAILED" : "PASSED";
  const char *vcolor = failed ? IW_TEST_TC_RED : IW_TEST_TC_GREEN;
  char counters[256];
  int off = snprintf(counters, sizeof(counters), "%lu passed", iw_test_state.passed);
  if (failed) {
    off += snprintf(counters + off, sizeof(counters) - (size_t) off, ", %lu failed", failed);
  }
  if (iw_test_state.skipped_cases) {
    off += snprintf(counters + off, sizeof(counters) - (size_t) off, ", %lu skipped",
                    iw_test_state.skipped_cases);
  }
  snprintf(counters + off, sizeof(counters) - (size_t) off, ", %lu assertion(s), %.2fs",
           iw_test_state.total_assertions, total_secs);
  int pad = (int) width - 2 - (int) strlen(counters) - (int) strlen(verdict);
  if (pad < 2) {
    pad = 2;
  }
  fprintf(iw_test_out, "  %s%*s%s%s%s\n", counters, pad, "",
          iw_test_c(&ui, vcolor), verdict, iw_test_c(&ui, IW_TEST_TC_RESET));
  if (failed) {
    fprintf(iw_test_out, "  %sfailed:%s %s", iw_test_c(&ui, IW_TEST_TC_RED),
            iw_test_c(&ui, IW_TEST_TC_RESET), failed_names);
    if (failed_rest) {
      fprintf(iw_test_out, ", ... and %lu more", failed_rest);
    }
    fputc('\n', iw_test_out);
    if (output) {
      fprintf(iw_test_out, "  %soutput: %s%s\n", iw_test_c(&ui, IW_TEST_TC_DIM), output,
              iw_test_c(&ui, IW_TEST_TC_RESET));
    }
  }
  fputc('\n', iw_test_out);
  fflush(iw_test_out);

  free(filter_buf);
  return failed ? 1 : 0;
}

/* -------------------------------------------------------------------------- */
/*                                Assertions                                  */
/* -------------------------------------------------------------------------- */

/** Increments the assertion counter, used by the assertion macros. */
#define IW_TEST_TICK() (iw_test_state.assertions += 1)

/** Plain assertion, reports the failure and continues the case. */
#define IW_TEST_ASSERT(cond_)                                                  \
        do {                                                                   \
          IW_TEST_TICK();                                                      \
          if (!(cond_)) {                                                      \
            iw_test_fail(__FILE__, __LINE__, "assertion failed: `%s`",#cond_); \
          }                                                                    \
        } while (0)

/** Plain assertion, reports the failure and aborts the case. */
#define IW_TEST_ASSERT_FATAL(cond_)                                            \
        do {                                                                   \
          IW_TEST_TICK();                                                      \
          if (!(cond_)) {                                                      \
            iw_test_fail(__FILE__, __LINE__, "assertion failed: `%s`",#cond_); \
            iw_test_abort_case();                                              \
          }                                                                    \
        } while (0)

/** Reports the `actual != expected` mismatch of two values. */
#define IW_TEST_REPORT_MISMATCH(actual_, expected_)                          \
        switch (IW_TEST_KIND(iw_test_a_)) {                                  \
          case 1:                                                            \
            iw_test_fail_eq_unsigned(__FILE__, __LINE__,#actual_,#expected_, \
                                     (unsigned long long) iw_test_a_,        \
                                     (unsigned long long) iw_test_e_);       \
            break;                                                           \
          case 2:                                                            \
            iw_test_fail_eq_ptr(__FILE__, __LINE__,#actual_,#expected_,      \
                                (const void*) (uintptr_t) iw_test_a_,        \
                                (const void*) (uintptr_t) iw_test_e_);       \
            break;                                                           \
          default:                                                           \
            iw_test_fail_eq_signed(__FILE__, __LINE__,#actual_,#expected_,   \
                                   (long long) iw_test_a_,                   \
                                   (long long) iw_test_e_);                  \
            break;                                                           \
        }

/** Asserts `actual == expected`.
 *
 *  The operands are captured through `__typeof__`, so both operands must be
 *  scalars (integers, enums, pointers). Use ::IW_TEST_ASSERT_STR_EQUAL or
 *  ::IW_TEST_ASSERT_NSTR_EQUAL for strings and memory regions.
 */
#define IW_TEST_ASSERT_EQUAL(actual_, expected_)          \
        do {                                              \
          __typeof__(actual_) iw_test_a_ = (actual_);     \
          __typeof__(expected_) iw_test_e_ = (expected_); \
          bool iw_test_eq_;                               \
          IW_TEST_TICK();                                 \
          IW_TEST_DIAG_PUSH                               \
            iw_test_eq_ = iw_test_a_ == iw_test_e_;       \
          IW_TEST_DIAG_POP                                \
          if (!iw_test_eq_) {                             \
            IW_TEST_REPORT_MISMATCH(actual_, expected_);  \
          }                                               \
        } while (0)

/** Asserts `actual == expected`, aborts the case on mismatch. */
#define IW_TEST_ASSERT_EQUAL_FATAL(actual_, expected_)    \
        do {                                              \
          __typeof__(actual_) iw_test_a_ = (actual_);     \
          __typeof__(expected_) iw_test_e_ = (expected_); \
          bool iw_test_eq_;                               \
          IW_TEST_TICK();                                 \
          IW_TEST_DIAG_PUSH                               \
            iw_test_eq_ = iw_test_a_ == iw_test_e_;       \
          IW_TEST_DIAG_POP                                \
          if (!iw_test_eq_) {                             \
            IW_TEST_REPORT_MISMATCH(actual_, expected_);  \
            iw_test_abort_case();                         \
          }                                               \
        } while (0)

/** Asserts `actual != expected`. */
#define IW_TEST_ASSERT_NOT_EQUAL(actual_, expected_)                              \
        do {                                                                      \
          __typeof__(actual_) iw_test_a_ = (actual_);                             \
          __typeof__(expected_) iw_test_e_ = (expected_);                         \
          bool iw_test_eq_;                                                       \
          IW_TEST_TICK();                                                         \
          IW_TEST_DIAG_PUSH                                                       \
            iw_test_eq_ = iw_test_a_ == iw_test_e_;                               \
          IW_TEST_DIAG_POP                                                        \
          if (iw_test_eq_) {                                                      \
            iw_test_fail(__FILE__, __LINE__, "`%s` == `%s`",#actual_,#expected_); \
          }                                                                       \
        } while (0)

/** Asserts `actual != expected`, aborts the case on mismatch. */
#define IW_TEST_ASSERT_NOT_EQUAL_FATAL(actual_, expected_)                        \
        do {                                                                      \
          __typeof__(actual_) iw_test_a_ = (actual_);                             \
          __typeof__(expected_) iw_test_e_ = (expected_);                         \
          bool iw_test_eq_;                                                       \
          IW_TEST_TICK();                                                         \
          IW_TEST_DIAG_PUSH                                                       \
            iw_test_eq_ = iw_test_a_ == iw_test_e_;                               \
          IW_TEST_DIAG_POP                                                        \
          if (iw_test_eq_) {                                                      \
            iw_test_fail(__FILE__, __LINE__, "`%s` == `%s`",#actual_,#expected_); \
            iw_test_abort_case();                                                 \
          }                                                                       \
        } while (0)

/** Asserts the expression evaluates to true. */
#define IW_TEST_ASSERT_TRUE(cond_)                                       \
        do {                                                             \
          IW_TEST_TICK();                                                \
          if (!(cond_)) {                                                \
            iw_test_fail(__FILE__, __LINE__, "`%s` is not true",#cond_); \
          }                                                              \
        } while (0)

/** Asserts the expression evaluates to true, aborts the case otherwise. */
#define IW_TEST_ASSERT_TRUE_FATAL(cond_)                                 \
        do {                                                             \
          IW_TEST_TICK();                                                \
          if (!(cond_)) {                                                \
            iw_test_fail(__FILE__, __LINE__, "`%s` is not true",#cond_); \
            iw_test_abort_case();                                        \
          }                                                              \
        } while (0)

/** Asserts the expression evaluates to false. */
#define IW_TEST_ASSERT_FALSE(cond_)                                       \
        do {                                                              \
          IW_TEST_TICK();                                                 \
          if (cond_) {                                                    \
            iw_test_fail(__FILE__, __LINE__, "`%s` is not false",#cond_); \
          }                                                               \
        } while (0)

/** Asserts the expression evaluates to false, aborts the case otherwise. */
#define IW_TEST_ASSERT_FALSE_FATAL(cond_)                                 \
        do {                                                              \
          IW_TEST_TICK();                                                 \
          if (cond_) {                                                    \
            iw_test_fail(__FILE__, __LINE__, "`%s` is not false",#cond_); \
            iw_test_abort_case();                                         \
          }                                                               \
        } while (0)

/** Asserts the pointer is not `0`. */
#define IW_TEST_ASSERT_PTR_NOT_NULL(ptr_)                           \
        do {                                                        \
          const void *iw_test_p_ = (const void*) (ptr_);            \
          IW_TEST_TICK();                                           \
          if (!iw_test_p_) {                                        \
            iw_test_fail(__FILE__, __LINE__, "`%s` is NULL",#ptr_); \
          }                                                         \
        } while (0)

/** Asserts the pointer is not `0`, aborts the case otherwise. */
#define IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(ptr_)                     \
        do {                                                        \
          const void *iw_test_p_ = (const void*) (ptr_);            \
          IW_TEST_TICK();                                           \
          if (!iw_test_p_) {                                        \
            iw_test_fail(__FILE__, __LINE__, "`%s` is NULL",#ptr_); \
            iw_test_abort_case();                                   \
          }                                                         \
        } while (0)

/** Asserts the pointer is `0`. */
#define IW_TEST_ASSERT_PTR_NULL(ptr_)                                                   \
        do {                                                                            \
          const void *iw_test_p_ = (const void*) (ptr_);                                \
          IW_TEST_TICK();                                                               \
          if (iw_test_p_) {                                                             \
            iw_test_fail(__FILE__, __LINE__, "`%s` is not NULL: %p",#ptr_, iw_test_p_); \
          }                                                                             \
        } while (0)

/** Asserts the pointer is `0`, aborts the case otherwise. */
#define IW_TEST_ASSERT_PTR_NULL_FATAL(ptr_)                                             \
        do {                                                                            \
          const void *iw_test_p_ = (const void*) (ptr_);                                \
          IW_TEST_TICK();                                                               \
          if (iw_test_p_) {                                                             \
            iw_test_fail(__FILE__, __LINE__, "`%s` is not NULL: %p",#ptr_, iw_test_p_); \
            iw_test_abort_case();                                                       \
          }                                                                             \
        } while (0)

/** Asserts two C strings are equal. */
#define IW_TEST_ASSERT_STR_EQUAL(actual_, expected_)                                             \
        do {                                                                                     \
          const char *iw_test_a_ = (const char*) (actual_);                                      \
          const char *iw_test_e_ = (const char*) (expected_);                                    \
          IW_TEST_TICK();                                                                        \
          if (!iw_test_a_ || !iw_test_e_ || strcmp(iw_test_a_, iw_test_e_)) {                    \
            iw_test_fail(__FILE__, __LINE__, "`%s` != `%s`: \"%s\" vs \"%s\"",                   \
                         #actual_,#expected_, iw_test_str(iw_test_a_), iw_test_str(iw_test_e_)); \
          }                                                                                      \
        } while (0)

/** Asserts two C strings are equal, aborts the case otherwise. */
#define IW_TEST_ASSERT_STR_EQUAL_FATAL(actual_, expected_)                                       \
        do {                                                                                     \
          const char *iw_test_a_ = (const char*) (actual_);                                      \
          const char *iw_test_e_ = (const char*) (expected_);                                    \
          IW_TEST_TICK();                                                                        \
          if (!iw_test_a_ || !iw_test_e_ || strcmp(iw_test_a_, iw_test_e_)) {                    \
            iw_test_fail(__FILE__, __LINE__, "`%s` != `%s`: \"%s\" vs \"%s\"",                   \
                         #actual_,#expected_, iw_test_str(iw_test_a_), iw_test_str(iw_test_e_)); \
            iw_test_abort_case();                                                                \
          }                                                                                      \
        } while (0)

/** Asserts the first `len` bytes of two memory regions are equal. */
#define IW_TEST_ASSERT_NSTR_EQUAL(actual_, expected_, len_)                                      \
        do {                                                                                     \
          const char *iw_test_a_ = (const char*) (actual_);                                      \
          const char *iw_test_e_ = (const char*) (expected_);                                    \
          size_t iw_test_len_ = (size_t) (len_);                                                 \
          IW_TEST_TICK();                                                                        \
          if (!iw_test_a_ || !iw_test_e_ || memcmp(iw_test_a_, iw_test_e_, iw_test_len_) != 0) { \
            size_t iw_test_i_ = 0;                                                               \
            while (  iw_test_a_ && iw_test_e_ && iw_test_i_ < iw_test_len_                       \
                  && iw_test_a_[iw_test_i_] == iw_test_e_[iw_test_i_]) {                         \
              ++iw_test_i_;                                                                      \
            }                                                                                    \
            iw_test_fail(__FILE__, __LINE__, "`%s` != `%s` (%zu byte(s)): mismatch at %zu",      \
                         #actual_,#expected_, iw_test_len_, iw_test_i_);                         \
          }                                                                                      \
        } while (0)

/** Asserts two memory regions of `len` bytes are equal. */
#define IW_TEST_ASSERT_MEM_EQUAL(actual_, expected_, len_) \
        IW_TEST_ASSERT_NSTR_EQUAL(actual_, expected_, len_)

/** Asserts two doubles are equal within the given tolerance. */
#define IW_TEST_ASSERT_DOUBLE_EQUAL(actual_, expected_, tolerance_)                   \
        do {                                                                          \
          double iw_test_a_ = (double) (actual_);                                     \
          double iw_test_e_ = (double) (expected_);                                   \
          double iw_test_t_ = (double) (tolerance_);                                  \
          IW_TEST_TICK();                                                             \
          if (!(fabs(iw_test_a_ - iw_test_e_) <= fabs(iw_test_t_))) {                 \
            iw_test_fail(__FILE__, __LINE__, "`%s` != `%s` (tolerance %g): %g vs %g", \
                         #actual_,#expected_, iw_test_t_, iw_test_a_, iw_test_e_);    \
          }                                                                           \
        } while (0)

/** Records an unconditional failure of the current case. */
#define IW_TEST_FAIL(msg_) iw_test_fail(__FILE__, __LINE__, "%s", (msg_))

/** Records a formatted unconditional failure of the current case. */
#define IW_TEST_FAILF(...) iw_test_fail(__FILE__, __LINE__, __VA_ARGS__)

/** Marks the current case as skipped and terminates it. */
#define IW_TEST_SKIP(...) iw_test_skip(__FILE__, __LINE__, __VA_ARGS__)

/* -------------------------------------------------------------------------- */
/*                                  Helpers                                   */
/* -------------------------------------------------------------------------- */

/// Compares the raw content of two files, returns `0` if they are equal.
static int iw_test_cmp_files(FILE *f1, FILE *f2) {
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(f1);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(f2);
  fseek(f1, 0, SEEK_SET);
  fseek(f2, 0, SEEK_SET);
  int c1 = getc(f1);
  int c2 = getc(f2);
  int pos = 0, line = 1;
  while (c1 != EOF && c2 != EOF) {
    pos++;
    if ((c1 == '\n') && (c2 == '\n')) {
      line++;
      pos = 0;
    } else if (c1 != c2) {
      fprintf(stderr, "\nDiff at: %d:%d\n", line, pos);
      return c1 - c2;
    }
    c1 = getc(f1);
    c2 = getc(f2);
  }
  if (c1 - c2) {
    fprintf(stderr, "\nDiff at: %d:%d\n", line, pos);
  }
  return c1 - c2;
}

#endif /* IW_IWTESTS_H */
