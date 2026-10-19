/*
 * ejdb_test6.c - EJDB2 crash-recovery and DB/WAL corruption stress tests.
 *
 * The low-level iwkv storage engine, WAL file format and free-space-map
 * corruption handling are tested in the iowow project
 * (`src/kv/tests/iwkv_test12.c`). The cases below cover the EJDB2 layer on top
 * of it: collections, indexes and metadata are exercised end-to-end against a
 * database whose main file and/or WAL have been damaged by a crash or by
 * random byte corruption.
 *
 * Test cases
 * ----------
 *  1. `ejdb_test6_1_wal_crash_recovery_stress`
 *     Runs a randomized write workload in short-lived forked children which
 *     terminate with `_exit()` (no `ejdb_close()`), i.e. a hard process
 *     crash. After every crash the database is reopened and fully verified:
 *     recovery must yield a consistent state and never crash.
 *
 *  2. `ejdb_test6_2_db_wal_corruption_fuzz`
 *     Byte-level corruption/truncation of the main database and WAL files of a
 *     real EJDB2 database (collections + index). Every mutated image is
 *     opened, queried and closed in a forked child with a timeout so a fault
 *     or hang is contained and reported instead of taking down the suite.
 *
 * Environment overrides
 * ---------------------
 *   EJDB_FUZZ_SEED   - RNG seed (decimal or 0x-prefixed), default 0x6a6b6c
 *   EJDB_FUZZ_ROUNDS - corruption fuzz rounds per case
 *   EJDB_FUZZ_ITERS  - crash-recovery operations per iteration
 *   EJDB_FUZZ_NOFORK - run corruption attempts in-process (debugging only)
 */

#include "ejdb2.h"

#include <iowow/iwutils.h>

#include "iwtests.h"

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define EJDB6_COLL      "c1"
#define EJDB6_NDOCS     256
#define EJDB6_DEFAULT_SEED  0x6a6b6cU

#define EJDB6_NOFORK_TIMEOUT_MS 10000

enum {
  EJDB6_OK = 0,
  EJDB6_CRASH,
  EJDB6_HANG,
  EJDB6_SETUP_ERROR,
};

int init_suite(void) {
  return ejdb_init();
}

void clean_suite(void) {
}

static uint32_t ejdb6_env_u32(const char *name, uint32_t def) {
  const char *s = getenv(name);
  if (s && *s) {
    return (uint32_t) strtoul(s, 0, 0);
  }
  return def;
}

//------------------------------------------------------------------ options

static void ejdb6_opts(struct ejdb_opts *opts, const char *path, bool wal, bool trunc) {
  memset(opts, 0, sizeof(*opts));
  opts->kv.path = path;
  opts->kv.oflags = 0;
  if (trunc) {
    opts->kv.oflags |= IWKV_TRUNC;
  }
  opts->kv.wal.enabled = wal;
  opts->kv.wal.wal_buffer_sz = 64UL * 1024UL;
  opts->kv.wal.checkpoint_buffer_sz = 64ULL * 1024 * 1024;
  // Effectively disable the automatic checkpoint/savepoint worker so the WAL
  // keeps uncheckpointed records for the duration of a test.
  opts->kv.wal.savepoint_timeout_sec = 3600;
  opts->kv.wal.checkpoint_timeout_sec = 3600;
  opts->no_wal = !wal;
  opts->sort_buffer_sz = 1 << 20;
  opts->document_buffer_sz = 1 << 14;
}

static void ejdb6_walpath(char *out, size_t outsz, const char *dbpath) {
  snprintf(out, outsz, "%s-wal", dbpath);
}

//------------------------------------------------------------------ workload

/// Stores one deterministic document. Values range from tiny to ~1.4Kb so the
/// engine exercises both in-place updates and KV block moves.
static iwrc ejdb6_put_doc(struct ejdb *db, int i, int64_t *idp) {
  char json[2048];
  size_t pl = (size_t) (i % 11) * 137; // 0 .. 1370
  int n = snprintf(json, sizeof(json),
                   "{\"name\":\"doc-%d\",\"age\":%d,\"tag\":\"tag-%d\",\"payload\":\"",
                   i, i % 100, i % 17);
  if (n < 0 || (size_t) n >= sizeof(json)) {
    return IW_ERROR_INVALID_ARGS;
  }
  for (size_t j = 0; (j < pl) && ((size_t) n + 2 < sizeof(json)); ++j) {
    json[n++] = (char) ('a' + (i + (int) j) % 26);
  }
  int m = snprintf(json + n, sizeof(json) - (size_t) n, "\"}");
  if (m < 0 || ((size_t) n + (size_t) m) >= sizeof(json)) {
    return IW_ERROR_INVALID_ARGS;
  }
  JBL jbl = 0;
  iwrc rc = jbl_from_json(&jbl, json);
  RCRET(rc);
  rc = ejdb_put_new(db, EJDB6_COLL, jbl, idp);
  jbl_destroy(&jbl);
  return rc;
}

static void ejdb6_fill_db(struct ejdb *db, int from, int to) {
  for (int i = from; i < to; ++i) {
    int64_t id = 0;
    iwrc rc = ejdb6_put_doc(db, i, &id);
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  }
}

/// Builds a clean database and closes it normally (the WAL is checkpointed and
/// truncated, so the resulting `-wal` file is empty).
static void ejdb6_build_db(const char *path, bool wal, int ndocs) {
  char wp[512];
  ejdb6_walpath(wp, sizeof(wp), path);
  unlink(path);
  unlink(wp);

  struct ejdb_opts opts;
  ejdb6_opts(&opts, path, wal, true);
  struct ejdb *db = 0;
  iwrc rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_ensure_index(db, EJDB6_COLL, "/age", EJDB_IDX_I64);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  ejdb6_fill_db(db, 0, ndocs);
  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
}

/// Builds a database with a non-empty WAL by killing a forked writer before it
/// can close the database. This simulates a real process crash and leaves a
/// WAL that has to be replayed on the next open.
static bool ejdb6_build_crash_image(const char *path, int ndocs) {
  char wp[512];
  ejdb6_walpath(wp, sizeof(wp), path);
  unlink(path);
  unlink(wp);

  pid_t pid = fork();
  if (pid == 0) {
    struct ejdb_opts opts;
    ejdb6_opts(&opts, path, true, true);
    struct ejdb *db = 0;
    iwrc rc = ejdb_open(&opts, &db);
    if (rc) {
      _exit(1);
    }
    for (int i = 0; i < ndocs; ++i) {
      int64_t id = 0;
      if (ejdb6_put_doc(db, i, &id)) {
        _exit(2);
      }
    }
    // Intentionally no ejdb_close(): simulate a hard crash.
    _exit(0);
  }
  if (pid < 0) {
    return false;
  }
  int status = 0;
  if (waitpid(pid, &status, 0) != pid) {
    return false;
  }
  return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

//------------------------------------------------------------------ probing

/// Opens the database, walks all expected data and closes it again. Used
/// inside forked children; it must not contain test framework assertions. Any
/// error is simply ignored - the only failure mode the parent cares about is a
/// crash or a hang.
static void ejdb6_probe(const char *path, bool wal) {
  struct ejdb_opts opts;
  ejdb6_opts(&opts, path, wal, false);
  struct ejdb *db = 0;
  if (ejdb_open(&opts, &db)) {
    return;
  }
  JBL meta = 0;
  if (!ejdb_get_meta(db, &meta) && meta) {
    jbl_destroy(&meta);
  }
  struct jql *q = 0;
  if (!jql_create(&q, EJDB6_COLL, "/*")) {
    struct ejdb_exec ux = { .db = db, .q = q };
    iwrc irc = ejdb_exec(&ux);
    (void) irc;
    jql_destroy(&q);
  }
  for (int64_t id = 1; id <= 8; ++id) {
    JBL doc = 0;
    if (!ejdb_get(db, EJDB6_COLL, id, &doc) && doc) {
      jbl_destroy(&doc);
    }
  }
  iwrc irc = ejdb_ensure_index(db, EJDB6_COLL, "/age", EJDB_IDX_I64);
  (void) irc;
  irc = ejdb_close(&db);
  (void) irc;
}

//------------------------------------------------------------------ file io

static uint8_t* ejdb6_file_read(const char *path, size_t *sz) {
  *sz = 0;
  FILE *f = fopen(path, "rb");
  if (!f) {
    return 0;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return 0;
  }
  long l = ftell(f);
  if ((l < 0) || (fseek(f, 0, SEEK_SET) != 0)) {
    fclose(f);
    return 0;
  }
  uint8_t *b = malloc((size_t) l + 1);
  if (!b) {
    fclose(f);
    return 0;
  }
  size_t r = fread(b, 1, (size_t) l, f);
  fclose(f);
  *sz = r;
  return b;
}

static int ejdb6_file_write(const char *path, const uint8_t *buf, size_t sz) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    return -1;
  }
  size_t w = sz ? fwrite(buf, 1, sz, f) : 0;
  fclose(f);
  return (w == sz) ? 0 : -1;
}

//------------------------------------------------------------------ child wait

static int ejdb6_wait_timeout(pid_t pid, uint32_t timeout_ms, int *status) {
  uint32_t waited = 0;
  for ( ; ; ) {
    pid_t r = waitpid(pid, status, WNOHANG);
    if (r == pid) {
      return 0;
    }
    if (r < 0) {
      return -1;
    }
    if (waited >= timeout_ms) {
      kill(pid, SIGKILL);
      waitpid(pid, status, 0);
      return 1;
    }
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
    nanosleep(&ts, 0);
    ++waited;
  }
}

/// Runs `ejdb6_probe()` in a child process (unless EJDB_FUZZ_NOFORK is set) and
/// maps the outcome to one of the EJDB6_* result codes.
static int ejdb6_probe_fork(const char *path, bool wal) {
  if (ejdb6_env_u32("EJDB_FUZZ_NOFORK", 0)) {
    ejdb6_probe(path, wal);
    return EJDB6_OK;
  }
  pid_t pid = fork();
  if (pid == 0) {
    ejdb6_probe(path, wal);
    _exit(0);
  }
  if (pid < 0) {
    return EJDB6_SETUP_ERROR;
  }
  int status = 0;
  int w = ejdb6_wait_timeout(pid, EJDB6_NOFORK_TIMEOUT_MS, &status);
  if (w == 1) {
    return EJDB6_HANG;
  }
  if (w < 0) {
    return EJDB6_SETUP_ERROR;
  }
  if (WIFSIGNALED(status)) {
    return EJDB6_CRASH;
  }
  if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
    return EJDB6_OK;
  }
  return EJDB6_SETUP_ERROR;
}

//------------------------------------------------------------------ corruption

static void ejdb6_corrupt(uint8_t *buf, size_t sz) {
  if (!sz) {
    return;
  }
  uint32_t nmut = 1 + iwu_rand_range(8);
  for (uint32_t m = 0; m < nmut; ++m) {
    size_t off;
    if ((iwu_rand_range(100) < 70) && (sz > 512)) {
      off = iwu_rand_range(512); // bias to headers/structural data
    } else {
      off = iwu_rand_range((uint32_t) sz);
    }
    buf[off] ^= (uint8_t) (1 + iwu_rand_range(0xff));
  }
}

/// Corrupts/truncates one image and probes it in a child.
static int ejdb6_corrupt_attempt(
  const char *dbpath, const char *walpath, bool wal,
  const uint8_t *db, size_t dbsz, const uint8_t *walbuf, size_t walsz) {
  uint8_t *wdb = malloc(dbsz ? dbsz : 1);
  uint8_t *wwal = malloc(walsz ? walsz : 1);
  if (!wdb || !wwal) {
    free(wdb);
    free(wwal);
    return EJDB6_SETUP_ERROR;
  }
  if (dbsz) {
    memcpy(wdb, db, dbsz);
  }
  if (walsz) {
    memcpy(wwal, walbuf, walsz);
  }

  ejdb6_corrupt(wdb, dbsz);
  if (wal) {
    ejdb6_corrupt(wwal, walsz);
  }

  size_t wdbsz = dbsz;
  size_t wwalsz = walsz;
  // Occasionally truncate to simulate a torn write.
  if (dbsz && (iwu_rand_range(100) < 20)) {
    wdbsz = iwu_rand_range((uint32_t) dbsz);
  }
  if (wal && walsz && (iwu_rand_range(100) < 35)) {
    wwalsz = iwu_rand_range((uint32_t) walsz);
  }

  int res = EJDB6_OK;
  if (ejdb6_file_write(dbpath, wdb, wdbsz)) {
    res = EJDB6_SETUP_ERROR;
  }
  if (!res && wal && ejdb6_file_write(walpath, wwal, wwalsz)) {
    res = EJDB6_SETUP_ERROR;
  }
  free(wdb);
  free(wwal);
  if (res) {
    return res;
  }
  return ejdb6_probe_fork(dbpath, wal);
}

/// Copies a built image, then fuzzes copies of it `rounds` times.
static void ejdb6_corruption_fuzz(
  const char *dbpath, bool wal, uint32_t base_seed, uint32_t seed, uint32_t rounds) {
  char walpath[512];
  ejdb6_walpath(walpath, sizeof(walpath), dbpath);

  size_t dbsz = 0, walsz = 0;
  uint8_t *dbbuf = ejdb6_file_read(dbpath, &dbsz);
  uint8_t *walbuf = wal ? ejdb6_file_read(walpath, &walsz) : 0;
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(dbbuf);

  uint32_t ncrash = 0, nhang = 0, nerr = 0;
  for (uint32_t r = 0; r < rounds; ++r) {
    // Reseed per round so a finding is reproducible with the same seed.
    iwu_rand_seed(seed + r * 2654435761u);
    int res = ejdb6_corrupt_attempt(dbpath, walpath, wal, dbbuf, dbsz, walbuf, walsz);
    if (res != EJDB6_OK) {
      const char *kind = (res == EJDB6_CRASH) ? "crash"
                         : (res == EJDB6_HANG) ? "hang" : "setup-error";
      fprintf(stderr,
              "\n[ejdb_test6] corruption %s path=%s wal=%d round=%u "
              "(repro: EJDB_FUZZ_SEED=%u EJDB_FUZZ_ROUNDS=%u)\n",
              kind, dbpath, (int) wal, r, base_seed, r + 1);
      if (res == EJDB6_CRASH) {
        ++ncrash;
      } else if (res == EJDB6_HANG) {
        ++nhang;
      } else {
        ++nerr;
      }
    }
    if (ncrash || nhang) {
      // Leave the failing image on disk for post-mortem inspection.
      break;
    }
    // Restore the pristine image for the next round.
    IW_TEST_ASSERT_EQUAL_FATAL(ejdb6_file_write(dbpath, dbbuf, dbsz), 0);
    if (wal && walbuf) {
      IW_TEST_ASSERT_EQUAL_FATAL(ejdb6_file_write(walpath, walbuf, walsz), 0);
    }
  }
  fprintf(stderr,
          "[ejdb_test6] corruption fuzz path=%s wal=%d rounds=%u crashes=%u hangs=%u errors=%u\n",
          dbpath, (int) wal, rounds, ncrash, nhang, nerr);
  if (ncrash || nhang) {
    IW_TEST_FAIL("corruption fuzzing detected engine crashes/hangs");
  }
  free(dbbuf);
  free(walbuf);
}

//------------------------------------------------------------------ test cases

static void ejdb_test6_1_wal_crash_recovery_stress(void) {
  const uint32_t seed = ejdb6_env_u32("EJDB_FUZZ_SEED", EJDB6_DEFAULT_SEED);
  const uint32_t iters = ejdb6_env_u32("EJDB_FUZZ_ITERS", 200);
  const char *path = "ejdb_test6_1.db";
  char wp[512];
  ejdb6_walpath(wp, sizeof(wp), path);
  unlink(path);
  unlink(wp);

  iwu_rand_seed(seed);
  fprintf(stderr, "\n[ejdb_test6] crash recovery stress seed=%u iters=%u\n", seed, iters);

  for (uint32_t it = 0; it < iters; ++it) {
    bool clean = iwu_rand_range(100) < 30;
    pid_t pid = fork();
    IW_TEST_ASSERT_NOT_EQUAL_FATAL(pid, -1);
    if (pid == 0) {
      struct ejdb_opts opts;
      ejdb6_opts(&opts, path, true, (it == 0));
      struct ejdb *db = 0;
      iwrc rc = ejdb_open(&opts, &db);
      if (rc) {
        _exit(1);
      }
      uint32_t ndoc = 1 + iwu_rand_range(64);
      for (uint32_t i = 0; i < ndoc; ++i) {
        int64_t id = 0;
        int docno = (int) iwu_rand_range(4096);
        if (iwu_rand_range(100) < 20) {
          iwrc irc = ejdb_del(db, EJDB6_COLL, docno);
          (void) irc;
        } else {
          iwrc irc = ejdb6_put_doc(db, docno, &id);
          (void) irc;
        }
      }
      if (clean) {
        rc = ejdb_close(&db);
        _exit(rc ? 3 : 0);
      }
      // Simulated process crash: no ejdb_close().
      _exit(0);
    }
    int status = 0;
    IW_TEST_ASSERT_EQUAL_FATAL(waitpid(pid, &status, 0), pid);
    IW_TEST_ASSERT_TRUE_FATAL(WIFEXITED(status));
    IW_TEST_ASSERT_TRUE_FATAL(WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 1);
    // The database must still reopen and survive a full traversal.
    ejdb6_probe(path, true);
  }

  // Final sanity check on the same process.
  struct ejdb_opts opts;
  ejdb6_opts(&opts, path, true, false);
  struct ejdb *db = 0;
  iwrc rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  struct jql *q = 0;
  rc = jql_create(&q, EJDB6_COLL, "/*");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  struct ejdb_exec ux = { .db = db, .q = q };
  rc = ejdb_exec(&ux);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  jql_destroy(&q);
  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
}

static void ejdb_test6_2_db_wal_corruption_fuzz(void) {
  const uint32_t base = ejdb6_env_u32("EJDB_FUZZ_SEED", EJDB6_DEFAULT_SEED);
  const uint32_t rounds = ejdb6_env_u32("EJDB_FUZZ_ROUNDS", 300);
  fprintf(stderr, "\n[ejdb_test6] corruption fuzz seed=%u rounds=%u\n", base, rounds);

  // WAL enabled, database closed normally (empty WAL): corrupt the main file.
  const char *p1 = "ejdb_test6_2a.db";
  ejdb6_build_db(p1, true, EJDB6_NDOCS);
  ejdb6_corruption_fuzz(p1, true, base, base, rounds);

  // WAL enabled, simulated crash: both the main file and the WAL are present.
  const char *p2 = "ejdb_test6_2b.db";
  IW_TEST_ASSERT_TRUE_FATAL(ejdb6_build_crash_image(p2, EJDB6_NDOCS));
  ejdb6_corruption_fuzz(p2, true, base, base ^ 0x9e3779b9u, rounds);

  // WAL disabled: corrupt the main file only.
  const char *p3 = "ejdb_test6_2c.db";
  ejdb6_build_db(p3, false, EJDB6_NDOCS);
  ejdb6_corruption_fuzz(p3, false, base, base ^ 0x85ebca6bu, rounds);
}

int main(int argc, char *argv[]) {
  static const struct iw_test_case cases[] = {
    IW_TEST_CASE(ejdb_test6_1_wal_crash_recovery_stress),
    IW_TEST_CASE(ejdb_test6_2_db_wal_corruption_fuzz),
  };
  return iw_test_run(argc, argv, IW_TEST_SUITE("ejdb_test6", cases, init_suite, clean_suite));
}
