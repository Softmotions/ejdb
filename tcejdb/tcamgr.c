/*************************************************************************************************
 * The command line utility of the abstract database API
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
#include <tcadb.h>
#include "myconf.h"


/* global variables */
const char *g_progname;                  // program name


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void printerr(TCADB *adb);
static int sepstrtochr(const char *str);
static char *strtozsv(const char *str, int sep, int *sp);
static int printdata(const char *ptr, int size, bool px, int sep);
static void setskeltran(ADBSKEL *skel);
static bool mapbdbproc(void *map, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
                       void *op);
static int runcreate(int argc, char **argv);
static int runinform(int argc, char **argv);
static int runput(int argc, char **argv);
static int runout(int argc, char **argv);
static int runget(int argc, char **argv);
static int runlist(int argc, char **argv);
static int runoptimize(int argc, char **argv);
static int runmisc(int argc, char **argv);
static int runmap(int argc, char **argv);
static int runversion(int argc, char **argv);
static int proccreate(const char *name);
static int procinform(const char *name);
static int procput(const char *name, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
                   int dmode);
static int procout(const char *name, const char *kbuf, int ksiz);
static int procget(const char *name, const char *kbuf, int ksiz, int sep, bool px, bool pz);
static int proclist(const char *name, int sep, int max, bool pv, bool px, const char *fmstr);
static int procoptimize(const char *name, const char *params);
static int procmisc(const char *name, const char *func, const TCLIST *args, int sep, bool px);
static int procmap(const char *name, const char *dest, const char *fmstr);
static int procversion(void);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "create")){
    rv = runcreate(argc, argv);
  } else if(!strcmp(argv[1], "inform")){
    rv = runinform(argc, argv);
  } else if(!strcmp(argv[1], "put")){
    rv = runput(argc, argv);
  } else if(!strcmp(argv[1], "out")){
    rv = runout(argc, argv);
  } else if(!strcmp(argv[1], "get")){
    rv = runget(argc, argv);
  } else if(!strcmp(argv[1], "list")){
    rv = runlist(argc, argv);
  } else if(!strcmp(argv[1], "optimize")){
    rv = runoptimize(argc, argv);
  } else if(!strcmp(argv[1], "misc")){
    rv = runmisc(argc, argv);
  } else if(!strcmp(argv[1], "map")){
    rv = runmap(argc, argv);
  } else if(!strcmp(argv[1], "version") || !strcmp(argv[1], "--version")){
    rv = runversion(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
static void usage(void){
  fprintf(stderr, "%s: the command line utility of the abstract database API\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s create name\n", g_progname);
  fprintf(stderr, "  %s inform name\n", g_progname);
  fprintf(stderr, "  %s put [-sx] [-sep chr] [-dk|-dc|-dai|-dad] name key value\n", g_progname);
  fprintf(stderr, "  %s out [-sx] [-sep chr] name key\n", g_progname);
  fprintf(stderr, "  %s get [-sx] [-sep chr] [-px] [-pz] name key\n", g_progname);
  fprintf(stderr, "  %s list [-sep chr] [-m num] [-pv] [-px] [-fm str] name\n", g_progname);
  fprintf(stderr, "  %s optimize name [params]\n", g_progname);
  fprintf(stderr, "  %s misc [-sx] [-sep chr] [-px] name func [arg...]\n", g_progname);
  fprintf(stderr, "  %s map [-fm str] name dest\n", g_progname);
  fprintf(stderr, "  %s version\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* get the character of separation string */
static int sepstrtochr(const char *str){
  if(!strcmp(str, "\\t")) return '\t';
  if(!strcmp(str, "\\r")) return '\r';
  if(!strcmp(str, "\\n")) return '\n';
  return *(unsigned char *)str;
}


/* encode a string as a zero separaterd string */
static char *strtozsv(const char *str, int sep, int *sp){
  int size = strlen(str);
  char *buf = tcmemdup(str, size);
  for(int i = 0; i < size; i++){
    if(buf[i] == sep) buf[i] = '\0';
  }
  *sp = size;
  return buf;
}


/* print error information */
static void printerr(TCADB *adb){
  const char *path = tcadbpath(adb);
  fprintf(stderr, "%s: %s: error\n", g_progname, path ? path : "-");
}


/* print record data */
static int printdata(const char *ptr, int size, bool px, int sep){
  int len = 0;
  while(size-- > 0){
    if(px){
      if(len > 0) putchar(' ');
      len += printf("%02X", *(unsigned char *)ptr);
    } else if(sep > 0){
      if(*ptr == '\0'){
        putchar(sep);
      } else {
        putchar(*ptr);
      }
      len++;
    } else {
      putchar(*ptr);
      len++;
    }
    ptr++;
  }
  return len;
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


/* mapping function */
static bool mapbdbproc(void *map, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
                       void *op){
  bool err = false;
  if(!tcadbmapbdbemit(map, kbuf, ksiz, vbuf, vsiz)) err = true;
  return !err;
}


/* parse arguments of create command */
static int runcreate(int argc, char **argv){
  char *name = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else {
      usage();
    }
  }
  if(!name) usage();
  int rv = proccreate(name);
  return rv;
}


/* parse arguments of inform command */
static int runinform(int argc, char **argv){
  char *name = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else {
      usage();
    }
  }
  if(!name) usage();
  name = tcsprintf("%s#mode=r", name);
  int rv = procinform(name);
  tcfree(name);
  return rv;
}


/* parse arguments of put command */
static int runput(int argc, char **argv){
  char *name = NULL;
  char *key = NULL;
  char *value = NULL;
  int dmode = 0;
  bool sx = false;
  int sep = -1;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-dk")){
        dmode = -1;
      } else if(!strcmp(argv[i], "-dc")){
        dmode = 1;
      } else if(!strcmp(argv[i], "-dai")){
        dmode = 10;
      } else if(!strcmp(argv[i], "-dad")){
        dmode = 11;
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else if(!strcmp(argv[i], "-sep")){
        if(++i >= argc) usage();
        sep = sepstrtochr(argv[i]);
      } else {
        usage();
      }
    } else if(!name){
      name = argv[i];
    } else if(!key){
      key = argv[i];
    } else if(!value){
      value = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !key || !value) usage();
  char *kbuf, *vbuf;
  int ksiz, vsiz;
  if(sx){
    kbuf = tchexdecode(key, &ksiz);
    vbuf = tchexdecode(value, &vsiz);
  } else if(sep > 0){
    kbuf = strtozsv(key, sep, &ksiz);
    vbuf = strtozsv(value, sep, &vsiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
    vsiz = strlen(value);
    vbuf = tcmemdup(value, vsiz);
  }
  int rv = procput(name, kbuf, ksiz, vbuf, vsiz, dmode);
  tcfree(vbuf);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of out command */
static int runout(int argc, char **argv){
  char *name = NULL;
  char *key = NULL;
  bool sx = false;
  int sep = -1;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else if(!strcmp(argv[i], "-sep")){
        if(++i >= argc) usage();
        sep = sepstrtochr(argv[i]);
      } else {
        usage();
      }
    } else if(!name){
      name = argv[i];
    } else if(!key){
      key = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !key) usage();
  int ksiz;
  char *kbuf;
  if(sx){
    kbuf = tchexdecode(key, &ksiz);
  } else if(sep > 0){
    kbuf = strtozsv(key, sep, &ksiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
  }
  int rv = procout(name, kbuf, ksiz);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of get command */
static int runget(int argc, char **argv){
  char *name = NULL;
  char *key = NULL;
  bool sx = false;
  int sep = -1;
  bool px = false;
  bool pz = false;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else if(!strcmp(argv[i], "-sep")){
        if(++i >= argc) usage();
        sep = sepstrtochr(argv[i]);
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else if(!strcmp(argv[i], "-pz")){
        pz = true;
      } else {
        usage();
      }
    } else if(!name){
      name = argv[i];
    } else if(!key){
      key = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !key) usage();
  int ksiz;
  char *kbuf;
  if(sx){
    kbuf = tchexdecode(key, &ksiz);
  } else if(sep > 0){
    kbuf = strtozsv(key, sep, &ksiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
  }
  name = tcsprintf("%s#mode=r", name);
  int rv = procget(name, kbuf, ksiz, sep, px, pz);
  tcfree(name);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of list command */
static int runlist(int argc, char **argv){
  char *name = NULL;
  int sep = -1;
  int max = -1;
  bool pv = false;
  bool px = false;
  char *fmstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-sep")){
        if(++i >= argc) usage();
        sep = sepstrtochr(argv[i]);
      } else if(!strcmp(argv[i], "-m")){
        if(++i >= argc) usage();
        max = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-pv")){
        pv = true;
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else if(!strcmp(argv[i], "-fm")){
        if(++i >= argc) usage();
        fmstr = argv[i];
      } else {
        usage();
      }
    } else if(!name){
      name = argv[i];
    } else {
      usage();
    }
  }
  if(!name) usage();
  name = tcsprintf("%s#mode=r", name);
  int rv = proclist(name, sep, max, pv, px, fmstr);
  tcfree(name);
  return rv;
}


/* parse arguments of optimize command */
static int runoptimize(int argc, char **argv){
  char *name = NULL;
  char *params = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!params){
      params = argv[i];
    } else {
      usage();
    }
  }
  if(!name) usage();
  int rv = procoptimize(name, params);
  return rv;
}


/* parse arguments of misc command */
static int runmisc(int argc, char **argv){
  char *name = NULL;
  char *func = NULL;
  TCLIST *args = tcmpoollistnew(tcmpoolglobal());
  bool sx = false;
  int sep = -1;
  bool px = false;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else if(!strcmp(argv[i], "-sep")){
        if(++i >= argc) usage();
        sep = sepstrtochr(argv[i]);
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else {
        usage();
      }
    } else if(!name){
      name = argv[i];
    } else if(!func){
      func = argv[i];
    } else {
      if(sx){
        int size;
        char *buf = tchexdecode(argv[i], &size);
        tclistpush(args, buf, size);
        tcfree(buf);
      } else if(sep > 0){
        int size;
        char *buf = strtozsv(argv[i], sep, &size);
        tclistpush(args, buf, size);
        tcfree(buf);
      } else {
        tclistpush2(args, argv[i]);
      }
    }
  }
  if(!name || !func) usage();
  int rv = procmisc(name, func, args, sep, px);
  return rv;
}


/* parse arguments of map command */
static int runmap(int argc, char **argv){
  char *name = NULL;
  char *dest = NULL;
  char *fmstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-fm")){
        if(++i >= argc) usage();
        fmstr = argv[i];
      } else {
        usage();
      }
    } else if(!name){
      name = argv[i];
    } else if(!dest){
      dest = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !dest) usage();
  name = tcsprintf("%s#mode=r", name);
  int rv = procmap(name, dest, fmstr);
  tcfree(name);
  return rv;
}


/* parse arguments of version command */
static int runversion(int argc, char **argv){
  int rv = procversion();
  return rv;
}


/* perform create command */
static int proccreate(const char *name){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  if(!tcadbclose(adb)){
    printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform inform command */
static int procinform(const char *name){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  const char *path = tcadbpath(adb);
  if(!path) path = "(unknown)";
  printf("path: %s\n", path);
  const char *type = "(unknown)";
  switch(tcadbomode(adb)){
    case ADBOVOID: type = "not opened"; break;
    case ADBOMDB: type = "on-memory hash database"; break;
    case ADBONDB: type = "on-memory tree database"; break;
    case ADBOHDB: type = "hash database"; break;
    case ADBOBDB: type = "B+ tree database"; break;
    case ADBOFDB: type = "fixed-length database"; break;
    case ADBOTDB: type = "table database"; break;
    case ADBOSKEL: type = "skeleton database"; break;
  }
  printf("database type: %s\n", type);
  printf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  printf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  if(!tcadbclose(adb)){
    printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform put command */
static int procput(const char *name, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
                   int dmode){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  int inum;
  double dnum;
  switch(dmode){
    case -1:
      if(!tcadbputkeep(adb, kbuf, ksiz, vbuf, vsiz)){
        printerr(adb);
        err = true;
      }
      break;
    case 1:
      if(!tcadbputcat(adb, kbuf, ksiz, vbuf, vsiz)){
        printerr(adb);
        err = true;
      }
      break;
    case 10:
      inum = tcadbaddint(adb, kbuf, ksiz, tcatoi(vbuf));
      if(inum == INT_MIN){
        printerr(adb);
        err = true;
      } else {
        printf("%d\n", inum);
      }
      break;
    case 11:
      dnum = tcadbadddouble(adb, kbuf, ksiz, tcatof(vbuf));
      if(isnan(dnum)){
        printerr(adb);
        err = true;
      } else {
        printf("%.6f\n", dnum);
      }
      break;
    default:
      if(!tcadbput(adb, kbuf, ksiz, vbuf, vsiz)){
        printerr(adb);
        err = true;
      }
      break;
  }
  if(!tcadbclose(adb)){
    if(!err) printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform out command */
static int procout(const char *name, const char *kbuf, int ksiz){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  if(!tcadbout(adb, kbuf, ksiz)){
    printerr(adb);
    err = true;
  }
  if(!tcadbclose(adb)){
    if(!err) printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform get command */
static int procget(const char *name, const char *kbuf, int ksiz, int sep, bool px, bool pz){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  int vsiz;
  char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
  if(vbuf){
    printdata(vbuf, vsiz, px, sep);
    if(!pz) putchar('\n');
    tcfree(vbuf);
  } else {
    printerr(adb);
    err = true;
  }
  if(!tcadbclose(adb)){
    if(!err) printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform list command */
static int proclist(const char *name, int sep, int max, bool pv, bool px, const char *fmstr){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  if(fmstr){
    TCLIST *keys = tcadbfwmkeys2(adb, fmstr, max);
    for(int i = 0; i < tclistnum(keys); i++){
      int ksiz;
      const char *kbuf = tclistval(keys, i, &ksiz);
      printdata(kbuf, ksiz, px, sep);
      if(pv){
        int vsiz;
        char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
        if(vbuf){
          putchar('\t');
          printdata(vbuf, vsiz, px, sep);
          tcfree(vbuf);
        }
      }
      putchar('\n');
    }
    tclistdel(keys);
  } else {
    if(!tcadbiterinit(adb)){
      printerr(adb);
      err = true;
    }
    int ksiz;
    char *kbuf;
    int cnt = 0;
    while((kbuf = tcadbiternext(adb, &ksiz)) != NULL){
      printdata(kbuf, ksiz, px, sep);
      if(pv){
        int vsiz;
        char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
        if(vbuf){
          putchar('\t');
          printdata(vbuf, vsiz, px, sep);
          tcfree(vbuf);
        }
      }
      putchar('\n');
      tcfree(kbuf);
      if(max >= 0 && ++cnt >= max) break;
    }
  }
  if(!tcadbclose(adb)){
    if(!err) printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform optimize command */
static int procoptimize(const char *name, const char *params){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  if(!tcadboptimize(adb, params)){
    printerr(adb);
    err = true;
  }
  if(!tcadbclose(adb)){
    if(!err) printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform misc command */
static int procmisc(const char *name, const char *func, const TCLIST *args, int sep, bool px){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  TCLIST *res = tcadbmisc(adb, func, args);
  if(res){
    for(int i = 0; i < tclistnum(res); i++){
      int rsiz;
      const char *rbuf = tclistval(res, i, &rsiz);
      printdata(rbuf, rsiz, px, sep);
      printf("\n");
    }
    tclistdel(res);
  } else {
    printerr(adb);
    err = true;
  }
  if(!tcadbclose(adb)){
    if(!err) printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform map command */
static int procmap(const char *name, const char *dest, const char *fmstr){
  TCADB *adb = tcadbnew();
  ADBSKEL skel;
  if(*name == '@'){
    setskeltran(&skel);
    if(!tcadbsetskel(adb, &skel)){
      printerr(adb);
      skel.del(skel.opq);
      tcadbdel(adb);
      return 1;
    }
    name++;
  } else if(*name == '%'){
    if(!tcadbsetskelmulti(adb, 8)){
      printerr(adb);
      tcadbdel(adb);
      return 1;
    }
    name++;
  }
  if(!tcadbopen(adb, name)){
    printerr(adb);
    tcadbdel(adb);
    return 1;
  }
  bool err = false;
  TCBDB *bdb = tcbdbnew();
  if(!tcbdbopen(bdb, dest, BDBOWRITER | BDBOCREAT | BDBOTRUNC)){
    printerr(adb);
    tcbdbdel(bdb);
    tcadbdel(adb);
    return 1;
  }
  if(fmstr){
    TCLIST *keys = tcadbfwmkeys2(adb, fmstr, -1);
    if(!tcadbmapbdb(adb, keys, bdb, mapbdbproc, NULL, -1)){
      printerr(adb);
      err = true;
    }
    tclistdel(keys);
  } else {
    if(!tcadbmapbdb(adb, NULL, bdb, mapbdbproc, NULL, -1)){
      printerr(adb);
      err = true;
    }
  }
  if(!tcbdbclose(bdb)){
    printerr(adb);
    err = true;
  }
  tcbdbdel(bdb);
  if(!tcadbclose(adb)){
    printerr(adb);
    err = true;
  }
  tcadbdel(adb);
  return err ? 1 : 0;
}


/* perform version command */
static int procversion(void){
  printf("Tokyo Cabinet version %s (%d:%s) for %s\n",
         tcversion, _TC_LIBVER, _TC_FORMATVER, TCSYSNAME);
  printf("Copyright (C) 2006-2012 FAL Labs\n");
  return 0;
}



// END OF FILE
