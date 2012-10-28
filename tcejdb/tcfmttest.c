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

typedef struct {                         // type of structure for write thread
  TCFDB *fdb;
  int rnum;
  bool rnd;
  int id;
} TARGWRITE;

typedef struct {                         // type of structure for read thread
  TCFDB *fdb;
  int rnum;
  bool wb;
  bool rnd;
  int id;
} TARGREAD;

typedef struct {                         // type of structure for remove thread
  TCFDB *fdb;
  int rnum;
  bool rnd;
  int id;
} TARGREMOVE;

typedef struct {                         // type of structure for wicked thread
  TCFDB *fdb;
  int rnum;
  bool nc;
  int id;
  TCMAP *map;
} TARGWICKED;

typedef struct {                         // type of structure for typical thread
  TCFDB *fdb;
  int rnum;
  bool nc;
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
static void eprint(TCFDB *fdb, int line, const char *func);
static void mprint(TCFDB *fdb);
static void sysprint(void);
static int myrand(int range);
static int myrandnd(int range);
static bool iterfunc(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int runtypical(int argc, char **argv);
static int procwrite(const char *path, int tnum, int rnum, int width, int64_t limsiz,
                     int omode, bool rnd);
static int procread(const char *path, int tnum, int omode, bool wb, bool rnd);
static int procremove(const char *path, int tnum, int omode, bool rnd);
static int procwicked(const char *path, int tnum, int rnum, int omode, bool nc);
static int proctypical(const char *path, int tnum, int rnum, int width, int64_t limsiz,
                       int omode, bool nc, int rratio);
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
  fprintf(stderr, "%s: test cases of the fixed-length database API of Tokyo Cabinet\n",
          g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write [-nl|-nb] [-rnd] path tnum rnum [width [limsiz]]\n", g_progname);
  fprintf(stderr, "  %s read [-nl|-nb] [-wb] [-rnd] path tnum\n", g_progname);
  fprintf(stderr, "  %s remove [-nl|-nb] [-rnd] path tnum\n", g_progname);
  fprintf(stderr, "  %s wicked [-nl|-nb] [-nc] path tnum rnum\n", g_progname);
  fprintf(stderr, "  %s typical [-nl|-nb] [-nc] [-rr num] path tnum rnum [width [limsiz]]\n",
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
  char *wstr = NULL;
  char *lstr = NULL;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
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
    } else if(!tstr){
      tstr = argv[i];
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
  if(!path || !tstr || !rstr) usage();
  int tnum = tcatoix(tstr);
  int rnum = tcatoix(rstr);
  if(tnum < 1 || rnum < 1) usage();
  int width = wstr ? tcatoix(wstr) : -1;
  int64_t limsiz = lstr ? tcatoix(lstr) : -1;
  int rv = procwrite(path, tnum, rnum, width, limsiz, omode, rnd);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  int omode = 0;
  bool wb = false;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
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
    } else if(!tstr){
      tstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !tstr) usage();
  int tnum = tcatoix(tstr);
  if(tnum < 1) usage();
  int rv = procread(path, tnum, omode, wb, rnd);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  int omode = 0;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
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
    } else if(!tstr){
      tstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !tstr) usage();
  int tnum = tcatoix(tstr);
  if(tnum < 1) usage();
  int rv = procremove(path, tnum, omode, rnd);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  int omode = 0;
  bool nc = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int rv = procwicked(path, tnum, rnum, omode, nc);
  return rv;
}


/* parse arguments of typical command */
static int runtypical(int argc, char **argv){
  char *path = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  char *wstr = NULL;
  char *lstr = NULL;
  int omode = 0;
  int rratio = -1;
  bool nc = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
    } else if(!wstr){
      wstr = argv[i];
    } else if(!lstr){
      lstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !tstr || !rstr) usage();
  int tnum = tcatoix(tstr);
  int rnum = tcatoix(rstr);
  if(tnum < 1 || rnum < 1) usage();
  int width = wstr ? tcatoix(wstr) : -1;
  int64_t limsiz = lstr ? tcatoix(lstr) : -1;
  int rv = proctypical(path, tnum, rnum, width, limsiz, omode, nc, rratio);
  return rv;
}


/* perform write command */
static int procwrite(const char *path, int tnum, int rnum, int width, int64_t limsiz,
                     int omode, bool rnd){
  iprintf("<Writing Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  width=%d  limsiz=%lld"
          "  omode=%d  rnd=%d\n\n",
          g_randseed, path, tnum, rnum, width, (long long)limsiz, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbsetmutex(fdb)){
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
  TARGWRITE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].fdb = fdb;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadwrite(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].fdb = fdb;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadwrite, targs + i) != 0){
        eprint(fdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(fdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
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
static int procread(const char *path, int tnum, int omode, bool wb, bool rnd){
  iprintf("<Reading Test>\n  seed=%u  path=%s  tnum=%d  omode=%d  wb=%d  rnd=%d\n\n",
          g_randseed, path, tnum, omode, wb, rnd);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbopen(fdb, path, FDBOREADER | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  int rnum = tcfdbrnum(fdb) / tnum;
  TARGREAD targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].fdb = fdb;
    targs[0].rnum = rnum;
    targs[0].wb = wb;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadread(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].fdb = fdb;
      targs[i].rnum = rnum;
      targs[i].wb = wb;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadread, targs + i) != 0){
        eprint(fdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(fdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
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
static int procremove(const char *path, int tnum, int omode, bool rnd){
  iprintf("<Removing Test>\n  seed=%u  path=%s  tnum=%d  omode=%d  rnd=%d\n\n",
          g_randseed, path, tnum, omode, rnd);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbopen(fdb, path, FDBOWRITER | omode)){
    eprint(fdb, __LINE__, "tcfdbopen");
    err = true;
  }
  int rnum = tcfdbrnum(fdb) / tnum;
  TARGREMOVE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].fdb = fdb;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadremove(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].fdb = fdb;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadremove, targs + i) != 0){
        eprint(fdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(fdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
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
static int procwicked(const char *path, int tnum, int rnum, int omode, bool nc){
  iprintf("<Writing Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  omode=%d  nc=%d\n\n",
          g_randseed, path, tnum, rnum, omode, nc);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbsetmutex(fdb)){
    eprint(fdb, __LINE__, "tcfdbsetmutex");
    err = true;
  }
  if(!tcfdbtune(fdb, RECBUFSIZ * 2, EXHEADSIZ + (RECBUFSIZ * 2 + sizeof(int)) * rnum * tnum)){
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
  TARGWICKED targs[tnum];
  pthread_t threads[tnum];
  TCMAP *map = tcmapnew();
  if(tnum == 1){
    targs[0].fdb = fdb;
    targs[0].rnum = rnum;
    targs[0].nc = nc;
    targs[0].id = 0;
    targs[0].map = map;
    if(threadwicked(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].fdb = fdb;
      targs[i].rnum = rnum;
      targs[i].nc = nc;
      targs[i].id = i;
      targs[i].map = map;
      if(pthread_create(threads + i, NULL, threadwicked, targs + i) != 0){
        eprint(fdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(fdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  if(!nc){
    if(!tcfdbsync(fdb)){
      eprint(fdb, __LINE__, "tcfdbsync");
      err = true;
    }
    if(tcfdbrnum(fdb) != tcmaprnum(map)){
      eprint(fdb, __LINE__, "(validation)");
      err = true;
    }
    int end = rnum * tnum;
    for(int i = 1; i <= end && !err; i++){
      char kbuf[RECBUFSIZ];
      int ksiz = sprintf(kbuf, "%d", i);
      int vsiz;
      const char *vbuf = tcmapget(map, kbuf, ksiz, &vsiz);
      int rsiz;
      char *rbuf = tcfdbget2(fdb, kbuf, ksiz, &rsiz);
      if(vbuf){
        iputchar('.');
        if(vsiz > tcfdbwidth(fdb)) vsiz = tcfdbwidth(fdb);
        if(!rbuf){
          eprint(fdb, __LINE__, "tcfdbget");
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
  }
  tcmapdel(map);
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


/* perform typical command */
static int proctypical(const char *path, int tnum, int rnum, int width, int64_t limsiz,
                       int omode, bool nc, int rratio){
  iprintf("<Typical Access Test>\n  seed=%u  path=%s  tnum=%d  rnum=%d  width=%d  limsiz=%lld"
          "  omode=%d  nc=%d  rratio=%d\n\n",
          g_randseed, path, tnum, rnum, width, (long long)limsiz, omode, nc, rratio);
  bool err = false;
  double stime = tctime();
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbsetmutex(fdb)){
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
  TARGTYPICAL targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].fdb = fdb;
    targs[0].rnum = rnum;
    targs[0].nc = nc;
    targs[0].rratio = rratio;
    targs[0].id = 0;
    if(threadtypical(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].fdb = fdb;
      targs[i].rnum = rnum;
      targs[i].nc = nc;
      targs[i].rratio= rratio;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadtypical, targs + i) != 0){
        eprint(fdb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(fdb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
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


/* thread the write function */
static void *threadwrite(void *targ){
  TCFDB *fdb = ((TARGWRITE *)targ)->fdb;
  int rnum = ((TARGWRITE *)targ)->rnum;
  bool rnd = ((TARGWRITE *)targ)->rnd;
  int id = ((TARGWRITE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + (rnd ? myrand(i) + 1 : i));
    if(!tcfdbput2(fdb, buf, len, buf, len)){
      eprint(fdb, __LINE__, "tcfdbput2");
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


/* thread the read function */
static void *threadread(void *targ){
  TCFDB *fdb = ((TARGREAD *)targ)->fdb;
  int rnum = ((TARGREAD *)targ)->rnum;
  bool wb = ((TARGREAD *)targ)->wb;
  bool rnd = ((TARGREAD *)targ)->rnd;
  int id = ((TARGREAD *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum && !err; i++){
    uint64_t kid = base + (rnd ? myrandnd(i) + 1 : i);
    int vsiz;
    if(wb){
      char vbuf[RECBUFSIZ];
      int vsiz = tcfdbget4(fdb, kid, vbuf, RECBUFSIZ);
      if(vsiz < 0 && (!rnd || tcfdbecode(fdb) != TCENOREC)){
        eprint(fdb, __LINE__, "tcfdbget4");
        err = true;
      }
    } else {
      char *vbuf = tcfdbget(fdb, kid, &vsiz);
      if(!vbuf && (!rnd || tcfdbecode(fdb) != TCENOREC)){
        eprint(fdb, __LINE__, "tcfdbget");
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
  TCFDB *fdb = ((TARGREMOVE *)targ)->fdb;
  int rnum = ((TARGREMOVE *)targ)->rnum;
  bool rnd = ((TARGREMOVE *)targ)->rnd;
  int id = ((TARGREMOVE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", base + (rnd ? myrand(i + 1) + 1 : i));
    if(!tcfdbout2(fdb, kbuf, ksiz) && (!rnd || tcfdbecode(fdb) != TCENOREC)){
      eprint(fdb, __LINE__, "tcfdbout2");
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
  TCFDB *fdb = ((TARGWICKED *)targ)->fdb;
  int rnum = ((TARGWICKED *)targ)->rnum;
  bool nc = ((TARGWICKED *)targ)->nc;
  int id = ((TARGWICKED *)targ)->id;
  TCMAP *map = ((TARGWICKED *)targ)->map;
  bool err = false;
  for(int i = 1; i <= rnum && !err; i++){
    uint64_t kid = myrand(rnum * (id + 1)) + 1;
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%llu", (unsigned long long)kid);
    char vbuf[RECBUFSIZ];
    int vsiz = myrand(RECBUFSIZ);
    memset(vbuf, '*', vsiz);
    vbuf[vsiz] = '\0';
    char *rbuf;
    if(!nc) tcglobalmutexlock();
    switch(myrand(16)){
      case 0:
        if(id == 0) iputchar('0');
        if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbput2");
          err = true;
        }
        if(!nc) tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 1:
        if(id == 0) iputchar('1');
        if(!tcfdbput3(fdb, kbuf, vbuf)){
          eprint(fdb, __LINE__, "tcfdbput3");
          err = true;
        }
        if(!nc) tcmapput2(map, kbuf, vbuf);
        break;
      case 2:
        if(id == 0) iputchar('2');
        if(!tcfdbputkeep2(fdb, kbuf, ksiz, vbuf, vsiz) && tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbputkeep2");
          err = true;
        }
        if(!nc) tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 3:
        if(id == 0) iputchar('3');
        if(!tcfdbputkeep3(fdb, kbuf, vbuf) && tcfdbecode(fdb) != TCEKEEP){
          eprint(fdb, __LINE__, "tcfdbputkeep3");
          err = true;
        }
        if(!nc) tcmapputkeep2(map, kbuf, vbuf);
        break;
      case 4:
        if(id == 0) iputchar('4');
        if(!tcfdbputcat2(fdb, kbuf, ksiz, vbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbputcat2");
          err = true;
        }
        if(!nc) tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        break;
      case 5:
        if(id == 0) iputchar('5');
        if(!tcfdbputcat3(fdb, kbuf, vbuf)){
          eprint(fdb, __LINE__, "tcfdbputcat3");
          err = true;
        }
        if(!nc) tcmapputcat2(map, kbuf, vbuf);
        break;
      case 6:
        if(id == 0) iputchar('6');
        if(myrand(2) == 0){
          if(!tcfdbout2(fdb, kbuf, ksiz) && tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbout2");
            err = true;
          }
          if(!nc) tcmapout(map, kbuf, ksiz);
        }
        break;
      case 7:
        if(id == 0) iputchar('7');
        if(myrand(2) == 0){
          if(!tcfdbout3(fdb, kbuf) && tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbout3");
            err = true;
          }
          if(!nc) tcmapout2(map, kbuf);
        }
        break;
      case 8:
        if(id == 0) iputchar('8');
        if(!(rbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz))){
          if(tcfdbecode(fdb) != TCENOREC){
            eprint(fdb, __LINE__, "tcfdbget2");
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
        if(!tcfdbput2(fdb, kbuf, ksiz, rbuf, vsiz)){
          eprint(fdb, __LINE__, "tcfdbput2");
          err = true;
        }
        if(!nc) tcmapput(map, kbuf, ksiz, rbuf, vsiz);
        tcfree(rbuf);
        break;
      case 9:
        if(id == 0) iputchar('9');
        if(!(rbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz)) && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbget2");
          err = true;
        }
        tcfree(rbuf);
        break;
      case 10:
        if(id == 0) iputchar('A');
        if(!(rbuf = tcfdbget3(fdb, kbuf)) && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbge3");
          err = true;
        }
        tcfree(rbuf);
        break;
      case 11:
        if(id == 0) iputchar('B');
        if(myrand(1) == 0) vsiz = 1;
        if((vsiz = tcfdbget4(fdb, kid, vbuf, vsiz)) < 0 && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbget4");
          err = true;
        }
        break;
      case 14:
        if(id == 0) iputchar('E');
        if(myrand(rnum / 50) == 0){
          if(!tcfdbiterinit(fdb)){
            eprint(fdb, __LINE__, "tcfdbiterinit");
            err = true;
          }
        }
        for(int j = myrand(rnum) / 1000 + 1; j >= 0; j--){
          if(tcfdbiternext(fdb) < 1){
            int ecode = tcfdbecode(fdb);
            if(ecode != TCEINVALID && ecode != TCENOREC){
              eprint(fdb, __LINE__, "tcfdbiternext");
              err = true;
            }
          }
        }
        break;
      default:
        if(id == 0) iputchar('@');
        if(tcfdbtranbegin(fdb)){
          if(myrand(2) == 0){
            if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
              eprint(fdb, __LINE__, "tcfdbput");
              err = true;
            }
            if(!nc) tcmapput(map, kbuf, ksiz, vbuf, vsiz);
          } else {
            if(!tcfdbout2(fdb, kbuf, ksiz) && tcfdbecode(fdb) != TCENOREC){
              eprint(fdb, __LINE__, "tcfdbout");
              err = true;
            }
            if(!nc) tcmapout(map, kbuf, ksiz);
          }
          if(nc && myrand(2) == 0){
            if(!tcfdbtranabort(fdb)){
              eprint(fdb, __LINE__, "tcfdbtranabort");
              err = true;
            }
          } else {
            if(!tcfdbtrancommit(fdb)){
              eprint(fdb, __LINE__, "tcfdbtrancommit");
              err = true;
            }
          }
        } else {
          eprint(fdb, __LINE__, "tcfdbtranbegin");
          err = true;
        }
        if(myrand(1000) == 0){
          if(!tcfdbforeach(fdb, iterfunc, NULL)){
            eprint(fdb, __LINE__, "tcfdbforeach");
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
        if(!tcfdboptimize(fdb, RECBUFSIZ, -1) && tcfdbecode(fdb) != TCEINVALID){
          eprint(fdb, __LINE__, "tcfdboptimize");
          err = true;
        }
        if(!tcfdbiterinit(fdb)){
          eprint(fdb, __LINE__, "tcfdbiterinit");
          err = true;
        }
      }
    }
  }
  return err ? "error" : NULL;
}


/* thread the typical function */
static void *threadtypical(void *targ){
  TCFDB *fdb = ((TARGTYPICAL *)targ)->fdb;
  int rnum = ((TARGTYPICAL *)targ)->rnum;
  bool nc = ((TARGTYPICAL *)targ)->nc;
  int rratio = ((TARGTYPICAL *)targ)->rratio;
  int id = ((TARGTYPICAL *)targ)->id;
  bool err = false;
  TCMAP *map = (!nc && id == 0) ? tcmapnew2(rnum + 1) : NULL;
  int base = id * rnum;
  int mrange = tclmax(50 + rratio, 100);
  int width = tcfdbwidth(fdb);
  for(int i = 1; !err && i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + myrandnd(i) + 1);
    int rnd = myrand(mrange);
    if(rnd < 10){
      if(!tcfdbput2(fdb, buf, len, buf, len)){
        eprint(fdb, __LINE__, "tcfdbput2");
        err = true;
      }
      if(map) tcmapput(map, buf, len, buf, len);
    } else if(rnd < 15){
      if(!tcfdbputkeep2(fdb, buf, len, buf, len) && tcfdbecode(fdb) != TCEKEEP){
        eprint(fdb, __LINE__, "tcfdbputkeep2");
        err = true;
      }
      if(map) tcmapputkeep(map, buf, len, buf, len);
    } else if(rnd < 20){
      if(!tcfdbputcat2(fdb, buf, len, buf, len)){
        eprint(fdb, __LINE__, "tcfdbputcat2");
        err = true;
      }
      if(map) tcmapputcat(map, buf, len, buf, len);
    } else if(rnd < 25){
      if(!tcfdbout2(fdb, buf, len) && tcfdbecode(fdb) && tcfdbecode(fdb) != TCENOREC){
        eprint(fdb, __LINE__, "tcfdbout");
        err = true;
      }
      if(map) tcmapout(map, buf, len);
    } else if(rnd < 26){
      if(myrand(10) == 0 && !tcfdbiterinit(fdb) && tcfdbecode(fdb) != TCENOREC){
        eprint(fdb, __LINE__, "tcfdbiterinit");
        err = true;
      }
      for(int j = 0; !err && j < 10; j++){
        if(tcfdbiternext(fdb) < 1 &&
           tcfdbecode(fdb) != TCEINVALID && tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbiternext");
          err = true;
        }
      }
    } else {
      int vsiz;
      char *vbuf = tcfdbget2(fdb, buf, len, &vsiz);
      if(vbuf){
        if(map){
          int msiz = 0;
          const char *mbuf = tcmapget(map, buf, len, &msiz);
          if(msiz > width) msiz = width;
          if(!mbuf || msiz != vsiz || memcmp(mbuf, vbuf, vsiz)){
            eprint(fdb, __LINE__, "(validation)");
            err = true;
          }
        }
        tcfree(vbuf);
      } else {
        if(tcfdbecode(fdb) != TCENOREC){
          eprint(fdb, __LINE__, "tcfdbget");
          err = true;
        }
        if(map && tcmapget(map, buf, len, &vsiz)){
          eprint(fdb, __LINE__, "(validation)");
          err = true;
        }
      }
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(map){
    tcmapiterinit(map);
    int ksiz;
    const char *kbuf;
    while(!err && (kbuf = tcmapiternext(map, &ksiz)) != NULL){
      int vsiz;
      char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
      if(vbuf){
        int msiz = 0;
        const char *mbuf = tcmapget(map, kbuf, ksiz, &msiz);
        if(msiz > width) msiz = width;
        if(!mbuf || msiz != vsiz || memcmp(mbuf, vbuf, vsiz)){
          eprint(fdb, __LINE__, "(validation)");
          err = true;
        }
        tcfree(vbuf);
      } else {
        eprint(fdb, __LINE__, "(validation)");
        err = true;
      }
    }
    tcmapdel(map);
  }
  return err ? "error" : NULL;
}



// END OF FILE
