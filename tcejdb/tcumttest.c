/*************************************************************************************************
 * The test cases of the on-memory database API
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
#include "myconf.h"

#define RECBUFSIZ      48                // buffer for records

typedef struct {                         // type of structure for combo thread
  TCMDB *mdb;
  TCNDB *ndb;
  int rnum;
  bool rnd;
  int id;
} TARGCOMBO;

typedef struct {                         // type of structure for typical thread
  TCMDB *mdb;
  TCNDB *ndb;
  int rnum;
  bool nc;
  int rratio;
  int id;
} TARGTYPICAL;


/* global variables */
const char *g_progname;                  // program name
unsigned int g_randseed;                 // random seed


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void iputchar(int c);
static void eprint(int line, const char *func);
static int myrand(int range);
static int myrandnd(int range);
static int runcombo(int argc, char **argv);
static int runtypical(int argc, char **argv);
static int proccombo(int tnum, int rnum, int bnum, bool tr, bool rnd);
static int proctypical(int tnum, int rnum, int bnum, bool tr, bool nc, int rratio);
static void *threadwrite(void *targ);
static void *threadread(void *targ);
static void *threadremove(void *targ);
static void *threadtypical(void *targ);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  const char *ebuf = getenv("TCRNDSEED");
  g_randseed = ebuf ? tcatoix(ebuf) : tctime() * 1000;
  srand(g_randseed);
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "combo")){
    rv = runcombo(argc, argv);
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
  fprintf(stderr, "%s: test cases of the on-memory database API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s combo [-tr] [-rnd] tnum rnum [bnum]\n", g_progname);
  fprintf(stderr, "  %s typical [-tr] [-nc] [-rr num] tnum rnum [bnum]\n", g_progname);
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


/* print error message of on-memory database */
static void eprint(int line, const char *func){
  fprintf(stderr, "%s: %d: %s: error\n", g_progname, line, func);
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


/* parse arguments of combo command */
static int runcombo(int argc, char **argv){
  char *tstr = NULL;
  char *rstr = NULL;
  char *bstr = NULL;
  bool tr = false;
  bool rnd = false;
  for(int i = 2; i < argc; i++){
    if(!tstr && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tr")){
        tr = true;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else {
        usage();
      }
    } else if(!tstr){
      tstr = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else {
      usage();
    }
  }
  if(!tstr || !rstr) usage();
  int tnum = tcatoix(tstr);
  int rnum = tcatoix(rstr);
  if(tnum < 1 || rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int rv = proccombo(tnum, rnum, bnum, tr, rnd);
  return rv;
}


/* parse arguments of typical command */
static int runtypical(int argc, char **argv){
  char *tstr = NULL;
  char *rstr = NULL;
  char *bstr = NULL;
  bool tr = false;
  int rratio = -1;
  bool nc = false;
  for(int i = 2; i < argc; i++){
    if(!tstr && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tr")){
        tr = true;
      } else if(!strcmp(argv[i], "-nc")){
        nc = true;
      } else if(!strcmp(argv[i], "-rr")){
        if(++i >= argc) usage();
        rratio = tcatoix(argv[i]);
      } else {
        usage();
      }
    } else if(!tstr){
      tstr = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else {
      usage();
    }
  }
  if(!tstr || !rstr) usage();
  int tnum = tcatoix(tstr);
  int rnum = tcatoix(rstr);
  if(tnum < 1 || rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int rv = proctypical(tnum, rnum, bnum, tr, nc, rratio);
  return rv;
}


/* perform combo command */
static int proccombo(int tnum, int rnum, int bnum, bool tr, bool rnd){
  iprintf("<Combination Test>\n  seed=%u  tnum=%d  rnum=%d  bnum=%d  tr=%d  rnd=%d\n\n",
          g_randseed, tnum, rnum, bnum, tr, rnd);
  bool err = false;
  double stime = tctime();
  TCMDB *mdb = (bnum > 0) ? tcmdbnew2(bnum) : tcmdbnew();
  TCNDB *ndb = tcndbnew();
  TARGCOMBO targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].mdb = mdb;
    targs[0].ndb = tr ? ndb : NULL;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadwrite(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].mdb = mdb;
      targs[i].ndb = tr ? ndb : NULL;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadwrite, targs + i) != 0){
        eprint(__LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(__LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  if(tnum == 1){
    targs[0].mdb = mdb;
    targs[0].ndb = tr ? ndb : NULL;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadread(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].mdb = mdb;
      targs[i].ndb = tr ? ndb : NULL;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadread, targs + i) != 0){
        eprint(__LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(__LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  if(tnum == 1){
    targs[0].mdb = mdb;
    targs[0].ndb = tr ? ndb : NULL;
    targs[0].rnum = rnum;
    targs[0].rnd = rnd;
    targs[0].id = 0;
    if(threadremove(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].mdb = mdb;
      targs[i].ndb = tr ? ndb : NULL;
      targs[i].rnum = rnum;
      targs[i].rnd = rnd;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadremove, targs + i) != 0){
        eprint(__LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(__LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  if(tr){
    iprintf("record number: %llu\n", (unsigned long long)tcndbrnum(ndb));
    iprintf("size: %llu\n", (unsigned long long)tcndbmsiz(ndb));
  } else {
    iprintf("record number: %llu\n", (unsigned long long)tcmdbrnum(mdb));
    iprintf("size: %llu\n", (unsigned long long)tcmdbmsiz(mdb));
  }
  tcndbdel(ndb);
  tcmdbdel(mdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform typical command */
static int proctypical(int tnum, int rnum, int bnum, bool tr, bool nc, int rratio){
  iprintf("<Typical Access Test>\n  seed=%u  tnum=%d  rnum=%d  bnum=%d  tr=%d  nc=%d"
          "  rratio=%d\n\n", g_randseed, tnum, rnum, bnum, tr, nc, rratio);
  bool err = false;
  double stime = tctime();
  TCMDB *mdb = (bnum > 0) ? tcmdbnew2(bnum) : tcmdbnew();
  TCNDB *ndb = tcndbnew();
  TARGTYPICAL targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].mdb = mdb;
    targs[0].ndb = tr ? ndb : NULL;
    targs[0].rnum = rnum;
    targs[0].nc = nc;
    targs[0].rratio = rratio;
    targs[0].id = 0;
    if(threadtypical(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].mdb = mdb;
      targs[i].ndb = tr ? ndb : NULL;
      targs[i].rnum = rnum;
      targs[i].nc = nc;
      targs[i].rratio= rratio;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadtypical, targs + i) != 0){
        eprint(__LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(__LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  if(tr){
    iprintf("record number: %llu\n", (unsigned long long)tcndbrnum(ndb));
    iprintf("size: %llu\n", (unsigned long long)tcndbmsiz(ndb));
  } else {
    iprintf("record number: %llu\n", (unsigned long long)tcmdbrnum(mdb));
    iprintf("size: %llu\n", (unsigned long long)tcmdbmsiz(mdb));
  }
  tcndbdel(ndb);
  tcmdbdel(mdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* thread the write function */
static void *threadwrite(void *targ){
  TCMDB *mdb = ((TARGCOMBO *)targ)->mdb;
  TCNDB *ndb = ((TARGCOMBO *)targ)->ndb;
  int rnum = ((TARGCOMBO *)targ)->rnum;
  bool rnd = ((TARGCOMBO *)targ)->rnd;
  int id = ((TARGCOMBO *)targ)->id;
  double stime = tctime();
  if(id == 0) iprintf("writing:\n");
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + (rnd ? myrand(i) : i));
    if(ndb){
      tcndbput(ndb, buf, len, buf, len);
    } else {
      tcmdbput(mdb, buf, len, buf, len);
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(id == 0) iprintf("time: %.3f\n", tctime() - stime);
  return NULL;
}


/* thread the read function */
static void *threadread(void *targ){
  TCMDB *mdb = ((TARGCOMBO *)targ)->mdb;
  TCNDB *ndb = ((TARGCOMBO *)targ)->ndb;
  int rnum = ((TARGCOMBO *)targ)->rnum;
  bool rnd = ((TARGCOMBO *)targ)->rnd;
  int id = ((TARGCOMBO *)targ)->id;
  double stime = tctime();
  if(id == 0) iprintf("reading:\n");
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", base + (rnd ? myrand(i) : i));
    int vsiz;
    char *vbuf = ndb ? tcndbget(ndb, kbuf, ksiz, &vsiz) : tcmdbget(mdb, kbuf, ksiz, &vsiz);
    if(vbuf) tcfree(vbuf);
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(id == 0) iprintf("time: %.3f\n", tctime() - stime);
  return NULL;
}


/* thread the remove function */
static void *threadremove(void *targ){
  TCMDB *mdb = ((TARGCOMBO *)targ)->mdb;
  TCNDB *ndb = ((TARGCOMBO *)targ)->ndb;
  int rnum = ((TARGCOMBO *)targ)->rnum;
  bool rnd = ((TARGCOMBO *)targ)->rnd;
  int id = ((TARGCOMBO *)targ)->id;
  double stime = tctime();
  if(id == 0) iprintf("removing:\n");
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + (rnd ? myrand(i) : i));
    if(ndb){
      tcndbout(ndb, buf, len);
    } else {
      tcmdbout(mdb, buf, len);
    }
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(id == 0) iprintf("time: %.3f\n", tctime() - stime);
  return NULL;
}


/* thread the typical function */
static void *threadtypical(void *targ){
  TCMDB *mdb = ((TARGTYPICAL *)targ)->mdb;
  TCNDB *ndb = ((TARGCOMBO *)targ)->ndb;
  int rnum = ((TARGTYPICAL *)targ)->rnum;
  bool nc = ((TARGTYPICAL *)targ)->nc;
  int rratio = ((TARGTYPICAL *)targ)->rratio;
  int id = ((TARGTYPICAL *)targ)->id;
  bool err = false;
  TCMAP *map = (!nc && id == 0) ? tcmapnew2(rnum + 1) : NULL;
  int base = id * rnum;
  int mrange = tclmax(50 + rratio, 100);
  for(int i = 1; !err && i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + myrandnd(i));
    int rnd = myrand(mrange);
    if(rnd < 10){
      if(ndb){
        tcndbput(ndb, buf, len, buf, len);
      } else {
        tcmdbput(mdb, buf, len, buf, len);
      }
      if(map) tcmapput(map, buf, len, buf, len);
    } else if(rnd < 15){
      if(ndb){
        tcndbputkeep(ndb, buf, len, buf, len);
      } else {
        tcmdbputkeep(mdb, buf, len, buf, len);
      }
      if(map) tcmapputkeep(map, buf, len, buf, len);
    } else if(rnd < 20){
      if(ndb){
        tcndbputcat(ndb, buf, len, buf, len);
      } else {
        tcmdbputcat(mdb, buf, len, buf, len);
      }
      if(map) tcmapputcat(map, buf, len, buf, len);
    } else if(rnd < 30){
      if(ndb){
        tcndbout(ndb, buf, len);
      } else {
        tcmdbout(mdb, buf, len);
      }
      if(map) tcmapout(map, buf, len);
    } else if(rnd < 31){
      if(myrand(10) == 0) tcmdbiterinit(mdb);
      for(int j = 0; !err && j < 10; j++){
        int ksiz;
        char *kbuf = ndb ? tcndbiternext(ndb, &ksiz) : tcmdbiternext(mdb, &ksiz);
        if(kbuf) tcfree(kbuf);
      }
    } else {
      int vsiz;
      char *vbuf = ndb ? tcndbget(ndb, buf, len, &vsiz) : tcmdbget(mdb, buf, len, &vsiz);
      if(vbuf){
        if(map){
          int msiz;
          const char *mbuf = tcmapget(map, buf, len, &msiz);
          if(msiz != vsiz || memcmp(mbuf, vbuf, vsiz)){
            eprint(__LINE__, "(validation)");
            err = true;
          }
        }
        tcfree(vbuf);
      } else {
        if(map && tcmapget(map, buf, len, &vsiz)){
          eprint(__LINE__, "(validation)");
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
      char *vbuf = ndb ? tcndbget(ndb, kbuf, ksiz, &vsiz) : tcmdbget(mdb, kbuf, ksiz, &vsiz);
      if(vbuf){
        int msiz;
        const char *mbuf = tcmapget(map, kbuf, ksiz, &msiz);
        if(!mbuf || msiz != vsiz || memcmp(mbuf, vbuf, vsiz)){
          eprint(__LINE__, "(validation)");
          err = true;
        }
        tcfree(vbuf);
      } else {
        eprint(__LINE__, "(validation)");
        err = true;
      }
    }
    tcmapdel(map);
  }
  return err ? "error" : NULL;
}



// END OF FILE
