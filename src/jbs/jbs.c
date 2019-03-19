#include "ejdb2.h"
#include "ejdb2cfg.h"
#include <fio_cli.h>

EJDB db;
EJDB_OPTS opts;

int main(int argc, char const *argv[]) {
  iwrc rc = 0;
  fio_cli_start(argc, argv, 0, 0,
                "EJDB " EJDB2_VERSION " standalone REST/Websockets server. http://ejdb.org\n",
                FIO_CLI_STRING("--file -f Database file path. Default: db.jb"),
                FIO_CLI_INT("--port -p HTTP port number to listen to. Default: 9191"),
                FIO_CLI_STRING("--bind -b Address server listen. Default: 0.0.0.0"),
                FIO_CLI_STRING("--access -a Server access token matched to X-Access-Token HTTP header value"),
                FIO_CLI_BOOL("--trunc -t Truncate existing database file on open"),
                FIO_CLI_BOOL("--wal -w Use write ahead logging (WAL)")
               );
  fio_cli_set_default("--file", "db.jb");
  fio_cli_set_default("-f", "db.jb");
  fio_cli_set_default("--port", "9191");
  fio_cli_set_default("-p", "9191");

  EJDB_OPTS ov = {
    .kv = {
      .path = fio_cli_get("-f"),
      .oflags = fio_cli_get_i("-t") ? IWKV_TRUNC : 0
    },
    .no_wal = !fio_cli_get_i("-w"),
    .http = {
      .enabled = true,
      .blocking = true,
      .port = fio_cli_get_i("-p"),
      .bind = fio_cli_get("-b"),
      .access_token = fio_cli_get("-a")
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
