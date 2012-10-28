/*************************************************************************************************
 * The test cases of the utility API
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


/* global variables */
const char *g_progname;                  // program name
unsigned int g_randseed;                 // random seed


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void iputchar(int c);
static void sysprint(void);
static int myrand(int range);
static void *pdprocfunc(const void *vbuf, int vsiz, int *sp, void *op);
static bool iterfunc(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op);
static int intcompar(const void *ap, const void *bp);
static int runxstr(int argc, char **argv);
static int runlist(int argc, char **argv);
static int runmap(int argc, char **argv);
static int runtree(int argc, char **argv);
static int runmdb(int argc, char **argv);
static int runndb(int argc, char **argv);
static int runmisc(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int procxstr(int rnum);
static int proclist(int rnum, int anum, bool rd);
static int procmap(int rnum, int bnum, bool rd, bool tr, bool rnd, int dmode);
static int proctree(int rnum, bool rd, bool tr, bool rnd, int dmode);
static int procmdb(int rnum, int bnum, bool rd, bool tr, bool rnd, int dmode);
static int procndb(int rnum, bool rd, bool tr, bool rnd, int dmode);
static int procmisc(int rnum);
static int procwicked(int rnum);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  const char *ebuf = getenv("TCRNDSEED");
  g_randseed = ebuf ? tcatoix(ebuf) : tctime() * 1000;
  srand(g_randseed);
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "xstr")){
    rv = runxstr(argc, argv);
  } else if(!strcmp(argv[1], "list")){
    rv = runlist(argc, argv);
  } else if(!strcmp(argv[1], "map")){
    rv = runmap(argc, argv);
  } else if(!strcmp(argv[1], "tree")){
    rv = runtree(argc, argv);
  } else if(!strcmp(argv[1], "mdb")){
    rv = runmdb(argc, argv);
  } else if(!strcmp(argv[1], "ndb")){
    rv = runndb(argc, argv);
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
  fprintf(stderr, "%s: test cases of the utility API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s xstr rnum\n", g_progname);
  fprintf(stderr, "  %s list [-rd] rnum [anum]\n", g_progname);
  fprintf(stderr, "  %s map [-rd] [-tr] [-rnd] [-dk|-dc|-dai|-dad|-dpr] rnum [bnum]\n",
          g_progname);
  fprintf(stderr, "  %s tree [-rd] [-tr] [-rnd] [-dk|-dc|-dai|-dad|-dpr] rnum\n",
          g_progname);
  fprintf(stderr, "  %s mdb [-rd] [-tr] [-rnd] [-dk|-dc|-dai|-dad|-dpr] rnum [bnum]\n",
          g_progname);
  fprintf(stderr, "  %s ndb [-rd] [-tr] [-rnd] [-dk|-dc|-dai|-dad|-dpr] rnum\n",
          g_progname);
  fprintf(stderr, "  %s misc rnum\n", g_progname);
  fprintf(stderr, "  %s wicked rnum\n", g_progname);
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


/* compare two integers */
static int intcompar(const void *ap, const void *bp){
  return *(int *)ap - *(int *)bp;
}


/* parse arguments of xstr command */
static int runxstr(int argc, char **argv){
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = procxstr(rnum);
  return rv;
}


/* parse arguments of list command */
static int runlist(int argc, char **argv){
  char *rstr = NULL;
  char *astr = NULL;
  bool rd = false;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-rd")){
        rd = true;
      } else {
        usage();
      }
    } else if(!rstr){
      rstr = argv[i];
    } else if(!astr){
      astr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int anum = astr ? tcatoix(astr) : -1;
  int rv = proclist(rnum, anum, rd);
  return rv;
}


/* parse arguments of map command */
static int runmap(int argc, char **argv){
  char *rstr = NULL;
  char *bstr = NULL;
  bool rd = false;
  bool tr = false;
  bool rnd = false;
  int dmode = 0;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-rd")){
        rd = true;
      } else if(!strcmp(argv[i], "-tr")){
        tr = true;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else if(!strcmp(argv[i], "-dk")){
        dmode = -1;
      } else if(!strcmp(argv[i], "-dc")){
        dmode = 1;
      } else if(!strcmp(argv[i], "-dai")){
        dmode = 10;
      } else if(!strcmp(argv[i], "-dad")){
        dmode = 11;
      } else if(!strcmp(argv[i], "-dpr")){
        dmode = 12;
      } else {
        usage();
      }
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int rv = procmap(rnum, bnum, rd, tr, rnd, dmode);
  return rv;
}


/* parse arguments of tree command */
static int runtree(int argc, char **argv){
  char *rstr = NULL;
  bool rd = false;
  bool tr = false;
  bool rnd = false;
  int dmode = 0;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-rd")){
        rd = true;
      } else if(!strcmp(argv[i], "-tr")){
        tr = true;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else if(!strcmp(argv[i], "-dk")){
        dmode = -1;
      } else if(!strcmp(argv[i], "-dc")){
        dmode = 1;
      } else if(!strcmp(argv[i], "-dai")){
        dmode = 10;
      } else if(!strcmp(argv[i], "-dad")){
        dmode = 11;
      } else if(!strcmp(argv[i], "-dpr")){
        dmode = 12;
      } else {
        usage();
      }
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = proctree(rnum, rd, tr, rnd, dmode);
  return rv;
}


/* parse arguments of mdb command */
static int runmdb(int argc, char **argv){
  char *rstr = NULL;
  char *bstr = NULL;
  bool rd = false;
  bool tr = false;
  bool rnd = false;
  int dmode = 0;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-rd")){
        rd = true;
      } else if(!strcmp(argv[i], "-tr")){
        tr = true;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else if(!strcmp(argv[i], "-dk")){
        dmode = -1;
      } else if(!strcmp(argv[i], "-dc")){
        dmode = 1;
      } else if(!strcmp(argv[i], "-dai")){
        dmode = 10;
      } else if(!strcmp(argv[i], "-dad")){
        dmode = 11;
      } else if(!strcmp(argv[i], "-dpr")){
        dmode = 12;
      } else {
        usage();
      }
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int bnum = bstr ? tcatoix(bstr) : -1;
  int rv = procmdb(rnum, bnum, rd, tr, rnd, dmode);
  return rv;
}


/* parse arguments of ndb command */
static int runndb(int argc, char **argv){
  char *rstr = NULL;
  bool rd = false;
  bool tr = false;
  bool rnd = false;
  int dmode = 0;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-rd")){
        rd = true;
      } else if(!strcmp(argv[i], "-tr")){
        tr = true;
      } else if(!strcmp(argv[i], "-rnd")){
        rnd = true;
      } else if(!strcmp(argv[i], "-dk")){
        dmode = -1;
      } else if(!strcmp(argv[i], "-dc")){
        dmode = 1;
      } else if(!strcmp(argv[i], "-dai")){
        dmode = 10;
      } else if(!strcmp(argv[i], "-dad")){
        dmode = 11;
      } else if(!strcmp(argv[i], "-dpr")){
        dmode = 12;
      } else {
        usage();
      }
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = procndb(rnum, rd, tr, rnd, dmode);
  return rv;
}


/* parse arguments of misc command */
static int runmisc(int argc, char **argv){
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = procmisc(rnum);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoix(rstr);
  if(rnum < 1) usage();
  int rv = procwicked(rnum);
  return rv;
}


/* perform xstr command */
static int procxstr(int rnum){
  iprintf("<Extensible String Writing Test>\n  seed=%u  rnum=%d\n\n", g_randseed, rnum);
  double stime = tctime();
  TCXSTR *xstr = tcxstrnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    tcxstrcat(xstr, buf, len);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("size: %u\n", tcxstrsize(xstr));
  sysprint();
  tcxstrdel(xstr);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform list command */
static int proclist(int rnum, int anum, bool rd){
  iprintf("<List Writing Test>\n  seed=%u  rnum=%d  anum=%d  rd=%d\n\n",
          g_randseed, rnum, anum, rd);
  double stime = tctime();
  TCLIST *list = (anum > 0) ? tclistnew2(anum) : tclistnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    tclistpush(list, buf, len);
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rd){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    for(int i = 1; i <= rnum; i++){
      int len;
      tclistval(list, i, &len);
      if(rnum > 250 && i % (rnum / 250) == 0){
        iputchar('.');
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
  }
  iprintf("record number: %u\n", tclistnum(list));
  sysprint();
  tclistdel(list);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform map command */
static int procmap(int rnum, int bnum, bool rd, bool tr, bool rnd, int dmode){
  iprintf("<Map Writing Test>\n  seed=%u  rnum=%d  bnum=%d  rd=%d  tr=%d  rnd=%d  dmode=%d\n\n",
          g_randseed, rnum, bnum, rd, tr, rnd, dmode);
  double stime = tctime();
  TCMAP *map = (bnum > 0) ? tcmapnew2(bnum) : tcmapnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
    switch(dmode){
      case -1:
        tcmapputkeep(map, buf, len, buf, len);
        break;
      case 1:
        tcmapputcat(map, buf, len, buf, len);
        break;
      case 10:
        tcmapaddint(map, buf, len, myrand(3));
        break;
      case 11:
        tcmapadddouble(map, buf, len, myrand(3));
        break;
      case 12:
        tcmapputproc(map, buf, len, buf, len, pdprocfunc, NULL);
        break;
      default:
        tcmapput(map, buf, len, buf, len);
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rd){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
      tcmapget(map, buf, len, &len);
      if(rnum > 250 && i % (rnum / 250) == 0){
        iputchar('.');
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
  }
  if(tr){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    tcmapiterinit(map);
    int ksiz;
    const char *kbuf;
    int inum = 1;
    while((kbuf = tcmapiternext(map, &ksiz)) != NULL){
      tcmapiterval2(kbuf);
      if(rnum > 250 && inum % (rnum / 250) == 0){
        iputchar('.');
        if(inum == rnum || inum % (rnum / 10) == 0) iprintf(" (%08d)\n", inum);
      }
      inum++;
    }
    if(rnd && rnum > 250) iprintf(" (%08d)\n", inum);
  }
  iprintf("record number: %llu\n", (unsigned long long)tcmaprnum(map));
  iprintf("size: %llu\n", (unsigned long long)tcmapmsiz(map));
  sysprint();
  tcmapdel(map);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform tree command */
static int proctree(int rnum, bool rd, bool tr, bool rnd, int dmode){
  iprintf("<Tree Writing Test>\n  seed=%u  rnum=%d  rd=%d  tr=%d  rnd=%d  dmode=%d\n\n",
          g_randseed, rnum, rd, tr, rnd, dmode);
  double stime = tctime();
  TCTREE *tree = tctreenew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
    switch(dmode){
      case -1:
        tctreeputkeep(tree, buf, len, buf, len);
        break;
      case 1:
        tctreeputcat(tree, buf, len, buf, len);
        break;
      case 10:
        tctreeaddint(tree, buf, len, myrand(3));
        break;
      case 11:
        tctreeadddouble(tree, buf, len, myrand(3));
        break;
      case 12:
        tctreeputproc(tree, buf, len, buf, len, pdprocfunc, NULL);
        break;
      default:
        tctreeput(tree, buf, len, buf, len);
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rd){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
      tctreeget(tree, buf, len, &len);
      if(rnum > 250 && i % (rnum / 250) == 0){
        iputchar('.');
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
  }
  if(tr){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    tctreeiterinit(tree);
    int ksiz;
    const char *kbuf;
    int inum = 1;
    while((kbuf = tctreeiternext(tree, &ksiz)) != NULL){
      tctreeiterval2(kbuf);
      if(rnum > 250 && inum % (rnum / 250) == 0){
        iputchar('.');
        if(inum == rnum || inum % (rnum / 10) == 0) iprintf(" (%08d)\n", inum);
      }
      inum++;
    }
    if(rnd && rnum > 250) iprintf(" (%08d)\n", inum);
  }
  iprintf("record number: %llu\n", (unsigned long long)tctreernum(tree));
  iprintf("size: %llu\n", (unsigned long long)tctreemsiz(tree));
  sysprint();
  tctreedel(tree);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform mdb command */
static int procmdb(int rnum, int bnum, bool rd, bool tr, bool rnd, int dmode){
  iprintf("<On-memory Hash Database Writing Test>\n  seed=%u  rnum=%d  bnum=%d  rd=%d  tr=%d"
          "  rnd=%d  dmode=%d\n\n", g_randseed, rnum, bnum, rd, tr, rnd, dmode);
  double stime = tctime();
  TCMDB *mdb = (bnum > 0) ? tcmdbnew2(bnum) : tcmdbnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
    switch(dmode){
      case -1:
        tcmdbputkeep(mdb, buf, len, buf, len);
        break;
      case 1:
        tcmdbputcat(mdb, buf, len, buf, len);
        break;
      case 10:
        tcmdbaddint(mdb, buf, len, myrand(3));
        break;
      case 11:
        tcmdbadddouble(mdb, buf, len, myrand(3));
        break;
      case 12:
        tcmdbputproc(mdb, buf, len, buf, len, pdprocfunc, NULL);
        break;
      default:
        tcmdbput(mdb, buf, len, buf, len);
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rd){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
      tcfree(tcmdbget(mdb, buf, len, &len));
      if(rnum > 250 && i % (rnum / 250) == 0){
        iputchar('.');
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
  }
  if(tr){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    tcmdbiterinit(mdb);
    int ksiz;
    char *kbuf;
    int inum = 1;
    while((kbuf = tcmdbiternext(mdb, &ksiz)) != NULL){
      tcfree(kbuf);
      if(rnum > 250 && inum % (rnum / 250) == 0){
        iputchar('.');
        if(inum == rnum || inum % (rnum / 10) == 0) iprintf(" (%08d)\n", inum);
      }
      inum++;
    }
    if(rnd && rnum > 250) iprintf(" (%08d)\n", inum);
  }
  iprintf("record number: %llu\n", (unsigned long long)tcmdbrnum(mdb));
  iprintf("size: %llu\n", (unsigned long long)tcmdbmsiz(mdb));
  sysprint();
  tcmdbdel(mdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform ndb command */
static int procndb(int rnum, bool rd, bool tr, bool rnd, int dmode){
  iprintf("<On-memory Tree Database Writing Test>\n  seed=%u  rnum=%d  rd=%d  tr=%d"
          "  rnd=%d  dmode=%d\n\n", g_randseed, rnum, rd, tr, rnd, dmode);
  double stime = tctime();
  TCNDB *ndb = tcndbnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
    switch(dmode){
      case -1:
        tcndbputkeep(ndb, buf, len, buf, len);
        break;
      case 1:
        tcndbputcat(ndb, buf, len, buf, len);
        break;
      case 10:
        tcndbaddint(ndb, buf, len, myrand(3));
        break;
      case 11:
        tcndbadddouble(ndb, buf, len, myrand(3));
        break;
      case 12:
        tcndbputproc(ndb, buf, len, buf, len, pdprocfunc, NULL);
        break;
      default:
        tcndbput(ndb, buf, len, buf, len);
        break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rd){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      int len = sprintf(buf, "%08d", rnd ? myrand(rnum) + 1 : i);
      tcfree(tcndbget(ndb, buf, len, &len));
      if(rnum > 250 && i % (rnum / 250) == 0){
        iputchar('.');
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
  }
  if(tr){
    double itime = tctime();
    iprintf("time: %.3f\n", itime - stime);
    stime = itime;
    tcndbiterinit(ndb);
    int ksiz;
    char *kbuf;
    int inum = 1;
    while((kbuf = tcndbiternext(ndb, &ksiz)) != NULL){
      tcfree(kbuf);
      if(rnum > 250 && inum % (rnum / 250) == 0){
        iputchar('.');
        if(inum == rnum || inum % (rnum / 10) == 0) iprintf(" (%08d)\n", inum);
      }
      inum++;
    }
    if(rnd && rnum > 250) iprintf(" (%08d)\n", inum);
  }
  iprintf("record number: %llu\n", (unsigned long long)tcndbrnum(ndb));
  iprintf("size: %llu\n", (unsigned long long)tcndbmsiz(ndb));
  sysprint();
  tcndbdel(ndb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform misc command */
static int procmisc(int rnum){
  iprintf("<Miscellaneous Test>\n  seed=%u  rnum=%d\n\n", g_randseed, rnum);
  double stime = tctime();
  bool err = false;
  for(int i = 1; i <= rnum && !err; i++){
    const char *str = "5%2+3-1=4 \"Yes/No\" <a&b>";
    int slen = strlen(str);
    char *buf, *dec;
    int bsiz, dsiz, jl;
    time_t date, ddate;
    TCXSTR *xstr;
    TCLIST *list;
    TCMAP *map;
    TCTREE *tree;
    TCPTRLIST *ptrlist;
    buf = tcmemdup(str, slen);
    xstr = tcxstrfrommalloc(buf, slen);
    buf = tcxstrtomalloc(xstr);
    if(strcmp(buf, str)) err = true;
    tcfree(buf);
    if(tclmax(1, 2) != 2) err = true;
    if(tclmin(1, 2) != 1) err = true;
    tclrand();
    if(tcdrand() >= 1.0) err = true;
    tcdrandnd(50, 10);
    if(tcstricmp("abc", "ABC") != 0) err = true;
    if(!tcstrfwm("abc", "ab") || !tcstrifwm("abc", "AB")) err = true;
    if(!tcstrbwm("abc", "bc") || !tcstribwm("abc", "BC")) err = true;
    if(tcstrdist("abcde", "abdfgh") != 4 || tcstrdist("abdfgh", "abcde") != 4) err = true;
    if(tcstrdistutf("abcde", "abdfgh") != 4 || tcstrdistutf("abdfgh", "abcde") != 4) err = true;
    buf = tcmemdup("abcde", 5);
    tcstrtoupper(buf);
    if(strcmp(buf, "ABCDE")) err = true;
    tcstrtolower(buf);
    if(strcmp(buf, "abcde")) err = true;
    tcfree(buf);
    buf = tcmemdup("  ab  cd  ", 10);
    tcstrtrim(buf);
    if(strcmp(buf, "ab  cd")) err = true;
    tcstrsqzspc(buf);
    if(strcmp(buf, "ab cd")) err = true;
    tcstrsubchr(buf, "cd", "C");
    if(strcmp(buf, "ab C")) err = true;
    if(tcstrcntutf(buf) != 4) err = true;
    tcstrcututf(buf, 2);
    if(strcmp(buf, "ab")) err = true;
    tcfree(buf);
    if(i % 10 == 1){
      int anum = myrand(30);
      uint16_t ary[anum+1];
      for(int j = 0; j < anum; j++){
        ary[j] = myrand(65535) + 1;
      }
      char ustr[anum*3+1];
      tcstrucstoutf(ary, anum, ustr);
      uint16_t dary[anum+1];
      int danum;
      tcstrutftoucs(ustr, dary, &danum);
      if(danum != anum){
        err = true;
      } else {
        for(int j = 0; j < danum; j++){
          if(dary[j] != dary[j]) err = true;
        }
      }
      tcstrutfnorm(ustr, TCUNSPACE | TCUNLOWER | TCUNNOACC | TCUNWIDTH);
      if(tcstrucsnorm(dary, danum, TCUNSPACE | TCUNLOWER | TCUNNOACC | TCUNWIDTH) > danum)
        err = true;
      list = tcstrtokenize("a ab abc b bc bcd abcde \"I'm nancy\" x \"xx");
      if(tclistnum(list) != 10) err = true;
      int opts = myrand(TCKWMUBRCT + 1);
      if(myrand(2) == 0) opts |= TCKWNOOVER;
      if(myrand(2) == 0) opts |= TCKWPULEAD;
      TCLIST *texts = tcstrkwic(ustr, list, myrand(10), opts);
      tclistdel(texts);
      tclistdel(list);
      list = tclistnew3("hop", "step", "jump", "touchdown", NULL);
      if(tclistnum(list) != 4) err = true;
      tclistprintf(list, "%s:%010d:%7.3f", "game set", 123456789, 12345.6789);
      tclistdel(list);
      map = tcmapnew3("hop", "step", "jump", "touchdown", NULL);
      if(tcmaprnum(map) != 2) err = true;
      tcmapprintf(map, "joker", "%s:%010d:%7.3f", "game set", 123456789, 12345.6789);
      tcmapdel(map);
      list = tcstrsplit(",a,b..c,d,", ",.");
      if(tclistnum(list) != 7) err = true;
      buf = tcstrjoin(list, ':');
      if(strcmp(buf, ":a:b::c:d:")) err = true;
      tcfree(buf);
      tclistdel(list);
      char zbuf[RECBUFSIZ];
      memcpy(zbuf, "abc\0def\0ghij\0kl\0m", 17);
      list = tcstrsplit2(zbuf, 17);
      if(tclistnum(list) != 5) err = true;
      buf = tcstrjoin2(list, &bsiz);
      if(bsiz != 17 || memcmp(buf, "abc\0def\0ghij\0kl\0m", 17)) err = true;
      tcfree(buf);
      tclistdel(list);
      map = tcstrsplit3("abc.def,ghij.kl,", ",.");
      if(tcmaprnum(map) != 2) err = true;
      buf = tcstrjoin3(map, ':');
      if(strcmp(buf, "abc:def:ghij:kl")) err = true;
      tcfree(buf);
      tcmapdel(map);
      memcpy(zbuf, "abc\0def\0ghij\0kl\0m", 17);
      map = tcstrsplit4(zbuf, 17);
      if(tcmaprnum(map) != 2) err = true;
      buf = tcstrjoin4(map, &bsiz);
      if(bsiz != 15 || memcmp(buf, "abc\0def\0ghij\0kl", 15)) err = true;
      tcfree(buf);
      tcmapdel(map);
      if(!tcregexmatch("ABCDEFGHI", "*(b)c[d-f]*g(h)")) err = true;
      buf = tcregexreplace("ABCDEFGHI", "*(b)c[d-f]*g(h)", "[\\1][\\2][&]");
      if(strcmp(buf, "A[B][H][BCDEFGH]I")) err = true;
      tcfree(buf);
      buf = tcmalloc(48);
      for(int i = 0; i < 10; i++){
        char kbuf[RECBUFSIZ];
        int ksiz = sprintf(kbuf, "%d", myrand(1000000));
        tcmd5hash(kbuf, ksiz, buf);
      }
      tcfree(buf);
      anum = myrand(30) + 1;
      int tary[anum], qary[anum];
      for(int j = 0; j < anum; j++){
        int val = myrand(anum * 2 + 1);
        tary[j] = val;
        qary[j] = val;
      }
      int tnum = myrand(anum);
      tctopsort(tary, anum, sizeof(*tary), tnum, intcompar);
      qsort(qary, anum, sizeof(*qary), intcompar);
      for(int j = 0; j < tnum; j++){
        if(tary[j] != qary[j]) err = true;
      }
      qsort(tary, anum, sizeof(*tary), intcompar);
      for(int j = 0; j < anum; j++){
        if(tary[j] != qary[j]) err = true;
      }
      TCCHIDX *chidx = tcchidxnew(5);
      for(int i = 0; i < 10; i++){
        char kbuf[RECBUFSIZ];
        int ksiz = sprintf(kbuf, "%d", myrand(1000000));
        tcchidxhash(chidx, kbuf, ksiz);
      }
      tcchidxdel(chidx);
      char kbuf[TCNUMBUFSIZ];
      int ksiz = sprintf(kbuf, "%d", myrand(200));
      char *enc = tcmalloc(slen + 1);
      tcarccipher(str, slen, kbuf, ksiz, enc);
      tcarccipher(enc, slen, kbuf, ksiz, enc);
      if(memcmp(enc, str, slen)) err = true;
      tcfree(enc);
      buf = tczeromap(myrand(1024*256) + 1);
      tczerounmap(buf);
    }
    buf = tcmalloc(48);
    date = myrand(INT_MAX - 1000000);
    jl = 3600 * (myrand(23) - 11);
    tcdatestrwww(date, jl, buf);
    ddate = tcstrmktime(buf);
    if(ddate != date) err = true;
    tcdatestrhttp(date, jl, buf);
    ddate = tcstrmktime(buf);
    if(ddate != date) err = true;
    tcfree(buf);
    if(i % 100 == 1){
      map = myrand(2) == 0 ? tcmapnew() : tcmapnew2(myrand(10));
      tree = tctreenew();
      for(int j = 0; j < 10; j++){
        char kbuf[RECBUFSIZ];
        int ksiz = sprintf(kbuf, "%d", myrand(10));
        tcmapaddint(map, kbuf, ksiz, 1);
        const char *vbuf = tcmapget2(map, kbuf);
        if(*(int *)vbuf < 1) err = true;
        tctreeaddint(tree, kbuf, ksiz, 1);
        vbuf = tctreeget2(tree, kbuf);
        if(*(int *)vbuf < 1) err = true;
      }
      tcmapclear(map);
      tctreeclear(tree);
      for(int j = 0; j < 10; j++){
        char kbuf[RECBUFSIZ];
        int ksiz = sprintf(kbuf, "%d", myrand(10));
        tcmapadddouble(map, kbuf, ksiz, 1.0);
        const char *vbuf = tcmapget2(map, kbuf);
        if(*(double *)vbuf < 1.0) err = true;
        tctreeadddouble(tree, kbuf, ksiz, 1.0);
        vbuf = tctreeget2(tree, kbuf);
        if(*(double *)vbuf < 1.0) err = true;
      }
      for(int j = 0; j < 10; j++){
        char kbuf[RECBUFSIZ];
        sprintf(kbuf, "%d", myrand(10));
        tcmapprintf(map, kbuf, "%s:%f", kbuf, (double)i * j);
        tctreeprintf(tree, kbuf, "%s:%f", kbuf, (double)i * j);
      }
      if(!tcmapget4(map, "ace", "dummy")) err = true;
      if(!tctreeget4(tree, "ace", "dummy")) err = true;
      tctreedel(tree);
      tcmapdel(map);
    }
    if(i % 100 == 1){
      ptrlist = myrand(2) == 0 ? tcptrlistnew() : tcptrlistnew2(myrand(10));
      for(int j = 0; j < 10; j++){
        tcptrlistpush(ptrlist, tcsprintf("%d", j));
        tcptrlistunshift(ptrlist, tcsprintf("::%d", j));
      }
      for(int j = 0; j < 5; j++){
        tcfree(tcptrlistpop(ptrlist));
        tcfree(tcptrlistshift(ptrlist));
      }
      for(int j = 0; j < tcptrlistnum(ptrlist); j++){
        tcfree(tcptrlistval(ptrlist, j));
      }
      tcptrlistdel(ptrlist);
    }
    buf = tcurlencode(str, slen);
    if(strcmp(buf, "5%252%2B3-1%3D4%20%22Yes%2FNo%22%20%3Ca%26b%3E")) err = true;
    dec = tcurldecode(buf, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    tcfree(dec);
    tcfree(buf);
    if(i % 10 == 1){
      map = tcurlbreak("http://mikio:oikim@estraier.net:1978/foo/bar/baz.cgi?ab=cd&ef=jkl#quux");
      const char *elem;
      if(!(elem = tcmapget2(map, "self")) ||
         strcmp(elem, "http://mikio:oikim@estraier.net:1978/foo/bar/baz.cgi?ab=cd&ef=jkl#quux"))
        err = true;
      if(!(elem = tcmapget2(map, "scheme")) || strcmp(elem, "http")) err = true;
      if(!(elem = tcmapget2(map, "host")) || strcmp(elem, "estraier.net")) err = true;
      if(!(elem = tcmapget2(map, "port")) || strcmp(elem, "1978")) err = true;
      if(!(elem = tcmapget2(map, "authority")) || strcmp(elem, "mikio:oikim")) err = true;
      if(!(elem = tcmapget2(map, "path")) || strcmp(elem, "/foo/bar/baz.cgi")) err = true;
      if(!(elem = tcmapget2(map, "file")) || strcmp(elem, "baz.cgi")) err = true;
      if(!(elem = tcmapget2(map, "query")) || strcmp(elem, "ab=cd&ef=jkl")) err = true;
      if(!(elem = tcmapget2(map, "fragment")) || strcmp(elem, "quux")) err = true;
      tcmapdel(map);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "http://A:B@C.D:1/E/F/G.H?I=J#K");
      if(strcmp(buf, "http://A:B@c.d:1/E/F/G.H?I=J#K")) err = true;
      tcfree(buf);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "/E/F/G.H?I=J#K");
      if(strcmp(buf, "http://a:b@c.d:1/E/F/G.H?I=J#K")) err = true;
      tcfree(buf);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "G.H?I=J#K");
      if(strcmp(buf, "http://a:b@c.d:1/e/f/G.H?I=J#K")) err = true;
      tcfree(buf);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "?I=J#K");
      if(strcmp(buf, "http://a:b@c.d:1/e/f/g.h?I=J#K")) err = true;
      tcfree(buf);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "#K");
      if(strcmp(buf, "http://a:b@c.d:1/e/f/g.h?i=j#K")) err = true;
      tcfree(buf);
    }
    buf = tcbaseencode(str, slen);
    if(strcmp(buf, "NSUyKzMtMT00ICJZZXMvTm8iIDxhJmI+")) err = true;
    dec = tcbasedecode(buf, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    tcfree(dec);
    tcfree(buf);
    buf = tcquoteencode(str, slen);
    if(strcmp(buf, "5%2+3-1=3D4 \"Yes/No\" <a&b>")) err = true;
    dec = tcquotedecode(buf, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    tcfree(dec);
    tcfree(buf);
    buf = tcmimeencode(str, "UTF-8", true);
    if(strcmp(buf, "=?UTF-8?B?NSUyKzMtMT00ICJZZXMvTm8iIDxhJmI+?=")) err = true;
    char encname[32];
    dec = tcmimedecode(buf, encname);
    if(strcmp(dec, str) || strcmp(encname, "UTF-8")) err = true;
    tcfree(dec);
    tcfree(buf);
    if(i % 10 == 1){
      const char *mstr = "Subject: Hello\r\nContent-Type: multipart/mixed; boundary=____\r\n\r\n"
        "\r\n--____\r\nThis is a pen.\r\n--____\r\nIs this your bag?\r\n--____--\r\n";
      map = tcmapnew2(10);
      char *buf = tcmimebreak(mstr, strlen(mstr), map, &bsiz);
      const char *boundary = tcmapget2(map, "BOUNDARY");
      if(boundary){
        list = tcmimeparts(buf, bsiz, boundary);
        if(tclistnum(list) == 2){
          if(strcmp(tclistval2(list, 0), "This is a pen.")) err = true;
          if(strcmp(tclistval2(list, 1), "Is this your bag?")) err = true;
        } else {
          err = true;
        }
        tclistdel(list);
      } else {
        err = true;
      }
      tcfree(buf);
      tcmapdel(map);
    }
    buf = tcpackencode(str, slen, &bsiz);
    dec = tcpackdecode(buf, bsiz, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    tcfree(dec);
    tcfree(buf);
    buf = tcbsencode(str, slen, &bsiz);
    dec = tcbsdecode(buf, bsiz, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    tcfree(dec);
    tcfree(buf);
    int idx;
    buf = tcbwtencode(str, slen, &idx);
    if(memcmp(buf, "4\"o 5a23s-%+=> 1b/\"<&YNe", slen) || idx != 13) err = true;
    dec = tcbwtdecode(buf, slen, idx);
    if(memcmp(dec, str, slen)) err = true;
    tcfree(dec);
    tcfree(buf);
    if(_tc_deflate){
      if((buf = tcdeflate(str, slen, &bsiz)) != NULL){
        if((dec = tcinflate(buf, bsiz, &dsiz)) != NULL){
          if(slen != dsiz || memcmp(str, dec, dsiz)) err = true;
          tcfree(dec);
        } else {
          err = true;
        }
        tcfree(buf);
      } else {
        err = true;
      }
      if((buf = tcgzipencode(str, slen, &bsiz)) != NULL){
        if((dec = tcgzipdecode(buf, bsiz, &dsiz)) != NULL){
          if(slen != dsiz || memcmp(str, dec, dsiz)) err = true;
          tcfree(dec);
        } else {
          err = true;
        }
        tcfree(buf);
      } else {
        err = true;
      }
      if(tcgetcrc("hoge", 4) % 10000 != 7034) err = true;
    }
    if(_tc_bzcompress){
      if((buf = tcbzipencode(str, slen, &bsiz)) != NULL){
        if((dec = tcbzipdecode(buf, bsiz, &dsiz)) != NULL){
          if(slen != dsiz || memcmp(str, dec, dsiz)) err = true;
          tcfree(dec);
        } else {
          err = true;
        }
        tcfree(buf);
      } else {
        err = true;
      }
    }
    int anum = myrand(50)+1;
    unsigned int ary[anum];
    for(int j = 0; j < anum; j++){
      ary[j] = myrand(INT_MAX);
    }
    buf = tcberencode(ary, anum, &bsiz);
    int dnum;
    unsigned int *dary = tcberdecode(buf, bsiz, &dnum);
    if(anum != dnum || memcmp(ary, dary, sizeof(*dary) * dnum)) err = true;
    tcfree(dary);
    tcfree(buf);
    buf = tcxmlescape(str);
    if(strcmp(buf, "5%2+3-1=4 &quot;Yes/No&quot; &lt;a&amp;b&gt;")) err = true;
    dec = tcxmlunescape(buf);
    if(strcmp(dec, str)) err = true;
    tcfree(dec);
    tcfree(buf);
    buf = tccstrescape(str);
    if(strcmp(buf, "5%2+3-1=4 \\x22Yes/No\\x22 <a&b>")) err = true;
    dec = tccstrunescape(buf);
    if(strcmp(dec, str)) err = true;
    tcfree(dec);
    tcfree(buf);
    buf = tcjsonescape(str);
    if(strcmp(buf, "5%2+3-1=4 \\u0022Yes/No\\u0022 <a&b>")) err = true;
    dec = tcjsonunescape(buf);
    if(strcmp(dec, str)) err = true;
    tcfree(dec);
    tcfree(buf);
    if(i % 10 == 1){
      TCMAP *params = tcmapnew3("one", "=first=", "two", "&second&", "three", "#third#", NULL);
      char *estr = tcwwwformencode(params);
      TCMAP *nparams = tcmapnew2(1);
      tcwwwformdecode(estr, nparams);
      if(strcmp(estr, "one=%3Dfirst%3D&two=%26second%26&three=%23third%23") ||
         tcmaprnum(nparams) != tcmaprnum(params) || !tcmapget2(nparams, "two")) err = true;
      tcmapdel(nparams);
      tcfree(estr);
      tcmapdel(params);
      list = tcxmlbreak("<abc de=\"foo&amp;\" gh='&lt;bar&gt;'>xyz<br>\na<!--<mikio>--></abc>");
      for(int j = 0; j < tclistnum(list); j++){
        const char *elem = tclistval2(list, j);
        TCMAP *attrs = tcxmlattrs(elem);
        tcmapdel(attrs);
      }
      tclistdel(list);
    }
    if(i % 100 == 1){
      TCTMPL *tmpl = tctmplnew();
      const char *str =
        "{{ title XML }}{{UNKNOWN COMMAND}}{{CONF author 'Mikio Hirabayashi'}}\n"
        "{{ FOREACH docs \\}}\n"
        "{{ IF void }}===={{void}}{{ ELSE }}----{{void.void}}{{ END }}\n"
        "{{ IF .id }}ID:{{ .id }}{{ END }}\n"
        "{{ IF .title }}Title:{{ .title }}{{ END }}\n"
        "{{ IF author }}Author:{{ author }}{{ END }}\n"
        "{{ IF .coms }}{{ SET addr 'Setagaya, Tokyo' }}--\n"
        "{{ FOREACH .coms com }}{{ com.author MD5 }}: {{ com.body }}\n"
        "{{ END \\}}\n"
        "{{ END \\}}\n"
        "{{ END \\}}\n";
      tctmplsetsep(tmpl, "{{", "}}");
      tctmplload(tmpl, str);
      const char *cval = tctmplconf(tmpl, "author");
      if(!cval || strcmp(cval, "Mikio Hirabayashi")) err = true;
      TCMPOOL *mpool = tcmpoolnew();
      TCMAP *vars = tcmpoolmapnew(mpool);
      tcmapput2(vars, "title", "I LOVE YOU");
      TCLIST *docs = tcmpoollistnew(mpool);
      for(int j = 0; j < 3; j++){
        TCMAP *doc = tcmpoolmapnew(mpool);
        char vbuf[TCNUMBUFSIZ];
        sprintf(vbuf, "%d", i + j);
        tcmapput2(doc, "id", vbuf);
        sprintf(vbuf, "[%08d]", i + j);
        tcmapput2(doc, "title", vbuf);
        TCLIST *coms = tcmpoollistnew(mpool);
        for(int k = 0; k < 3; k++){
          TCMAP *com = tcmpoolmapnew(mpool);
          sprintf(vbuf, "u%d", k);
          tcmapput2(com, "author", vbuf);
          sprintf(vbuf, "this is the %dth pen.", (j + 1) * (k + 1) + i);
          tcmapput2(com, "body", vbuf);
          tclistpushmap(coms, com);
        }
        tcmapputlist(doc, "coms", coms);
        tclistpushmap(docs, doc);
      }
      tcmapputlist(vars, "docs", docs);
      char *res = tctmpldump(tmpl, vars);
      tcfree(res);
      tcmpoolclear(mpool, true);
      tcmpoolmalloc(mpool, 1);
      tcmpoollistnew(mpool);
      tcmpooldel(mpool);
      tctmpldel(tmpl);
    }
    if(i % 10 == 1){
      for(int16_t j = 1; j <= 0x2000; j *= 2){
        for(int16_t num = j - 1; num <= j + 1; num++){
          int16_t nnum = TCHTOIS(num);
          if(num != TCITOHS(nnum)) err = true;
        }
      }
      for(int32_t j = 1; j <= 0x20000000; j *= 2){
        for(int32_t num = j - 1; num <= j + 1; num++){
          int32_t nnum = TCHTOIL(num);
          if(num != TCITOHL(nnum)) err = true;
          char buf[TCNUMBUFSIZ];
          int step, nstep;
          TCSETVNUMBUF(step, buf, num);
          TCREADVNUMBUF(buf, nnum, nstep);
          if(num != nnum || step != nstep) err = true;
        }
      }
      for(int64_t j = 1; j <= 0x2000000000000000; j *= 2){
        for(int64_t num = j - 1; num <= j + 1; num++){
          int64_t nnum = TCHTOILL(num);
          if(num != TCITOHLL(nnum)) err = true;
          char buf[TCNUMBUFSIZ];
          int step, nstep;
          TCSETVNUMBUF64(step, buf, num);
          TCREADVNUMBUF64(buf, nnum, nstep);
          if(num != nnum || step != nstep) err = true;
        }
      }
      TCBITMAP *bitmap = TCBITMAPNEW(100);
      for(int j = 0; j < 100; j++){
        if(j % 3 == 0) TCBITMAPON(bitmap, j);
        if(j % 5 == 0) TCBITMAPOFF(bitmap, j);
      }
      for(int j = 0; j < 100; j++){
        if(j % 5 == 0){
          if(TCBITMAPCHECK(bitmap, j)) err = true;
        } else if(j % 3 == 0){
          if(!TCBITMAPCHECK(bitmap, j)) err = true;
        }
      }
      TCBITMAPDEL(bitmap);
      buf = tcmalloc(i / 8 + 2);
      TCBITSTRM strm;
      TCBITSTRMINITW(strm, buf);
      for(int j = 0; j < i; j++){
        int sign = j % 3 == 0 || j % 7 == 0;
        TCBITSTRMCAT(strm, sign);
      }
      TCBITSTRMSETEND(strm);
      int bnum = TCBITSTRMNUM(strm);
      if(bnum != i) err = true;
      TCBITSTRMINITR(strm, buf, bsiz);
      for(int j = 0; j < i; j++){
        int sign;
        TCBITSTRMREAD(strm, sign);
        if(sign != (j % 3 == 0 || j % 7 == 0)) err = true;
      }
      tcfree(buf);
    }
    if(i % 100 == 1){
      char path[RECBUFSIZ];
      sprintf(path, "%d", myrand(10));
      if(tcpathlock(path)){
        if(!tcpathunlock(path)) err = true;
      } else {
        err = true;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      iputchar('.');
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("time: %.3f\n", tctime() - stime);
  if(err){
    iprintf("error\n\n");
    return 1;
  }
  iprintf("ok\n\n");
  return 0;
}


/* perform wicked command */
static int procwicked(int rnum){
  iprintf("<Wicked Writing Test>\n  seed=%u  rnum=%d\n\n", g_randseed, rnum);
  double stime = tctime();
  TCMPOOL *mpool = tcmpoolglobal();
  TCXSTR *xstr = myrand(2) > 0 ? tcxstrnew() : tcxstrnew2("hello world");
  tcmpoolpushxstr(mpool, xstr);
  TCLIST *list = myrand(2) > 0 ? tclistnew() : tclistnew2(myrand(rnum) + rnum / 2);
  tcmpoolpushlist(mpool, list);
  TCPTRLIST *ptrlist = myrand(2) > 0 ? tcptrlistnew() : tcptrlistnew2(myrand(rnum) + rnum / 2);
  tcmpoolpush(mpool, ptrlist, (void (*)(void*))tcptrlistdel);
  TCMAP *map = myrand(2) > 0 ? tcmapnew() : tcmapnew2(myrand(rnum) + rnum / 2);
  tcmpoolpushmap(mpool, map);
  TCTREE *tree = myrand(2) > 0 ? tctreenew() : tctreenew2(tccmpdecimal, NULL);
  tcmpoolpushtree(mpool, tree);
  TCMDB *mdb = myrand(2) > 0 ? tcmdbnew() : tcmdbnew2(myrand(rnum) + rnum / 2);
  tcmpoolpush(mpool, mdb, (void (*)(void*))tcmdbdel);
  TCNDB *ndb = myrand(2) > 0 ? tcndbnew() : tcndbnew2(tccmpdecimal, NULL);
  tcmpoolpush(mpool, ndb, (void (*)(void*))tcndbdel);
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(i));
    char vbuf[RECBUFSIZ];
    int vsiz = sprintf(vbuf, "%d", myrand(i));
    char *tmp;
    switch(myrand(70)){
      case 0:
        iputchar('0');
        tcxstrcat(xstr, kbuf, ksiz);
        break;
      case 1:
        iputchar('1');
        tcxstrcat2(xstr, kbuf);
        break;
      case 2:
        iputchar('2');
        if(myrand(rnum / 100 + 1) == 0) tcxstrclear(xstr);
        break;
      case 3:
        iputchar('3');
        tcxstrprintf(xstr, "[%s:%d:%llu:%b:%llb]\n",
                     kbuf, i, (long long)i * 65521, i, (unsigned long long)i * 65521);
        break;
      case 4:
        iputchar('4');
        tclistpush(list, kbuf, ksiz);
        tcptrlistpush(ptrlist, tcmemdup(kbuf, ksiz));
        break;
      case 5:
        iputchar('5');
        tclistpush2(list, kbuf);
        break;
      case 6:
        iputchar('6');
        tmp = tcmemdup(kbuf, ksiz);
        tclistpushmalloc(list, tmp, strlen(tmp));
        break;
      case 7:
        iputchar('7');
        if(myrand(10) == 0){
          tcfree(tclistpop(list, &ksiz));
          tcfree(tcptrlistpop(ptrlist));
        }
        break;
      case 8:
        iputchar('8');
        if(myrand(10) == 0) tcfree(tclistpop2(list));
        break;
      case 9:
        iputchar('9');
        tclistunshift(list, kbuf, ksiz);
        tcptrlistunshift(ptrlist, tcmemdup(kbuf, ksiz));
        break;
      case 10:
        iputchar('A');
        tclistunshift2(list, kbuf);
        break;
      case 11:
        iputchar('B');
        if(myrand(10) == 0){
          tcfree(tclistshift(list, &ksiz));
          tcfree(tcptrlistshift(ptrlist));
        }
        break;
      case 12:
        iputchar('C');
        if(myrand(10) == 0) tcfree(tclistshift2(list));
        break;
      case 13:
        iputchar('D');
        tclistinsert(list, i / 10, kbuf, ksiz);
        if(tcptrlistnum(ptrlist) > i / 10)
          tcptrlistinsert(ptrlist, i / 10, tcmemdup(kbuf, ksiz));
        break;
      case 14:
        iputchar('E');
        tclistinsert2(list, i / 10, kbuf);
        break;
      case 15:
        iputchar('F');
        if(myrand(10) == 0){
          tcfree(tclistremove(list, i / 10, &ksiz));
          tcfree(tcptrlistremove(ptrlist, i / 10));
        }
        break;
      case 16:
        iputchar('G');
        if(myrand(10) == 0) tcfree(tclistremove2(list, i / 10));
        break;
      case 17:
        iputchar('H');
        tclistover(list, i / 10, kbuf, ksiz);
        if(tcptrlistnum(ptrlist) > i / 10){
          tcfree(tcptrlistval(ptrlist, i / 10));
          tcptrlistover(ptrlist, i / 10, tcmemdup(kbuf, ksiz));
        }
        break;
      case 18:
        iputchar('I');
        tclistover2(list, i / 10, kbuf);
        break;
      case 19:
        iputchar('J');
        if(myrand(rnum / 1000 + 1) == 0) tclistsort(list);
        break;
      case 20:
        iputchar('K');
        if(myrand(rnum / 1000 + 1) == 0) tclistsortci(list);
        break;
      case 21:
        iputchar('L');
        if(myrand(rnum / 1000 + 1) == 0) tclistlsearch(list, kbuf, ksiz);
        break;
      case 22:
        iputchar('M');
        if(myrand(rnum / 1000 + 1) == 0) tclistbsearch(list, kbuf, ksiz);
        break;
      case 23:
        iputchar('N');
        if(myrand(rnum / 100 + 1) == 0){
          tclistclear(list);
          for(int j = 0; j < tcptrlistnum(ptrlist); j++){
            tcfree(tcptrlistval(ptrlist, j));
          }
          tcptrlistclear(ptrlist);
        }
        break;
      case 24:
        iputchar('O');
        if(myrand(rnum / 100 + 1) == 0){
          int dsiz;
          char *dbuf = tclistdump(list, &dsiz);
          tclistdel(tclistload(dbuf, dsiz));
          tcfree(dbuf);
        }
        break;
      case 25:
        iputchar('P');
        if(myrand(100) == 0){
          if(myrand(2) == 0){
            for(int j = 0; j < tclistnum(list); j++){
              int rsiz;
              tclistval(list, j, &rsiz);
              tcptrlistval(ptrlist, j);
            }
          } else {
            for(int j = 0; j < tclistnum(list); j++){
              tclistval2(list, j);
            }
          }
        }
        break;
      case 26:
        iputchar('Q');
        tcmapput(map, kbuf, ksiz, vbuf, vsiz);
        tctreeput(tree, kbuf, ksiz, vbuf, vsiz);
        break;
      case 27:
        iputchar('R');
        tcmapput2(map, kbuf, vbuf);
        tctreeput2(tree, kbuf, vbuf);
        break;
      case 28:
        iputchar('S');
        tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
        tctreeputkeep(tree, kbuf, ksiz, vbuf, vsiz);
        break;
      case 29:
        iputchar('T');
        tcmapputkeep2(map, kbuf, vbuf);
        tctreeputkeep2(tree, kbuf, vbuf);
        break;
      case 30:
        iputchar('U');
        tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
        tctreeputcat(tree, kbuf, ksiz, vbuf, vsiz);
        break;
      case 31:
        iputchar('V');
        tcmapputcat2(map, kbuf, vbuf);
        tctreeputcat2(tree, kbuf, vbuf);
        break;
      case 32:
        iputchar('W');
        if(myrand(2) == 0){
          tcmapput3(map, kbuf, ksiz, vbuf, vsiz);
          tctreeput3(tree, kbuf, ksiz, vbuf, vsiz);
        }
        if(myrand(2) == 0){
          tcmapput4(map, kbuf, ksiz, vbuf, vsiz, vbuf, vsiz);
          tctreeputkeep3(tree, kbuf, ksiz, vbuf, vsiz);
        }
        if(myrand(2) == 0){
          tcmapputcat3(map, kbuf, ksiz, vbuf, vsiz);
          tctreeputcat3(tree, kbuf, ksiz, vbuf, vsiz);
        }
        break;
      case 33:
        iputchar('X');
        if(myrand(10) == 0){
          tcmapout(map, kbuf, ksiz);
          tctreeout(tree, kbuf, ksiz);
        }
        break;
      case 34:
        iputchar('Y');
        if(myrand(10) == 0){
          tcmapout2(map, kbuf);
          tctreeout2(tree, kbuf);
        }
        break;
      case 35:
        iputchar('Z');
        tcmapget3(map, kbuf, ksiz, &vsiz);
        tctreeget3(tree, kbuf, ksiz, &vsiz);
        break;
      case 36:
        iputchar('a');
        tcmapmove(map, kbuf, ksiz, true);
        break;
      case 37:
        iputchar('b');
        tcmapmove(map, kbuf, ksiz, false);
        break;
      case 38:
        iputchar('c');
        tcmapmove2(map, kbuf, true);
        break;
      case 39:
        iputchar('d');
        if(myrand(100) == 0){
          if(myrand(2) == 0){
            tcmapiterinit(map);
            tctreeiterinit(tree);
          } else {
            tcmapiterinit2(map, kbuf, ksiz);
            tctreeiterinit2(tree, kbuf, ksiz);
          }
        }
        break;
      case 40:
        iputchar('e');
        tcmapiternext(map, &vsiz);
        tctreeiternext(tree, &vsiz);
        break;
      case 41:
        iputchar('f');
        tcmapiternext2(map);
        tctreeiternext2(tree);
        break;
      case 42:
        iputchar('g');
        if(myrand(100) == 0){
          int anum;
          switch(myrand(4)){
            case 0:
              tclistdel(tcmapkeys(map));
              tclistdel(tctreekeys(tree));
              break;
            case 1:
              tcfree(tcmapkeys2(map, &anum));
              tcfree(tctreekeys2(tree, &anum));
              break;
            case 2:
              tclistdel(tcmapvals(map));
              tclistdel(tctreevals(tree));
              break;
            default:
              tcfree(tcmapvals2(map, &anum));
              tcfree(tctreevals2(tree, &anum));
              break;
          }
        }
        break;
      case 43:
        iputchar('h');
        if(myrand(rnum / 100 + 1) == 0){
          tcmapclear(map);
          tctreeclear(tree);
        }
        break;
      case 44:
        iputchar('i');
        if(myrand(20) == 0){
          tcmapcutfront(map, myrand(10));
          tctreecutfringe(tree, myrand(10));
        }
        break;
      case 45:
        iputchar('j');
        if(myrand(rnum / 100 + 1) == 0){
          int dsiz;
          char *dbuf = tcmapdump(map, &dsiz);
          tcfree(tcmaploadone(dbuf, dsiz, kbuf, ksiz, &vsiz));
          tcmapdel(tcmapload(dbuf, dsiz));
          tcfree(dbuf);
          dbuf = tctreedump(tree, &dsiz);
          tcfree(tctreeloadone(dbuf, dsiz, kbuf, ksiz, &vsiz));
          tctreedel(tctreeload(dbuf, dsiz, tccmplexical, NULL));
          tcfree(dbuf);
        }
        break;
      case 46:
        iputchar('k');
        tcmdbput(mdb, kbuf, ksiz, vbuf, vsiz);
        tcndbput(ndb, kbuf, ksiz, vbuf, vsiz);
        break;
      case 47:
        iputchar('l');
        tcmdbput2(mdb, kbuf, vbuf);
        tcndbput2(ndb, kbuf, vbuf);
        break;
      case 48:
        iputchar('m');
        tcmdbputkeep(mdb, kbuf, ksiz, vbuf, vsiz);
        tcndbputkeep(ndb, kbuf, ksiz, vbuf, vsiz);
        break;
      case 49:
        iputchar('n');
        tcmdbputkeep2(mdb, kbuf, vbuf);
        tcndbputkeep2(ndb, kbuf, vbuf);
        break;
      case 50:
        iputchar('o');
        tcmdbputcat(mdb, kbuf, ksiz, vbuf, vsiz);
        tcndbputcat(ndb, kbuf, ksiz, vbuf, vsiz);
        break;
      case 51:
        iputchar('p');
        tcmdbputcat2(mdb, kbuf, vbuf);
        tcndbputcat2(ndb, kbuf, vbuf);
        break;
      case 52:
        iputchar('q');
        if(myrand(2) == 0){
          tcmdbput3(mdb, kbuf, ksiz, vbuf, vsiz);
          tcndbput3(ndb, kbuf, ksiz, vbuf, vsiz);
        }
        if(myrand(2) == 0){
          tcmdbput4(mdb, kbuf, ksiz, vbuf, vsiz, vbuf, vsiz);
          tcndbputkeep3(ndb, kbuf, ksiz, vbuf, vsiz);
        }
        if(myrand(2) == 0){
          tcmdbputcat3(mdb, kbuf, ksiz, vbuf, vsiz);
          tcndbputcat3(ndb, kbuf, ksiz, vbuf, vsiz);
        }
        break;
      case 53:
        iputchar('r');
        if(myrand(10) == 0){
          tcmdbout(mdb, kbuf, ksiz);
          tcndbout(ndb, kbuf, ksiz);
        }
        break;
      case 54:
        iputchar('s');
        if(myrand(10) == 0){
          tcmdbout2(mdb, kbuf);
          tcndbout2(ndb, kbuf);
        }
        break;
      case 55:
        iputchar('t');
        tcfree(tcmdbget(mdb, kbuf, ksiz, &vsiz));
        tcfree(tcndbget(ndb, kbuf, ksiz, &vsiz));
        break;
      case 56:
        iputchar('u');
        tcfree(tcmdbget3(mdb, kbuf, ksiz, &vsiz));
        tcfree(tcndbget3(ndb, kbuf, ksiz, &vsiz));
        break;
      case 57:
        iputchar('v');
        if(myrand(100) == 0){
          if(myrand(2) == 0){
            tcmdbiterinit(mdb);
            tcndbiterinit(ndb);
          } else {
            tcmdbiterinit2(mdb, kbuf, ksiz);
            tcndbiterinit2(ndb, kbuf, ksiz);
          }
        }
        break;
      case 58:
        iputchar('w');
        tcfree(tcmdbiternext(mdb, &vsiz));
        tcfree(tcndbiternext(ndb, &vsiz));
        break;
      case 59:
        iputchar('x');
        tcfree(tcmdbiternext2(mdb));
        tcfree(tcndbiternext2(ndb));
        break;
      case 60:
        iputchar('y');
        if(myrand(rnum / 100 + 1) == 0){
          tcmdbvanish(mdb);
          tcndbvanish(ndb);
        }
        break;
      case 61:
        iputchar('z');
        if(myrand(200) == 0){
          tcmdbcutfront(mdb, myrand(100));
          tcndbcutfringe(ndb, myrand(100));
        }
        break;
      case 62:
        iputchar('+');
        if(myrand(200) == 0){
          tcmdbforeach(mdb, iterfunc, NULL);
          tcndbforeach(ndb, iterfunc, NULL);
        }
        break;
      case 63:
        iputchar('+');
        if(myrand(100) == 0){
          char *tptr = tcmpoolmalloc(mpool, 1);
          switch(myrand(5)){
            case 0:
              tcfree(tptr);
              tcmpoolpop(mpool, false);
              break;
            case 1:
              tcmpoolpop(mpool, true);
              break;
          }
        }
        break;
      case 64:
        iputchar('+');
        if(myrand(100) == 0){
          TCXSTR *txstr = tcmpoolxstrnew(mpool);
          switch(myrand(5)){
            case 0:
              tcxstrdel(txstr);
              tcmpoolpop(mpool, false);
              break;
            case 1:
              tcmpoolpop(mpool, true);
              break;
          }
        }
        break;
      case 65:
        iputchar('+');
        if(myrand(100) == 0){
          TCLIST *tlist = tcmpoollistnew(mpool);
          switch(myrand(5)){
            case 0:
              tclistdel(tlist);
              tcmpoolpop(mpool, false);
              break;
            case 1:
              tcmpoolpop(mpool, true);
              break;
          }
        }
        break;
      case 66:
        iputchar('+');
        if(myrand(100) == 0){
          TCMAP *tmap = tcmpoolmapnew(mpool);
          switch(myrand(5)){
            case 0:
              tcmapdel(tmap);
              tcmpoolpop(mpool, false);
              break;
            case 1:
              tcmpoolpop(mpool, true);
              break;
          }
        }
        break;
      case 67:
        iputchar('+');
        if(myrand(100) == 0){
          TCTREE *ttree = tcmpooltreenew(mpool);
          switch(myrand(5)){
            case 0:
              tctreedel(ttree);
              tcmpoolpop(mpool, false);
              break;
            case 1:
              tcmpoolpop(mpool, true);
              break;
          }
        }
        break;
      default:
        iputchar('@');
        if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
        break;
    }
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
  }
  if(rnum % 50 > 0) iprintf(" (%08d)\n", rnum);
  for(int i = 0; i < tcptrlistnum(ptrlist); i++){
    tcfree(tcptrlistval(ptrlist, i));
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}



// END OF FILE
