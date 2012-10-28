/*************************************************************************************************
 * The test cases of the abstract database API
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
#include <tcadb.h>
#include "myconf.h"

#define MULDIVNUM      8                 // division number of multiple database
#define RECBUFSIZ      48                // buffer for records

typedef struct {                         // type of structure for write thread
  TCADB *adb;
  int rnum;
  int id;
} TARGWRITE;

typedef struct {                         // type of structure for read thread
  TCADB *adb;
  int rnum;
  int id;
} TARGREAD;

typedef struct {                         // type of structure for remove thread
  TCADB *adb;
  int rnum;
  int id;
} TARGREMOVE;


/* global variables */
const char *g_progname;                  // program name
unsigned int g_randseed;                 // random seed
int g_dbgfd;                             // debugging output


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void iputchar(int c);
static void eprint(TCADB *adb, int line, const char *func);
static void sysprint(void);
static void setskeltran(ADBSKEL *skel);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int procwrite(const char *name, int tnum, int rnum);
static int procread(const char *name, int tnum);
static int procremove(const char *name, int tnum);
static void *threadwrite(void *targ);
static void *threadread(void *targ);
static void *threadremove(void *targ);


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
  fprintf(stderr, "%s: test cases of the abstract database API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write name tnum rnum\n", g_progname);
  fprintf(stderr, "  %s read name tnum\n", g_progname);
  fprintf(stderr, "  %s remove name tnum\n", g_progname);
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


/* print error message of abstract database */
static void eprint(TCADB *adb, int line, const char *func){
  const char *path = adb ? tcadbpath(adb) : NULL;
  fprintf(stderr, "%s: %s: %d: %s: error\n", g_progname, path ? path : "-", line, func);
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


/* set the transparent skeleton database */
static void setskeltran(ADBSKEL *skel){
  memset(skel, 0, sizeof(*skel));
  skel->opq = tcadbnew();
  skel->del = (void (*)(void *))tcadbdel;
  skel->open = (bool (*)(void *, const char *))tcadbopen;
  skel->close = (bool (*)(void *))tcadbclose;
  skel->put = (bool (*)(void *, const void *, int, const void *, int))tcadbput;
  skel->putkeep = (bool (*)(void *, const void *, int, const void *, int))tcadbputkeep;
  skel->putcat = (bool (*)(void *, const void *, int, const void *, int))tcadbputcat;
  skel->out = (bool (*)(void *, const void *, int))tcadbout;
  skel->get = (void *(*)(void *, const void *, int, int *))tcadbget;
  skel->vsiz = (int (*)(void *, const void *, int))tcadbvsiz;
  skel->iterinit = (bool (*)(void *))tcadbiterinit;
  skel->iternext = (void *(*)(void *, int *))tcadbiternext;
  skel->fwmkeys = (TCLIST *(*)(void *, const void *, int, int))tcadbfwmkeys;
  skel->addint = (int (*)(void *, const void *, int, int))tcadbaddint;
  skel->adddouble = (double (*)(void *, const void *, int, double))tcadbadddouble;
  skel->sync = (bool (*)(void *))tcadbsync;
  skel->optimize = (bool (*)(void *, const char *))tcadboptimize;
  skel->vanish = (bool (*)(void *))tcadbvanish;
  skel->copy = (bool (*)(void *, const char *))tcadbcopy;
  skel->tranbegin = (bool (*)(void *))tcadbtranbegin;
  skel->trancommit = (bool (*)(void *))tcadbtrancommit;
  skel->tranabort = (bool (*)(void *))tcadbtranabort;
  skel->path = (const char *(*)(void *))tcadbpath;
  skel->rnum = (uint64_t (*)(void *))tcadbrnum;
  skel->size = (uint64_t (*)(void *))tcadbsize;
  skel->misc = (TCLIST *(*)(void *, const char *, const TCLIST *))tcadbmisc;
  skel->putproc =
    (bool (*)(void *, const void *, int, const void *, int, TCPDPROC, void *))tcadbputproc;
  skel->foreach = (bool (*)(void *, TCITER, void *))tcadbforeach;
}


/* parse arguments of write command */
static int runwrite(int argc, char **argv){
  char *name = NULL;
  char *tstr = NULL;
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !tstr || !rstr) usage();
  int tnum = tcatoix(tstr);
  int rnum = tcatoix(rstr);
  if(tnum < 1 || rnum < 1) usage();
  int rv = procwrite(name, tnum, rnum);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *name = NULL;
  char *tstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !tstr) usage();
  int tnum = tcatoix(tstr);
  if(tnum < 1) usage();
  int rv = procread(name, tnum);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *name = NULL;
  char *tstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!tstr){
      tstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !tstr) usage();
  int tnum = tcatoix(tstr);
  if(tnum < 1) usage();
  int rv = procremove(name, tnum);
  return rv;
}


/* perform write command */
static int procwrite(const char *name, int tnum, int rnum){
  iprintf("<Writing Test>\n  seed=%u  name=%s  tnum=%d  rnum=%d\n\n",
          g_randseed, name, tnum, rnum);
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      eprint(adb, __LINE__, "tcadbsetskel");
      err = true;
      skel.del(skel.opq);
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, MULDIVNUM)){
      eprint(adb, __LINE__, "tcadbsetskelmulti");
      err = true;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    eprint(adb, __LINE__, "tcadbopen");
    err = true;
  }
  TARGWRITE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].adb = adb;
    targs[0].rnum = rnum;
    targs[0].id = 0;
    if(threadwrite(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].adb = adb;
      targs[i].rnum = rnum;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadwrite, targs + i) != 0){
        eprint(adb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(adb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  sysprint();
  if(!tcadbclose(adb)){
    eprint(adb, __LINE__, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform read command */
static int procread(const char *name, int tnum){
  iprintf("<Reading Test>\n  seed=%u  name=%s  tnum=%d\n\n", g_randseed, name, tnum);
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      eprint(adb, __LINE__, "tcadbsetskel");
      err = true;
      skel.del(skel.opq);
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, MULDIVNUM)){
      eprint(adb, __LINE__, "tcadbsetskelmulti");
      err = true;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    eprint(adb, __LINE__, "tcadbopen");
    err = true;
  }
  int rnum = tcadbrnum(adb) / tnum;
  TARGREAD targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].adb = adb;
    targs[0].rnum = rnum;
    targs[0].id = 0;
    if(threadread(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].adb = adb;
      targs[i].rnum = rnum;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadread, targs + i) != 0){
        eprint(adb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(adb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  sysprint();
  if(!tcadbclose(adb)){
    eprint(adb, __LINE__, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform remove command */
static int procremove(const char *name, int tnum){
  iprintf("<Removing Test>\n  seed=%u  name=%s  tnum=%d\n\n", g_randseed, name, tnum);
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      eprint(adb, __LINE__, "tcadbsetskel");
      err = true;
      skel.del(skel.opq);
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, MULDIVNUM)){
      eprint(adb, __LINE__, "tcadbsetskelmulti");
      err = true;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    eprint(adb, __LINE__, "tcadbopen");
    err = true;
  }
  int rnum = tcadbrnum(adb) / tnum;
  TARGREMOVE targs[tnum];
  pthread_t threads[tnum];
  if(tnum == 1){
    targs[0].adb = adb;
    targs[0].rnum = rnum;
    targs[0].id = 0;
    if(threadremove(targs) != NULL) err = true;
  } else {
    for(int i = 0; i < tnum; i++){
      targs[i].adb = adb;
      targs[i].rnum = rnum;
      targs[i].id = i;
      if(pthread_create(threads + i, NULL, threadremove, targs + i) != 0){
        eprint(adb, __LINE__, "pthread_create");
        targs[i].id = -1;
        err = true;
      }
    }
    for(int i = 0; i < tnum; i++){
      if(targs[i].id == -1) continue;
      void *rv;
      if(pthread_join(threads[i], &rv) != 0){
        eprint(adb, __LINE__, "pthread_join");
        err = true;
      } else if(rv){
        err = true;
      }
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  sysprint();
  if(!tcadbclose(adb)){
    eprint(adb, __LINE__, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* thread the write function */
static void *threadwrite(void *targ){
  TCADB *adb = ((TARGWRITE *)targ)->adb;
  int rnum = ((TARGWRITE *)targ)->rnum;
  int id = ((TARGWRITE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", base + i + 1);
    if(!tcadbput(adb, buf, len, buf, len)){
      eprint(adb, __LINE__, "tcadbput");
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
  TCADB *adb = ((TARGREAD *)targ)->adb;
  int rnum = ((TARGREAD *)targ)->rnum;
  int id = ((TARGREAD *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", base + i + 1);
    int vsiz;
    char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(adb, __LINE__, "tcadbget");
      err = true;
    }
    tcfree(vbuf);
    if(id == 0 && rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  return err ? "error" : NULL;
}


/* thread the remove function */
static void *threadremove(void *targ){
  TCADB *adb = ((TARGREMOVE *)targ)->adb;
  int rnum = ((TARGREMOVE *)targ)->rnum;
  int id = ((TARGREMOVE *)targ)->id;
  bool err = false;
  int base = id * rnum;
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", base + i + 1);
    if(!tcadbout(adb, kbuf, ksiz)){
      eprint(adb, __LINE__, "tcadbout");
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



// END OF FILE
