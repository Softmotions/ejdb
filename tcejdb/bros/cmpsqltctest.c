/*************************************************************************************************
 * Comparison test of SQLite and Tokyo Cabinet
 *************************************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <sqlite3.h>
#include <tctdb.h>

#define SQLFILE    "casket.sql"
#define TCTFILE    "casket.tct"
#define SQLBUFSIZ   1024
#define SAMPLENUM   10
#define RECORDNUM   1000000


/* function prototypes */
int main(int argc, char **argv);
static void test_sqlite(void);
static void test_tctdb(void);
static int myrand(void);
static int callback(void *opq, int argc, char **argv, char **colname);


/* main routine */
int main(int argc, char **argv){
  test_sqlite();
  test_tctdb();
  return 0;
}


/* perform SQLite test */
static void test_sqlite(void){
  double sum = 0.0;
  for(int i = 1; i <= SAMPLENUM; i++){
    unlink(SQLFILE);
    sync(); sync();
    printf("SQLite write sample:%d ... ", i);
    fflush(stdout);
    double stime = tctime();
    sqlite3 *db;
    if(sqlite3_open(SQLFILE, &db) != 0) assert(!__LINE__);
    char sql[SQLBUFSIZ], *errmsg;
    sprintf(sql, "CREATE TABLE tbl ( k TEXT PRIMARY KEY, a TEXT, b INTEGER,"
            " c TEXT, d INTEGER );");
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    sprintf(sql, "CREATE INDEX tbl_s ON tbl ( a );");
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    sprintf(sql, "CREATE INDEX tbl_n ON tbl ( b );");
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    sprintf(sql, "PRAGMA cache_size = %d;", RECORDNUM);
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    sprintf(sql, "BEGIN TRANSACTION;");
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    for(int j = 1; j <= RECORDNUM; j++){
      sprintf(sql, "INSERT INTO tbl VALUES ( '%08d', '%08d', %d, '%08d', %d );",
            j, j, myrand() % RECORDNUM, myrand() % RECORDNUM, j);
      if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    }
    sprintf(sql, "END TRANSACTION;");
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    sqlite3_close(db);
    double etime = tctime() - stime;
    printf("%.6f\n", etime);
    sum += etime;
  }
  printf("SQLite write average: %.6f\n", sum / SAMPLENUM);
  sum = 0.0;
  for(int i = 1; i <= SAMPLENUM; i++){
    sync(); sync();
    printf("SQLite read sample:%d ... ", i);
    fflush(stdout);
    double stime = tctime();
    sqlite3 *db;
    if(sqlite3_open(SQLFILE, &db) != 0) assert(!__LINE__);
    char sql[SQLBUFSIZ], *errmsg;
    sprintf(sql, "PRAGMA cache_size = %d;", RECORDNUM);
    if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    for(int j = 1; j <= RECORDNUM; j++){
      sprintf(sql, "SELECT * FROM tbl WHERE a = '%08d';", myrand() % RECORDNUM + 1);
      if(sqlite3_exec(db, sql, callback, 0, &errmsg) != SQLITE_OK) assert(!__LINE__);
    }
    sqlite3_close(db);
    double etime = tctime() - stime;
    printf("%.6f\n", etime);
    sum += etime;
  }
  printf("SQLite read average: %.6f\n", sum / SAMPLENUM);
}


/* perform TCHDB test */
static void test_tctdb(void){
  double sum = 0.0;
  for(int i = 1; i <= SAMPLENUM; i++){
    unlink(TCTFILE);
    sync(); sync();
    printf("TCTDB write sample:%d ... ", i);
    fflush(stdout);
    double stime = tctime();
    TCTDB *tdb = tctdbnew();
    if(!tctdbtune(tdb, RECORDNUM * 2, -1, -1, 0)) assert(!__LINE__);
    if(!tctdbsetcache(tdb, 0, RECORDNUM, RECORDNUM)) assert(!__LINE__);
    if(!tctdbopen(tdb, TCTFILE, TDBOWRITER | TDBOCREAT | TDBOTRUNC)) assert(!__LINE__);
    if(!tctdbsetindex(tdb, "a", TDBITLEXICAL)) assert(!__LINE__);
    if(!tctdbsetindex(tdb, "b", TDBITDECIMAL)) assert(!__LINE__);
    if(!tctdbtranbegin(tdb)) assert(!__LINE__);
    char buf[SQLBUFSIZ];
    int size;
    for(int j = 1; j <= RECORDNUM; j++){
      TCMAP *cols = tcmapnew2(7);
      size = sprintf(buf, "%08d", j);
      tcmapput(cols, "a", 1, buf, size);
      size = sprintf(buf, "%d", myrand() % RECORDNUM);
      tcmapput(cols, "b", 1, buf, size);
      size = sprintf(buf, "%08d", myrand() % RECORDNUM);
      tcmapput(cols, "c", 1, buf, size);
      size = sprintf(buf, "%d", j);
      tcmapput(cols, "d", 1, buf, size);
      size = sprintf(buf, "%08d", j);
      if(!tctdbput(tdb, buf, size, cols)) assert(!__LINE__);
      tcmapdel(cols);
    }
    if(!tctdbtrancommit(tdb)) assert(!__LINE__);
    if(!tctdbclose(tdb)) assert(!__LINE__);
    tctdbdel(tdb);
    double etime = tctime() - stime;
    printf("%.6f\n", etime);
    sum += etime;
  }
  printf("TCTDB write average: %.6f\n", sum / SAMPLENUM);
  sum = 0.0;
  for(int i = 1; i <= SAMPLENUM; i++){
    sync(); sync();
    printf("TCTDB read sample:%d ... ", i);
    fflush(stdout);
    double stime = tctime();
    TCTDB *tdb = tctdbnew();
    if(!tctdbsetcache(tdb, 0, RECORDNUM, RECORDNUM)) assert(!__LINE__);
    if(!tctdbopen(tdb, TCTFILE, TDBOREADER)) assert(!__LINE__);
    char buf[SQLBUFSIZ];
    for(int j = 1; j <= RECORDNUM; j++){
      TDBQRY *qry = tctdbqrynew(tdb);
      sprintf(buf, "%08d", myrand() % RECORDNUM + 1);
      tctdbqryaddcond(qry, "a", TDBQCSTREQ, buf);
      TCLIST *res = tctdbqrysearch(qry);
      for(int k = 0; k < tclistnum(res); k++){
        int ksiz;
        const char *kbuf = tclistval(res, k, &ksiz);
        TCMAP *cols = tctdbget(tdb, kbuf, ksiz);
        if(!cols) assert(!__LINE__);
        tcmapdel(cols);
      }
      tclistdel(res);
      tctdbqrydel(qry);
    }
    if(!tctdbclose(tdb)) assert(!__LINE__);
    double etime = tctime() - stime;
    printf("%.6f\n", etime);
    sum += etime;
  }
  printf("TCTDB read average: %.6f\n", sum / SAMPLENUM);
}


/* generate a random number */
static int myrand(void){
  static int cnt = 0;
  return (lrand48() + cnt++) & 0x7FFFFFFF;
}


/* iterator of SQLite */
static int callback(void *opq, int argc, char **argv, char **colname){
  return 0;
}



/* END OF FILE */
