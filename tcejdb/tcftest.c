/*************************************************************************************************
 * The test cases of the fixed-length database API
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
#include <tcfdb.h>
#include "myconf.h"

#define RECBUFSIZ      48                // buffer for records
#define EXHEADSIZ      256               // expected header size


/* global variables */
const char *g_progname;                  // program name
unsigned int g_randseed;                 // random seed
int g_dbgfd;                             // debugging output


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void iputchar(int c);
static void eprint(TCFDB *fdb, int line, const char *func);
static void mprint(TCFDB *fdb);
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
static int procwrite(const char *path, int rnum, int width, int64_t limsiz,
                     bool mt, int omode, bool rnd);
static int procread(const char *path, bool mt, int omode, bool wb, bool rnd);
static int procremove(const char *path, bool mt, int omode, bool rnd);
static int procrcat(const char *path, int rnum, int width, int64_t limsiz,
                    bool mt, int omode, int pnum, bool dai, bool dad, bool rl, bool ru);
static int procmisc(const char *path, int rnum, bool mt, int omode);
static int procwicked(const char *path, int rnum, bool mt, int omode);


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
  fprintf(stderr, "%s: test cases of the fixed-length database API of Tokyo Cabinet\n",
          g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write [-mt] [-nl|-nb] [-rnd] path rnum [width [limsiz]]\n", g_progname);
  fprintf(stderr, "  %s read [-mt] [-nl|-nb] [-wb] [-rnd] path\n", g_progname);
  fprintf(stderr, "  %s remove [-mt] [-nl|-nb] [-rnd] path\n", g_progname);
  fprintf(stderr, "  %s rcat [-mt] [-nl|-nb] [-pn num] [-dai|-dad|-rl|-ru]"
          " path rnum [width [limsiz]]\n", g_progname);
  fprintf(stderr, "  %s misc [-mt] [-nl|-nb] path rnum\n", g_progname);
  fprintf(stderr, "  %s wicked [-mt] [-nl|-nb] path rnum\n", g_progname);
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


/* print error message of fixed-length database */
static void eprint(TCFDB *fdb, int line, const char *func){
  const char *path = tcfdbpath(fdb);
  int ecode = tcfdbecode(fdb);
  fprintf(stderr, "%s: %s: %d: %s: error: %d: %s\n",
          g_progname, path ? path : "-", line, func, ecode, tcfdberrmsg(ecode));
}


/* print members of fixed-length database */
static void mprint(TCFDB *fdb){
  if(fdb->cnt_writerec < 0) return;
  iprintf("minimum ID number: %llu\n", (unsigned long long)tcfdbmin(fdb));
  iprintf("maximum ID number: %llu\n", (unsigned long long)tcfdbmax(fdb));
  iprintf("width of the value: %u\n", (unsigned int)tcfdbwidth(fdb));
  iprintf("limit file size: %llu\n", (unsigned long long)tcfdblimsiz(fdb));
  iprintf("limit ID number: %llu\n", (unsigned long long)tcfdblimid(fdb));
  iprintf("cnt_writerec: %lld\n", (long long)fdb->cnt_writerec);
  iprintf("cnt_readrec: %lld\n", (long long)fdb->cnt_readrec);
  iprintf("cnt_truncfile: %lld\n", (long long)fdb->cnt_truncfile);
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
  char *wstr = NULL;
  char *lstr = NULL;
  bool mt = false;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else if(!wstr){
      wstr = argv[i];
    } else if(!lstr){
      lstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int width = wstr ? tcatoix(wstr) : -1;
  int64_t limsiz = lstr ? tcatoix(lstr) : -1;
  int rv = procwrite(path, rnum, width, limsiz, mt, omode, rnd);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *path = NULL;
  bool mt = false;
  int omode = 0;
  bool wb = false;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int rv = procread(path, mt, omode, wb, rnd);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *path = NULL;
  bool mt = false;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int rv = procremove(path, mt, omode, rnd);
  return rv;
}


/* parse arguments of rcat command */
static int runrcat(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  char *wstr = NULL;
  char *lstr = NULL;
  bool mt = false;
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
      } else if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
    } else if(!wstr){
      wstr = argv[i];
    } else if(!lstr){
      lstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int width = wstr ? tcatoix(wstr) : -1;
  int64_t limsiz = lstr ? tcatoix(lstr) : -1;
  int rv = procrcat(path, rnum, width, limsiz, mt, omode, pnum, dai, dad, rl, ru);
  return rv;
}


/* parse arguments of misc command */
static int runmisc(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  bool mt = false;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int rv = procmisc(path, rnum, mt, omode);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *path = NULL;
  char *rstr = NULL;
  bool mt = false;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-mt")){
        mt = true;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int rv = procwicked(path, rnum, mt, omode);
  return rv;
}


/* perform write command */
static int procwrite(const char *path, int rnum, int width, int64_t limsiz,
                     bool mt, int omode, bool rnd){
  iprintf("<Writing Test>\n  seed=%u  path=%s  rnum=%d  width=%d  limsiz=%lld  mt=%d  omode=%d"
          "  rnd=%d\n\n", g_randseed, path, rnum, width, (long long)limsiz, mt, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(mt && !tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbtune(fdb, width, limsiz)){
    eprint(fdb, __LINE__, "tcfdbtune");
    err = true;
  }
  if(!rnd) omode |= FDBOTRUNC;
  if(!tcfdbopen(fdb, path, FDBOWRITER | FDBOCREAT | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
    if(!tcfdbput2(fdb, buf, len, buf, len)){
      eprint(fdb, __LINE__, "tcfdbput");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcfdbrnum(fdb));
  iprintf("size: %llu\n", (unsigned long long)tcfdbfsiz(fdb));
  mprint(fdb);
  sysprint();
  if(!tcfdbclose(fdb)){
    eprint(fdb, __LINE__, "tcfdbclose");
    err = true;
  }
  tcfdbdel(fdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform read command */
static int procread(const char *path, bool mt, int omode, bool wb, bool rnd){
  iprintf("<Reading Test>\n  seed=%u  path=%s  mt=%d  omode=%d  wb=%d  rnd=%d\n\n",
          g_randseed, path, mt, omode, wb, rnd);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(mt && !tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbopen(fdb, path, FDBOREADER | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  int rnum = tcfdbrnum(fdb);
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", rnd ? myrand(rnum) + 1 : i);
    int vsiz;
    if(wb){
      char vbuf[RECBUFSIZ];
      int vsiz = tcfdbget4(fdb, i, vbuf, RECBUFSIZ);
      if(vsiz < 0 && !(rnd && tcfdbecode(fdb) == TCENOREC)){
        eprint(fdb, __LINE__, "tcfdbget4");
        err = true;
        break;
      }
    } else {
      char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
      if(!vbuf && !(rnd && tcfdbecode(fdb) == TCENOREC)){
        eprint(fdb, __LINE__, "tcfdbget");
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
  iprintf("record number: %llu\n", (unsigned long long)tcfdbrnum(fdb));
  iprintf("size: %llu\n", (unsigned long long)tcfdbfsiz(fdb));
  mprint(fdb);
  sysprint();
  if(!tcfdbclose(fdb)){
    eprint(fdb, __LINE__, "tcfdbclose");
    err = true;
  }
  tcfdbdel(fdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform remove command */
static int procremove(const char *path, bool mt, int omode, bool rnd){
  iprintf("<Removing Test>\n  seed=%u  path=%s  mt=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, mt, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(mt && !tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbopen(fdb, path, FDBOWRITER | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  int rnum = tcfdbrnum(fdb);
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", rnd ? myrand(rnum) + 1 : i);
    if(!tcfdbout2(fdb, kbuf, ksiz) && !(rnd && tcfdbecode(fdb) == TCENOREC)){
      eprint(fdb, __LINE__, "tcfdbout");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcfdbrnum(fdb));
  iprintf("size: %llu\n", (unsigned long long)tcfdbfsiz(fdb));
  mprint(fdb);
  sysprint();
  if(!tcfdbclose(fdb)){
    eprint(fdb, __LINE__, "tcfdbclose");
    err = true;
  }
  tcfdbdel(fdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform rcat command */
static int procrcat(const char *path, int rnum, int width, int64_t limsiz,
                    bool mt, int omode, int pnum, bool dai, bool dad, bool rl, bool ru){
  iprintf("<Random Concatenating Test>\n"
          "  seed=%u  path=%s  rnum=%d  width=%d  limsiz=%lld  mt=%d  omode=%d  pnum=%d"
          "  dai=%d  dad=%d  rl=%d  ru=%d\n\n",
          g_randseed, path, rnum, width, (long long)limsiz, mt, omode, pnum, dai, dad, rl, ru);
  if(pnum < 1) pnum = rnum;
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(mt && !tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbtune(fdb, width, limsiz)){
    eprint(fdb, __LINE__, "tcfdbtune");
    err = true;
  }
  if(!tcfdbopen(fdb, path, FDBOWRITER | FDBOCREAT | FDBOTRUNC | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(pnum) + 1);
    if(dai){
      if(tcfdbaddint(fdb, tcfdbkeytoid(kbuf, ksiz), myrand(3)) == INT_MIN){
        eprint(fdb, __LINE__, "tcfdbaddint");
        err = true;
        break;
      }
    } else if(dad){
      if(isnan(tcfdbadddouble(fdb, tcfdbkeytoid(kbuf, ksiz), myrand(30) / 10.0))){
        eprint(fdb, __LINE__, "tcfdbadddouble");
        err = true;
        break;
      }
    } else if(rl){
      char vbuf[PATH_MAX];
      int vsiz = myrand(PATH_MAX);
      for(int j = 0; j < vsiz; j++){
        vbuf[j] = myrand(0x100);
      }
      if(!tcfdbputcat2(fdb, kbuf, ksiz, vbuf, vsiz)){
        eprint(fdb, __LINE__, "tcfdbputcat");
        err = true;
        break;
      }
    } else if(ru){
      int id = myrand(pnum) + 1;
      switch(myrand(8)){
        case 0:
          if(!tcfdbput(fdb, id, kbuf, ksiz)){
            eprint(fdb, __LINE__, "tcfdbput");
            err = true;
          }
          break;
        case 1:
          if(!tcfdbputkeep(fdb, id, kbuf, ksiz) && tcfdbecode(fdb) != TCEKEEP){
            eprint(fdb, __LINE__, "tcfdbputkeep");
            err = true;
          }
          break;
        case 2:
          if(!tcfdbout(fdb, id) && tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbout");
            err = true;
          }
          break;
        case 3:
          if(tcfdbaddint(fdb, id, 1) == INT_MIN && tcfdbecode(fdb) != TCEKEEP){
            eprint(fdb, __LINE__, "tcfdbaddint");
            err = true;
          }
          break;
        case 4:
          if(isnan(tcfdbadddouble(fdb, id, 1.0)) && tcfdbecode(fdb) != TCEKEEP){
            eprint(fdb, __LINE__, "tcfdbadddouble");
            err = true;
          }
          break;
        case 5:
          if(myrand(2) == 0){
            if(!tcfdbputproc(fdb, id, kbuf, ksiz, pdprocfunc, NULL) &&
               tcfdbecode(fdb) != TCEKEEP){
              eprint(fdb, __LINE__, "tcfdbputproc");
              err = true;
            }
          } else {
            if(!tcfdbputproc(fdb, id, NULL, 0, pdprocfunc, NULL) &&
               tcfdbecode(fdb) != TCEKEEP && tcfdbecode(fdb) != TCENOREC){
              eprint(fdb, __LINE__, "tcfdbputproc");
              err = true;
            }
          }
          break;
        default:
          if(!tcfdbputcat(fdb, id, kbuf, ksiz)){
            eprint(fdb, __LINE__, "tcfdbputcat");
            err = true;
          }
          break;
      }
      if(err) break;
    } else {
      if(!tcfdbputcat2(fdb, kbuf, ksiz, kbuf, ksiz)){
        eprint(fdb, __LINE__, "tcfdbputcat");
        err = true;
        break;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcfdbrnum(fdb));
  iprintf("size: %llu\n", (unsigned long long)tcfdbfsiz(fdb));
  mprint(fdb);
  sysprint();
  if(!tcfdbclose(fdb)){
    eprint(fdb, __LINE__, "tcfdbclose");
    err = true;
  }
  tcfdbdel(fdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform misc command */
static int procmisc(const char *path, int rnum, bool mt, int omode){
  iprintf("<Miscellaneous Test>\n  seed=%u  path=%s  rnum=%d  mt=%d  omode=%d\n\n",
          g_randseed, path, rnum, mt, omode);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(mt && !tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbtune(fdb, RECBUFSIZ, EXHEADSIZ + (RECBUFSIZ + sizeof(int)) * rnum)){
    eprint(fdb, __LINE__, "tcfdbtune");
    err = true;
  }
  if(!tcfdbopen(fdb, path, FDBOWRITER | FDBOCREAT | FDBOTRUNC | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  if(TCUSEPTHREAD){
    TCFDB *fdbdup = tcfdbnew();
    if(tcfdbopen(fdbdup, path, FDBOREADER)){
      eprint(fdb, __LINE__, "(validation)");
      err = true;
    } else if(tcfdbecode(fdbdup) != TCETHREAD){
      eprint(fdb, __LINE__, "(validation)");
      err = true;
    }
    tcfdbdel(fdbdup);
  }
  iprintf("writing:\n");
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    if(!tcfdbputkeep2(fdb, buf, len, buf, len)){
      eprint(fdb, __LINE__, "tcfdbputkeep");
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
    char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(fdb, __LINE__, "tcfdbget");
      err = true;
      break;
    } else if(vsiz != ksiz || memcmp(vbuf, kbuf, vsiz)){
      eprint(fdb, __LINE__, "(validation)");
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
  if(tcfdbrnum(fdb) != rnum){
    eprint(fdb, __LINE__, "(validation)");
    err = true;
  }
  iprintf("random writing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum) + 1);
    char vbuf[RECBUFSIZ];
    int vsiz = myrand(RECBUFSIZ);
    memset(vbuf, '*', vsiz);
    if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
      eprint(fdb, __LINE__, "tcfdbput");
      err = true;
      break;
    }
    int rsiz;
    char *rbuf = tcfdbget2(fdb, kbuf, ksiz, &rsiz);
    if(!rbuf){
      eprint(fdb, __LINE__, "tcfdbget");
      err = true;
      break;
    }
    if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(fdb, __LINE__, "(validation)");
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
  iprintf("random erasing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum) + 1);
    if(!tcfdbout2(fdb, kbuf, ksiz) && tcfdbecode(fdb) != TCENOREC){
      eprint(fdb, __LINE__, "tcfdbout");
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
    if(!tcfdbputkeep2(fdb, kbuf, ksiz, vbuf, vsiz) && tcfdbecode(fdb) != TCEKEEP){
      eprint(fdb, __LINE__, "tcfdbputkeep");
      err = true;
      break;
    }
    if(vsiz < 1){
      char tbuf[PATH_MAX];
      for(int j = 0; j < PATH_MAX; j++){
        tbuf[j] = myrand(0x100);
      }
      if(!tcfdbput2(fdb, kbuf, ksiz, tbuf, PATH_MAX)){
        eprint(fdb, __LINE__, "tcfdbput");
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
      if(!tcfdbout2(fdb, kbuf, ksiz)){
        eprint(fdb, __LINE__, "tcfdbout");
        err = true;
        break;
      }
      if(tcfdbout2(fdb, kbuf, ksiz) || tcfdbecode(fdb) != TCENOREC){
        eprint(fdb, __LINE__, "tcfdbout");
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
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum) + 1);
    int vsiz = myrand(RECBUFSIZ);
    char *vbuf = tcmalloc(vsiz + 1);
    memset(vbuf, '*', vsiz);
    switch(myrand(3)){
      case 0:
        if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbput");
          err = true;
        }
        break;
      case 1:
        if(!tcfdbputcat2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbputcat");
          err = true;
        }
        break;
      case 2:
        if(!tcfdbout2(fdb, kbuf, ksiz) && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbout");
          err = true;
        }
        break;
    }
    tcfree(vbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tcfdbclose(fdb)){
    eprint(fdb, __LINE__, "tcfdbclose");
    err = true;
  }
  if(!tcfdbopen(fdb, path, FDBOWRITER | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  iprintf("checking:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "[%d]", i);
    int vsiz;
    char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
    if(vbuf){
      tcfree(vbuf);
    } else if(tcfdbecode(fdb) != TCENOREC){
      eprint(fdb, __LINE__, "tcfdbget");
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
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    if(!tcfdbput2(fdb, buf, len, buf, len)){
      eprint(fdb, __LINE__, "tcfdbput");
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
    char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(fdb, __LINE__, "tcfdbget");
      err = true;
      break;
    } else if(vsiz != ksiz || memcmp(vbuf, kbuf, vsiz)){
      eprint(fdb, __LINE__, "(validation)");
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
  iprintf("checking iterator:\n");
  if(!tcfdbiterinit(fdb)){
    eprint(fdb, __LINE__, "tcfdbiterinit");
    err = true;
  }
  char *kbuf;
  int ksiz;
  int inum = 0;
  for(int i = 1; (kbuf = tcfdbiternext2(fdb, &ksiz)) != NULL; i++, inum++){
    int vsiz;
    char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(fdb, __LINE__, "tcfdbget2");
      err = true;
      tcfree(kbuf);
      break;
    }
    tcfree(vbuf);
    tcfree(kbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(tcfdbecode(fdb) != TCENOREC || inum != tcfdbrnum(fdb)){
    eprint(fdb, __LINE__, "(validation)");
    err = true;
  }
  iprintf("iteration updating:\n");
  if(!tcfdbiterinit(fdb)){
    eprint(fdb, __LINE__, "tcfdbiterinit");
    err = true;
  }
  inum = 0;
  for(int i = 1; (kbuf = tcfdbiternext2(fdb, &ksiz)) != NULL; i++, inum++){
    if(myrand(2) == 0){
      if(!tcfdbputcat2(fdb, kbuf, ksiz, "0123456789", 10)){
        eprint(fdb, __LINE__, "tcfdbputcat2");
        err = true;
        tcfree(kbuf);
        break;
      }
    } else {
      if(!tcfdbout2(fdb, kbuf, ksiz)){
        eprint(fdb, __LINE__, "tcfdbout");
        err = true;
        tcfree(kbuf);
        break;
      }
    }
    tcfree(kbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(tcfdbecode(fdb) != TCENOREC || inum < tcfdbrnum(fdb)){
    eprint(fdb, __LINE__, "(validation)");
    err = true;
  }
  if(myrand(10) == 0 && !tcfdbsync(fdb)){
    eprint(fdb, __LINE__, "tcfdbsync");
    err = true;
  }
  if(!tcfdbvanish(fdb)){
    eprint(fdb, __LINE__, "tcfdbvanish");
    err = true;
  }
  TCMAP *map = tcmapnew();
  iprintf("random writing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum) + 1);
    char vbuf[RECBUFSIZ];
    int vsiz = sprintf(vbuf, "%d", myrand(rnum) + 1);
    switch(myrand(7)){
      case 0:
        if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbput2");
          err = true;
        }
        tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 1:
        if(!tcfdbputkeep2(fdb, kbuf, ksiz, vbuf, vsiz) && tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbputkeep2");
          err = true;
        }
        tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 2:
        if(!tcfdbputcat2(fdb, kbuf, ksiz, vbuf, vsiz) && tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbputcat2");
          err = true;
        }
        tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 3:
        if(!tcfdbout2(fdb, kbuf, ksiz) && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbout2");
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
  iprintf("checking transaction commit:\n");
  if(!tcfdbtranbegin(fdb)){
    eprint(fdb, __LINE__, "tcfdbtranbegin");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum) + 1);
    char vbuf[RECBUFSIZ];
    int vsiz = sprintf(vbuf, "[%d]", myrand(rnum) + 1);
    switch(myrand(7)){
      case 0:
        if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbput2");
          err = true;
        }
        tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 1:
        if(!tcfdbputkeep2(fdb, kbuf, ksiz, vbuf, vsiz) && tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbputkeep2");
          err = true;
        }
        tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 2:
        if(!tcfdbputcat2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbputcat2");
          err = true;
        }
        tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 3:
        if(tcfdbaddint(fdb, tcfdbkeytoid(kbuf, ksiz), 1) == INT_MIN &&
           tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbaddint");
          err = true;
        }
        tcmapaddint(map, kbuf, ksiz, 1);
        break;
      case 4:
        if(isnan(tcfdbadddouble(fdb, tcfdbkeytoid(kbuf, ksiz), 1.0)) &&
           tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbadddouble");
          err = true;
        }
        tcmapadddouble(map, kbuf, ksiz, 1.0);
        break;
      case 5:
        if(myrand(2) == 0){
          void *op = (void *)(intptr_t)(myrand(3) + 1);
          if(!tcfdbputproc(fdb, tcfdbkeytoid(kbuf, ksiz), vbuf, vsiz, pdprocfunc, op) &&
             tcfdbecode(fdb) != TCEKEEP){
            eprint(fdb, __LINE__, "tcfdbputproc");
            err = true;
          }
          tcmapputproc(map, kbuf, ksiz, vbuf, vsiz, pdprocfunc, op);
        } else {
          vsiz = myrand(10);
          void *op = (void *)(intptr_t)(myrand(3) + 1);
          if(!tcfdbputproc(fdb, tcfdbkeytoid(kbuf, ksiz), NULL, vsiz, pdprocfunc, op) &&
             tcfdbecode(fdb) != TCEKEEP && tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbputproc");
            err = true;
          }
          tcmapputproc(map, kbuf, ksiz, NULL, vsiz, pdprocfunc, op);
        }
        break;
      case 6:
        if(!tcfdbout2(fdb, kbuf, ksiz) && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbout2");
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
  if(!tcfdbtrancommit(fdb)){
    eprint(fdb, __LINE__, "tcfdbtrancommit");
    err = true;
  }
  iprintf("checking transaction abort:\n");
  uint64_t ornum = tcfdbrnum(fdb);
  uint64_t ofsiz = tcfdbfsiz(fdb);
  if(!tcfdbtranbegin(fdb)){
    eprint(fdb, __LINE__, "tcfdbtranbegin");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum) + 1);
    char vbuf[RECBUFSIZ];
    int vsiz = sprintf(vbuf, "[%d]", myrand(rnum) + 1);
    switch(myrand(7)){
      case 0:
        if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbput2");
          err = true;
        }
        break;
      case 1:
        if(!tcfdbputkeep2(fdb, kbuf, ksiz, vbuf, vsiz) && tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbputkeep2");
          err = true;
        }
        break;
      case 2:
        if(!tcfdbputcat2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbputcat2");
          err = true;
        }
        break;
      case 3:
        if(tcfdbaddint(fdb, tcfdbkeytoid(kbuf, ksiz), 1) == INT_MIN &&
           tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbaddint");
          err = true;
        }
        break;
      case 4:
        if(isnan(tcfdbadddouble(fdb, tcfdbkeytoid(kbuf, ksiz), 1.0)) &&
           tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbadddouble");
          err = true;
        }
        break;
      case 5:
        if(myrand(2) == 0){
          void *op = (void *)(intptr_t)(myrand(3) + 1);
          if(!tcfdbputproc(fdb, tcfdbkeytoid(kbuf, ksiz), vbuf, vsiz, pdprocfunc, op) &&
             tcfdbecode(fdb) != TCEKEEP){
            eprint(fdb, __LINE__, "tcfdbputproc");
            err = true;
          }
        } else {
          vsiz = myrand(10);
          void *op = (void *)(intptr_t)(myrand(3) + 1);
          if(!tcfdbputproc(fdb, tcfdbkeytoid(kbuf, ksiz), NULL, vsiz, pdprocfunc, op) &&
             tcfdbecode(fdb) != TCEKEEP && tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbputproc");
            err = true;
          }
        }
        break;
      case 6:
        if(!tcfdbout2(fdb, kbuf, ksiz) && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbout2");
          err = true;
        }
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(!tcfdbtranabort(fdb)){
    eprint(fdb, __LINE__, "tcfdbtranabort");
    err = true;
  }
  iprintf("checking consistency:\n");
  if(tcfdbrnum(fdb) != ornum || tcfdbfsiz(fdb) != ofsiz || tcfdbrnum(fdb) != tcmaprnum(map)){
    eprint(fdb, __LINE__, "(validation)");
    err = true;
  }
  inum = 0;
  tcmapiterinit(map);
  const char *tkbuf;
  int tksiz;
  for(int i = 1; (tkbuf = tcmapiternext(map, &tksiz)) != NULL; i++, inum++){
    int tvsiz;
    const char *tvbuf = tcmapiterval(tkbuf, &tvsiz);
    if(tvsiz > RECBUFSIZ) tvsiz = RECBUFSIZ;
    int rsiz;
    char *rbuf = tcfdbget2(fdb, tkbuf, tksiz, &rsiz);
    if(!rbuf || rsiz != tvsiz || memcmp(rbuf, tvbuf, rsiz)){
      eprint(fdb, __LINE__, "(validation)");
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
  if(!tcfdbiterinit(fdb)){
    eprint(fdb, __LINE__, "tcfdbiterinit");
    err = true;
  }
  for(int i = 1; (kbuf = tcfdbiternext2(fdb, &ksiz)) != NULL; i++, inum++){
    int vsiz;
    char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
    int rsiz;
    const char *rbuf = tcmapget(map, kbuf, ksiz, &rsiz);
    if(rsiz > RECBUFSIZ) rsiz = RECBUFSIZ;
    if(!rbuf || rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(fdb, __LINE__, "(validation)");
      err = true;
      tcfree(vbuf);
      tcfree(kbuf);
      break;
    }
    tcfree(vbuf);
    tcfree(kbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  tcmapdel(map);
  if(!tcfdbvanish(fdb)){
    eprint(fdb, __LINE__, "tcfdbvanish");
    err = true;
  }
  if(rnum >= 100){
    if(!tcfdbtranbegin(fdb)){
      eprint(fdb, __LINE__, "tcfdbtranbegin");
      err = true;
    }
    if(!tcfdbput3(fdb, "99", "hirabayashi")){
      eprint(fdb, __LINE__, "tcfdbput3");
      err = true;
    }
    for(int i = 0; i < 10; i++){
      char buf[RECBUFSIZ];
      int size = sprintf(buf, "%d", myrand(rnum) + 1);
      if(!tcfdbput2(fdb, buf, size, buf, size)){
        eprint(fdb, __LINE__, "tcfdbput2");
        err = true;
      }
    }
    for(int i = myrand(3) + 1; i < PATH_MAX; i = i * 2 + myrand(3)){
      char vbuf[i];
      memset(vbuf, '@', i - 1);
      vbuf[i-1] = '\0';
      if(!tcfdbput3(fdb, "99", vbuf)){
        eprint(fdb, __LINE__, "tcfdbput3");
        err = true;
      }
    }
    if(!tcfdbforeach(fdb, iterfunc, NULL)){
      eprint(fdb, __LINE__, "tcfdbforeach");
      err = true;
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcfdbrnum(fdb));
  iprintf("size: %llu\n", (unsigned long long)tcfdbfsiz(fdb));
  mprint(fdb);
  sysprint();
  if(!tcfdbclose(fdb)){
    eprint(fdb, __LINE__, "tcfdbclose");
    err = true;
  }
  tcfdbdel(fdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform wicked command */
static int procwicked(const char *path, int rnum, bool mt, int omode){
  iprintf("<Wicked Writing Test>\n  seed=%u  path=%s  rnum=%d  mt=%d  omode=%d\n\n",
          g_randseed, path, rnum, mt, omode);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(mt && !tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbtune(fdb, RECBUFSIZ * 2, EXHEADSIZ + (RECBUFSIZ * 2 + sizeof(int)) * rnum)){
    eprint(fdb, __LINE__, "tcfdbtune");
    err = true;
  }
  if(!tcfdbopen(fdb, path, FDBOWRITER | FDBOCREAT | FDBOTRUNC | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  if(!tcfdbiterinit(fdb)){
    eprint(fdb, __LINE__, "tcfdbiterinit");
    err = true;
  }
  TCMAP *map = tcmapnew2(rnum / 5);
  for(int i = 1; i <= rnum && !err; i++){
    uint64_t id = myrand(rnum) + 1;
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%llu", (unsigned long long)id);
    char vbuf[RECBUFSIZ];
    int vsiz = myrand(RECBUFSIZ);
    memset(vbuf, '*', vsiz);
    vbuf[vsiz] = '\0';
    char *rbuf;
    switch(myrand(16)){
      case 0:
        iputchar('0');
        if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbput2");
          err = true;
        }
        tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 1:
        iputchar('1');
        if(!tcfdbput3(fdb, kbuf, vbuf)){
          eprint(fdb, __LINE__, "tcfdbput3");
          err = true;
        }
        tcmapput2(map, kbuf, vbuf);
        break;
      case 2:
        iputchar('2');
        if(!tcfdbputkeep2(fdb, kbuf, ksiz, vbuf, vsiz) && tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbputkeep2");
          err = true;
        }
        tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 3:
        iputchar('3');
        if(!tcfdbputkeep3(fdb, kbuf, vbuf) && tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbputkeep3");
          err = true;
        }
        tcmapputkeep2(map, kbuf, vbuf);
        break;
      case 4:
        iputchar('4');
        if(!tcfdbputcat2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbputcat2");
          err = true;
        }
        tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 5:
        iputchar('5');
        if(!tcfdbputcat3(fdb, kbuf, vbuf)){
          eprint(fdb, __LINE__, "tcfdbputcat3");
          err = true;
        }
        tcmapputcat2(map, kbuf, vbuf);
        break;
      case 6:
        iputchar('6');
        if(myrand(10) == 0){
          if(!tcfdbout2(fdb, kbuf, ksiz) && tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbout2");
            err = true;
          }
          tcmapout(map, kbuf, ksiz);
        }
        break;
      case 7:
        iputchar('7');
        if(myrand(10) == 0){
          if(!tcfdbout3(fdb, kbuf) && tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbout3");
            err = true;
          }
          tcmapout2(map, kbuf);
        }
        break;
      case 8:
        iputchar('8');
        if(!(rbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz))){
          if(tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbget2");
            err = true;
          }
          rbuf = tcsprintf("[%d]", myrand(i + 1));
          vsiz = strlen(rbuf);
        }
        vsiz += myrand(vsiz);
        rbuf = tcrealloc(rbuf, vsiz + 1);
        for(int j = 0; j < vsiz; j++){
          rbuf[j] = myrand(0x100);
        }
        if(!tcfdbput2(fdb, kbuf, ksiz, rbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbput2");
          err = true;
        }
        tcmapput(map, kbuf, ksiz, rbuf, vsiz);
        tcfree(rbuf);
        break;
      case 9:
        iputchar('9');
        if(!(rbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz)) && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbget2");
          err = true;
        }
        tcfree(rbuf);
        break;
      case 10:
        iputchar('A');
        if(!(rbuf = tcfdbget3(fdb, kbuf)) && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbget3");
          err = true;
        }
        tcfree(rbuf);
        break;
      case 11:
        iputchar('B');
        if(myrand(1) == 0) vsiz = 1;
        if((vsiz = tcfdbget4(fdb, id, vbuf, vsiz)) < 0 && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbget4");
          err = true;
        }
        break;
      case 12:
        iputchar('C');
        if(myrand(rnum / 128) == 0){
          if(myrand(2) == 0){
            if(!tcfdbiterinit(fdb)){
              eprint(fdb, __LINE__, "tcfdbiterinit");
              err = true;
            }
          } else {
            if(!tcfdbiterinit2(fdb, myrand(rnum) + 1) && tcfdbecode(fdb) != TCENOREC){
              eprint(fdb, __LINE__, "tcfdbiterinit2");
              err = true;
            }
          }
        }
        for(int j = myrand(rnum) / 1000 + 1; j >= 0; j--){
          if(tcfdbiternext(fdb) < 0){
            int ecode = tcfdbecode(fdb);
            if(ecode != TCEINVALID && ecode != TCENOREC){
              eprint(fdb, __LINE__, "tcfdbiternext");
              err = true;
            }
          }
        }
        break;
      default:
        iputchar('@');
        if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
        if(myrand(rnum / 16 + 1) == 0){
          int cnt = myrand(30);
          for(int j = 0; j < rnum && !err; j++){
            ksiz = sprintf(kbuf, "%d", i + j);
            if(tcfdbout2(fdb, kbuf, ksiz)){
              cnt--;
            } else if(tcfdbecode(fdb) != TCENOREC){
              eprint(fdb, __LINE__, "tcfdbout2");
              err = true;
            }
            tcmapout(map, kbuf, ksiz);
            if(cnt < 0) break;
          }
        }
        break;
    }
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
    if(i == rnum / 2){
      if(!tcfdbclose(fdb)){
        eprint(fdb, __LINE__, "tcfdbclose");
        err = true;
      }
      if(!tcfdbopen(fdb, path, FDBOWRITER | omode)){
        eprint(fdb, __LINE__, "tcfdbopen");
        err = true;
      }
    } else if(i == rnum / 4){
      char *npath = tcsprintf("%s-tmp", path);
      if(!tcfdbcopy(fdb, npath)){
        eprint(fdb, __LINE__, "tcfdbcopy");
        err = true;
      }
      TCFDB *nfdb = tcfdbnew();
      if(!tcfdbopen(nfdb, npath, FDBOREADER | omode)){
        eprint(nfdb, __LINE__, "tcfdbopen");
        err = true;
      }
      tcfdbdel(nfdb);
      unlink(npath);
      tcfree(npath);
      if(!tcfdboptimize(fdb, RECBUFSIZ, -1)){
        eprint(fdb, __LINE__, "tcfdboptimize");
        err = true;
      }
      if(!tcfdbiterinit(fdb)){
        eprint(fdb, __LINE__, "tcfdbiterinit");
        err = true;
      }
    } else if(i == rnum / 8){
      if(!tcfdbtranbegin(fdb)){
        eprint(fdb, __LINE__, "tcfdbtranbegin");
        err = true;
      }
    } else if(i == rnum / 8 + rnum / 16){
      if(!tcfdbtrancommit(fdb)){
        eprint(fdb, __LINE__, "tcfdbtrancommit");
        err = true;
      }
    }
  }
  if(rnum % 50 > 0) iprintf(" (%08d)\n", rnum);
  if(!tcfdbsync(fdb)){
    eprint(fdb, __LINE__, "tcfdbsync");
    err = true;
  }
  if(tcfdbrnum(fdb) != tcmaprnum(map)){
    eprint(fdb, __LINE__, "(validation)");
    err = true;
  }
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", i);
    int vsiz;
    const char *vbuf = tcmapget(map, kbuf, ksiz, &vsiz);
    int rsiz;
    char *rbuf = tcfdbget2(fdb, kbuf, ksiz, &rsiz);
    if(vbuf){
      iputchar('.');
      if(vsiz > RECBUFSIZ) vsiz = RECBUFSIZ;
      if(!rbuf){
        eprint(fdb, __LINE__, "tcfdbget2");
        err = true;
      } else if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
        eprint(fdb, __LINE__, "(validation)");
        err = true;
      }
    } else {
      iputchar('*');
      if(rbuf || tcfdbecode(fdb) != TCENOREC){
        eprint(fdb, __LINE__, "(validation)");
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
    if(vsiz > tcfdbwidth(fdb)) vsiz = tcfdbwidth(fdb);
    int rsiz;
    char *rbuf = tcfdbget2(fdb, kbuf, ksiz, &rsiz);
    if(!rbuf){
      eprint(fdb, __LINE__, "tcfdbget2");
      err = true;
    } else if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(fdb, __LINE__, "(validation)");
      err = true;
    }
    tcfree(rbuf);
    if(!tcfdbout2(fdb, kbuf, ksiz)){
      eprint(fdb, __LINE__, "tcfdbout2");
      err = true;
    }
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
  }
  int mrnum = tcmaprnum(map);
  if(mrnum % 50 > 0) iprintf(" (%08d)\n", mrnum);
  if(tcfdbrnum(fdb) != 0){
    eprint(fdb, __LINE__, "(validation)");
    err = true;
  }
  iprintf("record number: %llu\n", (unsigned long long)tcfdbrnum(fdb));
  iprintf("size: %llu\n", (unsigned long long)tcfdbfsiz(fdb));
  mprint(fdb);
  sysprint();
  tcmapdel(map);
  if(!tcfdbclose(fdb)){
    eprint(fdb, __LINE__, "tcfdbclose");
    err = true;
  }
  tcfdbdel(fdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}



// END OF FILE
