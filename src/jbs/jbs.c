#include "ejdb2.h"
#include "ejdb2cfg.h"

#include <fio_cli.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

EJDB db;  // -V707
EJDB_OPTS opts;

int main(int argc, char const *argv[]) {
  iwrc rc = 0;
  fio_cli_start(argc, argv, 0, 0,
                "EJDB " EJDB2_VERSION " standalone HTTP REST/Websocket server. http://ejdb.org\n",
                FIO_CLI_STRING("--file -f Database file path. Default: ejdb2.db"),
                FIO_CLI_INT("--port -p HTTP port number listen to. Default: 9191"),
                FIO_CLI_STRING("--bind -b Address server listen. Default: localhost"),
                FIO_CLI_STRING("--access -a Server access token matched to 'X-Access-Token' HTTP header value.\n"
                               "If token prefixed by '@' it will be treated as file."),
                FIO_CLI_BOOL("--trunc -t Cleanup existing database file on open"),
                FIO_CLI_BOOL("--wal -w Use write ahead logging (WAL). Must be set for data durability."),
                FIO_CLI_BOOL("--cors Enable Cross-Origin Resource Sharing (CORS)."),
                FIO_CLI_PRINT_HEADER("Advanced options"),
                FIO_CLI_INT(
                  "--sbz Max sorting buffer size. If exceeded, an overflow temp file for data will be created. "
                  "Default: 16777216, min: 1048576"),
                FIO_CLI_INT("--dsz Initial size of buffer to process/store document on queries. "
                            "Preferable average size of document. "
                            "Default: 65536, min: 16384"),
                FIO_CLI_INT("--bsz Max HTTP/WS API document body size. " "Default: 67108864, min: 524288"),
                FIO_CLI_BOOL("--trylock Fail if database is locked by another process."
                             " If not set, current process will wait for lock release")

                );
  fio_cli_set_default("--file", "ejdb2.db");
  fio_cli_set_default("-f", "ejdb2.db");
  fio_cli_set_default("--port", "9191");
  fio_cli_set_default("-p", "9191");
  fio_cli_set_default("--sbz", "16777216");
  fio_cli_set_default("--dsz", "65536");
  fio_cli_set_default("--bsz", "67108864");

  char access_token_buf[255];
  const char *access_token = fio_cli_get("-a");
  if (access_token && (*access_token == '@')) {
    access_token = access_token + 1;
    FILE *f = fopen(access_token, "r");
    if (!f) {
      rc = iwrc_set_errno(IW_ERROR_IO_ERRNO, errno);
      goto finish;
    }
    size_t n = fread(access_token_buf, 1, sizeof(access_token_buf), f);
    if ((n == 0) || (n == sizeof(access_token_buf))) {
      fclose(f);
      rc = IW_ERROR_INVALID_VALUE;
      iwlog_error("Invalid access token from %s", access_token);
      goto finish;
    }
    access_token_buf[n] = '\0';
    for (int i = 0; i < n; ++i) {
      if (isspace(access_token_buf[i])) {
        access_token_buf[i] = '\0';
        break;
      }
    }
    fclose(f);
    access_token = access_token_buf;
  } else if (access_token && (strlen(access_token) >= sizeof(access_token_buf))) {
    rc = IW_ERROR_INVALID_VALUE;
    iwlog_error2("Invalid access token");
    goto finish;
  }

  EJDB_OPTS ov = {
    .kv                    = {
      .path                = fio_cli_get("-f"),
      .oflags              = fio_cli_get_i("-t") ? IWKV_TRUNC : 0,
      .file_lock_fail_fast = fio_cli_get_bool("--trylock")
    },
    .no_wal                = !fio_cli_get_i("-w"),
    .sort_buffer_sz        = fio_cli_get_i("--sbz"),
    .document_buffer_sz    = fio_cli_get_i("--dsz"),
    .http                  = {
      .enabled             = true,
      .blocking            = true,
      .port                = fio_cli_get_i("-p"),
      .bind                = fio_cli_get("-b"),
      .access_token        = access_token,
      .max_body_size       = fio_cli_get_i("--bsz"),
      .cors                = fio_cli_get_bool("--cors")
    }
  };
  memcpy(&opts, &ov, sizeof(ov));

  rc = ejdb_open(&opts, &db);
  RCGO(rc, finish);
  IWRC(ejdb_close(&db), rc);
finish:
  fio_cli_end();
  if (rc) {
    iwlog_ecode_error3(rc);
  }
  return rc ? 1 : 0;
}
