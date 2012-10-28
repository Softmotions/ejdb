#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>

#undef TRUE
#define TRUE           1                 /* boolean true */
#undef FALSE
#define FALSE          0                 /* boolean false */

#define RECBUFSIZ      256               /* buffer for records */


/* global variables */
const char *progname;                    /* program name */
int showprgr;                            /* whether to show progression */


/* function prototypes */
int main(int argc, char **argv);
void usage(void);
int runwrite(int argc, char **argv);
int runread(int argc, char **argv);
int runtblwrite(int argc, char **argv);
int runtblread(int argc, char **argv);
int myrand(void);
int callback(void *opq, int argc, char **argv, char **colname);
int dowrite(char *name, int rnum);
int doread(char *name, int rnum);
int dotblwrite(char *name, int rnum);
int dotblread(char *name, int rnum);


/* main routine */
int main(int argc, char **argv){
  int rv;
  progname = argv[0];
  showprgr = TRUE;
  if(getenv("HIDEPRGR")) showprgr = FALSE;
  if(argc < 2) usage();
  rv = 0;
  if(!strcmp(argv[1], "write")){
    rv = runwrite(argc, argv);
  } else if(!strcmp(argv[1], "read")){
    rv = runread(argc, argv);
  } else if(!strcmp(argv[1], "tblwrite")){
    rv = runtblwrite(argc, argv);
  } else if(!strcmp(argv[1], "tblread")){
    rv = runtblread(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
void usage(void){
  fprintf(stderr, "%s: test cases for SQLite\n", progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write name rnum\n", progname);
  fprintf(stderr, "  %s read name rnum\n", progname);
  fprintf(stderr, "  %s tblwrite name rnum\n", progname);
  fprintf(stderr, "  %s tblread name rnum\n", progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* parse arguments of write command */
int runwrite(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  for(i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  rnum = atoi(rstr);
  if(rnum < 1) usage();
  rv = dowrite(name, rnum);
  return rv;
}


/* parse arguments of read command */
int runread(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  for(i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  rnum = atoi(rstr);
  if(rnum < 1) usage();
  rv = doread(name, rnum);
  return rv;
}


/* parse arguments of tblwrite command */
int runtblwrite(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  for(i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  rnum = atoi(rstr);
  if(rnum < 1) usage();
  rv = dotblwrite(name, rnum);
  return rv;
}


/* parse arguments of tblread command */
int runtblread(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  for(i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  rnum = atoi(rstr);
  if(rnum < 1) usage();
  rv = dotblread(name, rnum);
  return rv;
}


/* pseudo random number generator */
int myrand(void){
  static int cnt = 0;
  return (lrand48() + cnt++) & 0x7FFFFFFF;
}


/* call back function for select statement */
int callback(void *opq, int argc, char **argv, char **colname){
  return 0;
}


/* perform write command */
int dowrite(char *name, int rnum){
  sqlite3 *db;
  char sql[RECBUFSIZ], *errmsg;
  int i, err;
  if(showprgr) printf("<Writing Test of Hash>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  unlink(name);
  if(sqlite3_open(name, &db) != 0){
    fprintf(stderr, "sqlite3_open failed\n");
    return 1;
  }
  sprintf(sql, "CREATE TABLE tbl ( key TEXT PRIMARY KEY, val TEXT );");
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  sprintf(sql, "PRAGMA cache_size = %d;", rnum);
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  sprintf(sql, "BEGIN TRANSACTION;");
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    sprintf(sql, "INSERT INTO tbl VALUES ( '%08d', '%08d' );", i, i);
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
      fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
      sqlite3_free(errmsg);
      err = TRUE;
    }
    /* print progression */
    if(showprgr && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0){
        printf(" (%08d)\n", i);
        fflush(stdout);
      }
    }
  }
  /* close the database */
  sprintf(sql, "END TRANSACTION;");
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  sqlite3_close(db);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform read command */
int doread(char *name, int rnum){
  sqlite3 *db;
  char sql[RECBUFSIZ], *errmsg;
  int i, err;
  if(showprgr) printf("<Reading Test of Hash>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  if(sqlite3_open(name, &db) != 0){
    fprintf(stderr, "sqlite3_open failed\n");
    return 1;
  }
  sprintf(sql, "PRAGMA cache_size = %d;", rnum);
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    sprintf(sql, "SELECT * FROM tbl WHERE key = '%08d';", i);
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
      fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
      sqlite3_free(errmsg);
      err = TRUE;
    }
    /* print progression */
    if(showprgr && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0){
        printf(" (%08d)\n", i);
        fflush(stdout);
      }
    }
  }
  /* close the database */
  sqlite3_close(db);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform tblwrite command */
int dotblwrite(char *name, int rnum){
  sqlite3 *db;
  char sql[RECBUFSIZ], *errmsg;
  int i, err;
  if(showprgr) printf("<Writing Test of Table>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  unlink(name);
  if(sqlite3_open(name, &db) != 0){
    fprintf(stderr, "sqlite3_open failed\n");
    return 1;
  }
  sprintf(sql, "CREATE TABLE tbl ( key INTEGER PRIMARY KEY, s TEXT, n INTEGER,"
          " t TEXT, f TEXT );");
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  sprintf(sql, "CREATE INDEX tbl_s ON tbl ( s );");
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  sprintf(sql, "CREATE INDEX tbl_n ON tbl ( n );");
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  sprintf(sql, "PRAGMA cache_size = %d;", rnum);
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  sprintf(sql, "BEGIN TRANSACTION;");
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    sprintf(sql, "INSERT INTO tbl VALUES ( %d, '%08d', %d, '%08d', '%08d' );",
            i, i, myrand() % i, i, myrand() % rnum);
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
      fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
      sqlite3_free(errmsg);
    }
    /* print progression */
    if(showprgr && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0){
        printf(" (%08d)\n", i);
        fflush(stdout);
      }
    }
  }
  /* close the database */
  sprintf(sql, "END TRANSACTION;");
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  sqlite3_close(db);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform tblread command */
int dotblread(char *name, int rnum){
  sqlite3 *db;
  char sql[RECBUFSIZ], *errmsg;
  int i, err;
  if(showprgr) printf("<Reading Test of Table>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  if(sqlite3_open(name, &db) != 0){
    fprintf(stderr, "sqlite3_open failed\n");
    return 1;
  }
  sprintf(sql, "PRAGMA cache_size = %d;", rnum);
  if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
    fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
    sqlite3_free(errmsg);
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    sprintf(sql, "SELECT * FROM tbl WHERE key = '%08d';", i);
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK){
      fprintf(stderr, "sqlite3_exec failed: %s\n", errmsg);
      sqlite3_free(errmsg);
    }
    /* print progression */
    if(showprgr && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0){
        printf(" (%08d)\n", i);
        fflush(stdout);
      }
    }
  }
  /* close the database */
  sqlite3_close(db);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}



/* END OF FILE */
