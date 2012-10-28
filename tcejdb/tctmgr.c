/*************************************************************************************************
 * The command line utility of the table database API
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


/* global variables */
const char *g_progname;                  // program name
int g_dbgfd;                             // debugging output


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void printerr(TCTDB *tdb);
static int printdata(const char *ptr, int size, bool px);
static char *mygetline(FILE *ifp);
static int runcreate(int argc, char **argv);
static int runinform(int argc, char **argv);
static int runput(int argc, char **argv);
static int runout(int argc, char **argv);
static int runget(int argc, char **argv);
static int runlist(int argc, char **argv);
static int runsearch(int argc, char **argv);
static int runoptimize(int argc, char **argv);
static int runsetindex(int argc, char **argv);
static int runimporttsv(int argc, char **argv);
static int runversion(int argc, char **argv);
static int proccreate(const char *path, int bnum, int apow, int fpow, int opts);
static int procinform(const char *path, int omode);
static int procput(const char *path, const char *pkbuf, int pksiz, TCMAP *cols,
                   int omode, int dmode);
static int procout(const char *path, const char *pkbuf, int pksiz, int omode);
static int procget(const char *path, const char *pkbuf, int pksiz, int omode, bool px, bool pz);
static int proclist(const char *path, int omode, int max, bool pv, bool px, const char *fmstr);
static int procsearch(const char *path, TCLIST *conds, const char *oname, const char *otype,
                      int omode, int max, int skip, bool pv, bool px, bool kw, bool ph, int bt,
                      bool rm, const char *mtype);
static int procoptimize(const char *path, int bnum, int apow, int fpow, int opts, int omode,
                        bool df);
static int procsetindex(const char *path, const char *name, int omode, int type);
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
  } else if(!strcmp(argv[1], "search")){
    rv = runsearch(argc, argv);
  } else if(!strcmp(argv[1], "optimize")){
    rv = runoptimize(argc, argv);
  } else if(!strcmp(argv[1], "setindex")){
    rv = runsetindex(argc, argv);
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
  fprintf(stderr, "%s: the command line utility of the table database API\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s create [-tl] [-td|-tb|-tt|-tx] path [bnum [apow [fpow]]]\n", g_progname);
  fprintf(stderr, "  %s inform [-nl|-nb] path\n", g_progname);
  fprintf(stderr, "  %s put [-nl|-nb] [-sx] [-dk|-dc|-dai|-dad] path pkey [cols...]\n",
          g_progname);
  fprintf(stderr, "  %s out [-nl|-nb] [-sx] path pkey\n", g_progname);
  fprintf(stderr, "  %s get [-nl|-nb] [-sx] [-px] [-pz] path pkey\n", g_progname);
  fprintf(stderr, "  %s list [-nl|-nb] [-m num] [-pv] [-px] [-fm str] path\n", g_progname);
  fprintf(stderr, "  %s search [-nl|-nb] [-ord name type] [-m num] [-sk num] [-kw] [-pv] [-px]"
          " [-ph] [-bt num] [-rm] [-ms type] path [name op expr ...]\n", g_progname);
  fprintf(stderr, "  %s optimize [-tl] [-td|-tb|-tt|-tx] [-tz] [-nl|-nb] [-df]"
          " path [bnum [apow [fpow]]]\n", g_progname);
  fprintf(stderr, "  %s setindex [-nl|-nb] [-it type] path name\n", g_progname);
  fprintf(stderr, "  %s importtsv [-nl|-nb] [-sc] path [file]\n", g_progname);
  fprintf(stderr, "  %s version\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print error information */
static void printerr(TCTDB *tdb){
  const char *path = tctdbpath(tdb);
  int ecode = tctdbecode(tdb);
  fprintf(stderr, "%s: %s: %d: %s\n", g_progname, path ? path : "-", ecode, tctdberrmsg(ecode));
}


/* print record data */
static int printdata(const char *ptr, int size, bool px){
  int len = 0;
  while(size-- > 0){
    if(px){
      if(len > 0) putchar(' ');
      len += printf("%02X", *(unsigned char *)ptr);
    } else {
      if(strchr("\t\r\n", *ptr)){
        putchar(' ');
      } else {
        putchar(*ptr);
      }
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


/* parse arguments of create command */
static int runcreate(int argc, char **argv){
  char *path = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  int opts = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        opts |= TDBTEXCODEC;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
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
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = proccreate(path, bnum, apow, fpow, opts);
  return rv;
}


/* parse arguments of inform command */
static int runinform(int argc, char **argv){
  char *path = NULL;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
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
  char *pkey = NULL;
  TCLIST *vals = tcmpoollistnew(tcmpoolglobal());
  int omode = 0;
  int dmode = 0;
  bool sx = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-dk")){
        dmode = -1;
      } else if(!strcmp(argv[i], "-dc")){
        dmode = 1;
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
    } else if(!pkey){
      pkey = argv[i];
    } else {
      tclistpush2(vals, argv[i]);
    }
  }
  if(!path || !pkey) usage();
  TCMAP *cols = tcmapnew();
  char *pkbuf;
  int pksiz;
  if(sx){
    pkbuf = tchexdecode(pkey, &pksiz);
    for(int i = 0; i < tclistnum(vals) - 1; i += 2){
      const char *name = tclistval2(vals, i);
      const char *value = tclistval2(vals, i + 1);
      int nsiz;
      char *nbuf = tchexdecode(name, &nsiz);
      int vsiz;
      char *vbuf = tchexdecode(value, &vsiz);
      tcmapput(cols, nbuf, nsiz, vbuf, vsiz);
      tcfree(vbuf);
      tcfree(nbuf);
    }
  } else {
    pksiz = strlen(pkey);
    pkbuf = tcmemdup(pkey, pksiz);
    for(int i = 0; i < tclistnum(vals) - 1; i += 2){
      const char *name = tclistval2(vals, i);
      const char *value = tclistval2(vals, i + 1);
      tcmapput2(cols, name, value);
    }
  }
  int rv = procput(path, pkbuf, pksiz, cols, omode, dmode);
  tcmapdel(cols);
  tcfree(pkbuf);
  return rv;
}


/* parse arguments of out command */
static int runout(int argc, char **argv){
  char *path = NULL;
  char *pkey = NULL;
  int omode = 0;
  bool sx = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-sx")){
        sx = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!pkey){
      pkey = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !pkey) usage();
  int pksiz;
  char *pkbuf;
  if(sx){
    pkbuf = tchexdecode(pkey, &pksiz);
  } else {
    pksiz = strlen(pkey);
    pkbuf = tcmemdup(pkey, pksiz);
  }
  int rv = procout(path, pkbuf, pksiz, omode);
  tcfree(pkbuf);
  return rv;
}


/* parse arguments of get command */
static int runget(int argc, char **argv){
  char *path = NULL;
  char *pkey = NULL;
  int omode = 0;
  bool sx = false;
  bool px = false;
  bool pz = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
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
    } else if(!pkey){
      pkey = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !pkey) usage();
  int pksiz;
  char *pkbuf;
  if(sx){
    pkbuf = tchexdecode(pkey, &pksiz);
  } else {
    pksiz = strlen(pkey);
    pkbuf = tcmemdup(pkey, pksiz);
  }
  int rv = procget(path, pkbuf, pksiz, omode, px, pz);
  tcfree(pkbuf);
  return rv;
}


/* parse arguments of list command */
static int runlist(int argc, char **argv){
  char *path = NULL;
  int omode = 0;
  int max = -1;
  bool pv = false;
  bool px = false;
  char *fmstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
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
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  if(!path) usage();
  int rv = proclist(path, omode, max, pv, px, fmstr);
  return rv;
}


/* parse arguments of search command */
static int runsearch(int argc, char **argv){
  char *path = NULL;
  TCLIST *conds = tcmpoollistnew(tcmpoolglobal());
  char *oname = NULL;
  char *otype = NULL;
  int omode = 0;
  int max = -1;
  int skip = -1;
  bool pv = false;
  bool px = false;
  bool kw = false;
  bool ph = false;
  int bt = 0;
  bool rm = false;
  char *mtype = NULL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-ord")){
        if(++i >= argc) usage();
        oname = argv[i];
        if(++i >= argc) usage();
        otype = argv[i];
      } else if(!strcmp(argv[i], "-m")){
        if(++i >= argc) usage();
        max = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-sk")){
        if(++i >= argc) usage();
        skip = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-kw")){
        kw = true;
      } else if(!strcmp(argv[i], "-pv")){
        pv = true;
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else if(!strcmp(argv[i], "-ph")){
        ph = true;
      } else if(!strcmp(argv[i], "-bt")){
        if(++i >= argc) usage();
        bt = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-rm")){
        rm = true;
      } else if(!strcmp(argv[i], "-ms")){
        if(++i >= argc) usage();
        mtype = argv[i];
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      tclistpush2(conds, argv[i]);
    }
  }
  if(!path || tclistnum(conds) % 3 != 0) usage();
  int rv = procsearch(path, conds, oname, otype, omode, max, skip,
                      pv, px, kw, ph, bt, rm, mtype);
  return rv;
}


/* parse arguments of optimize command */
static int runoptimize(int argc, char **argv){
  char *path = NULL;
  char *bstr = NULL;
  char *astr = NULL;
  char *fstr = NULL;
  int opts = UINT8_MAX;
  int omode = 0;
  bool df = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-tl")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= TDBTLARGE;
      } else if(!strcmp(argv[i], "-td")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= TDBTDEFLATE;
      } else if(!strcmp(argv[i], "-tb")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= TDBTBZIP;
      } else if(!strcmp(argv[i], "-tt")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= TDBTTCBS;
      } else if(!strcmp(argv[i], "-tx")){
        if(opts == UINT8_MAX) opts = 0;
        opts |= TDBTEXCODEC;
      } else if(!strcmp(argv[i], "-tz")){
        if(opts == UINT8_MAX) opts = 0;
      } else if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-df")){
        df = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
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
  int bnum = bstr ? tcatoix(bstr) : -1;
  int apow = astr ? tcatoix(astr) : -1;
  int fpow = fstr ? tcatoix(fstr) : -1;
  int rv = procoptimize(path, bnum, apow, fpow, opts, omode, df);
  return rv;
}


/* parse arguments of setindex command */
static int runsetindex(int argc, char **argv){
  char *path = NULL;
  char *name = NULL;
  int omode = 0;
  int type = TDBITLEXICAL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
      } else if(!strcmp(argv[i], "-it")){
        if(++i >= argc) usage();
        type = tctdbstrtoindextype(argv[i]);
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!name){
      name = argv[i];
    } else {
      usage();
    }
  }
  if(!path || !name) usage();
  if(type < 0) usage();
  int rv = procsetindex(path, name, omode, type);
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
        omode |= TDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= TDBOLCKNB;
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
static int proccreate(const char *path, int bnum, int apow, int fpow, int opts){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  if(!tctdbtune(tdb, bnum, apow, fpow, opts)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | TDBOTRUNC)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  if(!tctdbclose(tdb)){
    printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform inform command */
static int procinform(const char *path, int omode){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL);
  if(!tctdbopen(tdb, path, TDBOREADER | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  const char *npath = tctdbpath(tdb);
  if(!npath) npath = "(unknown)";
  printf("path: %s\n", npath);
  printf("database type: table\n");
  uint8_t flags = tctdbflags(tdb);
  printf("additional flags:");
  if(flags & TDBFOPEN) printf(" open");
  if(flags & TDBFFATAL) printf(" fatal");
  printf("\n");
  printf("bucket number: %llu\n", (unsigned long long)tctdbbnum(tdb));
  if(tdb->hdb->cnt_writerec >= 0)
    printf("used bucket number: %lld\n", (long long)tctdbbnumused(tdb));
  printf("alignment: %u\n", tctdbalign(tdb));
  printf("free block pool: %u\n", tctdbfbpmax(tdb));
  printf("index number: %d\n", tctdbinum(tdb));
  TDBIDX *idxs = tdb->idxs;
  int inum = tdb->inum;
  for(int i = 0; i < inum; i++){
    TDBIDX *idxp = idxs + i;
    switch(idxp->type){
      case TDBITLEXICAL:
        printf("  name=%s, type=lexical, rnum=%lld, fsiz=%lld\n",
               idxp->name, (long long)tcbdbrnum(idxp->db), (long long)tcbdbfsiz(idxp->db));
        break;
      case TDBITDECIMAL:
        printf("  name=%s, type=decimal, rnum=%lld, fsiz=%lld\n",
               idxp->name, (long long)tcbdbrnum(idxp->db), (long long)tcbdbfsiz(idxp->db));
        break;
      case TDBITTOKEN:
        printf("  name=%s, type=token, rnum=%lld, fsiz=%lld\n",
               idxp->name, (long long)tcbdbrnum(idxp->db), (long long)tcbdbfsiz(idxp->db));
        break;
      case TDBITQGRAM:
        printf("  name=%s, type=qgram, rnum=%lld, fsiz=%lld\n",
               idxp->name, (long long)tcbdbrnum(idxp->db), (long long)tcbdbfsiz(idxp->db));
        break;
    }
  }
  printf("unique ID seed: %lld\n", (long long)tctdbuidseed(tdb));
  printf("inode number: %lld\n", (long long)tctdbinode(tdb));
  char date[48];
  tcdatestrwww(tctdbmtime(tdb), INT_MAX, date);
  printf("modified time: %s\n", date);
  uint8_t opts = tctdbopts(tdb);
  printf("options:");
  if(opts & TDBTLARGE) printf(" large");
  if(opts & TDBTDEFLATE) printf(" deflate");
  if(opts & TDBTBZIP) printf(" bzip");
  if(opts & TDBTTCBS) printf(" tcbs");
  if(opts & TDBTEXCODEC) printf(" excodec");
  printf("\n");
  printf("record number: %llu\n", (unsigned long long)tctdbrnum(tdb));
  printf("file size: %llu\n", (unsigned long long)tctdbfsiz(tdb));
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform put command */
static int procput(const char *path, const char *pkbuf, int pksiz, TCMAP *cols,
                   int omode, int dmode){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  char pknumbuf[TCNUMBUFSIZ];
  if(pksiz < 1){
    pksiz = sprintf(pknumbuf, "%lld", (long long)tctdbgenuid(tdb));
    pkbuf = pknumbuf;
  }
  const char *vbuf;
  switch(dmode){
    case -1:
      if(!tctdbputkeep(tdb, pkbuf, pksiz, cols)){
        printerr(tdb);
        err = true;
      }
      break;
    case 1:
      if(!tctdbputcat(tdb, pkbuf, pksiz, cols)){
        printerr(tdb);
        err = true;
      }
      break;
    case 10:
      vbuf = tcmapget2(cols, "_num");
      if(!vbuf) vbuf = "1";
      if(tctdbaddint(tdb, pkbuf, pksiz, tcatoi(vbuf)) == INT_MIN){
        printerr(tdb);
        err = true;
      }
      break;
    case 11:
      vbuf = tcmapget2(cols, "_num");
      if(!vbuf) vbuf = "1.0";
      if(isnan(tctdbadddouble(tdb, pkbuf, pksiz, tcatof(vbuf)))){
        printerr(tdb);
        err = true;
      }
      break;
    default:
      if(!tctdbput(tdb, pkbuf, pksiz, cols)){
        printerr(tdb);
        err = true;
      }
      break;
  }
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform out command */
static int procout(const char *path, const char *pkbuf, int pksiz, int omode){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  if(!tctdbout(tdb, pkbuf, pksiz)){
    printerr(tdb);
    err = true;
  }
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform get command */
static int procget(const char *path, const char *pkbuf, int pksiz, int omode, bool px, bool pz){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  if(!tctdbopen(tdb, path, TDBOREADER | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
  if(cols){
    tcmapiterinit(cols);
    const char *kbuf;
    int ksiz;
    while((kbuf = tcmapiternext(cols, &ksiz)) != NULL){
      int vsiz;
      const char *vbuf = tcmapiterval(kbuf, &vsiz);
      printdata(kbuf, ksiz, px);
      putchar('\t');
      printdata(vbuf, vsiz, px);
      putchar(pz ? '\t' : '\n');
    }
    tcmapdel(cols);
  } else {
    printerr(tdb);
    err = true;
  }
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform list command */
static int proclist(const char *path, int omode, int max, bool pv, bool px, const char *fmstr){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  if(!tctdbopen(tdb, path, TDBOREADER | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  if(fmstr){
    TCLIST *pkeys = tctdbfwmkeys2(tdb, fmstr, max);
    for(int i = 0; i < tclistnum(pkeys); i++){
      int pksiz;
      const char *pkbuf = tclistval(pkeys, i, &pksiz);
      printdata(pkbuf, pksiz, px);
      if(pv){
        TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
        if(cols){
          tcmapiterinit(cols);
          const char *kbuf;
          int ksiz;
          while((kbuf = tcmapiternext(cols, &ksiz)) != NULL){
            int vsiz;
            const char *vbuf = tcmapiterval(kbuf, &vsiz);
            putchar('\t');
            printdata(kbuf, ksiz, px);
            putchar('\t');
            printdata(vbuf, vsiz, px);
          }
          tcmapdel(cols);
        }
      }
      putchar('\n');
    }
    tclistdel(pkeys);
  } else {
    if(!tctdbiterinit(tdb)){
      printerr(tdb);
      err = true;
    }
    int cnt = 0;
    TCMAP *cols;
    while((cols = tctdbiternext3(tdb)) != NULL){
      int pksiz;
      const char *pkbuf = tcmapget(cols, "", 0, &pksiz);
      if(pkbuf){
        printdata(pkbuf, pksiz, px);
        if(pv){
          tcmapiterinit(cols);
          const char *kbuf;
          int ksiz;
          while((kbuf = tcmapiternext(cols, &ksiz)) != NULL){
            if(*kbuf == '\0') continue;
            int vsiz;
            const char *vbuf = tcmapiterval(kbuf, &vsiz);
            putchar('\t');
            printdata(kbuf, ksiz, px);
            putchar('\t');
            printdata(vbuf, vsiz, px);
          }
        }
      }
      tcmapdel(cols);
      putchar('\n');
      if(max >= 0 && ++cnt >= max) break;
    }
  }
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform search command */
static int procsearch(const char *path, TCLIST *conds, const char *oname, const char *otype,
                      int omode, int max, int skip, bool pv, bool px, bool kw, bool ph, int bt,
                      bool rm, const char *mtype){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  if(!tctdbopen(tdb, path, (rm ? TDBOWRITER : TDBOREADER) | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  TDBQRY *qry = tctdbqrynew(tdb);
  int cnum = tclistnum(conds);
  for(int i = 0; i < cnum - 2; i += 3){
    const char *name = tclistval2(conds, i);
    const char *opstr = tclistval2(conds, i + 1);
    const char *expr  = tclistval2(conds, i + 2);
    int op = tctdbqrystrtocondop(opstr);
    if(op >= 0) tctdbqryaddcond(qry, name, op, expr);
  }
  if(oname){
    int type = tctdbqrystrtoordertype(otype);
    if(type >= 0) tctdbqrysetorder(qry, oname, type);
  }
  tctdbqrysetlimit(qry, max, skip);
  if(rm){
    double stime = tctime();
    if(!tctdbqrysearchout(qry)){
      printerr(tdb);
      err = true;
    }
    double etime = tctime();
    if(ph){
      TCLIST *hints = tcstrsplit(tctdbqryhint(qry), "\n");
      int hnum = tclistnum(hints);
      for(int i = 0; i < hnum; i++){
        const char *hint = tclistval2(hints, i);
        if(*hint == '\0') continue;
        printf("\t:::: %s\n", hint);
      }
      tclistdel(hints);
      printf("\t:::: number of records: %d\n", tctdbqrycount(qry));
      printf("\t:::: elapsed time: %.5f\n", etime - stime);
    }
  } else if(bt > 0){
    double sum = 0;
    for(int i = 1; i <= bt; i++){
      double stime = tctime();
      TCLIST *res = tctdbqrysearch(qry);
      double etime = tctime();
      tclistdel(res);
      printf("%d: %.5f sec.\n", i, etime - stime);
      sum += etime - stime;
    }
    printf("----\n");
    printf("total: %.5f sec. (%.5f s/q = %.5f q/s)\n", sum, sum / bt, bt / sum);
  } else {
    double stime = tctime();
    TCLIST *res;
    TCLIST *hints;
    int count;
    int mtnum = mtype ? tctdbmetastrtosettype(mtype) : -1;
    if(mtnum >= 0){
      TDBQRY *qrys[cnum/3+1];
      int qnum = 0;
      for(int i = 0; i < cnum - 2; i += 3){
        const char *name = tclistval2(conds, i);
        const char *opstr = tclistval2(conds, i + 1);
        const char *expr  = tclistval2(conds, i + 2);
        int op = tctdbqrystrtocondop(opstr);
        if(op >= 0){
          qrys[qnum] = tctdbqrynew(tdb);
          tctdbqryaddcond(qrys[qnum], name, op, expr);
          if(oname){
            int type = tctdbqrystrtoordertype(otype);
            if(type >= 0) tctdbqrysetorder(qrys[qnum], oname, type);
          }
          tctdbqrysetlimit(qrys[qnum], max, skip);
          qnum++;
        }
      }
      res = tctdbmetasearch(qrys, qnum, mtnum);
      hints = qnum > 0 ? tcstrsplit(tctdbqryhint(qrys[0]), "\n") : tclistnew2(1);
      count = qnum > 0 ? tctdbqrycount(qrys[0]) : 0;
      for(int i = 0; i < qnum; i++){
        tctdbqrydel(qrys[i]);
      }
    } else {
      res = tctdbqrysearch(qry);
      hints = tcstrsplit(tctdbqryhint(qry), "\n");
      count = tctdbqrycount(qry);
    }
    double etime = tctime();
    if(max < 0) max = INT_MAX;
    int rnum = tclistnum(res);
    for(int i = 0; i < rnum && max > 0; i++){
      int pksiz;
      const char *pkbuf = tclistval(res, i, &pksiz);
      if(kw){
        TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
        if(cols){
          TCLIST *texts = tctdbqrykwic(qry, cols, NULL, 16, TCKWMUTAB);
          int tnum = tclistnum(texts);
          for(int j = 0; j < tnum && max > 0; j++){
            int tsiz;
            const char *text = tclistval(texts, j, &tsiz);
            printdata(pkbuf, pksiz, px);
            putchar('\t');
            printdata(text, tsiz, px);
            putchar('\n');
            max--;
          }
          tclistdel(texts);
          tcmapdel(cols);
        }
      } else {
        printdata(pkbuf, pksiz, px);
        if(pv){
          TCMAP *cols = tctdbget(tdb, pkbuf, pksiz);
          if(cols){
            tcmapiterinit(cols);
            const char *kbuf;
            int ksiz;
            while((kbuf = tcmapiternext(cols, &ksiz)) != NULL){
              int vsiz;
              const char *vbuf = tcmapiterval(kbuf, &vsiz);
              putchar('\t');
              printdata(kbuf, ksiz, px);
              putchar('\t');
              printdata(vbuf, vsiz, px);
            }
            tcmapdel(cols);
          }
        }
        putchar('\n');
        max--;
      }
    }
    if(ph){
      int hnum = tclistnum(hints);
      for(int i = 0; i < hnum; i++){
        const char *hint = tclistval2(hints, i);
        if(*hint == '\0') continue;
        printf("\t:::: %s\n", hint);
      }
      printf("\t:::: number of records: %d\n", count);
      printf("\t:::: elapsed time: %.5f\n", etime - stime);
    }
    tclistdel(hints);
    tclistdel(res);
  }
  tctdbqrydel(qry);
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform optimize command */
static int procoptimize(const char *path, int bnum, int apow, int fpow, int opts, int omode,
                        bool df){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  int64_t msiz = 0;
  TCMAP *info = tcsysinfo();
  if(info){
    msiz = tcatoi(tcmapget4(info, "total", "0"));
    tcmapdel(info);
  }
  if(!tctdbsetinvcache(tdb, msiz >= (1 << 30) ? msiz / 4 : 0, 1.0)) printerr(tdb);
  if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  if(df){
    if(!tctdbdefrag(tdb, INT64_MAX)){
      printerr(tdb);
      err = true;
    }
  } else {
    if(!tctdboptimize(tdb, bnum, apow, fpow, opts)){
      printerr(tdb);
      err = true;
    }
  }
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform setindex command */
static int procsetindex(const char *path, const char *name, int omode, int type){
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  int64_t msiz = 0;
  TCMAP *info = tcsysinfo();
  if(info){
    msiz = tcatoi(tcmapget4(info, "total", "0"));
    tcmapdel(info);
  }
  if(!tctdbsetinvcache(tdb, msiz >= (1 << 30) ? msiz / 4 : 0, 1.0)) printerr(tdb);
  if(!tctdbopen(tdb, path, TDBOWRITER | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    return 1;
  }
  bool err = false;
  if(!tctdbsetindex(tdb, name, type)){
    printerr(tdb);
    err = true;
  }
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
  return err ? 1 : 0;
}


/* perform importtsv command */
static int procimporttsv(const char *path, const char *file, int omode, bool sc){
  FILE *ifp = file ? fopen(file, "rb") : stdin;
  if(!ifp){
    fprintf(stderr, "%s: could not open\n", file ? file : "(stdin)");
    return 1;
  }
  TCTDB *tdb = tctdbnew();
  if(g_dbgfd >= 0) tctdbsetdbgfd(tdb, g_dbgfd);
  if(!tctdbsetcodecfunc(tdb, _tc_recencode, NULL, _tc_recdecode, NULL)) printerr(tdb);
  int64_t msiz = 0;
  TCMAP *info = tcsysinfo();
  if(info){
    msiz = tcatoi(tcmapget4(info, "total", "0"));
    tcmapdel(info);
  }
  if(!tctdbsetinvcache(tdb, msiz >= (1 << 30) ? msiz / 4 : 0, 1.0)) printerr(tdb);
  if(!tctdbopen(tdb, path, TDBOWRITER | TDBOCREAT | omode)){
    printerr(tdb);
    tctdbdel(tdb);
    if(ifp != stdin) fclose(ifp);
    return 1;
  }
  bool err = false;
  char *line, numbuf[TCNUMBUFSIZ];
  int cnt = 0;
  while(!err && (line = mygetline(ifp)) != NULL){
    char *pv = strchr(line, '\t');
    if(!pv){
      tcfree(line);
      continue;
    }
    *pv = '\0';
    if(sc) tcstrutfnorm(line, TCUNSPACE | TCUNLOWER | TCUNNOACC | TCUNWIDTH);
    const char *pkey;
    if(*line == '\0'){
      sprintf(numbuf, "%lld", (long long)tctdbgenuid(tdb));
      pkey = numbuf;
    } else {
      pkey = line;
    }
    if(!tctdbput3(tdb, pkey, pv + 1)){
      printerr(tdb);
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
  if(!tctdbclose(tdb)){
    if(!err) printerr(tdb);
    err = true;
  }
  tctdbdel(tdb);
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
