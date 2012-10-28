/*************************************************************************************************
 * Writing test of Berkeley DB
 *************************************************************************************************/


#include <db.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#undef TRUE
#define TRUE           1                 /* boolean true */
#undef FALSE
#define FALSE          0                 /* boolean false */

#define RECBUFSIZ       32               /* buffer for records */
#define SMALL_PAGESIZE  512              /* small page size */
#define BIG_PAGESIZE    65536            /* maximum page size */
#define BIG_CACHESIZE   (rnum * 2 * 15)  /* maximum cache size < avail mem. */
#define SMALL_CACHESIZE (5 * 1048576)    /* should be Btree fanout */


/* global variables */
const char *progname;                    /* program name */
int showprgr;                            /* whether to show progression */


/* function prototypes */
int main(int argc, char **argv);
void usage(void);
int runwrite(int argc, char **argv);
int runread(int argc, char **argv);
int runbtwrite(int argc, char **argv);
int runbtread(int argc, char **argv);
int myrand(void);
int dowrite(char *name, int rnum);
int doread(char *name, int rnum);
int dobtwrite(char *name, int rnum, int rnd);
int dobtread(char *name, int rnum, int rnd);


/* main routine */
int main(int argc, char **argv){
  int rv;
  progname = argv[0];
  showprgr = TRUE;
  if(getenv("HIDEPRGR")) showprgr = FALSE;
  srand48(1978);
  if(argc < 2) usage();
  rv = 0;
  if(!strcmp(argv[1], "write")){
    rv = runwrite(argc, argv);
  } else if(!strcmp(argv[1], "read")){
    rv = runread(argc, argv);
  } else if(!strcmp(argv[1], "btwrite")){
    rv = runbtwrite(argc, argv);
  } else if(!strcmp(argv[1], "btread")){
    rv = runbtread(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
void usage(void){
  fprintf(stderr, "%s: test cases for Berkeley DB\n", progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write name rnum\n", progname);
  fprintf(stderr, "  %s read name rnum\n", progname);
  fprintf(stderr, "  %s btwrite [-rnd] name rnum\n", progname);
  fprintf(stderr, "  %s btread [-rnd] name rnum\n", progname);
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


/* parse arguments of btwrite command */
int runbtwrite(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rnd, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  rnd = FALSE;
  for(i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      if(!name && !strcmp(argv[i], "-rnd")){
        rnd = TRUE;
      } else {
        usage();
      }
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
  rv = dobtwrite(name, rnum, rnd);
  return rv;
}


/* parse arguments of btread command */
int runbtread(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rnd, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  rnd = FALSE;
  for(i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      if(!name && !strcmp(argv[i], "-rnd")){
        rnd = TRUE;
      } else {
        usage();
      }
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
  rv = dobtread(name, rnum, rnd);
  return rv;
}


/* pseudo random number generator */
int myrand(void){
  static int cnt = 0;
  return (lrand48() + cnt++) & 0x7FFFFFFF;
}


/* perform write command */
int dowrite(char *name, int rnum){
  DB *dbp;
  DBT key, data;
  int i, err, len;
  char buf[RECBUFSIZ];
  if(showprgr) printf("<Writing Test of Hash>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  if(db_create(&dbp, NULL, 0) != 0){
    fprintf(stderr, "db_create failed\n");
    return 1;
  }
  if(dbp->set_pagesize(dbp, SMALL_PAGESIZE) != 0){
    fprintf(stderr, "DB->set_pagesize failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  if(dbp->set_cachesize(dbp, 0, BIG_CACHESIZE, 0) != 0){
    fprintf(stderr, "DB->set_cachesize failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  if(dbp->open(dbp, NULL, name, NULL, DB_HASH, DB_CREATE | DB_TRUNCATE, 00644) != 0){
    fprintf(stderr, "DB->open failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  err = FALSE;
  memset(&key, 0, sizeof(key));
  memset(&data, 0, sizeof(data));
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", i);
    key.data = buf;
    key.size = len;
    data.data = buf;
    data.size = len;
    if(dbp->put(dbp, NULL, &key, &data, 0) != 0){
      fprintf(stderr, "DB->put failed\n");
      err = TRUE;
      break;
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
  if(dbp->close(dbp, 0) != 0){
    fprintf(stderr, "DB->close failed\n");
    err = TRUE;
  }
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform read command */
int doread(char *name, int rnum){
  DB *dbp;
  DBT key, data;
  int i, err, len;
  char buf[RECBUFSIZ];
  if(showprgr) printf("<Reading Test of Hash>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  if(db_create(&dbp, NULL, 0) != 0){
    fprintf(stderr, "db_create failed\n");
    return 1;
  }
  if(dbp->set_cachesize(dbp, 0, BIG_CACHESIZE, 0) != 0){
    fprintf(stderr, "DB->set_cachesize failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  if(dbp->open(dbp, NULL, name, NULL, DB_HASH, DB_RDONLY, 00644) != 0){
    fprintf(stderr, "DB->open failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  err = FALSE;
  memset(&key, 0, sizeof(key));
  memset(&data, 0, sizeof(data));
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", i);
    key.data = buf;
    key.size = len;
    if(dbp->get(dbp, NULL, &key, &data, 0) != 0){
      fprintf(stderr, "DB->get failed\n");
      err = TRUE;
      break;
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
  if(dbp->close(dbp, 0) != 0){
    fprintf(stderr, "DB->close failed\n");
    err = TRUE;
  }
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform btwrite command */
int dobtwrite(char *name, int rnum, int rnd){
  DB *dbp;
  DBT key, data;
  int i, err, len;
  char buf[RECBUFSIZ];
  if(showprgr) printf("<Writing Test of B+ Tree>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  if(db_create(&dbp, NULL, 0) != 0){
    fprintf(stderr, "db_create failed\n");
    return 1;
  }
  if(dbp->set_pagesize(dbp, BIG_PAGESIZE) != 0){
    fprintf(stderr, "DB->set_pagesize failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  if(dbp->set_cachesize(dbp, 0, rnd ? BIG_CACHESIZE : SMALL_CACHESIZE, 0) != 0){
    fprintf(stderr, "DB->set_cachesize failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  if(dbp->open(dbp, NULL, name, NULL, DB_BTREE, DB_CREATE | DB_TRUNCATE, 00644) != 0){
    fprintf(stderr, "DB->open failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  err = FALSE;
  memset(&key, 0, sizeof(key));
  memset(&data, 0, sizeof(data));
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", rnd ? myrand() % rnum + 1 : i);
    key.data = data.data = buf;
    key.size = data.size = len;
    if(dbp->put(dbp, NULL, &key, &data, 0) != 0){
      fprintf(stderr, "DB->put failed\n");
      err = TRUE;
      break;
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
  if(dbp->close(dbp, 0) != 0){
    fprintf(stderr, "DB->close failed\n");
    err = TRUE;
  }
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform btread command */
int dobtread(char *name, int rnum, int rnd){
  DB *dbp;
  DBT key, data;
  int i, err, len;
  char buf[RECBUFSIZ];
  if(showprgr) printf("<Reading Test of B+ Tree>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  if(db_create(&dbp, NULL, 0) != 0){
    fprintf(stderr, "db_create failed\n");
    return 1;
  }
  if(dbp->set_cachesize(dbp, 0, rnd ? BIG_CACHESIZE : SMALL_CACHESIZE, 0) != 0){
    fprintf(stderr, "DB->set_cachesize failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  if(dbp->open(dbp, NULL, name, NULL, DB_BTREE, DB_RDONLY, 00644) != 0){
    fprintf(stderr, "DB->open failed\n");
    dbp->close(dbp, 0);
    return 1;
  }
  err = FALSE;
  memset(&key, 0, sizeof(key));
  memset(&data, 0, sizeof(data));
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", rnd ? myrand() % rnum + 1 : i);
    key.data = buf;
    key.size = len;
    if(dbp->get(dbp, NULL, &key, &data, 0) != 0){
      fprintf(stderr, "DB->get failed\n");
      err = TRUE;
      break;
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
  if(dbp->close(dbp, 0) != 0){
    fprintf(stderr, "DB->close failed\n");
    err = TRUE;
  }
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}



/* END OF FILE */
