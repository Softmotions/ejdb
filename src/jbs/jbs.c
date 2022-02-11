#include "jbr.h"
#include "ejdb2cfg.h"

#include <iowow/iwpool.h>
#include <iowow/iwconv.h>
#include <iwnet/utils.h>

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

struct env {
  const char *program;
  EJDB      db;
  EJDB_OPTS opts;
  IWPOOL   *pool;
} env;

static int _usage(const char *err) {
  if (err) {
    fprintf(stderr, "\n%s\n", err);
  }
  fprintf(stderr, "\n\tEJDB " EJDB2_VERSION " HTTP REST/Websocket server. http://ejdb.org\n");
  fprintf(stderr, "\nUsage:\n\n\t %s [options]\n\n", env.program);
  fprintf(stderr, "\t-v, --version\t\tPrint program version.\n");
  fprintf(stderr, "\t-f, --file=<>\t\tDatabase file path. Default: ejdb2.db\n");
  fprintf(stderr, "\t-p, --port=NUM\t\tHTTP server port numer. Default: 9191\n");
  fprintf(stderr, "\t-l, --listen=<>\t\tNetwork address server will listen. Default: localhost\n");
  fprintf(stderr, "\t-k, --key=<>\t\tPEM private key file for TLS 1.2 HTTP server.\n");
  fprintf(stderr, "\t-c, --certs=<>\t\tPEM certificates file for TLS 1.2 HTTP server.\n");
  fprintf(stderr, "\t-a, --access=TOKEN|@FILE\t\tAccess token to match 'X-Access-Token' HTTP header value.\n");
  fprintf(stderr, "\t-r, --access-read\t\tAllows unrestricted read-only data access.\n");  
  fprintf(stderr, "\t-C, --cors\t\tEnable COSR response headers for HTTP server\n");
  fprintf(stderr, "\t-t, --trunc\t\tCleanup/reset database file on open.\n");
  fprintf(stderr, "\t-w, --wal\t\tuse the write ahead log (WAL). Used to provide data durability.\n");
  fprintf(stderr, "\nAdvanced options:\n");
  fprintf(stderr,
          "\t-S, --sbz=NUM\t\tMax sorting buffer size. If exceeded, an overflow temp file for data will be created."
          "Default: 16777216, min: 1048576\n");
  fprintf(stderr, "\t-D, --dsz=NUM\t\tInitial size of buffer to process/store document on queries."
          " Preferable average size of document. Default: 65536, min: 16384\n");
  fprintf(stderr, "\t-T, --trylock Exit with error if database is locked by another process."
          " If not set, current process will wait for lock release.");
  fprintf(stderr, "\n\n");
  return 1;
}

static void _on_signal(int signo) {
  if (env.db) {
    jbr_shutdown_request(env.db);
  }
}

static void _version(void) {
  fprintf(stdout, EJDB2_VERSION);
}

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGALRM, SIG_IGN);
  signal(SIGUSR1, SIG_IGN);
  signal(SIGUSR2, SIG_IGN);
  if (signal(SIGTERM, _on_signal) == SIG_ERR) {
    return EXIT_FAILURE;
  }
  if (signal(SIGINT, _on_signal) == SIG_ERR) {
    return EXIT_FAILURE;
  }

  IWPOOL *pool;
  int ec = 0, ch;

  env.program = argc ? argv[0] : "";
  env.opts.http.enabled = true;
  env.opts.http.blocking = true;
  env.opts.no_wal = true;

  iwrc rc = ejdb_init();
  if (rc) {
    iwlog_ecode_error3(rc);
    return EXIT_FAILURE;
  }

  RCA(pool = env.pool = iwpool_create_empty(), finish);

  static const struct option long_options[] = {
    { "help",        0, 0, 'h' },
    { "version",     0, 0, 'v' },
    { "file",        1, 0, 'f' },
    { "port",        1, 0, 'p' },
    { "bind",        1, 0, 'b' }, // for backward compatibility
    { "listen",      1, 0, 'l' },
    { "key",         1, 0, 'k' },
    { "certs",       1, 0, 'c' },
    { "access",      1, 0, 'a' },
    { "access-read", 0, 0, 'r' },
    { "cors",        0, 0, 'C' },
    { "trunc",       0, 0, 't' },
    { "wal",         0, 0, 'w' },
    { "sbz",         1, 0, 'S' },
    { "dsz",         1, 0, 'D' },
    { "trylock",     0, 0, 'T' }
  };

  while ((ch = getopt_long(argc, argv, "f:p:b:l:k:c:a:S:D:rCtwThv", long_options, 0)) != -1) {
    switch (ch) {
      case 'h':
        ec = _usage(0);
        goto finish;
      case 'v':
        _version();
        goto finish;
      case 'f':
        env.opts.kv.path = iwpool_strdup2(pool, optarg);
        break;
      case 'p':
        env.opts.http.port = iwatoi(optarg);
        break;
      case 'b':
      case 'l':
        env.opts.http.bind = iwpool_strdup2(pool, optarg);
        break;
      case 'k':
        env.opts.http.ssl_private_key = iwpool_strdup2(pool, optarg);
        break;
      case 'c':
        env.opts.http.ssl_certs = iwpool_strdup2(pool, optarg);
        break;
      case 'a':
        env.opts.http.access_token = iwpool_strdup2(pool, optarg);
        env.opts.http.access_token_len = env.opts.http.access_token ? strlen(env.opts.http.access_token) : 0;
        break;
      case 'C':
        env.opts.http.cors = true;
        break;
      case 't':
        env.opts.kv.oflags |= IWKV_TRUNC;
        break;
      case 'w':
        env.opts.no_wal = false;
        break;
      case 'S':
        env.opts.sort_buffer_sz = iwatoi(optarg);
        break;
      case 'D':
        env.opts.document_buffer_sz = iwatoi(optarg);
        break;
      case 'T':
        env.opts.kv.file_lock_fail_fast = true;
        break;
      case 'r':
        env.opts.http.read_anon = true;
        break;
      default:
        ec = _usage(0);
        goto finish;
    }
  }

  if (!env.opts.kv.path) {
    env.opts.kv.path = "ejdb2.db";
  }
  if (env.opts.http.port < 1) {
    env.opts.http.port = 9191;
  }
  if (env.opts.sort_buffer_sz < 1) {
    env.opts.sort_buffer_sz = 16777216;
  } else if (env.opts.sort_buffer_sz < 1048576) {
    env.opts.sort_buffer_sz = 1048576;
  }
  if (env.opts.document_buffer_sz < 1) {
    env.opts.document_buffer_sz = 65536;
  } else if (env.opts.document_buffer_sz < 16384) {
    env.opts.document_buffer_sz = 16384;
  }

  RCC(rc, finish, ejdb_open(&env.opts, &env.db));
  RCC(rc, finish, ejdb_close(&env.db));

finish:
  if (rc) {
    iwlog_ecode_error3(rc);
  }
  iwpool_destroy(env.pool);
  fflush(0);
  return rc == 0 ? ec : 1;
}
