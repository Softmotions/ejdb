/*************************************************************************************************
 * The test cases of the B+ tree database API
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
#include <tcbdb.h>
#include "myconf.h"

#define RECBUFSIZ      48                // buffer for records

typedef struct {                         // type of structure for write thread
  TCBDB *bdb;
  int rnum;
  bool rnd;
  int id;
} TARGWRITE;

typedef struct {                         // type of structure for read thread
  TCBDB *bdb;
  int rnum;
  bool wb;
  bool rnd;
  int id;
} TARGREAD;

typedef struct {                         // type of structure for remove thread
  TCBDB *bdb;
  int rnum;
  bool rnd;
  int id;
} TARGREMOVE;

typedef struct {                         // type of structure for wicked thread
  TCBDB *bdb;
  int rnum;
  bool nc;
  int id;
  TCMAP *map;
} TARGWICKED;

typedef struct {                         // type of structure for typical thread
  TCBDB *bdb;
  int rnum;
  bool nc;
  int rratio;
  int id;
} TARGTYPICAL;

typedef struct {                         // type of structure for race thread
  TCBDB *bdb;
  int rnum;
  int id;
} TARGRACE;


/* global variables */
const char *g_progname;                  // program name
unsigned int g_randseed;                 // random seed
int g_dbgfd;                             // debugging output


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void iputchar(int c);
static void eprint(TCBDB *bdb, int line, const char *func);
static void mprint(TCBDB *bdb);
static void sysprint(void);
static int myrand(int range);
static int myrandnd(int range);
static bool iterfunc(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int runtypical(int argc, char **argv);
static int runrace(int argc, char **argv);
static int procwrite(const char *path, int tnum, int rnum, int lmemb, int nmemb,
                     int bnum, int apow, int fpow, int opts, int xmsiz, int dfunit, int omode,
                     bool rnd);
static int procread(const char *path, int tnum, int xmsiz, int dfunit, int omode,
                    bool wb, bool rnd);
static int procremove(const char *path, int tnum, int xmsiz, int dfunit, int omode, bool rnd);
static int procwicked(const char *path, int tnum, int rnum, int opts, int omode, bool nc);
static int proctypical(const char *path, int tnum, int rnum, int lmemb, int nmemb,
                       int bnum, int apow, int fpow, int opts, int xmsiz, int dfunit, int omode,
                       bool nc, int rratio);
static int procrace(const char *path, int tnum, int rnum, int lmemb, int nmemb,
                    int bnum, int apow, int fpow, int opts, int xmsiz, int dfunit, int omode);
static void *threadwrite(void *targ);
static void *threadread(void *targ);
static void *threadremove(void *targ);
static void *threadwicked(void *targ);
static void *threadtypical(void *targ);
static void *threadrace(void *targ);


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
  } else if(!strcmp(argv[1], "race")){
    rv = runrace(argc, argv);
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
  fprintf(stderr, "%s: test cases of the B+ tree database API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write [-tl] [-td|-tb|-tt|-tx] [-xm num] [-df num] [-nl|-nb] [-rnd]"
          " path tnum rnum [lmemb [nmemb [bnum [apow [fpow]]]]]\n", g_progname);
  fprintf(stderr, "  %s read [-xm num] [-df num] [-nl|-nb] [-wb] [-rnd] path tnum\n",
          g_progname);
  fprintf(stderr, "  %s remove [-xm num] [-df num] [-nl|-nb] [-rnd] path tnum\n", g_progname);
  fprintf(stderr, "  %s wicked [-tl] [-td|-tb|-tt|-tx] [-nl|-nb] [-nc] path tnum rnum\n",
          g_progname);
  fprintf(stderr, "  %s typical [-tl] [-td|-tb|-tt|-tx] [-xm num] [-df num] [-nl|-nb]"
          " [-nc] [-rr num] path tnum rnum [lmemb [nmemb [bnum [apow [fpow]]]]]\n", g_progname);
  fprintf(stderr, "  %s race [-tl] [-td|-tb|-tt|-tx] [-xm num] [-df num] [-nl|-nb]"
          " path tnum rnum [lmemb [nmemb [bnum [apow [fpow]]]]]\n", g_progname);
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


/* print error message of hash database */
static void eprint(TCBDB *bdb, int line, const char *func){
  const char *path = tcbdbpath(bdb);
  int ecode = tcbdbecode(bdb);
  fprintf(stderr, "%s: %s: %d: %s: error: %d: %s\n",
          g_progname, path ? path : "-", line, func, ecode, tcbdberrmsg(ecode));
}


/* print members of hash database */
static void mprint(TCBDB *bdb){
  if(bdb->hdb->cnt_writerec < 0) return;
  iprintf("max leaf member: %d\n", tcbdblmemb(bdb));
  iprintf("max node member: %d\n", tcbdbnmemb(bdb));
  iprintf("leaf number: %lld\n", (long long)tcbdblnum(bdb));
  iprintf("node number: %lld\n", (long long)tcbdbnnum(bdb));
  iprintf("bucket number: %lld\n", (long long)tcbdbbnum(bdb));
  iprintf("used bucket number: %lld\n", (long long)tcbdbbnumused(bdb));
  iprintf("cnt_saveleaf: %lld\n", (long long)bdb->cnt_saveleaf);
  iprintf("cnt_loadleaf: %lld\n", (long long)bdb->cnt_loadleaf);
  iprintf("cnt_killleaf: %lld\n", (long long)bdb->cnt_killleaf);
  iprintf("cnt_adjleafc: %lld\n", (long long)bdb->cnt_adjleafc);
  iprintf("cnt_savenode: %lld\n", (long long)bdb->cnt_savenode);
  iprintf("cnt_loadnode: %lld\n", (long long)bdb->cnt_loadnode);
  iprintf("cnt_adjnodec: %lld\n", (long long)bdb->cnt_adjnodec);
  iprintf("cnt_writerec: %lld\n", (long long)bdb->hdb->cnt_writerec);
  iprintf("cnt_reuserec: %lld\n", (long long)bdb->hdb->cnt_reuserec);
  iprintf("cnt_moverec: %lld\n", (long long)bdb->hdb->cnt_moverec);
  iprintf("cnt_readrec: %lld\n", (long long)bdb->hdb->cnt_readrec);
  iprintf("cnt_searchfbp: %lld\n", (long long)bdb->hdb->cnt_searchfbp);
  iprintf("cnt_insertfbp: %lld\n", (long long)bdb->hdb->cnt_insertfbp);
  iprintf("cnt_splicefbp: %lld\n", (long long)bdb->hdb->cnt_splicefbp);
  iprintf("cnt_dividefbp: %lld\n", (long long)bdb->hdb->cnt_dividefbp);
  iprintf("cnt_mergefbp: %lld\n", (long long)bdb->hdb->cnt_mergefbp);
  iprintf("cnt_reducefbp: %lld\n", (long long)bdb->hdb->cnt_reducefbp);
  iprintf("cnt_appenddrp: %lld\n", (long long)bdb->hdb->cnt_appenddrp);
  iprintf("cnt_deferdrp: %lld\n", (long long)bdb->hdb->cnt_deferdrp);
  iprintf("cnt_flushdrp: %lld\n", (long long)bdb->hdb->cnt_flushdrp);
  iprintf("cnt_adjrecc: %lld\n", (long long)bdb->hdb->cnt_adjrecc);
  iprintf("cnt_defrag: %lld\n", (long long)bdb->hdb->cnt_defrag);
  iprintf("cnt_shiftrec: %lld\n", (long long)bdb->hdb->cnt_shiftrec);
  iprintf("cnt_trunc: %lld\n", (long long)bdb->hdb->cnt_trunc);
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


/* iterator function */
static bool iterfunc(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op){
  unsigned int sum = 0;
  while(--ksiz >= 0){
    sum += ((char *)kbuf)[ksiz];
  }
  while(--vsiz >= 0){
    sum += ((char *)vbuf)[vsiz];
  }
  return myrand(100 + (sum & 0xff)) > 0;
}


/* parse arguments of write command */
static int runwrite(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  char *lmstr = NULL;
  char *nmstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  int opts = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        opts |= BDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= BDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= BDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= BDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= BDBTEXCODEC;
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
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
    } else if(!lmstr){
      lmstr = argv[i];
    } else if(!nmstr){
      nmstr = argv[i];
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
  int lmemb = lmstr ? tcatoix(lmstr) : -1;
  int nmemb = nmstr ? tcatoix(nmstr) : -1;
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procwrite(path, tnum, rnum, lmemb, nmemb, bnum, apow, fpow, opts, xmsiz, dfunit,
                     omode, rnd);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  bool rnd = false;
  bool wb = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-wb")){
        wb = true;
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
  int rv = procread(path, tnum, xmsiz, dfunit, omode, wb, rnd);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
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
  int rv = procremove(path, tnum, xmsiz, dfunit, omode, rnd);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  int opts = 0;
  int omode = 0;
  bool nc = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        opts |= BDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= BDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= BDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= BDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= BDBTEXCODEC;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-nc")){
        nc = true;
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
  int rv = procwicked(path, tnum, rnum, opts, omode, nc);
  return rv;
}


/* parse arguments of typical command */
static int runtypical(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  char *lmstr = NULL;
  char *nmstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  int opts = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  int rratio = -1;
  bool nc = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        opts |= BDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= BDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= BDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= BDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= BDBTEXCODEC;
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-nc")){
        nc = true;
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
    } else if(!lmstr){
      lmstr = argv[i];
    } else if(!nmstr){
      nmstr = argv[i];
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
  int lmemb = lmstr ? tcatoix(lmstr) : -1;
  int nmemb = nmstr ? tcatoix(nmstr) : -1;
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = proctypical(path, tnum, rnum, lmemb, nmemb, bnum, apow, fpow, opts, xmsiz, dfunit,
                       omode, nc, rratio);
  return rv;
}


/* parse arguments of race command */
static int runrace(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  char *lmstr = NULL;
  char *nmstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  int opts = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        opts |= BDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= BDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= BDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= BDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= BDBTEXCODEC;
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else if(!lmstr){
      lmstr = argv[i];
    } else if(!nmstr){
      nmstr = argv[i];
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
  int lmemb = lmstr ? tcatoix(lmstr) : -1;
  int nmemb = nmstr ? tcatoix(nmstr) : -1;
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procrace(path, tnum, rnum, lmemb, nmemb, bnum, apow, fpow, opts, xmsiz, dfunit,
                    omode);
  return rv;
}


/* perform write command */
static int procwrite(const char *path, int tnum, int rnum, int lmemb, int nmemb,
                     int bnum, int apow, int fpow, int opts, int xmsiz, int dfunit, int omode,
                     bool rnd){
  iprintf("<Writing Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  lmemb=%d  nmemb=%d"
          "  bnum=%d  apow=%d  fpow=%d  opts=%d  xmsiz=%d  dfunit=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, tnum, rnum, lmemb, nmemb, bnum, apow, fpow, opts, xmsiz, dfunit,
          omode, rnd);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(!tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(!tcbdbtune(bdb, lmemb, nmemb, bnum, apow, fpow, opts)){
    eprint(bdb, __LINE__, "tcbdbtune");
    err = true;
  }
  if(xmsiz >= 0 && !tcbdbsetxmsiz(bdb, xmsiz)){
    eprint(bdb, __LINE__, "tcbdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tcbdbsetdfunit(bdb, dfunit)){
    eprint(bdb, __LINE__, "tcbdbsetdfunit");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  TARGWRITE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].bdb = bdb;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadwrite(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].bdb = bdb;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadwrite, targs + i) != 0){
        eprint(bdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(bdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcbdbrnum(bdb));
  iprintf("size: %llu\n", (unsigned long long)tcbdbfsiz(bdb));
  mprint(bdb);
  sysprint();
  if(!tcbdbclose(bdb)){
    eprint(bdb, __LINE__, "tcbdbclose");
    err = true;
  }
  tcbdbdel(bdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform read command */
static int procread(const char *path, int tnum, int xmsiz, int dfunit, int omode,
                    bool wb, bool rnd){
  iprintf("<Reading Test>\n  seed=%u  path=%s  tnum=%d  xmsiz=%d  dfunit=%d  omode=%d"
          "  wb=%d  rnd=%d\n\n", g_randseed, path, tnum, xmsiz, dfunit, omode, wb, rnd);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(!tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(xmsiz >= 0 && !tcbdbsetxmsiz(bdb, xmsiz)){
    eprint(bdb, __LINE__, "tcbdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tcbdbsetdfunit(bdb, dfunit)){
    eprint(bdb, __LINE__, "tcbdbsetdfunit");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOREADER | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  int rnum = tcbdbrnum(bdb) / tnum;
  TARGREAD targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].bdb = bdb;
    targs[0].rnum = rnum;
    targs[0].wb = wb;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadread(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].bdb = bdb;
      targs[i].rnum = rnum;
      targs[i].wb = wb;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadread, targs + i) != 0){
        eprint(bdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(bdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcbdbrnum(bdb));
  iprintf("size: %llu\n", (unsigned long long)tcbdbfsiz(bdb));
  mprint(bdb);
  sysprint();
  if(!tcbdbclose(bdb)){
    eprint(bdb, __LINE__, "tcbdbclose");
    err = true;
  }
  tcbdbdel(bdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform remove command */
static int procremove(const char *path, int tnum, int xmsiz, int dfunit, int omode, bool rnd){
  iprintf("<Removing Test>\n  seed=%u  path=%s  tnum=%d  xmsiz=%d  dfunit=%d  omode=%d"
          "  rnd=%d\n\n", g_randseed, path, tnum, xmsiz, dfunit, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(!tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(xmsiz >= 0 && !tcbdbsetxmsiz(bdb, xmsiz)){
    eprint(bdb, __LINE__, "tcbdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tcbdbsetdfunit(bdb, dfunit)){
    eprint(bdb, __LINE__, "tcbdbsetdfunit");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  int rnum = tcbdbrnum(bdb) / tnum;
  TARGREMOVE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].bdb = bdb;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadremove(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].bdb = bdb;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadremove, targs + i) != 0){
        eprint(bdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(bdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcbdbrnum(bdb));
  iprintf("size: %llu\n", (unsigned long long)tcbdbfsiz(bdb));
  mprint(bdb);
  sysprint();
  if(!tcbdbclose(bdb)){
    eprint(bdb, __LINE__, "tcbdbclose");
    err = true;
  }
  tcbdbdel(bdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform wicked command */
static int procwicked(const char *path, int tnum, int rnum, int opts, int omode, bool nc){
  iprintf("<Writing Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  opts=%d  omode=%d  nc=%d\n\n",
          g_randseed, path, tnum, rnum, opts, omode, nc);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(!tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(!tcbdbtune(bdb, 10, 10, rnum / 50, 10, -1, opts)){
    eprint(bdb, __LINE__, "tcbdbtune");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  TARGWICKED targs[tnum];
  pthread_t threads[tnum];
  TCMAP *map = tcmapnew();
  if(tnum == 1){
    targs[0].bdb = bdb;
    targs[0].rnum = rnum;
    targs[0].nc = nc;
    targs[0].id = 0;
    targs[0].map = map;
    if(threadwicked(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].bdb = bdb;
      targs[i].rnum = rnum;
      targs[i].nc = nc;
      targs[i].id = i;
      targs[i].map = map;
      if(pthread_create(threads + i, NULL, threadwicked, targs + i) != 0){
        eprint(bdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(bdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  if(!nc){
    if(!tcbdbsync(bdb)){
      eprint(bdb, __LINE__, "tcbdbsync");
      err = true;
    }
    if(tcbdbrnum(bdb) != tcmaprnum(map)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
    }
    int end = rnum * tnum;
    for(int i = 1; i <= end && !err; i++){
      char kbuf[RECBUFSIZ];
      int ksiz = sprintf(kbuf, "%d", i - 1);
      int vsiz;
      const char *vbuf = tcmapget(map, kbuf, ksiz, &vsiz);
      int rsiz;
      char *rbuf = tcbdbget(bdb, kbuf, ksiz, &rsiz);
      if(vbuf){
        iputchar('.');
        if(!rbuf){
          eprint(bdb, __LINE__, "tcbdbget");
          err = true;
        } else if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
          eprint(bdb, __LINE__, "(validation)");
          err = true;
        }
      } else {
        iputchar('*');
        if(rbuf || tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "(validation)");
          err = true;
        }
      }
      tcfree(rbuf);
      if(i % 50 == 0) iprintf(" (%08d)\n", i);
    }
    if(rnum % 50 > 0) iprintf(" (%08d)\n", rnum);
  }
  tcmapdel(map);
  iprintf("record number: %llu\n", (unsigned long long)tcbdbrnum(bdb));
  iprintf("size: %llu\n", (unsigned long long)tcbdbfsiz(bdb));
  mprint(bdb);
  sysprint();
  if(!tcbdbclose(bdb)){
    eprint(bdb, __LINE__, "tcbdbclose");
    err = true;
  }
  tcbdbdel(bdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform typical command */
static int proctypical(const char *path, int tnum, int rnum, int lmemb, int nmemb,
                       int bnum, int apow, int fpow, int opts, int xmsiz, int dfunit, int omode,
                       bool nc, int rratio){
  iprintf("<Typical Access Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  lmemb=%d  nmemb=%d"
          "  bnum=%d  apow=%d  fpow=%d  opts=%d  xmsiz=%d  dfunit=%d  omode=%d"
          "  nc=%d  rratio=%d\n\n",
          g_randseed, path, tnum, rnum, lmemb, nmemb, bnum, apow, fpow, opts, xmsiz, dfunit,
          omode, nc, rratio);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(!tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(!tcbdbtune(bdb, lmemb, nmemb, bnum, apow, fpow, opts)){
    eprint(bdb, __LINE__, "tcbdbtune");
    err = true;
  }
  if(xmsiz >= 0 && !tcbdbsetxmsiz(bdb, xmsiz)){
    eprint(bdb, __LINE__, "tcbdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tcbdbsetdfunit(bdb, dfunit)){
    eprint(bdb, __LINE__, "tcbdbsetdfunit");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  TARGTYPICAL targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].bdb = bdb;
    targs[0].rnum = rnum;
    targs[0].nc = nc;
    targs[0].rratio = rratio;
    targs[0].id = 0;
    if(threadtypical(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].bdb = bdb;
      targs[i].rnum = rnum;
      targs[i].nc = nc;
      targs[i].rratio = rratio;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadtypical, targs + i) != 0){
        eprint(bdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(bdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcbdbrnum(bdb));
  iprintf("size: %llu\n", (unsigned long long)tcbdbfsiz(bdb));
  mprint(bdb);
  sysprint();
  if(!tcbdbclose(bdb)){
    eprint(bdb, __LINE__, "tcbdbclose");
    err = true;
  }
  tcbdbdel(bdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform race command */
static int procrace(const char *path, int tnum, int rnum, int lmemb, int nmemb,
                    int bnum, int apow, int fpow, int opts, int xmsiz, int dfunit, int omode){
  iprintf("<Race Condition Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  lmemb=%d  nmemb=%d"
          "  bnum=%d  apow=%d  fpow=%d  opts=%d  xmsiz=%d  dfunit=%d  omode=%d\n\n",
          g_randseed, path, tnum, rnum, lmemb, nmemb, bnum, apow, fpow, opts,
          xmsiz, dfunit, omode);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(!tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(!tcbdbtune(bdb, lmemb, nmemb, bnum, apow, fpow, opts)){
    eprint(bdb, __LINE__, "tcbdbtune");
    err = true;
  }
  if(xmsiz >= 0 && !tcbdbsetxmsiz(bdb, xmsiz)){
    eprint(bdb, __LINE__, "tcbdbsetxmsiz");
    err = true;
  }
  if(dfunit >= 0 && !tcbdbsetdfunit(bdb, dfunit)){
    eprint(bdb, __LINE__, "tcbdbsetdfunit");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  TARGRACE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].bdb = bdb;
    targs[0].rnum = rnum;
    targs[0].id = 0;
    if(threadrace(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].bdb = bdb;
      targs[i].rnum = rnum;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadrace, targs + i) != 0){
        eprint(bdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(bdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcbdbrnum(bdb));
  iprintf("size: %llu\n", (unsigned long long)tcbdbfsiz(bdb));
  mprint(bdb);
  sysprint();
  if(!tcbdbclose(bdb)){
    eprint(bdb, __LINE__, "tcbdbclose");
    err = true;
  }
  tcbdbdel(bdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* thread the write function */
static void *threadwrite(void *targ){
  TCBDB *bdb = ((TARGWRITE *)targ)->bdb;
  int rnum = ((TARGWRITE *)targ)->rnum;
  bool rnd = ((TARGWRITE *)targ)->rnd;
  int id = ((TARGWRITE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + (rnd ? myrand(i) : i));
    if(!tcbdbput(bdb, buf, len, buf, len)){
      eprint(bdb, __LINE__, "tcbdbput");
      err = true;
      break;
    }
    if(id <= 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  return err ? "error" : NULL;
}


/* thread the read function */
static void *threadread(void *targ){
  TCBDB *bdb = ((TARGREAD *)targ)->bdb;
  int rnum = ((TARGREAD *)targ)->rnum;
  bool wb = ((TARGREAD *)targ)->wb;
  bool rnd = ((TARGREAD *)targ)->rnd;
  int id = ((TARGREAD *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", base + (rnd ? myrandnd(i) : i));
    int vsiz;
    if(wb){
      int vsiz;
      const char *vbuf = tcbdbget3(bdb, kbuf, ksiz, &vsiz);
      if(!vbuf && (!rnd || tcbdbecode(bdb) != TCENOREC)){
        eprint(bdb, __LINE__, "tcbdbget3");
        err = true;
      }
    } else {
      char *vbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz);
      if(!vbuf && (!rnd || tcbdbecode(bdb) != TCENOREC)){
        eprint(bdb, __LINE__, "tcbdbget");
        err = true;
      }
      tcfree(vbuf);
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
  TCBDB *bdb = ((TARGREMOVE *)targ)->bdb;
  int rnum = ((TARGREMOVE *)targ)->rnum;
  bool rnd = ((TARGREMOVE *)targ)->rnd;
  int id = ((TARGREMOVE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", base + (rnd ? myrand(i + 1) : i));
    if(!tcbdbout(bdb, kbuf, ksiz) && (!rnd || tcbdbecode(bdb) != TCENOREC)){
      eprint(bdb, __LINE__, "tcbdbout");
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
  TCBDB *bdb = ((TARGWICKED *)targ)->bdb;
  int rnum = ((TARGWICKED *)targ)->rnum;
  bool nc = ((TARGWICKED *)targ)->nc;
  int id = ((TARGWICKED *)targ)->id;
  TCMAP *map = ((TARGWICKED *)targ)->map;
  BDBCUR *cur = tcbdbcurnew(bdb);
  bool err = false;
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum * (id + 1)));
    char vbuf[RECBUFSIZ];
    int vsiz = myrand(RECBUFSIZ);
    memset(vbuf, '*', vsiz);
    vbuf[vsiz] = '\0';
    char *rbuf;
    if(!nc) tcglobalmutexlock();
    switch(myrand(16)){
      case 0:
        if(id == 0) iputchar('0');
        if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbput");
          err = true;
        }
        if(!nc) tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 1:
        if(id == 0) iputchar('1');
        if(!tcbdbput2(bdb, kbuf, vbuf)){
          eprint(bdb, __LINE__, "tcbdbput2");
          err = true;
        }
        if(!nc) tcmapput2(map, kbuf, vbuf);
        break;
      case 2:
        if(id == 0) iputchar('2');
        if(!tcbdbputkeep(bdb, kbuf, ksiz, vbuf, vsiz) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbputkeep");
          err = true;
        }
        if(!nc) tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 3:
        if(id == 0) iputchar('3');
        if(!tcbdbputkeep2(bdb, kbuf, vbuf) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbputkeep2");
          err = true;
        }
        if(!nc) tcmapputkeep2(map, kbuf, vbuf);
        break;
      case 4:
        if(id == 0) iputchar('4');
        if(!tcbdbputcat(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbputcat");
          err = true;
        }
        if(!nc) tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 5:
        if(id == 0) iputchar('5');
        if(!tcbdbputcat2(bdb, kbuf, vbuf)){
          eprint(bdb, __LINE__, "tcbdbputcat2");
          err = true;
        }
        if(!nc) tcmapputcat2(map, kbuf, vbuf);
        break;
      case 6:
        if(id == 0) iputchar('6');
        if(nc){
          if(!tcbdbputdup(bdb, kbuf, ksiz, vbuf, vsiz)){
            eprint(bdb, __LINE__, "tcbdbputdup");
            err = true;
          }
        }
        break;
      case 7:
        if(id == 0) iputchar('7');
        if(nc){
          if(!tcbdbputdup2(bdb, kbuf, vbuf)){
            eprint(bdb, __LINE__, "tcbdbputdup2");
            err = true;
          }
        }
        break;
      case 8:
        if(id == 0) iputchar('8');
        if(myrand(2) == 0){
          if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbout");
            err = true;
          }
          if(!nc) tcmapout(map, kbuf, ksiz);
        }
        break;
      case 9:
        if(id == 0) iputchar('9');
        if(myrand(2) == 0){
          if(!tcbdbout2(bdb, kbuf) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbout2");
            err = true;
          }
          if(!nc) tcmapout2(map, kbuf);
        }
        break;
      case 10:
        if(id == 0) iputchar('A');
        if(!(rbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz))){
          if(tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbget");
            err = true;
          }
          rbuf = tcsprintf("[%d]", myrand(i + 1));
          vsiz = strlen(rbuf);
        }
        vsiz += myrand(vsiz);
        if(myrand(3) == 0) vsiz += PATH_MAX;
        rbuf = tcrealloc(rbuf, vsiz + 1);
        for(int j = 0; j < vsiz; j++){
          rbuf[j] = myrand(0x100);
        }
        if(!tcbdbput(bdb, kbuf, ksiz, rbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbput");
          err = true;
        }
        if(!nc) tcmapput(map, kbuf, ksiz, rbuf, vsiz);
        tcfree(rbuf);
        break;
      case 11:
        if(id == 0) iputchar('B');
        if(!(rbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz)) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbget");
          err = true;
        }
        tcfree(rbuf);
        break;
      case 12:
        if(id == 0) iputchar('C');
        if(!(rbuf = tcbdbget2(bdb, kbuf)) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbget2");
          err = true;
        }
        tcfree(rbuf);
        break;
      case 13:
        if(id == 0) iputchar('D');
        if(!tcbdbget3(bdb, kbuf, ksiz, &vsiz) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbget3");
          err = true;
        }
        break;
      case 14:
        if(id == 0) iputchar('E');
        if(myrand(rnum / 50) == 0){
          switch(myrand(5)){
            case 0:
              if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurfirst");
                err = true;
              }
              break;
            case 1:
              if(!tcbdbcurlast(cur) && tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurlast");
                err = true;
              }
              break;
            default:
              if(!tcbdbcurjump(cur, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurjump");
                err = true;
              }
              break;
          }
        }
        TCXSTR *ikey = tcxstrnew();
        TCXSTR *ival = tcxstrnew();
        for(int j = myrand(rnum) / 1000 + 1; j >= 0; j--){
          if(j % 3 == 0){
            if(!tcbdbcurrec(cur, ikey, ival)){
              int ecode = tcbdbecode(bdb);
              if(ecode != TCEINVALID && ecode != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurrec");
                err = true;
              }
            }
          } else {
            int iksiz;
            if(!tcbdbcurkey3(cur, &iksiz)){
              int ecode = tcbdbecode(bdb);
              if(ecode != TCEINVALID && ecode != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurkey3");
                err = true;
              }
            }
          }
          if(myrand(5) == 0){
            if(!tcbdbcurprev(cur)){
              int ecode = tcbdbecode(bdb);
              if(ecode != TCEINVALID && ecode != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurprev");
                err = true;
              }
            }
          } else {
            if(!tcbdbcurnext(cur)){
              int ecode = tcbdbecode(bdb);
              if(ecode != TCEINVALID && ecode != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurnext");
                err = true;
              }
            }
          }
        }
        tcxstrdel(ival);
        tcxstrdel(ikey);
        break;
      default:
        if(id == 0) iputchar('@');
        if(tcbdbtranbegin(bdb)){
          if(myrand(2) == 0){
            if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
              eprint(bdb, __LINE__, "tcbdbput");
              err = true;
            }
            if(!nc) tcmapput(map, kbuf, ksiz, vbuf, vsiz);
          } else {
            if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
              eprint(bdb, __LINE__, "tcbdbout");
              err = true;
            }
            if(!nc) tcmapout(map, kbuf, ksiz);
          }
          if(nc && myrand(2) == 0){
            if(!tcbdbtranabort(bdb)){
              eprint(bdb, __LINE__, "tcbdbtranabort");
              err = true;
            }
          } else {
            if(!tcbdbtrancommit(bdb)){
              eprint(bdb, __LINE__, "tcbdbtrancommit");
              err = true;
            }
          }
        } else {
          eprint(bdb, __LINE__, "tcbdbtranbegin");
          err = true;
        }
        if(myrand(1000) == 0){
          if(!tcbdbforeach(bdb, iterfunc, NULL)){
            eprint(bdb, __LINE__, "tcbdbforeach");
            err = true;
          }
        }
        if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
        break;
    }
    if(!nc) tcglobalmutexunlock();
    if(id == 0){
      if(i % 50 == 0) iprintf(" (%08d)\n", i);
      if(id == 0 && i == rnum / 4){
        if(!tcbdboptimize(bdb, -1, -1, -1, -1, -1, -1) && tcbdbecode(bdb) != TCEINVALID){
          eprint(bdb, __LINE__, "tcbdboptimize");
          err = true;
        }
        if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbcurfirst");
          err = true;
        }
      }
    }
  }
  tcbdbcurdel(cur);
  return err ? "error" : NULL;
}


/* thread the typical function */
static void *threadtypical(void *targ){
  TCBDB *bdb = ((TARGTYPICAL *)targ)->bdb;
  int rnum = ((TARGTYPICAL *)targ)->rnum;
  bool nc = ((TARGTYPICAL *)targ)->nc;
  int rratio = ((TARGTYPICAL *)targ)->rratio;
  int id = ((TARGTYPICAL *)targ)->id;
  bool err = false;
  TCMAP *map = (!nc && id == 0) ? tcmapnew2(rnum + 1) : NULL;
  int base = id * rnum;
  int mrange = tclmax(50 + rratio, 100);
  BDBCUR *cur = tcbdbcurnew(bdb);
  for(int i = 1; !err && i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + myrandnd(i));
    int rnd = myrand(mrange);
    if(rnd < 10){
      if(!tcbdbput(bdb, buf, len, buf, len)){
        eprint(bdb, __LINE__, "tcbdbput");
        err = true;
      }
      if(map) tcmapput(map, buf, len, buf, len);
    } else if(rnd < 15){
      if(!tcbdbputkeep(bdb, buf, len, buf, len) && tcbdbecode(bdb) != TCEKEEP){
        eprint(bdb, __LINE__, "tcbdbputkeep");
        err = true;
      }
      if(map) tcmapputkeep(map, buf, len, buf, len);
    } else if(rnd < 20){
      if(!tcbdbputcat(bdb, buf, len, buf, len)){
        eprint(bdb, __LINE__, "tcbdbputcat");
        err = true;
      }
      if(map) tcmapputcat(map, buf, len, buf, len);
    } else if(rnd < 25){
      if(!tcbdbout(bdb, buf, len) && tcbdbecode(bdb) && tcbdbecode(bdb) != TCENOREC){
        eprint(bdb, __LINE__, "tcbdbout");
        err = true;
      }
      if(map) tcmapout(map, buf, len);
    } else if(rnd < 27){
      switch(myrand(3)){
        case 0:
          if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbcurfirst");
            err = true;
          }
          for(int j = 0; !err && j < 10; j++){
            int ksiz;
            char *kbuf = tcbdbcurkey(cur, &ksiz);
            if(kbuf){
              int vsiz;
              char *vbuf = tcbdbcurval(cur, &vsiz);
              if(vbuf){
                tcfree(vbuf);
              } else if(tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurval");
                err = true;
              }
              tcfree(kbuf);
            } else if(tcbdbecode(bdb) != TCENOREC){
              eprint(bdb, __LINE__, "tcbdbcurkey");
              err = true;
            }
            tcbdbcurnext(cur);
          }
          break;
        case 1:
          if(!tcbdbcurlast(cur) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbcurlast");
            err = true;
          }
          for(int j = 0; !err && j < 10; j++){
            int ksiz;
            char *kbuf = tcbdbcurkey(cur, &ksiz);
            if(kbuf){
              int vsiz;
              char *vbuf = tcbdbcurval(cur, &vsiz);
              if(vbuf){
                tcfree(vbuf);
              } else if(tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurval");
                err = true;
              }
              tcfree(kbuf);
            } else if(tcbdbecode(bdb) != TCENOREC){
              eprint(bdb, __LINE__, "tcbdbcurkey");
              err = true;
            }
            tcbdbcurprev(cur);
          }
          break;
        case 2:
          if(!tcbdbcurjump(cur, buf, len) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbcurjump");
            err = true;
          }
          for(int j = 0; !err && j < 10; j++){
            int ksiz;
            char *kbuf = tcbdbcurkey(cur, &ksiz);
            if(kbuf){
              int vsiz;
              char *vbuf = tcbdbcurval(cur, &vsiz);
              if(vbuf){
                tcfree(vbuf);
              } else if(tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurval");
                err = true;
              }
              tcfree(kbuf);
            } else if(tcbdbecode(bdb) != TCENOREC){
              eprint(bdb, __LINE__, "tcbdbcurkey");
              err = true;
            }
            tcbdbcurnext(cur);
          }
          break;
      }
    } else {
      int vsiz;
      char *vbuf = tcbdbget(bdb, buf, len, &vsiz);
      if(vbuf){
        if(map){
          int msiz;
          const char *mbuf = tcmapget(map, buf, len, &msiz);
          if(!mbuf || msiz != vsiz || memcmp(mbuf, vbuf, vsiz)){
            eprint(bdb, __LINE__, "(validation)");
            err = true;
          }
        }
        tcfree(vbuf);
      } else {
        if(tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbget");
          err = true;
        }
        if(map && tcmapget(map, buf, len, &vsiz)){
          eprint(bdb, __LINE__, "(validation)");
          err = true;
        }
      }
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  tcbdbcurdel(cur);
  if(map){
    tcmapiterinit(map);
    int ksiz;
    const char *kbuf;
    while(!err && (kbuf = tcmapiternext(map, &ksiz)) != NULL){
      int vsiz;
      char *vbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz);
      if(vbuf){
        int msiz;
        const char *mbuf = tcmapget(map, kbuf, ksiz, &msiz);
        if(!mbuf || msiz != vsiz || memcmp(mbuf, vbuf, vsiz)){
          eprint(bdb, __LINE__, "(validation)");
          err = true;
        }
        tcfree(vbuf);
      } else {
        eprint(bdb, __LINE__, "(validation)");
        err = true;
      }
    }
    tcmapdel(map);
  }
  return err ? "error" : NULL;
}


/* thread the race function */
static void *threadrace(void *targ){
  TCBDB *bdb = ((TARGRACE *)targ)->bdb;
  int rnum = ((TARGRACE *)targ)->rnum;
  int id = ((TARGRACE *)targ)->id;
  bool err = false;
  int mid = rnum * 2;
  for(int i = 1; !err && i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%d", myrandnd(i));
    int rnd = myrand(100);
    if(rnd < 10){
      if(!tcbdbputkeep(bdb, buf, len, buf, len) && tcbdbecode(bdb) != TCEKEEP){
        eprint(bdb, __LINE__, "tcbdbputkeep");
        err = true;
      }
    } else if(rnd < 20){
      if(!tcbdbputcat(bdb, buf, len, buf, len)){
        eprint(bdb, __LINE__, "tcbdbputcat");
        err = true;
      }
    } else if(rnd < 30){
      if(!tcbdbputdup(bdb, buf, len, buf, len)){
        eprint(bdb, __LINE__, "tcbdbputdup");
        err = true;
      }
    } else if(rnd < 40){
      if(!tcbdbputdupback(bdb, buf, len, buf, len)){
        eprint(bdb, __LINE__, "tcbdbputdupback");
        err = true;
      }
    } else if(rnd < 50){
      if(!tcbdbout(bdb, buf, len) && tcbdbecode(bdb) != TCENOREC){
        eprint(bdb, __LINE__, "tcbdbout");
        err = true;
      }
    } else {
      if(myrand(10) == 0){
        int rsiz = myrand(1024);
        char *rbuf = tcmalloc(rsiz + 1);
        for(int j = 0; j < rsiz; j++){
          rbuf[j] = myrand('z' - 'a') + 'a';
        }
        if(myrand(2) == 0){
          if(!tcbdbput(bdb, buf, len, rbuf, rsiz)){
            eprint(bdb, __LINE__, "tcbdbputcat");
            err = true;
          }
        } else {
          if(!tcbdbputcat(bdb, buf, len, rbuf, rsiz)){
            eprint(bdb, __LINE__, "tcbdbputcat");
            err = true;
          }
        }
        tcfree(rbuf);
      } else {
        if(!tcbdbput(bdb, buf, len, buf, len)){
          eprint(bdb, __LINE__, "tcbdbput");
          err = true;
        }
      }
    }
    if(id == 0){
      if(myrand(mid) == 0){
        iprintf("[v]");
        if(!tcbdbvanish(bdb)){
          eprint(bdb, __LINE__, "tcbdbvanish");
          err = true;
        }
      }
      if(myrand(mid) == 0){
        iprintf("[o]");
        if(!tcbdboptimize(bdb, -1, -1, myrand(rnum) + 1, myrand(10), myrand(10), 0)){
          eprint(bdb, __LINE__, "tcbdbvanish");
          err = true;
        }
      }
      if(myrand(mid) == 0){
        iprintf("[d]");
        if(!tcbdbdefrag(bdb, -1)){
          eprint(bdb, __LINE__, "tcbdbdefrag");
          err = true;
        }
      }
      if(myrand(mid) == 0){
        iprintf("[i]");
        BDBCUR *cur = tcbdbcurnew(bdb);
        if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbcurfirst");
          err = true;
        }
        char *kbuf;
        int ksiz;
        while((kbuf = tcbdbcurkey(cur, &ksiz)) != NULL){
          int vsiz;
          char *vbuf = tcbdbcurval(cur, &vsiz);
          if(vbuf){
            tcfree(vbuf);
          } else if(tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbget");
            err = true;
          }
          tcfree(kbuf);
          tcbdbcurnext(cur);
        }
        if(tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "(validation)");
          err = true;
        }
        tcbdbcurdel(cur);
      }
      if(rnum > 250 && i % (rnum / 250) == 0){
        iputchar('.');
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
  }
  return err ? "error" : NULL;
}



// END OF FILE
