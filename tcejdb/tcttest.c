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
static void *pdprocfunc(const void *vbuf, int vsiz, int *sp, void *op);
static bool iterfunc(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int runrcat(int argc, char **argv);
static int runmisc(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int procwrite(const char *path, int rnum, int bnum, int apow, int fpow,
                     bool mt, int opts, int rcnum, int lcnum, int ncnum, int xmsiz, int dfunit,
                     int iflags, int omode, bool rnd);
static int procread(const char *path, bool mt, int rcnum, int lcnum, int ncnum,
                    int xmsiz, int dfunit, int omode, bool rnd);
static int procremove(const char *path, bool mt, int rcnum, int lcnum, int ncnum,
                      int xmsiz, int dfunit, int omode, bool rnd);
static int procrcat(const char *path, int rnum, int bnum, int apow, int fpow,
                    bool mt, int opts, int rcnum, int lcnum, int ncnum, int xmsiz, int dfunit,
                    int iflags, int omode, int pnum, bool dai, bool dad, bool rl, bool ru);
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
  fprintf(stderr, "%s: test cases of the table database API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write [-mt] [-tl] [-td|-tb|-tt|-tx] [-rc num] [-lc num] [-nc num]"
          " [-xm num] [-df num] [-ip] [-is] [-in] [-it] [-if] [-ix] [-nl|-nb] [-rnd]"
          " path rnum [bnum [apow [fpow]]]\n", g_progname);
  fprintf(stderr, "  %s read [-mt] [-rc num] [-lc num] [-nc num] [-xm num] [-df num]"
          " [-nl|-nb] [-rnd] path\n", g_progname);
  fprintf(stderr, "  %s remove [-mt] [-rc num] [-lc num] [-nc num] [-xm num] [-df num]"
          " [-nl|-nb] [-rnd] path\n", g_progname);
  fprintf(stderr, "  %s rcat [-mt] [-tl] [-td|-tb|-tt|-tx] [-rc num] [-lc num] [-nc num]"
          " [-xm num] [-df num] [-ip] [-is] [-in] [-it] [-if] [-ix] [-nl|-nb] [-pn num]"
          " [-dai|-dad|-rl|-ru] path rnum [bnum [apow [fpow]]]\n", g_progname);
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


/* duplication callback function */
static void *pdprocfunc(const void *vbuf, int vsiz, int *sp, void *op){
  if(myrand(4) == 0) return (void *)-1;
  if(myrand(2) == 0) return NULL;
  int len = myrand(RECBUFSIZ - 5);
  char buf[RECBUFSIZ];
  memcpy(buf, "proc", 5);
  memset(buf + 5, '*', len);
  *sp = len + 5;
  return tcmemdup(buf, len + 5);
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
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  bool mt = false;
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
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-tl")){
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
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procwrite(path, rnum, bnum, apow, fpow, mt, opts, rcnum, lcnum, ncnum, xmsiz, dfunit,
                     iflags, omode, rnd);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *path = NULL;
  bool mt = false;
  int rcnum = 0;
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
  int rv = procread(path, mt, rcnum, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *path = NULL;
  bool mt = false;
  int rcnum = 0;
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
  int rv = procremove(path, mt, rcnum, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  return rv;
}


/* parse arguments of rcat command */
static int runrcat(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  bool mt = false;
  int opts = 0;
  int rcnum = 0;
  int lcnum = 0;
  int ncnum = 0;
  int xmsiz = -1;
  int dfunit = 0;
  int iflags = 0;
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
      } else if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-xm")){
        if(++i >= argc) usage();
        xmsiz = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-df")){
        if(++i >= argc) usage();
        dfunit = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-rc")){
        if(++i >= argc) usage();
        rcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-lc")){
        if(++i >= argc) usage();
        lcnum = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-nc")){
        if(++i >= argc) usage();
        ncnum = tcatoix(argv[i]);
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
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procrcat(path, rnum, bnum, apow, fpow, mt, opts, rcnum, lcnum, ncnum, xmsiz, dfunit,
                    iflags, omode, pnum, dai, dad, rl, ru);
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
static int procwrite(const char *path, int rnum, int bnum, int apow, int fpow,
                     bool mt, int opts, int rcnum, int lcnum, int ncnum, int xmsiz, int dfunit,
                     int iflags, int omode, bool rnd){
  iprintf("<Writing Test>\n  seed=%u  path=%s  rnum=%d  bnum=%d  apow=%d  fpow=%d  mt=%d"
          "  opts=%d  rcnum=%d  lcnum=%d  ncnum=%d  xmsiz=%d  dfunit=%d  iflags=%d"
          "  omode=%d  rnd=%d\n\n",
          g_randseed, path, rnum, bnum, apow, fpow, mt, opts, rcnum, lcnum, ncnum, xmsiz, dfunit,
          iflags, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
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
  if(!rnd) omode |= TDBOTRUNC;
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | omode)){
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
  for(int i = 1; i <= rnum; i++){
    int id = rnd ? myrand(rnum) + 1 : (int)tctdbgenuid(tdb);
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", id);
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
    if(!tctdbput(tdb, pkbuf, pksiz, cols)){
      eprint(tdb, __LINE__, "tctdbput");
      err = true;
      break;
    }
    tcmapdel(cols);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
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
static int procread(const char *path, bool mt, int rcnum, int lcnum, int ncnum,
                    int xmsiz, int dfunit, int omode, bool rnd){
  iprintf("<Reading Test>\n  seed=%u  path=%s  mt=%d  rcnum=%d  lcnum=%d  ncnum=%d"
          "  xmsiz=%d  dfunit=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, mt, rcnum, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
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
  int rnum = tctdbrnum(tdb);
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", rnd ? myrand(rnum) + 1 : i);
    TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
    if(cols){
      tcmapdel(cols);
    } else if(!rnd || tctdbecode(tdb) != TCENOREC){
      eprint(tdb, __LINE__, "tctdbget");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
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
static int procremove(const char *path, bool mt, int rcnum, int lcnum, int ncnum,
                      int xmsiz, int dfunit, int omode, bool rnd){
  iprintf("<Removing Test>\n  seed=%u  path=%s  mt=%d  rcnum=%d  lcnum=%d  ncnum=%d"
          "  xmsiz=%d  dfunit=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, mt, rcnum, lcnum, ncnum, xmsiz, dfunit, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
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
  int rnum = tctdbrnum(tdb);
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", rnd ? myrand(rnum) + 1 : i);
    if(!tctdbout(tdb, pkbuf, pksiz) && !(rnd && tctdbecode(tdb) == TCENOREC)){
      eprint(tdb, __LINE__, "tctdbout");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
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


/* perform rcat command */
static int procrcat(const char *path, int rnum, int bnum, int apow, int fpow,
                    bool mt, int opts, int rcnum, int lcnum, int ncnum, int xmsiz, int dfunit,
                    int iflags, int omode, int pnum, bool dai, bool dad, bool rl, bool ru){
  iprintf("<Random Concatenating Test>\n"
          "  seed=%u  path=%s  rnum=%d  bnum=%d  apow=%d  fpow=%d  mt=%d  opts=%d"
          "  rcnum=%d  lcnum=%d  ncnum=%d  xmsiz=%d  dfunit=%d  iflags=%d"
          "  omode=%d  pnum=%d  dai=%d  dad=%d  rl=%d  ru=%d\n\n",
          g_randseed, path, rnum, bnum, apow, fpow, mt, opts, rcnum, lcnum, rcnum, xmsiz, dfunit,
          iflags, omode, pnum, dai, dad, rl, ru);
  if(pnum < 1) pnum = rnum;
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
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
  for(int i = 1; i <= rnum; i++){
    int id = myrand(pnum) + 1;
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", id);
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
    int nsiz = sprintf(nbuf, "c%d", myrand(pnum) + 1);
    vsiz = sprintf(vbuf, "%d", myrand(rnum) + 1);
    tcmapput(cols, nbuf, nsiz, vbuf, vsiz);
    if(ru){
      switch(myrand(8)){
        case 0:
          if(!tctdbput(tdb, pkbuf, pksiz, cols)){
            eprint(tdb, __LINE__, "tctdbput");
            err = true;
          }
          break;
        case 1:
          if(!tctdbputkeep(tdb, pkbuf, pksiz, cols) && tctdbecode(tdb) != TCEKEEP){
            eprint(tdb, __LINE__, "tctdbputkeep");
            err = true;
          }
          break;
        case 2:
          if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
            eprint(tdb, __LINE__, "tctdbout");
            err = true;
          }
          break;
        case 3:
          if(tctdbaddint(tdb, pkbuf, pksiz, 1) == INT_MIN && tctdbecode(tdb) != TCEKEEP){
            eprint(tdb, __LINE__, "tctdbaddint");
            err = true;
          }
          break;
        case 4:
          if(isnan(tctdbadddouble(tdb, pkbuf, pksiz, 1.0)) && tctdbecode(tdb) != TCEKEEP){
            eprint(tdb, __LINE__, "tctdbadddouble");
            err = true;
          }
          break;
        case 5:
          if(myrand(2) == 0){
            if(!tctdbputproc(tdb, pkbuf, pksiz, pkbuf, pksiz, pdprocfunc, NULL) &&
               tctdbecode(tdb) != TCEKEEP){
              eprint(tdb, __LINE__, "tctdbputproc");
              err = true;
            }
          } else {
            if(!tctdbputproc(tdb, pkbuf, pksiz, NULL, 0, pdprocfunc, NULL) &&
               tctdbecode(tdb) != TCEKEEP && tctdbecode(tdb) != TCENOREC){
              eprint(tdb, __LINE__, "tctdbputproc");
              err = true;
            }
          }
          break;
        default:
          if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
            eprint(tdb, __LINE__, "tctdbputcat");
            err = true;
          }
          break;
      }
      if(err) break;
    } else {
      if(dai){
        if(tctdbaddint(tdb, pkbuf, pksiz, myrand(3)) == INT_MIN){
          eprint(tdb, __LINE__, "tctdbaddint");
          err = true;
          break;
        }
      } else if(dad){
        if(isnan(tctdbadddouble(tdb, pkbuf, pksiz, myrand(30) / 10.0))){
          eprint(tdb, __LINE__, "tctdbadddouble");
          err = true;
          break;
        }
      } else if(rl){
        char fbuf[PATH_MAX];
        int fsiz = myrand(PATH_MAX);
        for(int j = 0; j < fsiz; j++){
          fbuf[j] = myrand(0x100);
        }
        tcmapput(cols, "bin", 3, fbuf, fsiz);
        if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, __LINE__, "tctdbputcat");
          err = true;
          break;
        }
      } else {
        if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, __LINE__, "tctdbputcat");
          err = true;
          break;
        }
      }
    }
    tcmapdel(cols);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
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


/* perform misc command */
static int procmisc(const char *path, int rnum, bool mt, int opts, int omode){
  iprintf("<Miscellaneous Test>\n  seed=%u  path=%s  rnum=%d  mt=%d  opts=%d  omode=%d\n\n",
          g_randseed, path, rnum, mt, opts, omode);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
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
  if(TCUSEPTHREAD){
    TCTDB *tdbdup = tctdbnew();
    if(tctdbopen(tdbdup, path, TDBOREADER)){
      eprint(tdb, __LINE__, "(validation)");
      err = true;
    } else if(tctdbecode(tdbdup) != TCETHREAD){
      eprint(tdb, __LINE__, "(validation)");
      err = true;
    }
    tctdbdel(tdbdup);
  }
  iprintf("writing:\n");
  for(int i = 1; i <= rnum; i++){
    int id = (int)tctdbgenuid(tdb);
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", id);
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
    int nsiz = sprintf(nbuf, "c%d", myrand(32) + 1);
    vsiz = sprintf(vbuf, "%d", myrand(32) + 1);
    tcmapput(cols, nbuf, nsiz, vbuf, vsiz);
    if(i % 3 == 0){
      if(!tctdbputkeep(tdb, pkbuf, pksiz, cols)){
        eprint(tdb, __LINE__, "tctdbputkeep");
        err = true;
        break;
      }
    } else {
      if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
        eprint(tdb, __LINE__, "tctdbputcat");
        err = true;
        break;
      }
    }
    tcmapdel(cols);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("reading:\n");
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", i);
    TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
    if(cols){
      const char *vbuf = tcmapget2(cols, "str");
      if(!vbuf || strcmp(vbuf, pkbuf)){
        eprint(tdb, __LINE__, "(validation)");
        tcmapdel(cols);
        err = true;
        break;
      }
      tcmapdel(cols);
    } else {
      eprint(tdb, __LINE__, "tctdbget");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("erasing:\n");
  for(int i = 1; i <= rnum; i++){
    if(i % 2 == 1){
      char pkbuf[RECBUFSIZ];
      int pksiz = sprintf(pkbuf, "%d", i);
      if(!tctdbout(tdb, pkbuf, pksiz)){
        eprint(tdb, __LINE__, "tctdbout");
        err = true;
        break;
      }
      if(tctdbout(tdb, pkbuf, pksiz) || tctdbecode(tdb) != TCENOREC){
        eprint(tdb, __LINE__, "tctdbout");
        err = true;
        break;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(tctdbrnum(tdb) != rnum / 2){
    eprint(tdb, __LINE__, "tctdbrnum");
    err = true;
  }
  if(tctdbuidseed(tdb) != rnum){
    eprint(tdb, __LINE__, "tctdbuidseed");
    err = true;
  }
  iprintf("searching:\n");
  TDBQRY *qry = tctdbqrynew(tdb);
  TCLIST *res = tctdbqrysearch(qry);
  if(tclistnum(res) != rnum / 2){
    eprint(tdb, __LINE__, "tctdbqrysearch");
    err = true;
  }
  tclistdel(res);
  tctdbqrydel(qry);
  const char *names[] = { "", "str", "num", "type", "flag", "text", "c1" };
  int ops[] = { TDBQCSTREQ, TDBQCSTRINC, TDBQCSTRBW, TDBQCSTREW, TDBQCSTRAND, TDBQCSTROR,
                TDBQCSTROREQ, TDBQCSTRRX, TDBQCNUMEQ, TDBQCNUMGT, TDBQCNUMGE, TDBQCNUMLT,
                TDBQCNUMLE, TDBQCNUMBT, TDBQCNUMOREQ };
  int ftsops[] = { TDBQCFTSPH, TDBQCFTSAND, TDBQCFTSOR, TDBQCFTSEX };
  int types[] = { TDBQOSTRASC, TDBQOSTRDESC, TDBQONUMASC, TDBQONUMDESC };
  qry = tctdbqrynew(tdb);
  for(int i = 1; i <= rnum; i++){
    if(myrand(100) != 0){
      tctdbqrydel(qry);
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
        tctdbqrysetlimit(qry, 10, myrand(5) * 10);
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
    }
    if(myrand(10) == 0){
      TDBQRY *qrys[4];
      int num = myrand(5);
      for(int j = 0; j < num; j++){
        qrys[j] = qry;
      }
      TCLIST *res = tctdbmetasearch(qrys, num, TDBMSUNION + myrand(3));
      if(num > 0){
        for(int j = 0; j < 3 && j < tclistnum(res); j++){
          int pksiz;
          const char *pkbuf = tclistval(res, j, &pksiz);
          TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
          if(cols){
            TCLIST *texts = tctdbqrykwic(qrys[0], cols, NULL, myrand(10), TCKWNOOVER);
            tclistdel(texts);
            tcmapdel(cols);
          } else {
            eprint(tdb, __LINE__, "tctdbget");
            err = true;
          }
        }
      }
      tclistdel(res);
    } else {
      TCLIST *res = tctdbqrysearch(qry);
      tclistdel(res);
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  tctdbqrydel(qry);
  iprintf("random writing and reopening:\n");
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", myrand(rnum) + 1);
    switch(myrand(4)){
      default:
        if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbout");
          err = true;
        }
        break;
      case 1:
        if(!tctdbaddint(tdb, pkbuf, pksiz, 1)){
          eprint(tdb, __LINE__, "tctdbaddint");
          err = true;
        }
        break;
      case 2:
        if(!tctdbadddouble(tdb, pkbuf, pksiz, 1.0)){
          eprint(tdb, __LINE__, "tctdbadddouble");
          err = true;
        }
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tctdbclose(tdb)){
    eprint(tdb, __LINE__, "tctdbclose");
    err = true;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
    eprint(tdb, __LINE__, "tctdbopen");
    err = true;
  }
  iprintf("random updating:\n");
  for(int i = 1; i <= rnum; i++){
    if(myrand(2) == 0){
      char pkbuf[RECBUFSIZ];
      int pksiz = sprintf(pkbuf, "%X", myrand(rnum) + 1);
      TCMAP *cols = tcmapnew2(7);
      char vbuf[RECBUFSIZ*2];
      int vsiz = sprintf(vbuf, "%d", myrand(i) + 1);
      tcmapput(cols, "c1", 3, vbuf, vsiz);
      vsiz = sprintf(vbuf, " %d, %d ", myrand(i) + 1, rnum / (myrand(rnum) + 1));
      tcmapput(cols, "flag", 4, vbuf, vsiz);
      tcmapput(cols, "text", 4, vbuf, vsiz);
      if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
        eprint(tdb, __LINE__, "tctdbputcat");
        err = true;
        break;
      }
      tcmapdel(cols);
    } else {
      char pkbuf[RECBUFSIZ];
      int pksiz = sprintf(pkbuf, "%X", myrand(rnum) + 1);
      if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
        eprint(tdb, __LINE__, "tctdbout");
        err = true;
        break;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("checking iterator:\n");
  int inum = 0;
  if(!tctdbiterinit(tdb)){
    eprint(tdb, __LINE__, "tctdbiterinit");
    err = true;
  }
  char *pkbuf;
  int pksiz;
  for(int i = 1; (pkbuf = tctdbiternext(tdb, &pksiz)) != NULL; i++, inum++){
    TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
    if(!cols){
      eprint(tdb, __LINE__, "tctdbget");
      err = true;
      tcfree(pkbuf);
      break;
    }
    tcmapdel(cols);
    tcfree(pkbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  if(tctdbecode(tdb) != TCENOREC || inum != tctdbrnum(tdb)){
    eprint(tdb, __LINE__, "(validation)");
    err = true;
  }
  iprintf("checking search consistency:\n");
  for(int i = 1; i <= rnum; i++){
    TDBQRY *myqry = tctdbqrynew(tdb);
    qry = tctdbqrynew(tdb);
    int cnum = myrand(4);
    if(cnum < 1) cnum = 1;
    for(int j = 0; j < cnum; j++){
      const char *name = names[myrand(sizeof(names) / sizeof(*names))];
      int op = ops[myrand(sizeof(ops) / sizeof(*ops))];
      if(myrand(10) == 0) op = ftsops[myrand(sizeof(ftsops) / sizeof(*ftsops))];
      char expr[RECBUFSIZ*3];
      char *wp = expr;
      if(myrand(3) == 0){
        wp += sprintf(expr, "%f", myrand(i * 100) / 100.0);
      } else {
        wp += sprintf(expr, "%d", myrand(i));
      }
      if(myrand(10) == 0) wp += sprintf(wp, ",%d", myrand(i));
      if(myrand(10) == 0) wp += sprintf(wp, ",%d", myrand(i));
      tctdbqryaddcond(myqry, name, op | (myrand(2) == 0 ? TDBQCNOIDX : 0), expr);
      tctdbqryaddcond(qry, name, op | (myrand(2) == 0 ? TDBQCNOIDX : 0), expr);
    }
    int max = (myrand(10) == 0) ? 0 : myrand(10) + 1;
    if(max > 0){
      tctdbqrysetlimit(myqry, max, 0);
      tctdbqrysetlimit(qry, max, 0);
      TCLIST *myres = tctdbqrysearch(myqry);
      res = tctdbqrysearch(qry);
      if(tclistnum(myres) != tclistnum(res)){
        eprint(tdb, __LINE__, "(validation)");
        err = true;
      }
      tclistdel(res);
      tclistdel(myres);
    } else {
      TCLIST *myres = tctdbqrysearch(myqry);
      res = tctdbqrysearch(qry);
      if(tclistnum(myres) == tclistnum(res)){
        tclistsort(myres);
        tclistsort(res);
        int rnum = tclistnum(myres);
        for(int j = 0; j < rnum; j++){
          int myrsiz;
          const char *myrbuf = tclistval(myres, j, &myrsiz);
          int rsiz;
          const char *rbuf = tclistval(res, j, &rsiz);
          if(myrsiz != rsiz || memcmp(myrbuf, rbuf, myrsiz)){
            eprint(tdb, __LINE__, "(validation)");
            err = true;
            break;
          }
        }
      } else {
        eprint(tdb, __LINE__, "(validation)");
        err = true;
      }
      tclistdel(res);
      tclistdel(myres);
    }
    tctdbqrydel(qry);
    tctdbqrydel(myqry);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  qry = tctdbqrynew(tdb);
  tctdbqryaddcond(qry, "", TDBQCSTRBW, "1");
  if(!tctdbqrysearchout(qry)){
    eprint(tdb, __LINE__, "tctdbqrysearchout");
    err = true;
  }
  tctdbqrydel(qry);
  qry = tctdbqrynew(tdb);
  tctdbqryaddcond(qry, "", TDBQCSTRBW, "2");
  if(!tctdbqrysearchout2(qry)){
    eprint(tdb, __LINE__, "tctdbqrysearchout2");
    err = true;
  }
  tctdbqrydel(qry);
  if(myrand(4) == 0 && !tctdbdefrag(tdb, 0)){
    eprint(tdb, __LINE__, "tctdbdefrag");
    err = true;
  }
  if(myrand(4) == 0 && !tctdbcacheclear(tdb)){
    eprint(tdb, __LINE__, "tctdbcacheclear");
    err = true;
  }
  iprintf("checking transaction commit:\n");
  if(!tctdbtranbegin(tdb)){
    eprint(tdb, __LINE__, "tctdbtranbegin");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", myrand(rnum));
    switch(myrand(4)){
      default:
        if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbout");
          err = true;
        }
        break;
      case 1:
        if(!tctdbaddint(tdb, pkbuf, pksiz, 1)){
          eprint(tdb, __LINE__, "tctdbaddint");
          err = true;
        }
        break;
      case 2:
        if(!tctdbadddouble(tdb, pkbuf, pksiz, 1.0)){
          eprint(tdb, __LINE__, "tctdbadddouble");
          err = true;
        }
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tctdbtrancommit(tdb)){
    eprint(tdb, __LINE__, "tctdbtrancommit");
    err = true;
  }
  iprintf("checking transaction abort:\n");
  uint64_t ornum = tctdbrnum(tdb);
  uint64_t ofsiz = tctdbfsiz(tdb);
  if(!tctdbtranbegin(tdb)){
    eprint(tdb, __LINE__, "tctdbtranbegin");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", myrand(rnum));
    switch(myrand(4)){
      default:
        if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbout");
          err = true;
        }
        break;
      case 1:
        if(!tctdbaddint(tdb, pkbuf, pksiz, 1)){
          eprint(tdb, __LINE__, "tctdbaddint");
          err = true;
        }
        break;
      case 2:
        if(!tctdbadddouble(tdb, pkbuf, pksiz, 1.0)){
          eprint(tdb, __LINE__, "tctdbadddouble");
          err = true;
        }
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tctdbtranabort(tdb)){
    eprint(tdb, __LINE__, "tctdbtranabort");
    err = true;
  }
  if(tctdbrnum(tdb) != ornum || tctdbfsiz(tdb) != ofsiz){
    eprint(tdb, __LINE__, "(validation)");
    err = true;
  }
  if(!tctdbvanish(tdb)){
    eprint(tdb, __LINE__, "tctdbvanish");
    err = true;
  }
  if(!tctdbput3(tdb, "mikio", "str\tMIKIO\tbirth\t19780211")){
    eprint(tdb, __LINE__, "tctdbput3");
    err = true;
  }
  if(!tctdbtranbegin(tdb)){
    eprint(tdb, __LINE__, "tctdbtranbegin");
    err = true;
  }
  if(!tctdbput3(tdb, "mikio", "str\tMEKEO\tsex\tmale")){
    eprint(tdb, __LINE__, "tctdbput3");
    err = true;
  }
  for(int i = 0; i < 10; i++){
    char pkbuf[RECBUFSIZ];
    sprintf(pkbuf, "%d", myrand(10) + 1);
    char vbuf[RECBUFSIZ*2];
    sprintf(vbuf, "%d\t%d", myrand(10) + 1, myrand(rnum) + 1);
    if(!tctdbput3(tdb, pkbuf, vbuf)){
      eprint(tdb, __LINE__, "tctdbput");
      err = true;
    }
  }
  if(!tctdbforeach(tdb, iterfunc, NULL)){
    eprint(tdb, __LINE__, "tctdbforeach");
    err = true;
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
static int procwicked(const char *path, int rnum, bool mt, int opts, int omode){
  iprintf("<Wicked Writing Test>\n  seed=%u  path=%s  rnum=%d  mt=%d  opts=%d  omode=%d\n\n",
          g_randseed, path, rnum, mt, opts, omode);
  bool err = false;
  double stime = tctime();
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(mt && !tctdbsetmutex(tdb)){
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
  if(!tctdbsetinvcache(tdb, -1, 0.5)){
    eprint(tdb, __LINE__, "tctdbsetinvcache");
    err = true;
  }
  if(!tctdbsetdfunit(tdb, 8)){
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
  const char *names[] = { "", "str", "num", "type", "flag", "text", "c1" };
  int ops[] = { TDBQCSTREQ, TDBQCSTRINC, TDBQCSTRBW, TDBQCSTREW, TDBQCSTRAND, TDBQCSTROR,
                TDBQCSTROREQ, TDBQCSTRRX, TDBQCNUMEQ, TDBQCNUMGT, TDBQCNUMGE, TDBQCNUMLT,
                TDBQCNUMLE, TDBQCNUMBT, TDBQCNUMOREQ };
  int ftsops[] = { TDBQCFTSPH, TDBQCFTSAND, TDBQCFTSOR, TDBQCFTSEX };
  int types[] = { TDBQOSTRASC, TDBQOSTRDESC, TDBQONUMASC, TDBQONUMDESC };
  for(int i = 1; i <= rnum; i++){
    int id = myrand(2) == 0 ? myrand(rnum) + 1 : (int)tctdbgenuid(tdb);
    char pkbuf[RECBUFSIZ];
    int pksiz = sprintf(pkbuf, "%d", id);
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
    TCLIST *res;
    switch(myrand(17)){
      case 0:
        iputchar('0');
        if(!tctdbput(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, __LINE__, "tctdbput");
          err = true;
        }
        break;
      case 1:
        iputchar('1');
        cbuf = tcstrjoin4(cols, &csiz);
        if(!tctdbput2(tdb, pkbuf, pksiz, cbuf, csiz)){
          eprint(tdb, __LINE__, "tctdbput2");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 2:
        iputchar('2');
        cbuf = tcstrjoin3(cols, '\t');
        if(!tctdbput3(tdb, pkbuf, cbuf)){
          eprint(tdb, __LINE__, "tctdbput3");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 3:
        iputchar('3');
        if(!tctdbputkeep(tdb, pkbuf, pksiz, cols) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, __LINE__, "tctdbputkeep");
          err = true;
        }
        break;
      case 4:
        iputchar('4');
        cbuf = tcstrjoin4(cols, &csiz);
        if(!tctdbputkeep2(tdb, pkbuf, pksiz, cbuf, csiz) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, __LINE__, "tctdbputkeep2");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 5:
        iputchar('5');
        cbuf = tcstrjoin3(cols, '\t');
        if(!tctdbputkeep3(tdb, pkbuf, cbuf) && tctdbecode(tdb) != TCEKEEP){
          eprint(tdb, __LINE__, "tctdbputkeep3");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 6:
        iputchar('6');
        if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
          eprint(tdb, __LINE__, "tctdbputcat");
          err = true;
        }
        break;
      case 7:
        iputchar('7');
        cbuf = tcstrjoin4(cols, &csiz);
        if(!tctdbputcat2(tdb, pkbuf, pksiz, cbuf, csiz)){
          eprint(tdb, __LINE__, "tctdbputcat2");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 8:
        iputchar('8');
        cbuf = tcstrjoin3(cols, '\t');
        if(!tctdbputcat3(tdb, pkbuf, cbuf)){
          eprint(tdb, __LINE__, "tctdbputcat3");
          err = true;
        }
        tcfree(cbuf);
        break;
      case 9:
        iputchar('9');
        if(!tctdbout(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbout");
          err = true;
        }
        break;
      case 10:
        iputchar('A');
        if(!tctdbout2(tdb, pkbuf) && tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbout2");
          err = true;
        }
        break;
      case 11:
        iputchar('B');
        ncols = tctdbget(tdb, pkbuf, pksiz);
        if(ncols){
          tcmapdel(ncols);
        } else if(tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbget");
          err = true;
        }
        break;
      case 12:
        iputchar('C');
        cbuf = tctdbget2(tdb, pkbuf, pksiz, &csiz);
        if(cbuf){
          tcfree(cbuf);
        } else if(tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbget2");
          err = true;
        }
        break;
      case 13:
        iputchar('D');
        cbuf = tctdbget3(tdb, pkbuf);
        if(cbuf){
          tcfree(cbuf);
        } else if(tctdbecode(tdb) != TCENOREC){
          eprint(tdb, __LINE__, "tctdbget3");
          err = true;
        }
        break;
      case 14:
        iputchar('E');
        if(myrand(rnum / 128) == 0){
          if(myrand(2) == 0){
            if(!tctdbiterinit(tdb)){
              eprint(tdb, __LINE__, "tctdbiterinit");
              err = true;
            }
          } else {
            if(!tctdbiterinit2(tdb, pkbuf, pksiz) && tctdbecode(tdb) != TCENOREC){
              eprint(tdb, __LINE__, "tctdbiterinit2");
              err = true;
            }
          }
        }
        for(int j = myrand(rnum) / 1000 + 1; j >= 0; j--){
          if(j % 3 == 0){
            ncols = tctdbiternext3(tdb);
            if(ncols){
              tcmapdel(ncols);
            } else {
              int ecode = tctdbecode(tdb);
              if(ecode != TCEINVALID && ecode != TCENOREC){
                eprint(tdb, __LINE__, "tctdbiternext3");
                err = true;
              }
            }
          } else {
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
        }
        break;
      case 15:
        iputchar('F');
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
        res = tctdbqrysearch(qry);
        tclistdel(res);
        tctdbqrydel(qry);
        break;
      default:
        iputchar('@');
        if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
        break;
    }
    tcmapdel(cols);
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
    if(i == rnum / 2){
      if(!tctdbclose(tdb)){
        eprint(tdb, __LINE__, "tctdbclose");
        err = true;
      }
      if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
        eprint(tdb, __LINE__, "tctdbopen");
        err = true;
      }
    } else if(i == rnum / 4){
      char *npath = tcsprintf("%s-tmp", path);
      if(!tctdbcopy(tdb, npath)){
        eprint(tdb, __LINE__, "tctdbcopy");
        err = true;
      }
      TCTDB *ntdb = tctdbnew();
      if(!tctdbsetcodecfunc(ntdb, _tc_recencode, NULL, _tc_recdecode, NULL)){
        eprint(ntdb, __LINE__, "tctdbsetcodecfunc");
        err = true;
      }
      if(!tctdbopen(ntdb, npath, TDBOREADER | omode)){
        eprint(ntdb, __LINE__, "tctdbopen");
        err = true;
      }
      tctdbdel(ntdb);
      unlink(npath);
      tcfree(npath);
      if(!tctdboptimize(tdb, rnum / 50, -1, -1, -1)){
        eprint(tdb, __LINE__, "tctdboptimize");
        err = true;
      }
      if(!tctdbiterinit(tdb)){
        eprint(tdb, __LINE__, "tctdbiterinit");
        err = true;
      }
    } else if(i == rnum / 8){
      if(!tctdbtranbegin(tdb)){
        eprint(tdb, __LINE__, "tctdbtranbegin");
        err = true;
      }
    } else if(i == rnum / 8 + rnum / 16){
      if(!tctdbtrancommit(tdb)){
        eprint(tdb, __LINE__, "tctdbtrancommit");
        err = true;
      }
    }
  }
  if(!tctdbsync(tdb)){
    eprint(tdb, __LINE__, "tctdbsync");
    err = true;
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



// END OF FILE
