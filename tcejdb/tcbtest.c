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
static void *pdprocfunc(const void *vbuf, int vsiz, int *sp, void *op);
static bool iterfunc(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int runrcat(int argc, char **argv);
static int runqueue(int argc, char **argv);
static int runmisc(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int procwrite(const char *path, int rnum, int lmemb, int nmemb, int bnum,
                     int apow, int fpow, bool mt, TCCMP cmp, int opts, int lcnum, int ncnum,
                     int xmsiz, int dfunit, int lsmax, int capnum, int omode, bool rnd);
static int procread(const char *path, bool mt, TCCMP cmp, int lcnum, int ncnum,
                    int xmsiz, int dfunit, int omode, bool wb, bool rnd);
static int procremove(const char *path, bool mt, TCCMP cmp, int lcnum, int ncnum,
                      int xmsiz, int dfunit, int omode, bool rnd);
static int procrcat(const char *path, int rnum,
                    int lmemb, int nmemb, int bnum, int apow, int fpow,
                    bool mt, TCCMP cmp, int opts, int lcnum, int ncnum, int xmsiz, int dfunit,
                    int lsmax, int capnum, int omode, int pnum, bool dai, bool dad,
                    bool rl, bool ru);
static int procqueue(const char *path, int rnum, int lmemb, int nmemb, int bnum,
                     int apow, int fpow, bool mt, TCCMP cmp, int opts,
                     int lcnum, int ncnum, int xmsiz, int dfunit, int lsmax, int capnum,
                     int omode);
static int procmisc(const char *path, int rnum, bool mt, int opts, int omode);
static int procwicked(const char *path, int rnum, bool mt, int opts, int omode);


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
  } else if(!strcmp(argv[1], "rcat")){
    rv = runrcat(argc, argv);
  } else if(!strcmp(argv[1], "queue")){
    rv = runqueue(argc, argv);
  } else if(!strcmp(argv[1], "misc")){
    rv = runmisc(argc, argv);
  } else if(!strcmp(argv[1], "wicked")){
    rv = runwicked(argc, argv);
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
  fprintf(stderr, "  %s write [-mt] [-cd|-ci|-cj] [-tl] [-td|-tb|-tt|-tx] [-lc num] [-nc num]"
          " [-xm num] [-df num] [-ls num] [-ca num] [-nl|-nb] [-rnd] path rnum"
          " [lmemb [nmemb [bnum [apow [fpow]]]]]\n", g_progname);
  fprintf(stderr, "  %s read [-mt] [-cd|-ci|-cj] [-lc num] [-nc num] [-xm num] [-df num]"
          " [-nl|-nb] [-wb] [-rnd] path\n", g_progname);
  fprintf(stderr, "  %s remove [-mt] [-cd|-ci|-cj] [-lc num] [-nc num] [-xm num] [-df num]"
          " [-nl|-nb] [-rnd] path\n", g_progname);
  fprintf(stderr, "  %s rcat [-mt] [-cd|-ci|-cj] [-tl] [-td|-tb|-tt|-tx] [-lc num] [-nc num]"
          " [-xm num] [-df num] [-ls num] [-ca num] [-nl|-nb] [-pn num] [-dai|-dad|-rl|-ru]"
          " path rnum [lmemb [nmemb [bnum [apow [fpow]]]]]\n", g_progname);
  fprintf(stderr, "  %s queue [-mt] [-cd|-ci|-cj] [-tl] [-td|-tb|-tt|-tx] [-lc num] [-nc num]"
          " [-xm num] [-df num] [-ls num] [-ca num] [-nl|-nb] path rnum"
          " [lmemb [nmemb [bnum [apow [fpow]]]]]\n", g_progname);
  fprintf(stderr, "  %s misc [-mt] [-tl] [-td|-tb|-tt|-tx] [-nl|-nb] path rnum\n", g_progname);
  fprintf(stderr, "  %s wicked [-mt] [-tl] [-td|-tb|-tt|-tx] [-nl|-nb] path rnum\n", g_progname);
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


/* print members of B+ tree database */
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


/* duplication callback function */
static void *pdprocfunc(const void *vbuf, int vsiz, int *sp, void *op){
  if(op){
    char *buf = NULL;
    int len = 0;
    switch((int)(intptr_t)op){
      case 1:
        len = vsiz + 1;
        buf = tcmalloc(len + 1);
        memset(buf, '*', len);
        break;
      case 2:
        buf = (void *)-1;
        break;
    }
    *sp = len;
    return buf;
  }
  if(myrand(4) == 0) return (void *)-1;
  if(myrand(2) == 0) return NULL;
  int len = myrand(RECBUFSIZ);
  char buf[RECBUFSIZ];
  memset(buf, '*', len);
  *sp = len;
  return tcmemdup(buf, len);
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
  char *rstr = NULL;
  char *lmstr = NULL;
  char *nmstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  bool mt = false;
  TCCMP cmp = NULL;
  int opts = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int lsmax = 0;
  int capnum = 0;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
      } else if(!strcmp(argv[i], "-tl")){
        opts |= BDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= BDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= BDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= BDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= BDBTEXCODEC;
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
      } else if(!strcmp(argv[i], "-ls")){
        if(++i >= argc) usage();
        lsmax = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-ca")){
        if(++i >= argc) usage();
        capnum = tcatoix(argv[i]);
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
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int lmemb = lmstr ? tcatoix(lmstr) : -1;
  int nmemb = nmstr ? tcatoix(nmstr) : -1;
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procwrite(path, rnum, lmemb, nmemb, bnum, apow, fpow,
                     mt, cmp, opts, lcnum, ncnum, xmsiz, dfunit, lsmax, capnum, omode, rnd);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *path = NULL;
  bool mt = false;
  TCCMP cmp = NULL;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  bool wb = false;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
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
    } else {
      usage();
    }
  }
  if(!path) usage();
  int rv = procread(path, mt, cmp, lcnum, ncnum, xmsiz, dfunit, omode, wb, rnd);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *path = NULL;
  bool mt = false;
  TCCMP cmp = NULL;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
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
    } else {
      usage();
    }
  }
  if(!path) usage();
  int rv = procremove(path, mt, cmp, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  return rv;
}


/* parse arguments of rcat command */
static int runrcat(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  char *lmstr = NULL;
  char *nmstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  bool mt = false;
  TCCMP cmp = NULL;
  int opts = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int lsmax = 0;
  int capnum = 0;
  int omode = 0;
  int pnum = 0;
  bool dai = false;
  bool dad = false;
  bool rl = false;
  bool ru = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
      } else if(!strcmp(argv[i], "-tl")){
        opts |= BDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= BDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= BDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= BDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= BDBTEXCODEC;
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
      } else if(!strcmp(argv[i], "-ls")){
        if(++i >= argc) usage();
        lsmax = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-ca")){
        if(++i >= argc) usage();
        capnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-pn")){
        if(++i >= argc) usage();
        pnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-dai")){
        dai = true;
      } else if(!strcmp(argv[i], "-dad")){
        dad = true;
      } else if(!strcmp(argv[i], "-rl")){
        rl = true;
      } else if(!strcmp(argv[i], "-ru")){
        ru = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
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
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int lmemb = lmstr ? tcatoix(lmstr) : -1;
  int nmemb = nmstr ? tcatoix(nmstr) : -1;
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procrcat(path, rnum, lmemb, nmemb, bnum, apow, fpow, mt, cmp, opts,
                    lcnum, ncnum, xmsiz, dfunit, lsmax, capnum, omode, pnum, dai, dad, rl, ru);
  return rv;
}


/* parse arguments of queue command */
static int runqueue(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  char *lmstr = NULL;
  char *nmstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  bool mt = false;
  TCCMP cmp = NULL;
  int opts = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int lsmax = 0;
  int capnum = 0;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
      } else if(!strcmp(argv[i], "-tl")){
        opts |= BDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= BDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= BDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= BDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= BDBTEXCODEC;
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
      } else if(!strcmp(argv[i], "-ls")){
        if(++i >= argc) usage();
        lsmax = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-ca")){
        if(++i >= argc) usage();
        capnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
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
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int lmemb = lmstr ? tcatoix(lmstr) : -1;
  int nmemb = nmstr ? tcatoix(nmstr) : -1;
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procqueue(path, rnum, lmemb, nmemb, bnum, apow, fpow,
                     mt, cmp, opts, lcnum, ncnum, xmsiz, dfunit, lsmax, capnum, omode);
  return rv;
}


/* parse arguments of misc command */
static int runmisc(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  bool mt = false;
  int opts = 0;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-tl")){
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
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = procmisc(path, rnum, mt, opts, omode);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  bool mt = false;
  int opts = 0;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-tl")){
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
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = procwicked(path, rnum, mt, opts, omode);
  return rv;
}


/* perform write command */
static int procwrite(const char *path, int rnum, int lmemb, int nmemb, int bnum,
                     int apow, int fpow, bool mt, TCCMP cmp, int opts, int lcnum, int ncnum,
                     int xmsiz, int dfunit, int lsmax, int capnum, int omode, bool rnd){
  iprintf("<Writing Test>\n  seed=%u  path=%s  rnum=%d  lmemb=%d  nmemb=%d  bnum=%d  apow=%d"
          "  fpow=%d  mt=%d  cmp=%p  opts=%d  lcnum=%d  ncnum=%d  xmsiz=%d  dfunit=%d  lsmax=%d"
          "  capnum=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, rnum, lmemb, nmemb, bnum, apow, fpow, mt, (void *)(intptr_t)cmp,
          opts, lcnum, ncnum, xmsiz, dfunit, lsmax, capnum, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(mt && !tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcmpfunc");
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
  if(!tcbdbsetcache(bdb, lcnum, ncnum)){
    eprint(bdb, __LINE__, "tcbdbsetcache");
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
  if(!tcbdbsetlsmax(bdb, lsmax)){
    eprint(bdb, __LINE__, "tcbdbsetlsmax");
    err = true;
  }
  if(!tcbdbsetcapnum(bdb, capnum)){
    eprint(bdb, __LINE__, "tcbdbsetcapnum");
    err = true;
  }
  if(!rnd) omode |= BDBOTRUNC;
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len;
    if(cmp == tccmpdecimal){
      len = sprintf(buf, "%d", rnd ? myrand(rnum) + 1 : i);
    } else if(cmp == tccmpint32){
      int32_t lnum = rnd ? myrand(rnum) + 1 : i;
      memcpy(buf, &lnum, sizeof(lnum));
      len = sizeof(lnum);
    } else if(cmp == tccmpint64){
      int64_t llnum = rnd ? myrand(rnum) + 1 : i;
      memcpy(buf, &llnum, sizeof(llnum));
      len = sizeof(llnum);
    } else {
      len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
    }
    if(!tcbdbput(bdb, buf, len, buf, len)){
      eprint(bdb, __LINE__, "tcbdbput");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
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
static int procread(const char *path, bool mt, TCCMP cmp, int lcnum, int ncnum,
                    int xmsiz, int dfunit, int omode, bool wb, bool rnd){
  iprintf("<Reading Test>\n  seed=%u  path=%s  mt=%d  cmp=%p  lcnum=%d  ncnum=%d"
          "  xmsiz=%d  dfunit=%d  omode=%d  wb=%d  rnd=%d\n\n",
          g_randseed, path, mt, (void *)(intptr_t)cmp, lcnum, ncnum, xmsiz, dfunit,
          omode, wb, rnd);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(mt && !tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcmpfunc");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(!tcbdbsetcache(bdb, lcnum, ncnum)){
    eprint(bdb, __LINE__, "tcbdbsetcache");
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
  int rnum = tcbdbrnum(bdb);
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz;
    if(cmp == tccmpdecimal){
      ksiz = sprintf(kbuf, "%d", rnd ? myrand(rnum) + 1 : i);
    } else if(cmp == tccmpint32){
      int32_t lnum = rnd ? myrand(rnum) + 1 : i;
      memcpy(kbuf, &lnum, sizeof(lnum));
      ksiz = sizeof(lnum);
    } else if(cmp == tccmpint64){
      int64_t llnum = rnd ? myrand(rnum) + 1 : i;
      memcpy(kbuf, &llnum, sizeof(llnum));
      ksiz = sizeof(llnum);
    } else {
      ksiz = sprintf(kbuf, "%08d", rnd ? myrand(rnum) + 1 : i);
    }
    int vsiz;
    if(wb){
      int vsiz;
      const char *vbuf = tcbdbget3(bdb, kbuf, ksiz, &vsiz);
      if(!vbuf && !(rnd && tcbdbecode(bdb) == TCENOREC)){
        eprint(bdb, __LINE__, "tcbdbget3");
        err = true;
        break;
      }
    } else {
      char *vbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz);
      if(!vbuf && !(rnd && tcbdbecode(bdb) == TCENOREC)){
        eprint(bdb, __LINE__, "tcbdbget");
        err = true;
        break;
      }
      tcfree(vbuf);
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
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
static int procremove(const char *path, bool mt, TCCMP cmp, int lcnum, int ncnum,
                      int xmsiz, int dfunit, int omode, bool rnd){
  iprintf("<Removing Test>\n  seed=%u  path=%s  mt=%d  cmp=%p  lcnum=%d  ncnum=%d"
          "  xmsiz=%d  dfunit=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, mt, (void *)(intptr_t)cmp, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(mt && !tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcmpfunc");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(!tcbdbsetcache(bdb, lcnum, ncnum)){
    eprint(bdb, __LINE__, "tcbdbsetcache");
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
  int rnum = tcbdbrnum(bdb);
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz;
    if(cmp == tccmpdecimal){
      ksiz = sprintf(kbuf, "%d", rnd ? myrand(rnum) + 1 : i);
    } else if(cmp == tccmpint32){
      int32_t lnum = rnd ? myrand(rnum) + 1 : i;
      memcpy(kbuf, &lnum, sizeof(lnum));
      ksiz = sizeof(lnum);
    } else if(cmp == tccmpint64){
      int64_t llnum = rnd ? myrand(rnum) + 1 : i;
      memcpy(kbuf, &llnum, sizeof(llnum));
      ksiz = sizeof(llnum);
    } else {
      ksiz = sprintf(kbuf, "%08d", rnd ? myrand(rnum) + 1 : i);
    }
    if(!tcbdbout(bdb, kbuf, ksiz) && !(rnd && tcbdbecode(bdb) == TCENOREC)){
      eprint(bdb, __LINE__, "tcbdbout");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
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


/* perform rcat command */
static int procrcat(const char *path, int rnum,
                    int lmemb, int nmemb, int bnum, int apow, int fpow,
                    bool mt, TCCMP cmp, int opts, int lcnum, int ncnum, int xmsiz, int dfunit,
                    int lsmax, int capnum, int omode, int pnum, bool dai, bool dad,
                    bool rl, bool ru){
  iprintf("<Random Concatenating Test>\n"
          "  seed=%u  path=%s  rnum=%d  lmemb=%d  nmemb=%d  bnum=%d  apow=%d  fpow=%d"
          "  mt=%d  cmp=%p  opts=%d  lcnum=%d  ncnum=%d  xmsiz=%d  dfunit=%d"
          "  lsmax=%d  capnum=%d  omode=%d  pnum=%d  dai=%d  dad=%d  rl=%d  ru=%d\n\n",
          g_randseed, path, rnum, lmemb, nmemb, bnum, apow, fpow, mt, (void *)(intptr_t)cmp,
          opts, lcnum, ncnum, xmsiz, dfunit, lsmax, capnum, omode, pnum, dai, dad, rl, ru);
  if(pnum < 1) pnum = rnum;
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(mt && !tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcmpfunc");
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
  if(!tcbdbsetcache(bdb, lcnum, ncnum)){
    eprint(bdb, __LINE__, "tcbdbsetcache");
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
  if(!tcbdbsetlsmax(bdb, lsmax)){
    eprint(bdb, __LINE__, "tcbdbsetlsmax");
    err = true;
  }
  if(!tcbdbsetcapnum(bdb, capnum)){
    eprint(bdb, __LINE__, "tcbdbsetcapnum");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    if(ru){
      char fmt[RECBUFSIZ];
      sprintf(fmt, "%%0%dd", myrand(RECBUFSIZ));
      char kbuf[RECBUFSIZ];
      int ksiz = sprintf(kbuf, fmt, myrand(pnum));
      switch(myrand(10)){
        case 0:
          if(!tcbdbput(bdb, kbuf, ksiz, kbuf, ksiz)){
            eprint(bdb, __LINE__, "tcbdbput");
            err = true;
          }
          break;
        case 1:
          if(!tcbdbputkeep(bdb, kbuf, ksiz, kbuf, ksiz) && tcbdbecode(bdb) != TCEKEEP){
            eprint(bdb, __LINE__, "tcbdbputkeep");
            err = true;
          }
          break;
        case 2:
          if(!tcbdbputdup(bdb, kbuf, ksiz, kbuf, ksiz) && tcbdbecode(bdb) != TCEKEEP){
            eprint(bdb, __LINE__, "tcbdbputkeep");
            err = true;
          }
          break;
        case 3:
          if(!tcbdbputdupback(bdb, kbuf, ksiz, kbuf, ksiz) && tcbdbecode(bdb) != TCEKEEP){
            eprint(bdb, __LINE__, "tcbdbputkeep");
            err = true;
          }
          break;
        case 4:
          if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbout");
            err = true;
          }
          break;
        case 5:
          if(tcbdbaddint(bdb, kbuf, ksiz, 1) == INT_MIN && tcbdbecode(bdb) != TCEKEEP){
            eprint(bdb, __LINE__, "tcbdbaddint");
            err = true;
          }
          break;
        case 6:
          if(isnan(tcbdbadddouble(bdb, kbuf, ksiz, 1.0)) && tcbdbecode(bdb) != TCEKEEP){
            eprint(bdb, __LINE__, "tcbdbadddouble");
            err = true;
          }
          break;
        case 7:
          if(myrand(2) == 0){
            if(!tcbdbputproc(bdb, kbuf, ksiz, kbuf, ksiz, pdprocfunc, NULL) &&
               tcbdbecode(bdb) != TCEKEEP){
              eprint(bdb, __LINE__, "tcbdbputproc");
              err = true;
            }
          } else {
            if(!tcbdbputproc(bdb, kbuf, ksiz, NULL, 0, pdprocfunc, NULL) &&
               tcbdbecode(bdb) != TCEKEEP && tcbdbecode(bdb) != TCENOREC){
              eprint(bdb, __LINE__, "tcbdbputproc");
              err = true;
            }
          }
          break;
        default:
          if(!tcbdbputcat(bdb, kbuf, ksiz, kbuf, ksiz)){
            eprint(bdb, __LINE__, "tcbdbputcat");
            err = true;
          }
          break;
      }
      if(err) break;
    } else {
      char kbuf[RECBUFSIZ];
      int ksiz;
      if(cmp == tccmpdecimal){
        ksiz = sprintf(kbuf, "%d", myrand(pnum));
      } else if(cmp == tccmpint32){
        int32_t lnum = myrand(pnum);
        memcpy(kbuf, &lnum, sizeof(lnum));
        ksiz = sizeof(lnum);
      } else if(cmp == tccmpint64){
        int64_t llnum = myrand(pnum);
        memcpy(kbuf, &llnum, sizeof(llnum));
        ksiz = sizeof(llnum);
      } else {
        ksiz = sprintf(kbuf, "%d", myrand(pnum));
      }
      if(dai){
        if(tcbdbaddint(bdb, kbuf, ksiz, myrand(3)) == INT_MIN){
          eprint(bdb, __LINE__, "tcbdbaddint");
          err = true;
          break;
        }
      } else if(dad){
        if(isnan(tcbdbadddouble(bdb, kbuf, ksiz, myrand(30) / 10.0))){
          eprint(bdb, __LINE__, "tcbdbadddouble");
          err = true;
          break;
        }
      } else if(rl){
        char vbuf[PATH_MAX];
        int vsiz = myrand(PATH_MAX);
        for(int j = 0; j < vsiz; j++){
          vbuf[j] = myrand(0x100);
        }
        if(!tcbdbputcat(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbputcat");
          err = true;
          break;
        }
      } else {
        if(!tcbdbputcat(bdb, kbuf, ksiz, kbuf, ksiz)){
          eprint(bdb, __LINE__, "tcbdbputcat");
          err = true;
          break;
        }
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
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


/* perform queue command */
static int procqueue(const char *path, int rnum, int lmemb, int nmemb, int bnum,
                     int apow, int fpow, bool mt, TCCMP cmp, int opts,
                     int lcnum, int ncnum, int xmsiz, int dfunit, int lsmax, int capnum,
                     int omode){
  iprintf("<Queueing Test>\n  seed=%u  path=%s  rnum=%d  lmemb=%d  nmemb=%d  bnum=%d  apow=%d"
          "  fpow=%d  mt=%d  cmp=%p  opts=%d  lcnum=%d  ncnum=%d  xmsiz=%d  dfunit=%d"
          "  lsmax=%d  capnum=%d  omode=%d\n\n",
          g_randseed, path, rnum, lmemb, nmemb, bnum, apow, fpow, mt, (void *)(intptr_t)cmp,
          opts, lcnum, ncnum, xmsiz, dfunit, lsmax, capnum, omode);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(mt && !tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcmpfunc");
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
  if(!tcbdbsetcache(bdb, lcnum, ncnum)){
    eprint(bdb, __LINE__, "tcbdbsetcache");
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
  if(!tcbdbsetlsmax(bdb, lsmax)){
    eprint(bdb, __LINE__, "tcbdbsetlsmax");
    err = true;
  }
  if(!tcbdbsetcapnum(bdb, capnum)){
    eprint(bdb, __LINE__, "tcbdbsetcapnum");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  int deqfreq = (lmemb > 0) ? lmemb * 10 : 256;
  BDBCUR *cur = tcbdbcurnew(bdb);
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len;
    if(cmp == tccmpdecimal){
      len = sprintf(buf, "%d", i);
    } else if(cmp == tccmpint32){
      int32_t lnum = i;
      memcpy(buf, &lnum, sizeof(lnum));
      len = sizeof(lnum);
    } else if(cmp == tccmpint64){
      int64_t llnum = i;
      memcpy(buf, &llnum, sizeof(llnum));
      len = sizeof(llnum);
    } else {
      len = sprintf(buf, "%08d", i);
    }
    if(!tcbdbput(bdb, buf, len, buf, len)){
      eprint(bdb, __LINE__, "tcbdbput");
      err = true;
      break;
    }
    if(myrand(deqfreq) == 0){
      if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
        eprint(bdb, __LINE__, "tcbdbcurfirst");
        err = true;
        break;
      }
      int num = myrand(deqfreq * 2 + 1);
      while(num >= 0){
        if(tcbdbcurout(cur)){
          num--;
        } else {
          if(tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbcurout");
            err = true;
          }
          break;
        }
      }
      if(err) break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
    eprint(bdb, __LINE__, "tcbdbcurfirst");
    err = true;
  }
  while(true){
    if(tcbdbcurout(cur)) continue;
    if(tcbdbecode(bdb) != TCENOREC){
      eprint(bdb, __LINE__, "tcbdbcurout");
      err = true;
    }
    break;
  }
  tcbdbcurdel(cur);
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


/* perform misc command */
static int procmisc(const char *path, int rnum, bool mt, int opts, int omode){
  iprintf("<Miscellaneous Test>\n  seed=%u  path=%s  rnum=%d  mt=%d  opts=%d  omode=%d\n\n",
          g_randseed, path, rnum, mt, opts, omode);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(mt && !tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(!tcbdbtune(bdb, 10, 10, rnum / 50, 100, -1, opts)){
    eprint(bdb, __LINE__, "tcbdbtune");
    err = true;
  }
  if(!tcbdbsetcache(bdb, 128, 256)){
    eprint(bdb, __LINE__, "tcbdbsetcache");
    err = true;
  }
  if(!tcbdbsetxmsiz(bdb, rnum)){
    eprint(bdb, __LINE__, "tcbdbsetxmsiz");
    err = true;
  }
  if(!tcbdbsetdfunit(bdb, 8)){
    eprint(bdb, __LINE__, "tcbdbsetdfunit");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  if(TCUSEPTHREAD){
    TCBDB *bdbdup = tcbdbnew();
    if(tcbdbopen(bdbdup, path, BDBOREADER)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
    } else if(tcbdbecode(bdbdup) != TCETHREAD){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
    }
    tcbdbdel(bdbdup);
  }
  iprintf("writing:\n");
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    if(!tcbdbputkeep(bdb, buf, len, buf, len)){
      eprint(bdb, __LINE__, "tcbdbputkeep");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("reading:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", i);
    int vsiz;
    char *vbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(bdb, __LINE__, "tcbdbget");
      err = true;
      break;
    } else if(vsiz != ksiz || memcmp(vbuf, kbuf, vsiz)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
      tcfree(vbuf);
      break;
    }
    tcfree(vbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(tcbdbrnum(bdb) != rnum){
    eprint(bdb, __LINE__, "(validation)");
    err = true;
  }
  iprintf("random writing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    char vbuf[RECBUFSIZ];
    int vsiz = myrand(RECBUFSIZ);
    memset(vbuf, '*', vsiz);
    if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
      eprint(bdb, __LINE__, "tcbdbput");
      err = true;
      break;
    }
    int rsiz;
    char *rbuf = tcbdbget(bdb, kbuf, ksiz, &rsiz);
    if(!rbuf){
      eprint(bdb, __LINE__, "tcbdbget");
      err = true;
      break;
    }
    if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
      tcfree(rbuf);
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
    tcfree(rbuf);
  }
  iprintf("word writing:\n");
  const char *words[] = {
    "a", "A", "bb", "BB", "ccc", "CCC", "dddd", "DDDD", "eeeee", "EEEEEE",
    "mikio", "hirabayashi", "tokyo", "cabinet", "hyper", "estraier", "19780211", "birth day",
    "one", "first", "two", "second", "three", "third", "four", "fourth", "five", "fifth",
    "_[1]_", "uno", "_[2]_", "dos", "_[3]_", "tres", "_[4]_", "cuatro", "_[5]_", "cinco",
    "[\xe5\xb9\xb3\xe6\x9e\x97\xe5\xb9\xb9\xe9\x9b\x84]", "[\xe9\xa6\xac\xe9\xb9\xbf]", NULL
  };
  for(int i = 0; words[i] != NULL; i += 2){
    const char *kbuf = words[i];
    int ksiz = strlen(kbuf);
    const char *vbuf = words[i+1];
    int vsiz = strlen(vbuf);
    if(!tcbdbputkeep(bdb, kbuf, ksiz, vbuf, vsiz)){
      eprint(bdb, __LINE__, "tcbdbputkeep");
      err = true;
      break;
    }
    if(rnum > 250) iputchar('.');
  }
  if(rnum > 250) iprintf(" (%08d)\n", (int)(sizeof(words) / sizeof(*words)));
  iprintf("random erasing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
      eprint(bdb, __LINE__, "tcbdbout");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("writing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "[%d]", i);
    char vbuf[RECBUFSIZ];
    int vsiz = i % RECBUFSIZ;
    memset(vbuf, '*', vsiz);
    if(!tcbdbputkeep(bdb, kbuf, ksiz, vbuf, vsiz)){
      eprint(bdb, __LINE__, "tcbdbputkeep");
      err = true;
      break;
    }
    if(vsiz < 1){
      char tbuf[PATH_MAX];
      for(int j = 0; j < PATH_MAX; j++){
        tbuf[j] = myrand(0x100);
      }
      if(!tcbdbput(bdb, kbuf, ksiz, tbuf, PATH_MAX)){
        eprint(bdb, __LINE__, "tcbdbput");
        err = true;
        break;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("erasing:\n");
  for(int i = 1; i <= rnum; i++){
    if(i % 2 == 1){
      char kbuf[RECBUFSIZ];
      int ksiz = sprintf(kbuf, "[%d]", i);
      if(!tcbdbout(bdb, kbuf, ksiz)){
        eprint(bdb, __LINE__, "tcbdbout");
        err = true;
        break;
      }
      if(tcbdbout(bdb, kbuf, ksiz) || tcbdbecode(bdb) != TCENOREC){
        eprint(bdb, __LINE__, "tcbdbout");
        err = true;
        break;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("random writing and reopening:\n");
  for(int i = 1; i <= rnum; i++){
    if(myrand(10) == 0){
      int ksiz, vsiz;
      char *kbuf, *vbuf;
      ksiz = (myrand(5) == 0) ? myrand(UINT16_MAX) : myrand(RECBUFSIZ);
      kbuf = tcmalloc(ksiz + 1);
      for(int j = 0; j < ksiz; j++){
        kbuf[j] = 128 + myrand(128);
      }
      vsiz = (myrand(5) == 0) ? myrand(UINT16_MAX) : myrand(RECBUFSIZ);
      vbuf = tcmalloc(vsiz + 1);
      for(int j = 0; j < vsiz; j++){
        vbuf[j] = myrand(256);
      }
      switch(myrand(5)){
        case 0:
          if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
            eprint(bdb, __LINE__, "tcbdbput");
            err = true;
          }
          break;
        case 1:
          if(!tcbdbputcat(bdb, kbuf, ksiz, vbuf, vsiz)){
            eprint(bdb, __LINE__, "tcbdbputcat");
            err = true;
          }
          break;
        case 2:
          if(!tcbdbputdup(bdb, kbuf, ksiz, vbuf, vsiz)){
            eprint(bdb, __LINE__, "tcbdbputdup");
            err = true;
          }
          break;
        case 3:
          if(!tcbdbputdupback(bdb, kbuf, ksiz, vbuf, vsiz)){
            eprint(bdb, __LINE__, "tcbdbputdupback");
            err = true;
          }
          break;
        default:
          if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbout");
            err = true;
          }
          break;
      }
      tcfree(vbuf);
      tcfree(kbuf);
    } else {
      char kbuf[RECBUFSIZ];
      int ksiz = myrand(RECBUFSIZ);
      memset(kbuf, '@', ksiz);
      char vbuf[RECBUFSIZ];
      int vsiz = myrand(RECBUFSIZ);
      memset(vbuf, '@', vsiz);
      if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
        eprint(bdb, __LINE__, "tcbdbputcat");
        err = true;
        break;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tcbdbclose(bdb)){
    eprint(bdb, __LINE__, "tcbdbclose");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  iprintf("checking:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "[%d]", i);
    int vsiz;
    char *vbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz);
    if(i % 2 == 0){
      if(!vbuf){
        eprint(bdb, __LINE__, "tcbdbget");
        err = true;
        break;
      }
      if(vsiz != i % RECBUFSIZ && vsiz != PATH_MAX){
        eprint(bdb, __LINE__, "(validation)");
        err = true;
        tcfree(vbuf);
        break;
      }
    } else {
      if(vbuf || tcbdbecode(bdb) != TCENOREC){
        eprint(bdb, __LINE__, "(validation)");
        err = true;
        tcfree(vbuf);
        break;
      }
    }
    tcfree(vbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("writing:\n");
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    if(!tcbdbput(bdb, buf, len, buf, len)){
      eprint(bdb, __LINE__, "tcbdbput");
      err = true;
      break;
    }
    if(i % 10 == 0){
      TCLIST *vals = tclistnew();
      for(int j = myrand(5) + 1; j >= 0; j--){
        tclistpush(vals, buf, len);
      }
      if(!tcbdbputdup3(bdb, buf, len, vals)){
        eprint(bdb, __LINE__, "tcbdbput3");
        err = true;
        break;
      }
      tclistdel(vals);
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("reading:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", i);
    int vsiz;
    char *vbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(bdb, __LINE__, "tcbdbget");
      err = true;
      break;
    } else if(vsiz != ksiz || memcmp(vbuf, kbuf, vsiz)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
      tcfree(vbuf);
      break;
    }
    tcfree(vbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("checking words:\n");
  for(int i = 0; words[i] != NULL; i += 2){
    const char *kbuf = words[i];
    int ksiz = strlen(kbuf);
    const char *vbuf = words[i+1];
    int vsiz = strlen(vbuf);
    int rsiz;
    char *rbuf = tcbdbget(bdb, kbuf, ksiz, &rsiz);
    if(!rbuf){
      eprint(bdb, __LINE__, "tcbdbget");
      err = true;
      break;
    } else if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
      tcfree(rbuf);
      break;
    }
    tcfree(rbuf);
    if(rnum > 250) iputchar('.');
  }
  if(rnum > 250) iprintf(" (%08d)\n", (int)(sizeof(words) / sizeof(*words)));
  iprintf("checking cursor:\n");
  BDBCUR *cur = tcbdbcurnew(bdb);
  int inum = 0;
  if(!tcbdbcurfirst(cur)){
    eprint(bdb, __LINE__, "tcbdbcurfirst");
    err = true;
  }
  char *kbuf;
  int ksiz;
  for(int i = 1; (kbuf = tcbdbcurkey(cur, &ksiz)) != NULL; i++, inum++){
    int vsiz;
    char *vbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(bdb, __LINE__, "tcbdbget");
      err = true;
      tcfree(kbuf);
      break;
    }
    tcfree(vbuf);
    tcfree(kbuf);
    tcbdbcurnext(cur);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  if(tcbdbecode(bdb) != TCENOREC || inum != tcbdbrnum(bdb)){
    eprint(bdb, __LINE__, "(validation)");
    err = true;
  }
  iprintf("cursor updating:\n");
  if(!tcbdbcurfirst(cur)){
    eprint(bdb, __LINE__, "tcbdbcurfirst");
    err = true;
  }
  inum = 0;
  for(int i = 1; !err && (kbuf = tcbdbcurkey(cur, &ksiz)) != NULL; i++, inum++){
    switch(myrand(6)){
      case 0:
        if(!tcbdbputdup(bdb, kbuf, ksiz, "0123456789", 10)){
          eprint(bdb, __LINE__, "tcbdbputcat");
          err = true;
        }
        break;
      case 1:
        if(!tcbdbout(bdb, kbuf, ksiz)){
          eprint(bdb, __LINE__, "tcbdbout");
          err = true;
        }
        break;
      case 2:
        if(!tcbdbcurput(cur, kbuf, ksiz, BDBCPCURRENT)){
          eprint(bdb, __LINE__, "tcbdbcurput");
          err = true;
        }
        break;
      case 3:
        if(!tcbdbcurput(cur, kbuf, ksiz, BDBCPBEFORE)){
          eprint(bdb, __LINE__, "tcbdbcurput");
          err = true;
        }
        break;
      case 4:
        if(!tcbdbcurput(cur, kbuf, ksiz, BDBCPAFTER)){
          eprint(bdb, __LINE__, "tcbdbcurput");
          err = true;
        }
        break;
      default:
        if(!tcbdbcurout(cur) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbcurout");
          err = true;
        }
        break;
    }
    tcfree(kbuf);
    tcbdbcurnext(cur);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  if(tcbdbecode(bdb) != TCENOREC){
    eprint(bdb, __LINE__, "(validation)");
    err = true;
  }
  if(myrand(10) == 0 && !tcbdbsync(bdb)){
    eprint(bdb, __LINE__, "tcbdbsync");
    err = true;
  }
  iprintf("cursor updating from empty:\n");
  tcbdbcurfirst(cur);
  inum = 0;
  for(int i = 1; (kbuf = tcbdbcurkey(cur, &ksiz)) != NULL; i++, inum++){
    tcfree(kbuf);
    if(!tcbdbcurout(cur) && tcbdbecode(bdb) != TCENOREC){
      eprint(bdb, __LINE__, "tcbdbcurout");
      err = true;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  if(tcbdbrnum(bdb) != 0){
    eprint(bdb, __LINE__, "(validation)");
    err = true;
  }
  if(!tcbdbput2(bdb, "one", "first")){
    eprint(bdb, __LINE__, "tcbdbput");
    err = true;
  }
  if(!tcbdbcurlast(cur)){
    eprint(bdb, __LINE__, "tcbdbcurlast");
    err = true;
  }
  if(!tcbdbcurput2(cur, "second", BDBCPCURRENT) || !tcbdbcurput2(cur, "first", BDBCPBEFORE) ||
     !tcbdbcurput2(cur, "zero", BDBCPBEFORE) || !tcbdbcurput2(cur, "top", BDBCPBEFORE)){
    eprint(bdb, __LINE__, "tcbdbcurput2");
    err = true;
  }
  if(!tcbdbcurlast(cur)){
    eprint(bdb, __LINE__, "tcbdbcurlast");
    err = true;
  }
  if(!tcbdbcurput2(cur, "third", BDBCPAFTER) || !tcbdbcurput2(cur, "fourth", BDBCPAFTER) ||
     !tcbdbcurput2(cur, "end", BDBCPCURRENT) ||  !tcbdbcurput2(cur, "bottom", BDBCPAFTER)){
    eprint(bdb, __LINE__, "tcbdbcurput2");
    err = true;
  }
  if(!tcbdbvanish(bdb)){
    eprint(bdb, __LINE__, "tcbdbvanish");
    err = true;
  }
  TCMAP *map = tcmapnew();
  iprintf("random writing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    char vbuf[RECBUFSIZ];
    int vsiz = sprintf(vbuf, "%d", myrand(rnum));
    switch(myrand(4)){
      case 0:
        if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbput");
          err = true;
        }
        tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 1:
        if(!tcbdbputkeep(bdb, kbuf, ksiz, vbuf, vsiz) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbputkeep");
          err = true;
        }
        tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 2:
        if(!tcbdbputcat(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbputcat");
          err = true;
        }
        tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 3:
        if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbout");
          err = true;
        }
        tcmapout(map, kbuf, ksiz);
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(myrand(4) == 0 && !tcbdbdefrag(bdb, 0)){
    eprint(bdb, __LINE__, "tcbdbdefrag");
    err = true;
  }
  if(myrand(4) == 0 && !tcbdbcacheclear(bdb)){
    eprint(bdb, __LINE__, "tcbdbcacheclear");
    err = true;
  }
  iprintf("checking transaction commit:\n");
  if(!tcbdbtranbegin(bdb)){
    eprint(bdb, __LINE__, "tcbdbtranbegin");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    char vbuf[RECBUFSIZ];
    int vsiz = sprintf(vbuf, "[%d]", myrand(rnum));
    switch(myrand(7)){
      case 0:
        if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbput");
          err = true;
        }
        tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 1:
        if(!tcbdbputkeep(bdb, kbuf, ksiz, vbuf, vsiz) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbputkeep");
          err = true;
        }
        tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 2:
        if(!tcbdbputcat(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbputcat");
          err = true;
        }
        tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 3:
        if(tcbdbaddint(bdb, kbuf, ksiz, 1) == INT_MIN && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbaddint");
          err = true;
        }
        tcmapaddint(map, kbuf, ksiz, 1);
        break;
      case 4:
        if(isnan(tcbdbadddouble(bdb, kbuf, ksiz, 1.0)) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbadddouble");
          err = true;
        }
        tcmapadddouble(map, kbuf, ksiz, 1.0);
        break;
      case 5:
        if(myrand(2) == 0){
          void *op = (void *)(intptr_t)(myrand(3) + 1);
          if(!tcbdbputproc(bdb, kbuf, ksiz, vbuf, vsiz, pdprocfunc, op) &&
             tcbdbecode(bdb) != TCEKEEP){
            eprint(bdb, __LINE__, "tcbdbputproc");
            err = true;
          }
          tcmapputproc(map, kbuf, ksiz, vbuf, vsiz, pdprocfunc, op);
        } else {
          vsiz = myrand(10);
          void *op = (void *)(intptr_t)(myrand(3) + 1);
          if(!tcbdbputproc(bdb, kbuf, ksiz, NULL, vsiz, pdprocfunc, op) &&
             tcbdbecode(bdb) != TCEKEEP && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbputproc");
            err = true;
          }
          tcmapputproc(map, kbuf, ksiz, NULL, vsiz, pdprocfunc, op);
        }
        break;
      case 6:
        if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbout");
          err = true;
        }
        tcmapout(map, kbuf, ksiz);
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tcbdbtrancommit(bdb)){
    eprint(bdb, __LINE__, "tcbdbtrancommit");
    err = true;
  }
  iprintf("checking transaction abort:\n");
  uint64_t ornum = tcbdbrnum(bdb);
  uint64_t ofsiz = tcbdbfsiz(bdb);
  if(!tcbdbtranbegin(bdb)){
    eprint(bdb, __LINE__, "tcbdbtranbegin");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    char vbuf[RECBUFSIZ];
    int vsiz = sprintf(vbuf, "((%d))", myrand(rnum));
    switch(myrand(8)){
      case 0:
        if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbput");
          err = true;
        }
        break;
      case 1:
        if(!tcbdbputkeep(bdb, kbuf, ksiz, vbuf, vsiz) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbputkeep");
          err = true;
        }
        break;
      case 2:
        if(!tcbdbputcat(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbputcat");
          err = true;
        }
        break;
      case 3:
        if(!tcbdbputdup(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbputdup");
          err = true;
        }
        break;
      case 4:
        if(tcbdbaddint(bdb, kbuf, ksiz, 1) == INT_MIN && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbaddint");
          err = true;
        }
        break;
      case 5:
        if(isnan(tcbdbadddouble(bdb, kbuf, ksiz, 1.0)) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbadddouble");
          err = true;
        }
        break;
      case 6:
        if(myrand(2) == 0){
          void *op = (void *)(intptr_t)(myrand(3) + 1);
          if(!tcbdbputproc(bdb, kbuf, ksiz, vbuf, vsiz, pdprocfunc, op) &&
             tcbdbecode(bdb) != TCEKEEP){
            eprint(bdb, __LINE__, "tcbdbputproc");
            err = true;
          }
        } else {
          vsiz = myrand(10);
          void *op = (void *)(intptr_t)(myrand(3) + 1);
          if(!tcbdbputproc(bdb, kbuf, ksiz, NULL, vsiz, pdprocfunc, op) &&
             tcbdbecode(bdb) != TCEKEEP && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbputproc");
            err = true;
          }
        }
        break;
      case 7:
        if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbout");
          err = true;
        }
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tcbdbtranabort(bdb)){
    eprint(bdb, __LINE__, "tcbdbtranabort");
    err = true;
  }
  iprintf("checking consistency:\n");
  if(tcbdbrnum(bdb) != ornum || tcbdbfsiz(bdb) != ofsiz || tcbdbrnum(bdb) != tcmaprnum(map)){
    eprint(bdb, __LINE__, "(validation)");
    err = true;
  }
  inum = 0;
  tcmapiterinit(map);
  const char *tkbuf;
  int tksiz;
  for(int i = 1; (tkbuf = tcmapiternext(map, &tksiz)) != NULL; i++, inum++){
    int tvsiz;
    const char *tvbuf = tcmapiterval(tkbuf, &tvsiz);
    int rsiz;
    char *rbuf = tcbdbget(bdb, tkbuf, tksiz, &rsiz);
    if(!rbuf || rsiz != tvsiz || memcmp(rbuf, tvbuf, rsiz)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
      tcfree(rbuf);
      break;
    }
    tcfree(rbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  inum = 0;
  if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
    eprint(bdb, __LINE__, "tcbdbcurfirst");
    err = true;
  }
  for(int i = 1; (kbuf = tcbdbcurkey(cur, &ksiz)) != NULL; i++, inum++){
    int vsiz;
    char *vbuf = tcbdbcurval(cur, &vsiz);
    int rsiz;
    const char *rbuf = tcmapget(map, kbuf, ksiz, &rsiz);
    if(!rbuf || rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
      tcfree(vbuf);
      tcfree(kbuf);
      break;
    }
    tcfree(vbuf);
    tcfree(kbuf);
    tcbdbcurnext(cur);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  tcmapdel(map);
  if(!tcbdbvanish(bdb)){
    eprint(bdb, __LINE__, "tcbdbvanish");
    err = true;
  }
  if(!tcbdbtranbegin(bdb)){
    eprint(bdb, __LINE__, "tcbdbtranbegin");
    err = true;
  }
  if(!tcbdbput2(bdb, "mikio", "hirabayashi")){
    eprint(bdb, __LINE__, "tcbdbput2");
    err = true;
  }
  for(int i = 0; i < 10; i++){
    char buf[RECBUFSIZ];
    int size = sprintf(buf, "%d", myrand(rnum));
    if(!tcbdbput(bdb, buf, size, buf, size)){
      eprint(bdb, __LINE__, "tcbdbput");
      err = true;
    }
  }
  for(int i = myrand(3) + 1; i < PATH_MAX; i = i * 2 + myrand(3)){
    char vbuf[i];
    memset(vbuf, '@', i - 1);
    vbuf[i-1] = '\0';
    if(!tcbdbput2(bdb, "mikio", vbuf)){
      eprint(bdb, __LINE__, "tcbdbput2");
      err = true;
    }
  }
  if(!tcbdbforeach(bdb, iterfunc, NULL)){
    eprint(bdb, __LINE__, "tcbdbforeach");
    err = true;
  }
  tcbdbcurdel(cur);
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
static int procwicked(const char *path, int rnum, bool mt, int opts, int omode){
  iprintf("<Wicked Writing Test>\n  seed=%u  path=%s  rnum=%d  mt=%d  opts=%d  omode=%d\n\n",
          g_randseed, path, rnum, mt, opts, omode);
  bool err = false;
  double stime = tctime();
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(mt && !tcbdbsetmutex(bdb)){
    eprint(bdb, __LINE__, "tcbdbsetmutex");
    err = true;
  }
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
    eprint(bdb, __LINE__, "tcbdbsetcodecfunc");
    err = true;
  }
  if(!tcbdbtune(bdb, 10, 10, rnum / 50, 100, -1, opts)){
    eprint(bdb, __LINE__, "tcbdbtune");
    err = true;
  }
  if(!tcbdbsetcache(bdb, 128, 256)){
    eprint(bdb, __LINE__, "tcbdbsetcache");
    err = true;
  }
  if(!tcbdbsetxmsiz(bdb, rnum)){
    eprint(bdb, __LINE__, "tcbdbsetxmsiz");
    err = true;
  }
  if(!tcbdbsetdfunit(bdb, 8)){
    eprint(bdb, __LINE__, "tcbdbsetdfunit");
    err = true;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC | omode)){
    eprint(bdb, __LINE__, "tcbdbopen");
    err = true;
  }
  BDBCUR *cur = tcbdbcurnew(bdb);
  if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
    eprint(bdb, __LINE__, "tcbdbcurfirst");
    err = true;
  }
  TCMAP *map = tcmapnew2(rnum / 5);
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    char vbuf[RECBUFSIZ];
    int vsiz = myrand(RECBUFSIZ);
    memset(vbuf, '*', vsiz);
    vbuf[vsiz] = '\0';
    char *rbuf;
    switch(myrand(16)){
      case 0:
        iputchar('0');
        if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbput");
          err = true;
        }
        tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 1:
        iputchar('1');
        if(!tcbdbput2(bdb, kbuf, vbuf)){
          eprint(bdb, __LINE__, "tcbdbput2");
          err = true;
        }
        tcmapput2(map, kbuf, vbuf);
        break;
      case 2:
        iputchar('2');
        if(!tcbdbputkeep(bdb, kbuf, ksiz, vbuf, vsiz) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbputkeep");
          err = true;
        }
        tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 3:
        iputchar('3');
        if(!tcbdbputkeep2(bdb, kbuf, vbuf) && tcbdbecode(bdb) != TCEKEEP){
          eprint(bdb, __LINE__, "tcbdbputkeep2");
          err = true;
        }
        tcmapputkeep2(map, kbuf, vbuf);
        break;
      case 4:
        iputchar('4');
        if(!tcbdbputcat(bdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(bdb, __LINE__, "tcbdbputcat");
          err = true;
        }
        tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 5:
        iputchar('5');
        if(!tcbdbputcat2(bdb, kbuf, vbuf)){
          eprint(bdb, __LINE__, "tcbdbputcat2");
          err = true;
        }
        tcmapputcat2(map, kbuf, vbuf);
        break;
      case 6:
        iputchar('6');
        if(myrand(10) == 0){
          if(!tcbdbout(bdb, kbuf, ksiz) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbout");
            err = true;
          }
          tcmapout(map, kbuf, ksiz);
        }
        break;
      case 7:
        iputchar('7');
        if(myrand(10) == 0){
          if(!tcbdbout2(bdb, kbuf) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbout2");
            err = true;
          }
          tcmapout2(map, kbuf);
        }
        break;
      case 8:
        iputchar('8');
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
        tcmapput(map, kbuf, ksiz, rbuf, vsiz);
        tcfree(rbuf);
        break;
      case 9:
        iputchar('9');
        if(!(rbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz)) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbget");
          err = true;
        }
        tcfree(rbuf);
        break;
      case 10:
        iputchar('A');
        if(!(rbuf = tcbdbget2(bdb, kbuf)) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbget2");
          err = true;
        }
        tcfree(rbuf);
        break;
      case 11:
        iputchar('B');
        if(myrand(1) == 0) vsiz = 1;
        if(!tcbdbget3(bdb, kbuf, ksiz, &vsiz) && tcbdbecode(bdb) != TCENOREC){
          eprint(bdb, __LINE__, "tcbdbget3");
          err = true;
        }
        break;
      case 12:
        iputchar('C');
        if(myrand(rnum / 50) == 0){
          if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbcurfirst");
            err = true;
          }
        }
        TCXSTR *ikey = tcxstrnew();
        TCXSTR *ival = tcxstrnew();
        for(int j = myrand(rnum) / 1000 + 1; j >= 0; j--){
          if(j % 3 == 0){
            if(tcbdbcurrec(cur, ikey, ival)){
              if(tcbdbvnum(bdb, tcxstrptr(ikey), tcxstrsize(ikey)) != 1){
                eprint(bdb, __LINE__, "(validation)");
                err = true;
              }
              if(tcxstrsize(ival) != tcbdbvsiz(bdb, tcxstrptr(ikey), tcxstrsize(ikey))){
                eprint(bdb, __LINE__, "(validation)");
                err = true;
              }
            } else {
              int ecode = tcbdbecode(bdb);
              if(ecode != TCEINVALID && ecode != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurrec");
                err = true;
              }
            }
          } else {
            int iksiz;
            char *ikbuf = tcbdbcurkey(cur, &iksiz);
            if(ikbuf){
              tcfree(ikbuf);
            } else {
              int ecode = tcbdbecode(bdb);
              if(ecode != TCEINVALID && ecode != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurkey");
                err = true;
              }
            }
          }
          tcbdbcurnext(cur);
        }
        tcxstrdel(ival);
        tcxstrdel(ikey);
        break;
      default:
        iputchar('@');
        if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
        if(myrand(rnum / 32 + 1) == 0){
          if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
            eprint(bdb, __LINE__, "tcbdbcurfirst");
            err = true;
          }
          int cnt = myrand(30);
          for(int j = 0; j < rnum && !err; j++){
            ksiz = sprintf(kbuf, "%d", i + j);
            if(myrand(4) == 0){
              if(tcbdbout3(bdb, kbuf, ksiz)){
                cnt--;
              } else if(tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbout3");
                err = true;
              }
              tcmapout(map, kbuf, ksiz);
            } else if(myrand(30) == 0){
              int tksiz;
              char *tkbuf = tcbdbcurkey(cur, &tksiz);
              if(tkbuf){
                if(tcbdbcurout(cur)){
                  cnt--;
                } else {
                  eprint(bdb, __LINE__, "tcbdbcurout");
                  err = true;
                }
                tcmapout(map, tkbuf, tksiz);
                tcfree(tkbuf);
              } else if(tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbcurkey");
                err = true;
              }
            } else {
              if(tcbdbout(bdb, kbuf, ksiz)){
                cnt--;
              } else if(tcbdbecode(bdb) != TCENOREC){
                eprint(bdb, __LINE__, "tcbdbout");
                err = true;
              }
              tcmapout(map, kbuf, ksiz);
            }
            if(cnt < 0) break;
          }
        }
        break;
    }
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
    if(i == rnum / 2){
      if(!tcbdbclose(bdb)){
        eprint(bdb, __LINE__, "tcbdbclose");
        err = true;
      }
      if(!tcbdbopen(bdb, path, BDBOWRITER | omode)){
        eprint(bdb, __LINE__, "tcbdbopen");
        err = true;
      }
    } else if(i == rnum / 4){
      char *npath = tcsprintf("%s-tmp", path);
      if(!tcbdbcopy(bdb, npath)){
        eprint(bdb, __LINE__, "tcbdbcopy");
        err = true;
      }
      TCBDB *nbdb = tcbdbnew();
      if(!tcbdbsetcodecfunc(nbdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
        eprint(nbdb, __LINE__, "tcbdbsetcodecfunc");
        err = true;
      }
      if(!tcbdbopen(nbdb, npath, BDBOREADER | omode)){
        eprint(nbdb, __LINE__, "tcbdbopen");
        err = true;
      }
      tcbdbdel(nbdb);
      unlink(npath);
      tcfree(npath);
      if(!tcbdboptimize(bdb, -1, -1, -1, -1, -1, -1)){
        eprint(bdb, __LINE__, "tcbdboptimize");
        err = true;
      }
      if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
        eprint(bdb, __LINE__, "tcbdbcurfirst");
        err = true;
      }
    } else if(i == rnum / 8){
      if(!tcbdbtranbegin(bdb)){
        eprint(bdb, __LINE__, "tcbdbtranbegin");
        err = true;
      }
    } else if(i == rnum / 8 + rnum / 16){
      if(!tcbdbtrancommit(bdb)){
        eprint(bdb, __LINE__, "tcbdbtrancommit");
        err = true;
      }
    }
  }
  if(rnum % 50 > 0) iprintf(" (%08d)\n", rnum);
  if(!tcbdbsync(bdb)){
    eprint(bdb, __LINE__, "tcbdbsync");
    err = true;
  }
  if(tcbdbrnum(bdb) != tcmaprnum(map)){
    eprint(bdb, __LINE__, "(validation)");
    err = true;
  }
  for(int i = 1; i <= rnum && !err; i++){
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
  tcmapiterinit(map);
  int ksiz;
  const char *kbuf;
  for(int i = 1; (kbuf = tcmapiternext(map, &ksiz)) != NULL; i++){
    iputchar('+');
    int vsiz;
    const char *vbuf = tcmapiterval(kbuf, &vsiz);
    int rsiz;
    char *rbuf = tcbdbget(bdb, kbuf, ksiz, &rsiz);
    if(!rbuf){
      eprint(bdb, __LINE__, "tcbdbget");
      err = true;
    } else if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(bdb, __LINE__, "(validation)");
      err = true;
    }
    tcfree(rbuf);
    if(!tcbdbout(bdb, kbuf, ksiz)){
      eprint(bdb, __LINE__, "tcbdbout");
      err = true;
    }
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
  }
  int mrnum = tcmaprnum(map);
  if(mrnum % 50 > 0) iprintf(" (%08d)\n", mrnum);
  if(tcbdbrnum(bdb) != 0){
    eprint(bdb, __LINE__, "(validation)");
    err = true;
  }
  tcbdbcurdel(cur);
  iprintf("record number: %llu\n", (unsigned long long)tcbdbrnum(bdb));
  iprintf("size: %llu\n", (unsigned long long)tcbdbfsiz(bdb));
  mprint(bdb);
  sysprint();
  tcmapdel(map);
  if(!tcbdbclose(bdb)){
    eprint(bdb, __LINE__, "tcbdbclose");
    err = true;
  }
  tcbdbdel(bdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}



// END OF FILE
