/*************************************************************************************************
 * The command line utility of the fixed-length database API
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


/* global variables */
const char *g_progname;                  // program name
int g_dbgfd;                             // debugging output


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void printerr(TCFDB *fdb);
static int printdata(const char *ptr, int size, bool px);
static char *mygetline(FILE *ifp);
static int runcreate(int argc, char **argv);
static int runinform(int argc, char **argv);
static int runput(int argc, char **argv);
static int runout(int argc, char **argv);
static int runget(int argc, char **argv);
static int runlist(int argc, char **argv);
static int runoptimize(int argc, char **argv);
static int runimporttsv(int argc, char **argv);
static int runversion(int argc, char **argv);
static int proccreate(const char *path, int width, int64_t limsiz);
static int procinform(const char *path, int omode);
static int procput(const char *path, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
                   int omode, int dmode);
static int procout(const char *path, const char *kbuf, int ksiz, int omode);
static int procget(const char *path, const char *kbuf, int ksiz, int omode, bool px, bool pz);
static int proclist(const char *path, int omode, int max, bool pv, bool px,
                    const char *rlstr, const char *rustr, const char *ristr);
static int procoptimize(const char *path, int width, int64_t limsiz, int omode);
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
  fprintf(stderr, "%s: the command line utility of the fixed-length database API\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s create path [width [limsiz]]\n", g_progname);
  fprintf(stderr, "  %s inform [-nl|-nb] path\n", g_progname);
  fprintf(stderr, "  %s put [-nl|-nb] [-sx] [-dk|-dc|-dai|-dad] path key value\n", g_progname);
  fprintf(stderr, "  %s out [-nl|-nb] [-sx] path key\n", g_progname);
  fprintf(stderr, "  %s get [-nl|-nb] [-sx] [-px] [-pz] path key\n", g_progname);
  fprintf(stderr, "  %s list [-nl|-nb] [-m num] [-pv] [-px] [-rb lkey ukey] [-ri str] path\n",
          g_progname);
  fprintf(stderr, "  %s optimize [-nl|-nb] path [width [limsiz]]\n", g_progname);
  fprintf(stderr, "  %s importtsv [-nl|-nb] [-sc] path [file]\n", g_progname);
  fprintf(stderr, "  %s version\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print error information */
static void printerr(TCFDB *fdb){
  const char *path = tcfdbpath(fdb);
  int ecode = tcfdbecode(fdb);
  fprintf(stderr, "%s: %s: %d: %s\n", g_progname, path ? path : "-", ecode, tcfdberrmsg(ecode));
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


/* parse arguments of create command */
static int runcreate(int argc, char **argv){
  char *path = NULL;
  char *wstr = NULL;
  char *lstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      usage();
    } else if(!path){
      path = argv[i];
    } else if(!wstr){
      wstr = argv[i];
    } else if(!lstr){
      lstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path) usage();
  int width = wstr ? tcatoix(wstr) : -1;
  int64_t limsiz = lstr ? tcatoix(lstr) : -1;
  int rv = proccreate(path, width, limsiz);
  return rv;
}


/* parse arguments of inform command */
static int runinform(int argc, char **argv){
  char *path = NULL;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int omode = 0;
  int dmode = 0;
  bool sx = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int rv = procput(path, kbuf, ksiz, vbuf, vsiz, omode, dmode);
  tcfree(vbuf);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of out command */
static int runout(int argc, char **argv){
  char *path = NULL;
  char *key = NULL;
  int omode = 0;
  bool sx = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int rv = procout(path, kbuf, ksiz, omode);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of get command */
static int runget(int argc, char **argv){
  char *path = NULL;
  char *key = NULL;
  int omode = 0;
  bool sx = false;
  bool px = false;
  bool pz = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
  int rv = procget(path, kbuf, ksiz, omode, px, pz);
  tcfree(kbuf);
  return rv;
}


/* parse arguments of list command */
static int runlist(int argc, char **argv){
  char *path = NULL;
  int omode = 0;
  int max = -1;
  bool pv = false;
  bool px = false;
  char *rlstr = NULL;
  char *rustr = NULL;
  char *ristr = NULL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
      } else if(!strcmp(argv[i], "-m")){
        if(++i >= argc) usage();
        max = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-pv")){
        pv = true;
      } else if(!strcmp(argv[i], "-px")){
        px = true;
      } else if(!strcmp(argv[i], "-rb")){
        if(++i >= argc) usage();
        rlstr = argv[i];
        if(++i >= argc) usage();
        rustr = argv[i];
      } else if(!strcmp(argv[i], "-ri")){
        if(++i >= argc) usage();
        ristr = argv[i];
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
  int rv = proclist(path, omode, max, pv, px, rlstr, rustr, ristr);
  return rv;
}


/* parse arguments of optimize command */
static int runoptimize(int argc, char **argv){
  char *path = NULL;
  char *wstr = NULL;
  char *lstr = NULL;
  int omode = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-nl")){
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else if(!wstr){
      wstr = argv[i];
    } else if(!lstr){
      lstr = argv[i];
    } else {
      usage();
    }
  }
  if(!path) usage();
  int width = wstr ? tcatoix(wstr) : -1;
  int64_t limsiz = lstr ? tcatoix(lstr) : -1;
  int rv = procoptimize(path, width, limsiz, omode);
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
        omode |= FDBONOLCK;
      } else if(!strcmp(argv[i], "-nb")){
        omode |= FDBOLCKNB;
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
static int proccreate(const char *path, int width, int64_t limsiz){
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbtune(fdb, width, limsiz)){
    printerr(fdb);
    tcfdbdel(fdb);
    return 1;
  }
  if(!tcfdbopen(fdb, path, FDBOWRITER | FDBOCREAT | FDBOTRUNC)){
    printerr(fdb);
    tcfdbdel(fdb);
    return 1;
  }
  bool err = false;
  if(!tcfdbclose(fdb)){
    printerr(fdb);
    err = true;
  }
  tcfdbdel(fdb);
  return err ? 1 : 0;
}


/* perform inform command */
static int procinform(const char *path, int omode){
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbopen(fdb, path, FDBOREADER | omode)){
    printerr(fdb);
    tcfdbdel(fdb);
    return 1;
  }
  bool err = false;
  const char *npath = tcfdbpath(fdb);
  if(!npath) npath = "(unknown)";
  printf("path: %s\n", npath);
  const char *type = "(unknown)";
  switch(tcfdbtype(fdb)){
    case TCDBTHASH: type = "hash"; break;
    case TCDBTBTREE: type = "btree"; break;
    case TCDBTFIXED: type = "fixed"; break;
    case TCDBTTABLE: type = "table"; break;
  }
  printf("database type: %s\n", type);
  uint8_t flags = tcfdbflags(fdb);
  printf("additional flags:");
  if(flags & FDBFOPEN) printf(" open");
  if(flags & FDBFFATAL) printf(" fatal");
  printf("\n");
  printf("minimum ID number: %llu\n", (unsigned long long)tcfdbmin(fdb));
  printf("maximum ID number: %llu\n", (unsigned long long)tcfdbmax(fdb));
  printf("width of the value: %u\n", (unsigned int)tcfdbwidth(fdb));
  printf("limit file size: %llu\n", (unsigned long long)tcfdblimsiz(fdb));
  printf("limit ID number: %llu\n", (unsigned long long)tcfdblimid(fdb));
  printf("inode number: %lld\n", (long long)tcfdbinode(fdb));
  char date[48];
  tcdatestrwww(tcfdbmtime(fdb), INT_MAX, date);
  printf("modified time: %s\n", date);
  printf("record number: %llu\n", (unsigned long long)tcfdbrnum(fdb));
  printf("file size: %llu\n", (unsigned long long)tcfdbfsiz(fdb));
  if(!tcfdbclose(fdb)){
    if(!err) printerr(fdb);
    err = true;
  }
  tcfdbdel(fdb);
  return err ? 1 : 0;
}


/* perform put command */
static int procput(const char *path, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
                   int omode, int dmode){
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbopen(fdb, path, FDBOWRITER | omode)){
    printerr(fdb);
    tcfdbdel(fdb);
    return 1;
  }
  bool err = false;
  int inum;
  double dnum;
  switch(dmode){
    case -1:
      if(!tcfdbputkeep2(fdb, kbuf, ksiz, vbuf, vsiz)){
        printerr(fdb);
        err = true;
      }
      break;
    case 1:
      if(!tcfdbputcat2(fdb, kbuf, ksiz, vbuf, vsiz)){
        printerr(fdb);
        err = true;
      }
      break;
    case 10:
      inum = tcfdbaddint(fdb, tcfdbkeytoid(kbuf, ksiz), tcatoi(vbuf));
      if(inum == INT_MIN){
        printerr(fdb);
        err = true;
      } else {
        printf("%d\n", inum);
      }
      break;
    case 11:
      dnum = tcfdbadddouble(fdb, tcfdbkeytoid(kbuf, ksiz), tcatof(vbuf));
      if(isnan(dnum)){
        printerr(fdb);
        err = true;
      } else {
        printf("%.6f\n", dnum);
      }
      break;
    default:
      if(!tcfdbput2(fdb, kbuf, ksiz, vbuf, vsiz)){
        printerr(fdb);
        err = true;
      }
      break;
  }
  if(!tcfdbclose(fdb)){
    if(!err) printerr(fdb);
    err = true;
  }
  tcfdbdel(fdb);
  return err ? 1 : 0;
}


/* perform out command */
static int procout(const char *path, const char *kbuf, int ksiz, int omode){
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbopen(fdb, path, FDBOWRITER | omode)){
    printerr(fdb);
    tcfdbdel(fdb);
    return 1;
  }
  bool err = false;
  if(!tcfdbout2(fdb, kbuf, ksiz)){
    printerr(fdb);
    err = true;
  }
  if(!tcfdbclose(fdb)){
    if(!err) printerr(fdb);
    err = true;
  }
  tcfdbdel(fdb);
  return err ? 1 : 0;
}


/* perform get command */
static int procget(const char *path, const char *kbuf, int ksiz, int omode, bool px, bool pz){
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbopen(fdb, path, FDBOREADER | omode)){
    printerr(fdb);
    tcfdbdel(fdb);
    return 1;
  }
  bool err = false;
  int vsiz;
  char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
  if(vbuf){
    printdata(vbuf, vsiz, px);
    if(!pz) putchar('\n');
    tcfree(vbuf);
  } else {
    printerr(fdb);
    err = true;
  }
  if(!tcfdbclose(fdb)){
    if(!err) printerr(fdb);
    err = true;
  }
  tcfdbdel(fdb);
  return err ? 1 : 0;
}


/* perform list command */
static int proclist(const char *path, int omode, int max, bool pv, bool px,
                    const char *rlstr, const char *rustr, const char *ristr){
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbopen(fdb, path, FDBOREADER | omode)){
    printerr(fdb);
    tcfdbdel(fdb);
    return 1;
  }
  bool err = false;
  if(rlstr || ristr){
    TCLIST *keys = ristr ? tcfdbrange5(fdb, ristr, max) : tcfdbrange3(fdb, rlstr, rustr, max);
    for(int i = 0; i < tclistnum(keys); i++){
      int ksiz;
      const char *kbuf = tclistval(keys, i, &ksiz);
      printf("%s", kbuf);
      if(pv){
        int vsiz;
        char *vbuf = tcfdbget2(fdb, kbuf, ksiz, &vsiz);
        if(vbuf){
          putchar('\t');
          printdata(vbuf, vsiz, px);
          tcfree(vbuf);
        }
      }
      putchar('\n');
    }
    tclistdel(keys);
  } else {
    if(!tcfdbiterinit(fdb)){
      printerr(fdb);
      err = true;
    }
    int cnt = 0;
    uint64_t id;
    while((id = tcfdbiternext(fdb)) > 0){
      printf("%llu", (unsigned long long)id);
      if(pv){
        int vsiz;
        char *vbuf = tcfdbget(fdb, id, &vsiz);
        if(vbuf){
          putchar('\t');
          printdata(vbuf, vsiz, px);
          tcfree(vbuf);
        }
      }
      putchar('\n');
      if(max >= 0 && ++cnt >= max) break;
    }
  }
  if(!tcfdbclose(fdb)){
    if(!err) printerr(fdb);
    err = true;
  }
  tcfdbdel(fdb);
  return err ? 1 : 0;
}


/* perform optimize command */
static int procoptimize(const char *path, int width, int64_t limsiz, int omode){
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbopen(fdb, path, FDBOWRITER | omode)){
    printerr(fdb);
    tcfdbdel(fdb);
    return 1;
  }
  bool err = false;
  if(!tcfdboptimize(fdb, width, limsiz)){
    printerr(fdb);
    err = true;
  }
  if(!tcfdbclose(fdb)){
    if(!err) printerr(fdb);
    err = true;
  }
  tcfdbdel(fdb);
  return err ? 1 : 0;
}


/* perform importtsv command */
static int procimporttsv(const char *path, const char *file, int omode, bool sc){
  FILE *ifp = file ? fopen(file, "rb") : stdin;
  if(!ifp){
    fprintf(stderr, "%s: could not open\n", file ? file : "(stdin)");
    return 1;
  }
  TCFDB *fdb = tcfdbnew();
  if(g_dbgfd >= 0) tcfdbsetdbgfd(fdb, g_dbgfd);
  if(!tcfdbopen(fdb, path, FDBOWRITER | FDBOCREAT | omode)){
    printerr(fdb);
    tcfdbdel(fdb);
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
    if(!tcfdbput3(fdb, line, pv + 1)){
      printerr(fdb);
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
  if(!tcfdbclose(fdb)){
    if(!err) printerr(fdb);
    err = true;
  }
  tcfdbdel(fdb);
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
