/*************************************************************************************************
 * The command line utility of the B+ tree database API
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


/* global variables */
const char *g_progname;                  // program name
int g_dbgfd;                             // debugging output


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void printerr(TCBDB *bdb);
static int printdata(const char *ptr, int size, bool px);
static char *mygetline(FILE *ifp);
static int mycmpfunc(const char *aptr, int asiz, const char *bptr, int bsiz, void *op);
static int runcreate(int argc, char **argv);
static int runinform(int argc, char **argv);
static int runput(int argc, char **argv);
static int runout(int argc, char **argv);
static int runget(int argc, char **argv);
static int runlist(int argc, char **argv);
static int runoptimize(int argc, char **argv);
static int runimporttsv(int argc, char **argv);
static int runversion(int argc, char **argv);
static int proccreate(const char *path, int lmemb, int nmemb,
                      int bnum, int apow, int fpow, TCCMP cmp, int opts);
static int procinform(const char *path, int omode);
static int procput(const char *path, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
                   TCCMP cmp, int omode, int dmode);
static int procout(const char *path, const char *kbuf, int ksiz, TCCMP cmp, int omode);
static int procget(const char *path, const char *kbuf, int ksiz, TCCMP cmp, int omode,
                   bool px, bool pz);
static int proclist(const char *path, TCCMP cmp, int omode, int max, bool pv, bool px, bool bk,
                    const char *jstr, const char *bstr, const char *estr, const char *fmstr);
static int procoptimize(const char *path, int lmemb, int nmemb,
                        int bnum, int apow, int fpow, TCCMP cmp, int opts, int omode, bool df);
static int procimporttsv(const char *path, const char *file, int omode, bool sc);
static int procversion(void);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  g_dbgfd = -1;
  const char *ebuf = getenv("TCDBGFD");
  if(ebuf) g_dbgfd = tcatoix(ebuf);
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
  } else if(!strcmp(argv[1], "importtsv")){
    rv = runimporttsv(argc, argv);
  } else if(!strcmp(argv[1], "version") || !strcmp(argv[1], "--version")){
    rv = runversion(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
static void usage(void){
  fprintf(stderr, "%s: the command line utility of the B+ tree database API\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s create [-cd|-ci|-cj] [-tl] [-td|-tb|-tt|-tx] path"
          " [lmemb [nmemb [bnum [apow [fpow]]]]]\n", g_progname);
  fprintf(stderr, "  %s inform [-nl|-nb] path\n", g_progname);
  fprintf(stderr, "  %s put [-cd|-ci|-cj] [-nl|-nb] [-sx] [-dk|-dc|-dd|-db|-dai|-dad] path"
          " key value\n", g_progname);
  fprintf(stderr, "  %s out [-cd|-ci|-cj] [-nl|-nb] [-sx] path key\n", g_progname);
  fprintf(stderr, "  %s get [-cd|-ci|-cj] [-nl|-nb] [-sx] [-px] [-pz] path key\n", g_progname);
  fprintf(stderr, "  %s list [-cd|-ci|-cj] [-nl|-nb] [-m num] [-bk] [-pv] [-px] [-j str]"
          " [-rb bkey ekey] [-fm str] path\n", g_progname);
  fprintf(stderr, "  %s optimize [-cd|-ci|-cj] [-tl] [-td|-tb|-tt|-tx] [-tz] [-nl|-nb] [-df]"
          " path [lmemb [nmemb [bnum [apow [fpow]]]]]\n", g_progname);
  fprintf(stderr, "  %s importtsv [-nl|-nb] [-sc] path [file]\n", g_progname);
  fprintf(stderr, "  %s version\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print error information */
static void printerr(TCBDB *bdb){
  const char *path = tcbdbpath(bdb);
  int ecode = tcbdbecode(bdb);
  fprintf(stderr, "%s: %s: %d: %s\n", g_progname, path ? path : "-", ecode, tcbdberrmsg(ecode));
}


/* print record data */
static int printdata(const char *ptr, int size, bool px){
  int len = 0;
  while(size-- > 0){
    if(px){
      if(len > 0) putchar(' ');
      len += printf("%02X", *(unsigned char *)ptr);
    } else {
      putchar(*ptr);
      len++;
    }
    ptr++;
  }
  return len;
}


/* read a line from a file descriptor */
static char *mygetline(FILE *ifp){
  int len = 0;
  int blen = 1024;
  char *buf = tcmalloc(blen + 1);
  bool end = true;
  int c;
  while((c = fgetc(ifp)) != EOF){
    end = false;
    if(c == '\0') continue;
    if(blen <= len){
      blen *= 2;
      buf = tcrealloc(buf, blen + 1);
    }
    if(c == '\n' || c == '\r') c = '\0';
    buf[len++] = c;
    if(c == '\0') break;
  }
  if(end){
    tcfree(buf);
    return NULL;
  }
  buf[len] = '\0';
  return buf;
}


/* dummy comparison function */
static int mycmpfunc(const char *aptr, int asiz, const char *bptr, int bsiz, void *op){
  return 0;
}


/* parse arguments of create command */
static int runcreate(int argc, char **argv){
  char *path = NULL;
  char *lmstr = NULL;
  char *nmstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  TCCMP cmp = NULL;
  int opts = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-cd")){
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
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
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
  if(!path) usage();
  int lmemb = lmstr ? tcatoix(lmstr) : -1;
  int nmemb = nmstr ? tcatoix(nmstr) : -1;
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = proccreate(path, lmemb, nmemb, bnum, apow, fpow, cmp, opts);
  return rv;
}


/* parse arguments of inform command */
static int runinform(int argc, char **argv){
  char *path = NULL;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
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
  int rv = procinform(path, omode);
  return rv;
}


/* parse arguments of put command */
static int runput(int argc, char **argv){
  char *path = NULL;
  char *key = NULL;
  char *value = NULL;
  TCCMP cmp = NULL;
  int omode = 0;
  int dmode = 0;
  bool sx = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-dk")){
        dmode = -1;
      } else if(!strcmp(argv[i], "-dc")){
        dmode = 1;
      } else if(!strcmp(argv[i], "-dd")){
        dmode = 2;
      } else if(!strcmp(argv[i], "-db")){
        dmode = 3;
      } else if(!strcmp(argv[i], "-dai")){
        dmode = 10;
      } else if(!strcmp(argv[i], "-dad")){
        dmode = 11;
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!key){
      key = argv[i];
    } else if(!value){
      value = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !key || !value) usage();
  char *kbuf, *vbuf;
  int ksiz, vsiz;
  if(sx){
    kbuf = tchexdecode(key, &ksiz);
    vbuf = tchexdecode(value, &vsiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
    vsiz = strlen(value);
    vbuf = tcmemdup(value, vsiz);
  }
  int rv = procput(path, kbuf, ksiz, vbuf, vsiz, cmp, omode, dmode);
  tcfree(vbuf);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of out command */
static int runout(int argc, char **argv){
  char *path = NULL;
  char *key = NULL;
  TCCMP cmp = NULL;
  int omode = 0;
  bool sx = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!key){
      key = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !key) usage();
  int ksiz;
  char *kbuf;
  if(sx){
    kbuf = tchexdecode(key, &ksiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
  }
  int rv = procout(path, kbuf, ksiz, cmp, omode);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of get command */
static int runget(int argc, char **argv){
  char *path = NULL;
  char *key = NULL;
  TCCMP cmp = NULL;
  int omode = 0;
  bool sx = false;
  bool px = false;
  bool pz = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else if(!strcmp(argv[i], "-pz")){
        pz = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!key){
      key = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !key) usage();
  int ksiz;
  char *kbuf;
  if(sx){
    kbuf = tchexdecode(key, &ksiz);
  } else {
    ksiz = strlen(key);
    kbuf = tcmemdup(key, ksiz);
  }
  int rv = procget(path, kbuf, ksiz, cmp, omode, px, pz);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of list command */
static int runlist(int argc, char **argv){
  char *path = NULL;
  TCCMP cmp = NULL;
  int omode = 0;
  int max = -1;
  bool pv = false;
  bool px = false;
  bool bk = false;
  char *jstr = NULL;
  char *bstr = NULL;
  char *estr = NULL;
  char *fmstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-m")){
        if(++i >= argc) usage();
        max = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-bk")){
        bk = true;
      } else if(!strcmp(argv[i], "-pv")){
        pv = true;
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else if(!strcmp(argv[i], "-j")){
        if(++i >= argc) usage();
        jstr = argv[i];
      } else if(!strcmp(argv[i], "-rb")){
        if(++i >= argc) usage();
        bstr = argv[i];
        if(++i >= argc) usage();
        estr = argv[i];
      } else if(!strcmp(argv[i], "-fm")){
        if(++i >= argc) usage();
        fmstr = argv[i];
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
  int rv = proclist(path, cmp, omode, max, pv, px, bk, jstr, bstr, estr, fmstr);
  return rv;
}


/* parse arguments of optimize command */
static int runoptimize(int argc, char **argv){
  char *path = NULL;
  char *lmstr = NULL;
  char *nmstr = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  TCCMP cmp = NULL;
  int opts = UINT8_MAX;
  int omode = 0;
  bool df = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-cd")){
        cmp = tccmpdecimal;
      } else if(!strcmp(argv[i], "-ci")){
        cmp = tccmpint32;
      } else if(!strcmp(argv[i], "-cj")){
        cmp = tccmpint64;
      } else if(!strcmp(argv[i], "-tl")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= BDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= BDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= BDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= BDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= BDBTEXCODEC;
      } else if(!strcmp(argv[i], "-tz")){
        if(opts == UINT8_MAX) opts = 0;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-df")){
        df = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
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
  if(!path) usage();
  int lmemb = lmstr ? tcatoix(lmstr) : -1;
  int nmemb = nmstr ? tcatoix(nmstr) : -1;
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procoptimize(path, lmemb, nmemb, bnum, apow, fpow, cmp, opts, omode, df);
  return rv;
}


/* parse arguments of importtsv command */
static int runimporttsv(int argc, char **argv){
  char *path = NULL;
  char *file = NULL;
  int omode = 0;
  bool sc = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= BDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= BDBOLCKNB;
      } else if(!strcmp(argv[i], "-sc")){
        sc = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!file){
      file = argv[i];
    } else {
      usage();
    }
  }
  if(!path) usage();
  int rv = procimporttsv(path, file, omode, sc);
  return rv;
}


/* parse arguments of version command */
static int runversion(int argc, char **argv){
  int rv = procversion();
  return rv;
}


/* perform create command */
static int proccreate(const char *path, int lmemb, int nmemb,
                      int bnum, int apow, int fpow, TCCMP cmp, int opts){
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)) printerr(bdb);
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(bdb);
  if(!tcbdbtune(bdb, lmemb, nmemb, bnum, apow, fpow, opts)){
    printerr(bdb);
    tcbdbdel(bdb);
    return 1;
  }
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | BDBOTRUNC)){
    printerr(bdb);
    tcbdbdel(bdb);
    return 1;
  }
  bool err = false;
  if(!tcbdbclose(bdb)){
    printerr(bdb);
    err = true;
  }
  tcbdbdel(bdb);
  return err ? 1 : 0;
}


/* perform inform command */
static int procinform(const char *path, int omode){
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  tcbdbsetcmpfunc(bdb, mycmpfunc, NULL);
  tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL);
  if(!tcbdbopen(bdb, path, BDBOREADER | omode)){
    printerr(bdb);
    tcbdbdel(bdb);
    return 1;
  }
  bool err = false;
  const char *npath = tcbdbpath(bdb);
  if(!npath) npath = "(unknown)";
  printf("path: %s\n", npath);
  printf("database type: btree\n");
  uint8_t flags = tcbdbflags(bdb);
  printf("additional flags:");
  if(flags & BDBFOPEN) printf(" open");
  if(flags & BDBFFATAL) printf(" fatal");
  printf("\n");
  TCCMP cmp = tcbdbcmpfunc(bdb);
  printf("comparison function: ");
  if(cmp == tccmplexical){
    printf("lexical");
  } else if(cmp == tccmpdecimal){
    printf("decimal");
  } else if(cmp == tccmpint32){
    printf("int32");
  } else if(cmp == tccmpint64){
    printf("int64");
  } else {
    printf("custom");
  }
  printf("\n");
  printf("max leaf member: %d\n", tcbdblmemb(bdb));
  printf("max node member: %d\n", tcbdbnmemb(bdb));
  printf("leaf number: %llu\n", (unsigned long long)tcbdblnum(bdb));
  printf("node number: %llu\n", (unsigned long long)tcbdbnnum(bdb));
  printf("bucket number: %llu\n", (unsigned long long)tcbdbbnum(bdb));
  if(bdb->hdb->cnt_writerec >= 0)
    printf("used bucket number: %lld\n", (long long)tcbdbbnumused(bdb));
  printf("alignment: %u\n", tcbdbalign(bdb));
  printf("free block pool: %u\n", tcbdbfbpmax(bdb));
  printf("inode number: %lld\n", (long long)tcbdbinode(bdb));
  char date[48];
  tcdatestrwww(tcbdbmtime(bdb), INT_MAX, date);
  printf("modified time: %s\n", date);
  uint8_t opts = tcbdbopts(bdb);
  printf("options:");
  if(opts & BDBTLARGE) printf(" large");
  if(opts & BDBTDEFLATE) printf(" deflate");
  if(opts & BDBTBZIP) printf(" bzip");
  if(opts & BDBTTCBS) printf(" tcbs");
  if(opts & BDBTEXCODEC) printf(" excodec");
  printf("\n");
  printf("record number: %llu\n", (unsigned long long)tcbdbrnum(bdb));
  printf("file size: %llu\n", (unsigned long long)tcbdbfsiz(bdb));
  if(!tcbdbclose(bdb)){
    if(!err) printerr(bdb);
    err = true;
  }
  tcbdbdel(bdb);
  return err ? 1 : 0;
}


/* perform put command */
static int procput(const char *path, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
                   TCCMP cmp, int omode, int dmode){
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)) printerr(bdb);
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(bdb);
  if(!tcbdbopen(bdb, path, BDBOWRITER | omode)){
    printerr(bdb);
    tcbdbdel(bdb);
    return 1;
  }
  bool err = false;
  int inum;
  double dnum;
  switch(dmode){
    case -1:
      if(!tcbdbputkeep(bdb, kbuf, ksiz, vbuf, vsiz)){
        printerr(bdb);
        err = true;
      }
      break;
    case 1:
      if(!tcbdbputcat(bdb, kbuf, ksiz, vbuf, vsiz)){
        printerr(bdb);
        err = true;
      }
      break;
    case 2:
      if(!tcbdbputdup(bdb, kbuf, ksiz, vbuf, vsiz)){
        printerr(bdb);
        err = true;
      }
      break;
    case 3:
      if(!tcbdbputdupback(bdb, kbuf, ksiz, vbuf, vsiz)){
        printerr(bdb);
        err = true;
      }
      break;
    case 10:
      inum = tcbdbaddint(bdb, kbuf, ksiz, tcatoi(vbuf));
      if(inum == INT_MIN){
        printerr(bdb);
        err = true;
      } else {
        printf("%d\n", inum);
      }
      break;
    case 11:
      dnum = tcbdbadddouble(bdb, kbuf, ksiz, tcatof(vbuf));
      if(isnan(dnum)){
        printerr(bdb);
        err = true;
      } else {
        printf("%.6f\n", dnum);
      }
      break;
    default:
      if(!tcbdbput(bdb, kbuf, ksiz, vbuf, vsiz)){
        printerr(bdb);
        err = true;
      }
      break;
  }
  if(!tcbdbclose(bdb)){
    if(!err) printerr(bdb);
    err = true;
  }
  tcbdbdel(bdb);
  return err ? 1 : 0;
}


/* perform out command */
static int procout(const char *path, const char *kbuf, int ksiz, TCCMP cmp, int omode){
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)) printerr(bdb);
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(bdb);
  if(!tcbdbopen(bdb, path, BDBOWRITER | omode)){
    printerr(bdb);
    tcbdbdel(bdb);
    return 1;
  }
  bool err = false;
  if(!tcbdbout(bdb, kbuf, ksiz)){
    printerr(bdb);
    err = true;
  }
  if(!tcbdbclose(bdb)){
    if(!err) printerr(bdb);
    err = true;
  }
  tcbdbdel(bdb);
  return err ? 1 : 0;
}


/* perform get command */
static int procget(const char *path, const char *kbuf, int ksiz, TCCMP cmp, int omode,
                   bool px, bool pz){
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)) printerr(bdb);
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(bdb);
  if(!tcbdbopen(bdb, path, BDBOREADER | omode)){
    printerr(bdb);
    tcbdbdel(bdb);
    return 1;
  }
  bool err = false;
  int vsiz;
  char *vbuf = tcbdbget(bdb, kbuf, ksiz, &vsiz);
  if(vbuf){
    printdata(vbuf, vsiz, px);
    if(!pz) putchar('\n');
    tcfree(vbuf);
  } else {
    printerr(bdb);
    err = true;
  }
  if(!tcbdbclose(bdb)){
    if(!err) printerr(bdb);
    err = true;
  }
  tcbdbdel(bdb);
  return err ? 1 : 0;
}


/* perform list command */
static int proclist(const char *path, TCCMP cmp, int omode, int max, bool pv, bool px, bool bk,
                    const char *jstr, const char *bstr, const char *estr, const char *fmstr){
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)) printerr(bdb);
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(bdb);
  if(!tcbdbopen(bdb, path, BDBOREADER | omode)){
    printerr(bdb);
    tcbdbdel(bdb);
    return 1;
  }
  bool err = false;
  if(bstr || fmstr){
    TCLIST *keys = fmstr ? tcbdbfwmkeys2(bdb, fmstr, max) :
      tcbdbrange(bdb, bstr, strlen(bstr), true, estr, strlen(estr), true, max);
    int cnt = 0;
    for(int i = 0; i < tclistnum(keys); i++){
      int ksiz;
      const char *kbuf = tclistval(keys, i, &ksiz);
      if(pv){
        TCLIST *vals = tcbdbget4(bdb, kbuf, ksiz);
        if(vals){
          for(int j = 0; j < tclistnum(vals); j++){
            int vsiz;
            const char *vbuf = tclistval(vals, j, &vsiz);
            printdata(kbuf, ksiz, px);
            putchar('\t');
            printdata(vbuf, vsiz, px);
            putchar('\n');
            if(max >= 0 && ++cnt >= max) break;
          }
          tclistdel(vals);
        }
      } else {
        int num = tcbdbvnum(bdb, kbuf, ksiz);
        for(int j = 0; j < num; j++){
          printdata(kbuf, ksiz, px);
          putchar('\n');
          if(max >= 0 && ++cnt >= max) break;
        }
      }
      if(max >= 0 && cnt >= max) break;
    }
    tclistdel(keys);
  } else {
    BDBCUR *cur = tcbdbcurnew(bdb);
    if(bk){
      if(jstr){
        if(!tcbdbcurjumpback(cur, jstr, strlen(jstr)) && tcbdbecode(bdb) != TCENOREC){
          printerr(bdb);
          err = true;
        }
      } else {
        if(!tcbdbcurlast(cur) && tcbdbecode(bdb) != TCENOREC){
          printerr(bdb);
          err = true;
        }
      }
    } else {
      if(jstr){
        if(!tcbdbcurjump(cur, jstr, strlen(jstr)) && tcbdbecode(bdb) != TCENOREC){
          printerr(bdb);
          err = true;
        }
      } else {
        if(!tcbdbcurfirst(cur) && tcbdbecode(bdb) != TCENOREC){
          printerr(bdb);
          err = true;
        }
      }
    }
    TCXSTR *key = tcxstrnew();
    TCXSTR *val = tcxstrnew();
    int cnt = 0;
    while(tcbdbcurrec(cur, key, val)){
      printdata(tcxstrptr(key), tcxstrsize(key), px);
      if(pv){
        putchar('\t');
        printdata(tcxstrptr(val), tcxstrsize(val), px);
      }
      putchar('\n');
      if(bk){
        if(!tcbdbcurprev(cur) && tcbdbecode(bdb) != TCENOREC){
          printerr(bdb);
          err = true;
        }
      } else {
        if(!tcbdbcurnext(cur) && tcbdbecode(bdb) != TCENOREC){
          printerr(bdb);
          err = true;
        }
      }
      if(max >= 0 && ++cnt >= max) break;
    }
    tcxstrdel(val);
    tcxstrdel(key);
    tcbdbcurdel(cur);
  }
  if(!tcbdbclose(bdb)){
    if(!err) printerr(bdb);
    err = true;
  }
  tcbdbdel(bdb);
  return err ? 1 : 0;
}


/* perform optimize command */
static int procoptimize(const char *path, int lmemb, int nmemb,
                        int bnum, int apow, int fpow, TCCMP cmp, int opts, int omode, bool df){
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(cmp && !tcbdbsetcmpfunc(bdb, cmp, NULL)) printerr(bdb);
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(bdb);
  if(!tcbdbopen(bdb, path, BDBOWRITER | omode)){
    printerr(bdb);
    tcbdbdel(bdb);
    return 1;
  }
  bool err = false;
  if(df){
    if(!tcbdbdefrag(bdb, INT64_MAX)){
      printerr(bdb);
      err = true;
    }
  } else {
    if(!tcbdboptimize(bdb, lmemb, nmemb, bnum, apow, fpow, opts)){
      printerr(bdb);
      err = true;
    }
  }
  if(!tcbdbclose(bdb)){
    if(!err) printerr(bdb);
    err = true;
  }
  tcbdbdel(bdb);
  return err ? 1 : 0;
}


/* perform importtsv command */
static int procimporttsv(const char *path, const char *file, int omode, bool sc){
  FILE *ifp = file ? fopen(file, "rb") : stdin;
  if(!ifp){
    fprintf(stderr, "%s: could not open\n", file ? file : "(stdin)");
    return 1;
  }
  TCBDB *bdb = tcbdbnew();
  if(g_dbgfd >= 0) tcbdbsetdbgfd(bdb, g_dbgfd);
  if(!tcbdbsetcodecfunc(bdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(bdb);
  if(!tcbdbopen(bdb, path, BDBOWRITER | BDBOCREAT | omode)){
    printerr(bdb);
    tcbdbdel(bdb);
    if(ifp != stdin) fclose(ifp);
    return 1;
  }
  bool err = false;
  char *line;
  int cnt = 0;
  while(!err && (line = mygetline(ifp)) != NULL){
    char *pv = strchr(line, '\t');
    if(!pv){
      tcfree(line);
      continue;
    }
    *pv = '\0';
    if(sc) tcstrutfnorm(line, TCUNSPACE | TCUNLOWER | TCUNNOACC | TCUNWIDTH);
    if(!tcbdbputdup2(bdb, line, pv + 1)){
      printerr(bdb);
      err = true;
    }
    tcfree(line);
    if(cnt > 0 && cnt % 100 == 0){
      putchar('.');
      fflush(stdout);
      if(cnt % 5000 == 0) printf(" (%08d)\n", cnt);
    }
    cnt++;
  }
  printf(" (%08d)\n", cnt);
  if(!tcbdbclose(bdb)){
    if(!err) printerr(bdb);
    err = true;
  }
  tcbdbdel(bdb);
  if(ifp != stdin) fclose(ifp);
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
