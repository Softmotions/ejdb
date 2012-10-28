/*************************************************************************************************
 * Writing test of Tokyo Cabinet
 *************************************************************************************************/


#include <tcutil.h>
#include <tchdb.h>
#include <tcbdb.h>
#include <tcfdb.h>
#include <tctdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#undef TRUE
#define TRUE           1                 /* boolean true */
#undef FALSE
#define FALSE          0                 /* boolean false */

#define RECBUFSIZ      32                /* buffer for records */


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
int runflwrite(int argc, char **argv);
int runflread(int argc, char **argv);
int runtblwrite(int argc, char **argv);
int runtblread(int argc, char **argv);
int myrand(void);
int dowrite(char *name, int rnum);
int doread(char *name, int rnum);
int dobtwrite(char *name, int rnum, int rnd);
int dobtread(char *name, int rnum, int rnd);
int doflwrite(char *name, int rnum);
int doflread(char *name, int rnum);
int dotblwrite(char *name, int rnum);
int dotblread(char *name, int rnum);


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
  } else if(!strcmp(argv[1], "flwrite")){
    rv = runflwrite(argc, argv);
  } else if(!strcmp(argv[1], "flread")){
    rv = runflread(argc, argv);
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
  fprintf(stderr, "%s: test cases for Tokyo Cabinet\n", progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write name rnum\n", progname);
  fprintf(stderr, "  %s read name rnum\n", progname);
  fprintf(stderr, "  %s btwrite [-rnd] name rnum\n", progname);
  fprintf(stderr, "  %s btread [-rnd] name rnum\n", progname);
  fprintf(stderr, "  %s flwrite name rnum\n", progname);
  fprintf(stderr, "  %s flread name rnum\n", progname);
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


/* parse arguments of flwrite command */
int runflwrite(int argc, char **argv){
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
  rv = doflwrite(name, rnum);
  return rv;
}


/* parse arguments of read command */
int runflread(int argc, char **argv){
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
  rv = doflread(name, rnum);
  return rv;
}


/* parse arguments of tblwrite command */
int runtblwrite(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rnd, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  rnd = FALSE;
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
  int i, rnum, rnd, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  rnd = FALSE;
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


/* perform write command */
int dowrite(char *name, int rnum){
  TCHDB *hdb;
  int i, err, len;
  char buf[RECBUFSIZ];
  if(showprgr) printf("<Writing Test of Hash>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  hdb = tchdbnew();
  tchdbtune(hdb, rnum * 3, 0, 0, 0);
  tchdbsetxmsiz(hdb, rnum * 48);
  if(!tchdbopen(hdb, name, HDBOWRITER | HDBOCREAT | HDBOTRUNC)){
    fprintf(stderr, "tchdbopen failed\n");
    tchdbdel(hdb);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", i);
    if(!tchdbputasync(hdb, buf, len, buf, len)){
      fprintf(stderr, "tchdbputasync failed\n");
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
  if(!tchdbclose(hdb)){
    fprintf(stderr, "tchdbclose failed\n");
    tchdbdel(hdb);
    return 1;
  }
  tchdbdel(hdb);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform read command */
int doread(char *name, int rnum){
  TCHDB *hdb;
  int i, err, len;
  char buf[RECBUFSIZ], vbuf[RECBUFSIZ];
  if(showprgr) printf("<Reading Test of Hash>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  hdb = tchdbnew();
  tchdbsetxmsiz(hdb, rnum * 48);
  if(!tchdbopen(hdb, name, HDBOREADER)){
    fprintf(stderr, "tchdbopen failed\n");
    tchdbdel(hdb);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", i);
    if(tchdbget3(hdb, buf, len, vbuf, RECBUFSIZ) == -1){
      fprintf(stderr, "tchdbget3 failed\n");
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
  if(!tchdbclose(hdb)){
    fprintf(stderr, "tchdbclose failed\n");
    tchdbdel(hdb);
    return 1;
  }
  tchdbdel(hdb);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform btwrite command */
int dobtwrite(char *name, int rnum, int rnd){
  TCBDB *bdb;
  int i, err, len;
  char buf[RECBUFSIZ];
  if(showprgr) printf("<Writing Test of B+ Tree>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  bdb = tcbdbnew();
  if(rnd){
    tcbdbtune(bdb, 77, 256, -1, 0, 0, 0);
    tcbdbsetcache(bdb, rnum / 77, -1);
  } else {
    tcbdbtune(bdb, 101, 256, -1, 0, 0, 0);
    tcbdbsetcache(bdb, 256, 256);
  }
  tcbdbsetxmsiz(bdb, rnum * 32);
  if(!tcbdbopen(bdb, name, BDBOWRITER | BDBOCREAT | BDBOTRUNC)){
    fprintf(stderr, "tcbdbopen failed\n");
    tcbdbdel(bdb);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", rnd ? myrand() % rnum + 1 : i);
    if(!tcbdbput(bdb, buf, len, buf, len)){
      fprintf(stderr, "tcbdbput failed\n");
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
  if(!tcbdbclose(bdb)){
    fprintf(stderr, "tcbdbclose failed\n");
    tcbdbdel(bdb);
    return 1;
  }
  tcbdbdel(bdb);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform btread command */
int dobtread(char *name, int rnum, int rnd){
  TCBDB *bdb;
  int i, err, len, vlen;
  const char *val;
  char buf[RECBUFSIZ];
  if(showprgr) printf("<Reading Test of B+ Tree>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  bdb = tcbdbnew();
  if(rnd){
    tcbdbsetcache(bdb, rnum / 77 + 1, -1);
  } else {
    tcbdbsetcache(bdb, 256, 256);
  }
  tcbdbsetxmsiz(bdb, rnum * 32);
  if(!tcbdbopen(bdb, name, BDBOREADER)){
    fprintf(stderr, "tcbdbopen failed\n");
    tcbdbdel(bdb);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", rnd ? myrand() % rnum + 1 : i);
    if(!(val = tcbdbget3(bdb, buf, len, &vlen))){
      fprintf(stderr, "tdbdbget3 failed\n");
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
  if(!tcbdbclose(bdb)){
    fprintf(stderr, "tcbdbclose failed\n");
    tcbdbdel(bdb);
    return 1;
  }
  tcbdbdel(bdb);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform flwrite command */
int doflwrite(char *name, int rnum){
  TCFDB *fdb;
  int i, err, len;
  char buf[RECBUFSIZ];
  if(showprgr) printf("<Writing Test of Fixed-Length>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  fdb = tcfdbnew();
  tcfdbtune(fdb, 8, 1024 + rnum * 9);
  if(!tcfdbopen(fdb, name, FDBOWRITER | FDBOCREAT | FDBOTRUNC)){
    fprintf(stderr, "tcfdbopen failed\n");
    tcfdbdel(fdb);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", i);
    if(!tcfdbput(fdb, i, buf, len)){
      fprintf(stderr, "tcfdbput failed\n");
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
  if(!tcfdbclose(fdb)){
    fprintf(stderr, "tcfdbclose failed\n");
    tcfdbdel(fdb);
    return 1;
  }
  tcfdbdel(fdb);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform flread command */
int doflread(char *name, int rnum){
  TCFDB *fdb;
  int i, err;
  char vbuf[RECBUFSIZ];
  if(showprgr) printf("<Reading Test of Fixed-length>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  fdb = tcfdbnew();
  if(!tcfdbopen(fdb, name, FDBOREADER)){
    fprintf(stderr, "tcfdbopen failed\n");
    tcfdbdel(fdb);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    if(tcfdbget4(fdb, i, vbuf, RECBUFSIZ) == -1){
      fprintf(stderr, "tcfdbget4 failed\n");
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
  if(!tcfdbclose(fdb)){
    fprintf(stderr, "tcfdbclose failed\n");
    tcfdbdel(fdb);
    return 1;
  }
  tcfdbdel(fdb);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform tblwrite command */
int dotblwrite(char *name, int rnum){
  TCTDB *tdb;
  int i, err, pksiz, vsiz;
  char pkbuf[RECBUFSIZ], vbuf[RECBUFSIZ];
  TCMAP *cols;
  if(showprgr) printf("<Writing Test of Table>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  tdb = tctdbnew();
  tctdbtune(tdb, rnum * 3, 0, 0, 0);
  tctdbsetxmsiz(tdb, rnum * 80);
  tctdbsetcache(tdb, -1, rnum / 100, -1);
  if(!tctdbopen(tdb, name, TDBOWRITER | TDBOCREAT | TDBOTRUNC)){
    fprintf(stderr, "tctdbopen failed\n");
    tctdbdel(tdb);
    return 1;
  }
  if(!tctdbsetindex(tdb, "s", TDBITLEXICAL)){
    fprintf(stderr, "tctdbsetindex failed\n");
    tctdbdel(tdb);
    return 1;
  }
  if(!tctdbsetindex(tdb, "n", TDBITDECIMAL)){
    fprintf(stderr, "tctdbsetindex failed\n");
    tctdbdel(tdb);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    pksiz = sprintf(pkbuf, "%d", i);
    cols = tcmapnew2(7);
    vsiz = sprintf(vbuf, "%08d", i);
    tcmapput(cols, "s", 1, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%08d", myrand() % i);
    tcmapput(cols, "n", 1, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%08d", i);
    tcmapput(cols, "t", 1, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%08d", myrand() % rnum);
    tcmapput(cols, "f", 1, vbuf, vsiz);
    if(!tctdbput(tdb, pkbuf, pksiz, cols)){
      fprintf(stderr, "tctdbput failed\n");
      err = TRUE;
      break;
    }
    tcmapdel(cols);
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
  if(!tctdbclose(tdb)){
    fprintf(stderr, "tctdbclose failed\n");
    tctdbdel(tdb);
    return 1;
  }
  tctdbdel(tdb);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform tblread command */
int dotblread(char *name, int rnum){
  TCTDB *tdb;
  int i, j, err, pksiz, rsiz;
  char pkbuf[RECBUFSIZ];
  const char *rbuf;
  TCMAP *cols;
  TDBQRY *qry;
  TCLIST *res;
  if(showprgr) printf("<Reading Test of Table>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  tdb = tctdbnew();
  tctdbsetxmsiz(tdb, rnum * 80);
  tctdbsetcache(tdb, -1, rnum / 100, -1);
  if(!tctdbopen(tdb, name, TDBOREADER)){
    fprintf(stderr, "tctdbopen failed\n");
    tctdbdel(tdb);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* search for a record */
    pksiz = sprintf(pkbuf, "%08d", i);
    qry = tctdbqrynew(tdb);
    tctdbqryaddcond(qry, "s", TDBQCSTREQ, pkbuf);
    res = tctdbqrysearch(qry);
    for(j = 0; j < tclistnum(res); j++){
      rbuf = tclistval(res, j, &rsiz);
      cols = tctdbget(tdb, rbuf, rsiz);
      if(cols){
        tcmapdel(cols);
      } else {
        fprintf(stderr, "tctdbget failed\n");
        err = TRUE;
        break;
      }
    }
    tclistdel(res);
    tctdbqrydel(qry);
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
  if(!tctdbclose(tdb)){
    fprintf(stderr, "tctdbclose failed\n");
    tctdbdel(tdb);
    return 1;
  }
  tctdbdel(tdb);
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}



/* END OF FILE */
