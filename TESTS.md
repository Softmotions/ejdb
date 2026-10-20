# `iwtests.h` a minimal C11 test harness

`iwtests.h` is a single header test harness for `C11` projects. It provides
assertions, a test case runner, case selection from the command line and a
readable console report. There is no dependency on any test framework, only the
`C11` standard library is used, and the header can be dropped into any project:

```sh
cp iwtests.h myproject/tests/iwtests.h
```

A test executable is a plain program: one executable = one test suite, each
suite is a list of `void` functions (test cases). The executable exits with `0`
when all the executed cases passed and with `1` otherwise, so it can be used
directly by any build system or CI pipeline.

## Quick start

```c
#include "iwtests.h"

static void test_addition(void) {
  IW_TEST_ASSERT_EQUAL(2 + 2, 4);
}

static void test_strings(void) {
  IW_TEST_ASSERT_STR_EQUAL("foo", "foo");
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(malloc(1));
}

int main(int argc, char *argv[]) {
  static const struct iw_test_case cases[] = {
    IW_TEST_CASE(test_addition),
    IW_TEST_CASE(test_strings),
  };
  return iw_test_run(argc, argv, IW_TEST_SUITE("my_test", cases, 0, 0));
}
```

```sh
cc -std=gnu11 -Wall -Wextra -o my_test my_test.c
./my_test
```

```
 iowow · my_test · 2 case(s) · output: my_test.log
 ────────────────────────────────────────────────────────────────
  ✓ test_addition      1 assert(s)    0.000s
  ✓ test_strings       2 assert(s)    0.000s
 ────────────────────────────────────────────────────────────────
  2 passed, 3 assertion(s), 0.00s                           PASSED
```

(The harness name in the report header is defined by the `IW_TEST_HARNESS`
macro, it defaults to `iowow`.)

## Writing a suite

A suite is described by a ::iw_test_suite value built with ::IW_TEST_SUITE:

```c
static int suite_init(void) {        /* optional, 0 when not needed */
  return db_open();                  /* non zero fails the whole suite */
}

static void suite_cleanup(void) {    /* optional, 0 when not needed */
  db_close();
}

static const struct iw_test_case cases[] = {
  IW_TEST_CASE(test_addition),                    /* case name == function name */
  IW_TEST_CASE_AS("strings", test_strings),       /* explicit case name */
};

int main(int argc, char *argv[]) {
  return iw_test_run(argc, argv, IW_TEST_SUITE("my_test", cases, suite_init, suite_cleanup));
}
```

* ::IW_TEST_CASE registers a case named after the function,
  ::IW_TEST_CASE_AS takes an explicit case name.
* `IW_TEST_SUITE(name, cases, init, cleanup)` names the suite and passes the
  optional initialization and cleanup hooks (`0` when not needed). It must be
  used in place as an argument of ::iw_test_run().
* ::iw_test_run() parses the command line, executes the selected cases and
  returns the process exit code.
* The suite initializer returns `0` on success, a non zero value (or a fatal
  assertion) fails the suite before any case is run. The cleanup hook is called
  after the last case, also when cases failed.
* Cases are executed in the declared order and share the process state, there
  is no per case setup/teardown.
* Assertions of a case that failed do not stop the remaining cases, the case
  itself is aborted only by a fatal assertion.

## Assertions

All assertion macros are statements: they record the failure and continue. The
`_FATAL` variants abort the current case (the remaining cases are still
executed), which is the right choice after a failed allocation: the following
code cannot work.

| Macro                                        | Meaning                                                       |
| -------------------------------------------- | ------------------------------------------------------------- |
| `IW_TEST_ASSERT(cond)`                       | `cond` is true.                                                |
| `IW_TEST_ASSERT_TRUE(cond)`                  | Same, with a better report message.                            |
| `IW_TEST_ASSERT_FALSE(cond)`                 | `cond` is false.                                               |
| `IW_TEST_ASSERT_EQUAL(actual, expected)`     | `actual == expected` for integers, enums and pointers.         |
| `IW_TEST_ASSERT_NOT_EQUAL(actual, expected)` | `actual != expected`.                                          |
| `IW_TEST_ASSERT_PTR_NULL(ptr)`               | `ptr == 0`.                                                    |
| `IW_TEST_ASSERT_PTR_NOT_NULL(ptr)`           | `ptr != 0`.                                                    |
| `IW_TEST_ASSERT_STR_EQUAL(a, b)`             | `strcmp(a, b) == 0`, zero pointers fail the assertion.          |
| `IW_TEST_ASSERT_NSTR_EQUAL(a, b, len)`       | The first `len` bytes are equal, reports the mismatch offset.   |
| `IW_TEST_ASSERT_MEM_EQUAL(a, b, len)`        | Alias of `IW_TEST_ASSERT_NSTR_EQUAL`.                          |
| `IW_TEST_ASSERT_DOUBLE_EQUAL(a, b, eps)`     | `fabs(a - b) <= fabs(eps)`.                                    |
| `IW_TEST_FAIL(msg)`                          | Records an unconditional failure with a message.                |
| `IW_TEST_FAILF(fmt, ...)`                    | The same, with a `printf` format.                               |
| `IW_TEST_SKIP(fmt, ...)`                     | Reports the case as skipped and terminates it.                  |

Everything but `IW_TEST_ASSERT_NSTR_EQUAL`, `IW_TEST_ASSERT_MEM_EQUAL`,
`IW_TEST_ASSERT_DOUBLE_EQUAL`, `IW_TEST_FAIL`, `IW_TEST_FAILF` and
`IW_TEST_SKIP` has a `_FATAL` variant, e.g.
`IW_TEST_ASSERT_EQUAL_FATAL(rc, 0)`.

Notes:

* `IW_TEST_ASSERT_EQUAL` takes the type of the operands from the operands
  themselves (`__typeof__`), so both a `size_t` and a 32 bit status code are
  reported exactly and each operand is evaluated once. Array and struct
  operands are not supported: use `IW_TEST_ASSERT_NSTR_EQUAL` /
  `IW_TEST_ASSERT_MEM_EQUAL` for byte buffers and
  `IW_TEST_ASSERT_DOUBLE_EQUAL` for floating point values.
* Fatal assertions are implemented with `setjmp()`/`longjmp()`, they may be
  used in functions returning a value as well as in nested helper functions. A
  fatal assertion raised from a thread other than the case thread cannot be
  unwound, such a failure aborts the whole process (this indicates a bug
  anyway). Non fatal assertions are thread safe and may be used by worker
  threads.
* Memory allocated before a fatal assertion is not released. Run the successful
  builds with a sanitizer to check the resource management of the test code.

## Selecting test cases

Cases are selected by a case-insensitive name substring, which is the most
important feature for a fast development loop: a full suite may take minutes,
while a fix is usually verified by one or two cases.

```sh
./my_test                           # run every case of the suite
./my_test addition                  # only the cases matching "addition"
./my_test -f addition,strings -v    # the same, and list the skipped cases
./my_test --list                    # show the selectable cases
./my_test --help                    # all options
```

The pattern list can be passed as positional arguments or with
`-f/--filter`, patterns are separated by commas. A case is run when it matches
at least one pattern. The `IW_TEST_FILTER` environment variable provides the
same filter, which is handy for a project-wide run driven by a build system:

```sh
IW_TEST_FILTER=addition,strings ./run_all_tests.sh
```

A suite without matching cases produces no output and exits with `0`, therefore
a filtered project-wide run reports only what has really been executed:

```
 iowow · my_test · 2 of 7 case(s) · "addition,strings" · output: my_test.log
```

### Arguments for the test code

Everything after a `--` argument is not parsed by the harness and is passed to
the test code through `iw_test_argc`/`iw_test_argv`, which makes it possible to
combine the case filter with the options of the program under test:

```sh
./my_test -f reopen -- --listen localhost:8080 -n 1000
```

```c
for (int i = 0; i < iw_test_argc; ++i) {
  printf("test argument: %s\n", iw_test_argv[i]);
}
```

`iw_test_argc` is `0` and `iw_test_argv` is `0` when there were no arguments
after `--`. The values are set by ::iw_test_run() before the suite is
initialized.

## Test code output

The `stdout`/`stderr` written by the test cases (and by the suite
initialization/cleanup hooks) is redirected into `<program name>.log` in the
current directory by default, so the report is never polluted by the debug
output of the test code. The report names the file:

```
 iowow · my_test · 2 of 7 case(s) · "addition,strings" · output: my_test.log
```

The redirection is implemented with `dup(2)`/`dup2(2)` and is available on the
`POSIX` systems (Linux, FreeBSD, NetBSD, macOS) only; elsewhere the test output
is not redirected and stays on the console.

## Options and environment

| Option                   | Meaning                                                           |
| ------------------------ | ----------------------------------------------------------------- |
| `-f, --filter PATTERNS`  | Comma separated case name patterns, case-insensitive substring.    |
| `<pattern> ...`          | Positional patterns, equivalent to `--filter`.                     |
| `-o, --output FILE`      | Redirect the test code output into `FILE`.                        |
| `--no-log`               | Do not redirect the test code output.                             |
| `-l, --list`             | List the cases matching the filter, do not run them.              |
| `-v, --verbose`          | Show the cases skipped by the filter.                             |
| `--color`/`--no-color`   | Force or disable the colored output.                              |
| `-- [ARG ...]`           | Stop option parsing, `ARG`s are passed to the test code.          |
| `-h, --help`             | Usage help.                                                       |

| Environment variable | Meaning                                                          |
| -------------------- | ---------------------------------------------------------------- |
| `IW_TEST_FILTER`     | Same as `--filter`, ignored when patterns are given in the CLI.  |
| `IW_TEST_OUTPUT`     | Same as `--output`.                                              |
| `IW_TEST_NO_LOG`     | Same as `--no-log`, the test output is not redirected.           |
| `AUTARK_INTERACTIVE` | Force colors.                                                    |
| `IW_TEST_COLOR`      | Force colors, `IW_TEST_COLOR=0` disables them.                   |
| `IW_TEST_NO_COLOR`   | Disable colors.                                                  |
| `IW_TEST_ASCII`      | Disable the unicode decorations.                                 |
| `IW_TEST_HARNESS`    | Compile time: the name shown in the report header.               |

Exit codes: `0` - all executed cases passed (or nothing matched), `1` - at
least one case failed (or a fatal assertion was raised outside of a test case),
`2` - invalid command line.

## Console report

```
 iowow · my_test · 4 case(s) · output: my_test.log
 ────────────────────────────────────────────────────────────────────
  ✓ test_addition      1 assert(s)    0.000s
  ✗ test_database     12 assert(s)    0.049s
      my_test.c:41: `rc` != `0`: 458757 (0x70005) vs 0
      my_test.c:48: `val.size` != `8`: 0 vs 8
  • test_fuzz          skipped       0.000s
      my_test.c:63: set MY_FUZZ_ROUNDS to enable the fuzzer
  ✓ test_strings       2 assert(s)    0.000s
 ────────────────────────────────────────────────────────────────────
  2 passed, 1 failed, 1 skipped, 15 assertion(s), 0.06s       FAILED
  failed: test_database
  output: my_test.log
```

* `✓` - the case passed, `✗` - the case failed, `•` - the case was skipped.
* The report goes into the real `stdout`, the `stdout`/`stderr` of the test
  code goes into the log file named in the report header.
* Every failed assertion is reported with `file:line` and the compared values.
  Integer values greater than `255` are shown in the decimal and in the
  hexadecimal form (error codes are much easier to read that way).
* At most `IW_TEST_MAX_FAILURES` (10) failure details are retained per case,
  the remaining ones are only counted.
* Colors are used only when the report is written to a terminal (or when they
  are forced by `AUTARK_INTERACTIVE` or `IW_TEST_COLOR`), the unicode decorations are used when the locale is `UTF-8`.

  ```sh
  IW_TEST_NO_COLOR=1 IOWOW_RUN_TESTS=1 BUILD_TYPE=Debug ./build.sh
  ```
* A suite that fails to initialize is reported as a failed suite.
* The process exit code is `1` if at least one case failed.

## Standalone use

`iw_test_run()` is not required to use the assertions: the assertion macros and
their state are available as soon as `iwtests.h` is included. This is useful
for a test fixture which is not a suite of cases, for example a server serving
the requests of an external test script:

```c
#include "iwtests.h"

int main(int argc, char *argv[]) {
  serve_requests();                              /* assertions are used inside */
  return iw_test_failures() ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

* `iw_test_failures()` returns the number of failed assertions recorded so far,
  which is the number of failures of the whole program when no suite is run.
* A fatal assertion (`*_FATAL`) raised outside of a test case cannot be
  unwound: it flushes the output and terminates the process with the exit code
  `1`. `IW_TEST_SKIP()` used outside of a case does the same, so it makes sense
  only inside a suite.
* Assertions only record their failures, the details are printed by the
  console report of ::iw_test_run(). A standalone program has no report, so
  `iw_test_failures()` is the only way to detect the failures and it should be
  used as the exit condition.

## Helpers

| Helper                      | Purpose                                                    |
| --------------------------- | ----------------------------------------------------------- |
| `iw_test_cmp_files(f1, f2)` | Compares the content of two `FILE*`, returns `0` if equal.  |
| `iw_test_failures()`        | Number of failed assertions recorded so far.                |
| `iw_test_argc`, `iw_test_argv` | Arguments given after `--`, see above.                   |

`iw_test_cmp_files()` is handy for the "dump the structure and compare it with
the reference file" style tests:

```c
FILE *f = fopen("mytest.log", "w+");
db_dump(f, db);
FILE *ref = fopen("mytest.ref", "r+");
IW_TEST_ASSERT_EQUAL_FATAL(iw_test_cmp_files(ref, f), 0);
```

## Using the harness in the IOWOW project

The IOWOW test cases live in `src/<module>/tests/`, one executable per suite,
and are built and run by Autark:

```sh
IOWOW_RUN_TESTS=1 ./build.sh -c

# Only the cases whose name contains "reopen" or "walfuzz"
IW_TEST_FILTER=reopen,walfuzz IOWOW_RUN_TESTS=1 ./build.sh -c
```

A new test executable is listed in the `src/<module>/tests/Autark` file. The
binaries and their logs are placed into `autark-cache/src/<module>/tests/`.
See the [README](README.md#tests) for the project specific details.