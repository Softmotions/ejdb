/*************************************************************************************************
 * The test cases of the table database API
 *                                                               Copyright (C) 2006-2012 FAL Labs
 * This file is part of Tokyo Cabinet.
 * Tokyo Cabinet is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Cabinet is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Cabinet; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#include <tcutil.h>
#include <tctdb.h>
#include "myconf.h"

#define RECBUFSIZ      48                // buffer for records

typedef struct {                         // type of structure for write thread
  TCTDB *tdb;
  int rnum;
  bool rnd;
  int id;
} TARGWRITE;

typedef struct {                         // type of structure for read thread
  TCTDB *tdb;
  int rnum;
  bool rnd;
  int id;
} TARGREAD;

typedef struct {                         // type of structure for remove thread
  TCTDB *tdb;
  int rnum;
  bool rnd;
  int id;
} TARGREMOVE;

typedef struct {                         // type of structure for wicked thread
  TCTDB *tdb;
  int rnum;
  int id;
} TARGWICKED;

typedef struct {                         // type of structure for typical thread
  TCTDB *tdb;
  int rnum;
  int rratio;
  int id;
} TARGTYPICAL;


/* global variables */
const char *g_progname;                  // program name
unsigned int g_randseed;                 // random seed
int g_dbgfd;                             // debugging output


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void iputchar(int c);
static void eprint(TCTDB *tdb, int line, const char *func);
static void sysprint(void);
static int myrand(int range);
static int myrandnd(int range);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int runtypical(int argc, char **argv);
static int procwrite(const char *path, int tnum, int rnum, int bnum, int apow, int fpow,
                     int opts, int rcnum, int lcnum, int ncnum, int xmsiz, int dfunit,
                     int iflags, int omode, bool rnd);
static int procread(const char *path, int tnum, int rcnum, int lcnum, int ncnum,
                    int xmsiz, int dfunit, int omode, bool rnd);
static int procremove(const char *path, int tnum, int rcnum, int lcnum, int ncnum,
                      int xmsiz, int dfunit, int omode, bool rnd);
static int procwicked(const char *path, int tnum, int rnum, int opts, int omode);
static int proctypical(const char *path, int tnum, int rnum, int bnum, int apow, int fpow,
                       int opts, int rcnum, int lcnum, int ncnum, int xmsiz, int dfunit,
                       int omode, int rratio);
static void *threadwrite(void *targ);
static void *threadread(void *targ);
static void *threadremove(void *targ);
static void *threadwicked(void *targ);
static void *threadtypical(void *targ);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  const char *ebuf = getenv("TCRNDSEED");
  g_randseed = ebuf ? tcatoix(ebuf) : tctime() * 1000;
  srand(g_randseed);
  ebuf = getenv("TCDBGFD");
  g_dbgfd = ebuf ? tcatoix(ebuf) : UINT16_MAX;
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "write")){
    rv = runwrite(argc, argv);
  } else if(!strcmp(argv[1], "read")){
    rv = runread(argc, argv);
  } else if(!strcmp(argv[1], "remove")){
    rv = runremove(argc, argv);
  } else if(!strcmp(argv[1], "wicked")){
    rv = runwicked(argc, argv);
  } else if(!strcmp(argv[1], "typical")){
    rv = runtypical(argc, argv);
  } else {
    usage();
  }
  if(rv != 0){
    printf("FAILED: TCRNDSEED=%u PID=%d", g_randseed, (int)getpid());
    for(int i = 0; i < argc; i++){
      printf(" %s", argv[i]);
    }
    printf("\n\n");
  }
  return rv;
}


/* print the usage and exit */
static void usage(void){
  fprintf(stderr, "%s: test cases of the table database API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write [-tl] [-td|-tb|-tt|-tx] [-rc num] [-lc num] [-nc num]"
          " [-xm num] [-df num] [-ip] [-is] [-in] [-it] [-if] [-ix] [-nl|-nb] [-rnd]"
          " path tnum rnum [bnum [apow [fpow]]]\n", g_progname);
  fprintf(stderr, "  %s read [-rc num] [-lc num] [-nc num] [-xm num] [-df num] [-nl|-nb] [-rnd]"
          " path tnum\n", g_progname);
  fprintf(stderr, "  %s remove [-rc num] [-lc num] [-nc num] [-xm num] [-df num]"
          " [-nl|-nb] [-rnd] path tnum\n", g_progname);
  fprintf(stderr, "  %s wicked [-tl] [-td|-tb|-tt|-tx] [-nl|-nb] path tnum rnum\n", g_progname);
  fprintf(stderr, "  %s typical [-tl] [-td|-tb|-tt|-tx] [-rc num] [-lc num] [-nc num]"
          " [-xm num] [-df num] [-nl|-nb] [-rr num] path tnum rnum [bnum [apow [fpow]]]\n",
          g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print formatted information string and flush the buffer */
static void iprintf(const char *format, ...){
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  fflush(stdout);
  va_end(ap);
}


/* print a character and flush the buffer */
static void iputchar(int c){
  putchar(c);
  fflush(stdout);
}


/* print error message of table database */
static void eprint(TCTDB *tdb, int line, const char *func){
  const char *path = tctdbpath(tdb);
  int ecode = tctdbecode(tdb);
  fprintf(stderr, "%s: %s: %d: %s: error: %d: %s\n",
          g_progname, path ? path : "-", line, func, ecode, tctdberrmsg(ecode));
}


/* print system information */
static void sysprint(void){
  TCMAP *info = tcsysinfo();
  if(info){
    tcmapiterinit(info);
    const char *kbuf;
    while((kbuf = tcmapiternext2(info)) != NULL){
      iprintf("sys_%s: %s\n", kbuf, tcmapiterval2(kbuf));
    }
    tcmapdel(info);
  }
}


/* get a random number */
static int myrand(int range){
  if(range < 2) return 0;
  int high = (unsigned int)rand() >> 4;
  int low = range * (rand() / (RAND_MAX + 1.0));
  low &= (unsigned int)INT_MAX >> 4;
  return (high + low) % range;
}


/* get a random number based on normal distribution */
static int myrandnd(int range){
  int num = (int)tcdrandnd(range >> 1, range / 10);
  return (num < 0 || num >= range) ? 0 : num;
}


/* parse arguments of write command */
static int runwrite(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  int opts = 0;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int iflags = 0;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-ip")){
        iflags |= 1 << 0;
      } else if(!strcmp(argv[i], "-is")){
        iflags |= 1 << 1;
      } else if(!strcmp(argv[i], "-in")){
        iflags |= 1 << 2;
      } else if(!strcmp(argv[i], "-it")){
        iflags |= 1 << 3;
      } else if(!strcmp(argv[i], "-if")){
        iflags |= 1 << 4;
      } else if(!strcmp(argv[i], "-ix")){
        iflags |= 1 << 5;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else if(!astr){
      astr = argv[i];
    } else if(!fstr){
      fstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !tstr || !rstr) usage();
  int tnum = tcatoix(tstr);
  int rnum = tcatoix(rstr);
  if(tnum < 1 || rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procwrite(path, tnum, rnum, bnum, apow, fpow, opts, rcnum, lcnum, ncnum,
                     xmsiz, dfunit, iflags, omode, rnd);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !tstr) usage();
  int tnum = tcatoix(tstr);
  if(tnum < 1) usage();
  int rv = procread(path, tnum, rcnum, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !tstr) usage();
  int tnum = tcatoix(tstr);
  if(tnum < 1) usage();
  int rv = procremove(path, tnum, rcnum, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  int opts = 0;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !tstr || !rstr) usage();
  int tnum = tcatoix(tstr);
  int rnum = tcatoix(rstr);
  if(tnum < 1 || rnum < 1) usage();
  int rv = procwicked(path, tnum, rnum, opts, omode);
  return rv;
}


/* parse arguments of typical command */
static int runtypical(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  int opts = 0;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  int rratio = -1;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-rr")){
        if(++i >= argc) usage();
        rratio = tcatoix(argv[i]);
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else if(!astr){
      astr = argv[i];
    } else if(!fstr){
      fstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !tstr || !rstr) usage();
  int tnum = tcatoix(tstr);
  int rnum = tcatoix(rstr);
  if(tnum < 1 || rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = proctypical(path, tnum, rnum, bnum, apow, fpow, opts, rcnum, lcnum, ncnum,
                       xmsiz, dfunit, omode, rratio);
  return rv;
}


/* perform write command */
static int procwrite(const char *path, int tnum, int rnum, int bnum, int apow, int fpow,
                     int opts, int rcnum, int lcnum, int ncnum, int xmsiz, int dfunit,
                     int iflags, int omode, bool rnd){
  iprintf("<Writing Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  bnum=%d  apow=%d  fpow=%d"
          "  opts=%d  rcnum=%d  lcnum=%d  ncnum=%d  xmsiz=%d  dfunit=%d  iflags=%d"
          "  omode=%d  rnd=%d\n\n",
          g_randseed, path, tnum, rnum, bnum, apow, fpow, opts, rcnum, lcnum, ncnum,
          xmsiz, dfunit, iflags, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetmutex(tdb)){
    eprint(tdb, __LINE__, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, __LINE__, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbtune(tdb, bnum, apow, fpow, opts)){
    eprint(tdb, __LINE__, "tctdbtune");
    err = true;
  }
  if(!tctdbsetcache(tdb, rcnum, lcnum, ncnum)){
    eprint(tdb, __LINE__, "tctdbsetcache");
    err = true;
  }
  if(xmsiz >= 0 && !tctdbsetxmsiz(tdb, xmsiz)){
    eprint(tdb, __LINE__, "tctdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tctdbsetdfunit(tdb, dfunit)){
    eprint(tdb, __LINE__, "tctdbsetdfunit");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | TDBOTRUNC | omode)){
    eprint(tdb, __LINE__, "tctdbopen");
    err = true;
  }
  if((iflags & (1 << 0)) && !tctdbsetindex(tdb, "", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 1)) && !tctdbsetindex(tdb, "str", TDBITLEXICAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 2)) && !tctdbsetindex(tdb, "num", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 3)) && !tctdbsetindex(tdb, "type", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 4)) && !tctdbsetindex(tdb, "flag", TDBITTOKEN)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if((iflags & (1 << 5)) && !tctdbsetindex(tdb, "text", TDBITQGRAM)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  TARGWRITE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].tdb = tdb;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadwrite(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].tdb = tdb;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadwrite, targs + i) != 0){
        eprint(tdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(tdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  sysprint();
  if(!tctdbclose(tdb)){
    eprint(tdb, __LINE__, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform read command */
static int procread(const char *path, int tnum, int rcnum, int lcnum, int ncnum,
                    int xmsiz, int dfunit, int omode, bool rnd){
  iprintf("<Reading Test>\n  seed=%u  path=%s  tnum=%d  rcnum=%d  lcnum=%d  ncnum=%d"
          "  xmsiz=%d  dfunit=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, tnum, rcnum, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetmutex(tdb)){
    eprint(tdb, __LINE__, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, __LINE__, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbsetcache(tdb, rcnum, lcnum, ncnum)){
    eprint(tdb, __LINE__, "tctdbsetcache");
    err = true;
  }
  if(xmsiz >= 0 && !tctdbsetxmsiz(tdb, xmsiz)){
    eprint(tdb, __LINE__, "tctdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tctdbsetdfunit(tdb, dfunit)){
    eprint(tdb, __LINE__, "tctdbsetdfunit");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOREADER | omode)){
    eprint(tdb, __LINE__, "tctdbopen");
    err = true;
  }
  int rnum = tctdbrnum(tdb) / tnum;
  TARGREAD targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].tdb = tdb;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadread(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].tdb = tdb;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadread, targs + i) != 0){
        eprint(tdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(tdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  sysprint();
  if(!tctdbclose(tdb)){
    eprint(tdb, __LINE__, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform remove command */
static int procremove(const char *path, int tnum, int rcnum, int lcnum, int ncnum,
                      int xmsiz, int dfunit, int omode, bool rnd){
  iprintf("<Removing Test>\n  seed=%u  path=%s  tnum=%d  rcnum=%d  lcnum=%d  ncnum=%d"
          "  xmsiz=%d  dfunit=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, tnum, rcnum, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetmutex(tdb)){
    eprint(tdb, __LINE__, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, __LINE__, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbsetcache(tdb, rcnum, lcnum, ncnum)){
    eprint(tdb, __LINE__, "tctdbsetcache");
    err = true;
  }
  if(xmsiz >= 0 && !tctdbsetxmsiz(tdb, xmsiz)){
    eprint(tdb, __LINE__, "tctdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tctdbsetdfunit(tdb, dfunit)){
    eprint(tdb, __LINE__, "tctdbsetdfunit");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
    eprint(tdb, __LINE__, "tctdbopen");
    err = true;
  }
  int rnum = tctdbrnum(tdb) / tnum;
  TARGREMOVE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].tdb = tdb;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadremove(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].tdb = tdb;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadremove, targs + i) != 0){
        eprint(tdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(tdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  sysprint();
  if(!tctdbclose(tdb)){
    eprint(tdb, __LINE__, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform wicked command */
static int procwicked(const char *path, int tnum, int rnum, int opts, int omode){
  iprintf("<Writing Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  opts=%d  omode=%d\n\n",
          g_randseed, path, tnum, rnum, opts, omode);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetmutex(tdb)){
    eprint(tdb, __LINE__, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, __LINE__, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbtune(tdb, rnum / 50, 2, -1, opts)){
    eprint(tdb, __LINE__, "tctdbtune");
    err = true;
  }
  if(!tctdbsetcache(tdb, rnum / 10, 128, 256)){
    eprint(tdb, __LINE__, "tctdbsetcache");
    err = true;
  }
  if(!tctdbsetxmsiz(tdb, rnum * sizeof(int))){
    eprint(tdb, __LINE__, "tctdbsetxmsiz");
    err = true;
  }
  if(!tctdbsetdfunit(tdb, 8)){
    eprint(tdb, __LINE__, "tctdbsetdfunit");
    err = true;
  }
  if(!tctdbsetinvcache(tdb, -1, 0.5)){
    eprint(tdb, __LINE__, "tctdbsetinvcache");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | TDBOTRUNC | omode)){
    eprint(tdb, __LINE__, "tctdbopen");
    err = true;
  }
  if(!tctdbsetindex(tdb, "", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "str", TDBITLEXICAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "num", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "type", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "flag", TDBITTOKEN)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "text", TDBITQGRAM)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbiterinit(tdb)){
    eprint(tdb, __LINE__, "tctdbiterinit");
    err = true;
  }
  TARGWICKED targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].tdb = tdb;
    targs[0].rnum = rnum;
    targs[0].id = 0;
    if(threadwicked(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].tdb = tdb;
      targs[i].rnum = rnum;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadwicked, targs + i) != 0){
        eprint(tdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(tdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  sysprint();
  if(!tctdbclose(tdb)){
    eprint(tdb, __LINE__, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform typical command */
static int proctypical(const char *path, int tnum, int rnum, int bnum, int apow, int fpow,
                       int opts, int rcnum, int lcnum, int ncnum, int xmsiz, int dfunit,
                       int omode, int rratio){
  iprintf("<Typical Access Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  bnum=%d  apow=%d"
          "  fpow=%d  opts=%d  rcnum=%d  lcnum=%d  ncnum=%d  xmsiz=%d  dfunit=%d"
          "  omode=%d  rratio=%d\n\n",
          g_randseed, path, tnum, rnum, bnum, apow, fpow, opts, rcnum, lcnum, ncnum,
          xmsiz, dfunit, omode, rratio);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetmutex(tdb)){
    eprint(tdb, __LINE__, "tctdbsetmutex");
    err = true;
  }
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(tdb, __LINE__, "tctdbsetcodecfunc");
    err = true;
  }
  if(!tctdbtune(tdb, bnum, apow, fpow, opts)){
    eprint(tdb, __LINE__, "tctdbtune");
    err = true;
  }
  if(!tctdbsetcache(tdb, rcnum, lcnum, ncnum)){
    eprint(tdb, __LINE__, "tctdbsetcache");
    err = true;
  }
  if(xmsiz >= 0 && !tctdbsetxmsiz(tdb, xmsiz)){
    eprint(tdb, __LINE__, "tctdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tctdbsetdfunit(tdb, dfunit)){
    eprint(tdb, __LINE__, "tctdbsetdfunit");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | TDBOTRUNC | omode)){
    eprint(tdb, __LINE__, "tctdbopen");
    err = true;
  }
  if(!tctdbsetindex(tdb, "", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "str", TDBITLEXICAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "num", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "type", TDBITDECIMAL)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "flag", TDBITTOKEN)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  if(!tctdbsetindex(tdb, "text", TDBITQGRAM)){
    eprint(tdb, __LINE__, "tctdbsetindex");
    err = true;
  }
  TARGTYPICAL targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].tdb = tdb;
    targs[0].rnum = rnum;
    targs[0].rratio = rratio;
    targs[0].id = 0;
    if(threadtypical(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].tdb = tdb;
      targs[i].rnum = rnum;
      targs[i].rratio= rratio;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadtypical, targs + i) != 0){
        eprint(tdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(tdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  iprintf("size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  sysprint();
  if(!tctdbclose(tdb)){
    eprint(tdb, __LINE__, "tctdbclose");
    err = true;
  }
  tctdbdel(tdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* thread the write function */
static void *threadwrite(void *targ){
  TCTDB *tdb = ((TARGWRITE *)targ)->tdb;
  int rnum = ((TARGWRITE *)targ)->rnum;
  bool rnd = ((TARGWRITE *)targ)->rnd;
  int id = ((TARGWRITE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    int uid = rnd ? (base + myrand(i) + 1) : tctdbgenuid(tdb);
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", uid);
    TCMAP *cols = tcmapnew2(7);
    char vbuf[RECBUFSIZ*5];
    int vsiz = sprintf(vbuf, "%d", uid);
    tcmapput(cols, "str", 3, vbuf, vsiz);
    if(myrand(3) == 0){
      vsiz = sprintf(vbuf, "%.2f", (myrand(i * 100) + 1) / 100.0);
    } else {
      vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
    }
    tcmapput(cols, "num", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
    tcmapput(cols, "type", 4, vbuf, vsiz);
    int num = myrand(5);
    int pt = 0;
    char *wp = vbuf;
    for(int j = 0; j < num; j++){
      pt += myrand(5) + 1;
      if(wp > vbuf) *(wp++) = ',';
      wp += sprintf(wp, "%d", pt);
    }
    *wp = '\0';
    if(*vbuf != '\0'){
      tcmapput(cols, "flag", 4, vbuf, wp - vbuf);
      tcmapput(cols, "text", 4, vbuf, wp - vbuf);
    }
    if(!tctdbput(tdb, pkbuf, pksiz, cols)){
      eprint(tdb, __LINE__, "tctdbput");
      err = true;
      break;
    }
    tcmapdel(cols);
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  return err ? "error" : NULL;
}


/* thread the read function */
static void *threadread(void *targ){
  TCTDB *tdb = ((TARGREAD *)targ)->tdb;
  int rnum = ((TARGREAD *)targ)->rnum;
  bool rnd = ((TARGREAD *)targ)->rnd;
  int id = ((TARGREAD *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum && !err; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", base + (rnd ? myrandnd(i) : i));
    TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
    if(cols){
      tcmapdel(cols);
    } else if(!rnd || tctdbecode(tdb) != TCENOREC){
      eprint(tdb, __LINE__, "tctdbget");
      err = true;
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  return err ? "error" : NULL;
}


/* thread the remove function */
static void *threadremove(void *targ){
  TCTDB *tdb = ((TARGREMOVE *)targ)->tdb;
  int rnum = ((TARGREMOVE *)targ)->rnum;
  bool rnd = ((TARGREMOVE *)targ)->rnd;
  int id = ((TARGREMOVE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", base + (rnd ? myrand(i + 1) : i));
    if(!tctdbout(tdb, pkbuf, pksiz) && (!rnd || tctdbecode(tdb) != TCENOREC)){
      eprint(tdb, __LINE__, "tctdbout");
      err = true;
      break;
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  return err ? "error" : NULL;
}


/* thread the wicked function */
static void *threadwicked(void *targ){
  TCTDB *tdb = ((TARGWICKED *)targ)->tdb;
  int rnum = ((TARGWICKED *)targ)->rnum;
  int id = ((TARGWICKED *)targ)->id;
  bool err = false;
  const char *names[] = { "", "str", "num", "type", "flag", "c1" };
  int ops[] = { TDBQCSTREQ, TDBQCSTRINC, TDBQCSTRBW, TDBQCSTREW, TDBQCSTRAND, TDBQCSTROR,
                TDBQCSTROREQ, TDBQCSTRRX, TDBQCNUMEQ, TDBQCNUMGT, TDBQCNUMGE, TDBQCNUMLT,
                TDBQCNUMLE, TDBQCNUMBT, TDBQCNUMOREQ };
  int ftsops[] = { TDBQCFTSPH, TDBQCFTSAND, TDBQCFTSOR, TDBQCFTSEX };
  int types[] = { TDBQOSTRASC, TDBQOSTRDESC, TDBQONUMASC, TDBQONUMDESC };
  for(int i = 1; i <= rnum && !err; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", myrand(rnum * (id + 1)));
    TCMAP *cols = tcmapnew2(7);
    char vbuf[RECBUFSIZ*5];
    int vsiz = sprintf(vbuf, "%d", id);
    tcmapput(cols, "str", 3, vbuf, vsiz);
    if(myrand(3) == 0){
      vsiz = sprintf(vbuf, "%.2f", (myrand(i * 100) + 1) / 100.0);
    } else {
      vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
    }
    tcmapput(cols, "num", 3, vbuf, vsiz);
    vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
    tcmapput(cols, "type", 4, vbuf, vsiz);
    int num = myrand(5);
    int pt = 0;
    char *wp = vbuf;
    for(int j = 0; j < num; j++){
      pt += myrand(5) + 1;
      if(wp > vbuf) *(wp++) = ',';
      wp += sprintf(wp, "%d", pt);
    }
    *wp = '\0';
    if(*vbuf != '\0'){
      tcmapput(cols, "flag", 4, vbuf, wp - vbuf);
      tcmapput(cols, "text", 4, vbuf, wp - vbuf);
    }
    char nbuf[RECBUFSIZ];
    int nsiz = sprintf(nbuf, "c%d", myrand(i) + 1);
    vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
    tcmapput(cols, nbuf, nsiz, vbuf, vsiz);
    char *cbuf;
    int csiz;
    TCMAP *ncols;
    TDBQRY *qry;
    switch(myrand(17)){
      case 0:
        if(id == 0) iputchar('0');
        if(!tctdbput(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, __LINE__, "tctdbput");
          err = true;
        }
        break;
      case 1:
        if(id == 0) iputchar('1');
        cbuf = tcstrjoin4(cols, &csiz);
        if(!tctdbput2(tdb, pkbuf, pksiz, cbuf, csiz)){
          eprint(tdb, __LINE__, "tctdbput2");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 2:
        if(id == 0) iputchar('2');
        cbuf = tcstrjoin3(cols, '\t');
        if(!tctdbput3(tdb, pkbuf, cbuf)){
          eprint(tdb, __LINE__, "tctdbput3");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 3:
        if(id == 0) iputchar('3');
        if(!tctdbputkeep(tdb, pkbuf, pksiz, cols) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, __LINE__, "tctdbputkeep");
          err = true;
        }
        break;
      case 4:
        if(id == 0) iputchar('4');
        cbuf = tcstrjoin4(cols, &csiz);
        if(!tctdbputkeep2(tdb, pkbuf, pksiz, cbuf, csiz) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, __LINE__, "tctdbputkeep2");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 5:
        if(id == 0) iputchar('5');
        cbuf = tcstrjoin3(cols, '\t');
        if(!tctdbputkeep3(tdb, pkbuf, cbuf) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, __LINE__, "tctdbputkeep3");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 6:
        if(id == 0) iputchar('6');
        if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, __LINE__, "tctdbputcat");
          err = true;
        }
        break;
      case 7:
        if(id == 0) iputchar('7');
        cbuf = tcstrjoin4(cols, &csiz);
        if(!tctdbputcat2(tdb, pkbuf, pksiz, cbuf, csiz)){
          eprint(tdb, __LINE__, "tctdbputcat2");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 8:
        if(id == 0) iputchar('8');
        cbuf = tcstrjoin3(cols, '\t');
        if(!tctdbputcat3(tdb, pkbuf, cbuf)){
          eprint(tdb, __LINE__, "tctdbputcat3");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 9:
        if(id == 0) iputchar('9');
        if(myrand(2) == 0){
          if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
            eprint(tdb, __LINE__, "tctdbout");
            err = true;
          }
        }
        break;
      case 10:
        if(id == 0) iputchar('A');
        if(myrand(2) == 0){
          if(!tctdbout2(tdb, pkbuf) && tctdbecode(tdb) != TCENOREC){
            eprint(tdb, __LINE__, "tctdbout2");
            err = true;
          }
        }
        break;
      case 11:
        if(id == 0) iputchar('B');
        ncols = tctdbget(tdb, pkbuf, pksiz);
        if(ncols){
          tcmapdel(ncols);
        } else if(tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbget");
          err = true;
        }
        break;
      case 12:
        if(id == 0) iputchar('C');
        cbuf = tctdbget2(tdb, pkbuf, pksiz, &csiz);
        if(cbuf){
          tcfree(cbuf);
        } else if(tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbget2");
          err = true;
        }
        break;
      case 13:
        if(id == 0) iputchar('D');
        cbuf = tctdbget3(tdb, pkbuf);
        if(cbuf){
          tcfree(cbuf);
        } else if(tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbget3");
          err = true;
        }
        break;
      case 14:
        if(id == 0) iputchar('E');
        if(myrand(rnum / 50) == 0){
          if(!tctdbiterinit(tdb)){
            eprint(tdb, __LINE__, "tctdbiterinit");
            err = true;
          }
        }
        for(int j = myrand(rnum) / 1000 + 1; j >= 0; j--){
          int iksiz;
          char *ikbuf = tctdbiternext(tdb, &iksiz);
          if(ikbuf){
            tcfree(ikbuf);
          } else {
            int ecode = tctdbecode(tdb);
            if(ecode != TCEINVALID && ecode != TCENOREC){
              eprint(tdb, __LINE__, "tctdbiternext");
              err = true;
            }
          }
        }
        break;
      case 15:
        if(id == 0) iputchar('F');
        qry = tctdbqrynew(tdb);
        if(myrand(10) != 0){
          char expr[RECBUFSIZ];
          sprintf(expr, "%d", myrand(i) + 1);
          switch(myrand(6)){
            default:
              tctdbqryaddcond(qry, "str", TDBQCSTREQ, expr);
              break;
            case 1:
              tctdbqryaddcond(qry, "str", TDBQCSTRBW, expr);
              break;
            case 2:
              tctdbqryaddcond(qry, "str", TDBQCSTROREQ, expr);
              break;
            case 3:
              tctdbqryaddcond(qry, "num", TDBQCNUMEQ, expr);
              break;
            case 4:
              tctdbqryaddcond(qry, "num", TDBQCNUMGT, expr);
              break;
            case 5:
              tctdbqryaddcond(qry, "num", TDBQCNUMLT, expr);
              break;
          }
          switch(myrand(5)){
            case 0:
              tctdbqrysetorder(qry, "str", TDBQOSTRASC);
              break;
            case 1:
              tctdbqrysetorder(qry, "str", TDBQOSTRDESC);
              break;
            case 2:
              tctdbqrysetorder(qry, "num", TDBQONUMASC);
              break;
            case 3:
              tctdbqrysetorder(qry, "num", TDBQONUMDESC);
              break;
          }
          tctdbqrysetlimit(qry, 10, myrand(10));
        } else {
          int cnum = myrand(4);
          if(cnum < 1 && myrand(5) != 0) cnum = 1;
          for(int j = 0; j < cnum; j++){
            const char *name = names[myrand(sizeof(names) / sizeof(*names))];
            int op = ops[myrand(sizeof(ops) / sizeof(*ops))];
            if(myrand(10) == 0) op = ftsops[myrand(sizeof(ftsops) / sizeof(*ftsops))];
            if(myrand(20) == 0) op |= TDBQCNEGATE;
            if(myrand(20) == 0) op |= TDBQCNOIDX;
            char expr[RECBUFSIZ*3];
            char *wp = expr;
            if(myrand(3) == 0){
              wp += sprintf(expr, "%f", myrand(i * 100) / 100.0);
            } else {
              wp += sprintf(expr, "%d", myrand(i));
            }
            if(myrand(10) == 0) wp += sprintf(wp, ",%d", myrand(i));
            if(myrand(10) == 0) wp += sprintf(wp, ",%d", myrand(i));
            tctdbqryaddcond(qry, name, op, expr);
          }
          if(myrand(3) != 0){
            const char *name = names[myrand(sizeof(names) / sizeof(*names))];
            int type = types[myrand(sizeof(types) / sizeof(*types))];
            tctdbqrysetorder(qry, name, type);
          }
          if(myrand(3) != 0) tctdbqrysetlimit(qry, myrand(i), myrand(10));
        }
        if(myrand(10) == 0){
          TCLIST *res = tctdbqrysearch(qry);
          tclistdel(res);
        }
        tctdbqrydel(qry);
        break;
      default:
        if(id == 0) iputchar('@');
        if(tctdbtranbegin(tdb)){
          if(myrand(2) == 0){
            if(!tctdbput(tdb, pkbuf, pksiz, cols)){
              eprint(tdb, __LINE__, "tctdbput");
              err = true;
            }
          } else {
            if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
              eprint(tdb, __LINE__, "tctdbout");
              err = true;
            }
          }
          if(myrand(2) == 0){
            if(!tctdbtranabort(tdb)){
              eprint(tdb, __LINE__, "tctdbtranabort");
              err = true;
            }
          } else {
            if(!tctdbtrancommit(tdb)){
              eprint(tdb, __LINE__, "tctdbtrancommit");
              err = true;
            }
          }
        } else {
          eprint(tdb, __LINE__, "tctdbtranbegin");
          err = true;
        }
        if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
        break;
    }
    tcmapdel(cols);
    if(id == 0){
      if(i % 50 == 0) iprintf(" (%08d)\n", i);
      if(id == 0 && i == rnum / 4){
        if(!tctdboptimize(tdb, rnum / 50, -1, -1, -1) && tctdbecode(tdb) != TCEINVALID){
          eprint(tdb, __LINE__, "tctdboptimize");
          err = true;
        }
        if(!tctdbiterinit(tdb)){
          eprint(tdb, __LINE__, "tctdbiterinit");
          err = true;
        }
      }
    }
  }
  return err ? "error" : NULL;
}


/* thread the typical function */
static void *threadtypical(void *targ){
  TCTDB *tdb = ((TARGTYPICAL *)targ)->tdb;
  int rnum = ((TARGTYPICAL *)targ)->rnum;
  int rratio = ((TARGTYPICAL *)targ)->rratio;
  int id = ((TARGTYPICAL *)targ)->id;
  bool err = false;
  int base = id * rnum;
  int mrange = tclmax(50 + rratio, 100);
  for(int i = 1; !err && i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%08d", base + myrandnd(i));
    int rnd = myrand(mrange);
    if(rnd < 20){
      TCMAP *cols = tcmapnew2(7);
      char vbuf[RECBUFSIZ*5];
      int vsiz = sprintf(vbuf, "%d", id);
      tcmapput(cols, "str", 3, vbuf, vsiz);
      if(myrand(3) == 0){
        vsiz = sprintf(vbuf, "%.2f", (myrand(i * 100) + 1) / 100.0);
      } else {
        vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
      }
      tcmapput(cols, "num", 3, vbuf, vsiz);
      vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
      tcmapput(cols, "type", 4, vbuf, vsiz);
      int num = myrand(5);
      int pt = 0;
      char *wp = vbuf;
      for(int j = 0; j < num; j++){
        pt += myrand(5) + 1;
        if(wp > vbuf) *(wp++) = ',';
        wp += sprintf(wp, "%d", pt);
      }
      *wp = '\0';
      if(*vbuf != '\0'){
        tcmapput(cols, "flag", 4, vbuf, wp - vbuf);
        tcmapput(cols, "text", 4, vbuf, wp - vbuf);
      }
      char nbuf[RECBUFSIZ];
      int nsiz = sprintf(nbuf, "c%d", myrand(i) + 1);
      vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
      tcmapput(cols, nbuf, nsiz, vbuf, vsiz);
      if(myrand(2) == 0){
        if(!tctdbput(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, __LINE__, "tctdbput");
          err = true;
        }
      } else {
        if(!tctdbput(tdb, pkbuf, pksiz, cols) && tctdbecode(tdb) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, __LINE__, "tctdbput");
          err = true;
        }
      }
      tcmapdel(cols);
    } else if(rnd < 30){
      if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) && tctdbecode(tdb) != TCENOREC){
        eprint(tdb, __LINE__, "tctdbout");
        err = true;
      }
    } else if(rnd < 31){
      if(myrand(10) == 0 && !tctdbiterinit(tdb) && tctdbecode(tdb) != TCENOREC){
        eprint(tdb, __LINE__, "tctdbiterinit");
        err = true;
      }
      for(int j = 0; !err && j < 10; j++){
        int ksiz;
        char *kbuf = tctdbiternext(tdb, &ksiz);
        if(kbuf){
          tcfree(kbuf);
        } else if(tctdbecode(tdb) != TCEINVALID && tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbiternext");
          err = true;
        }
      }
    } else {
      TDBQRY *qry = tctdbqrynew(tdb);
      char expr[RECBUFSIZ];
      sprintf(expr, "%d", myrand(i) + 1);
      switch(myrand(6) * 1){
        default:
          tctdbqryaddcond(qry, "str", TDBQCSTREQ, expr);
          break;
        case 1:
          tctdbqryaddcond(qry, "str", TDBQCSTRBW, expr);
          break;
        case 2:
          tctdbqryaddcond(qry, "str", TDBQCSTROREQ, expr);
          break;
        case 3:
          tctdbqryaddcond(qry, "num", TDBQCNUMEQ, expr);
          break;
        case 4:
          tctdbqryaddcond(qry, "num", TDBQCNUMGT, expr);
          break;
        case 5:
          tctdbqryaddcond(qry, "num", TDBQCNUMLT, expr);
          break;
      }
      tctdbqrysetlimit(qry, 10, myrand(5) * 10);
      TCLIST *res = tctdbqrysearch(qry);
      tclistdel(res);
      tctdbqrydel(qry);
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  return err ? "error" : NULL;
}



// END OF FILE
