/*************************************************************************************************
 * Popular encoders and decoders of the utility API
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


/* global variables */
const char *g_progname;                  // program name


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void eprintf(const char *format, ...);
static int runurl(int argc, char **argv);
static int runbase(int argc, char **argv);
static int runquote(int argc, char **argv);
static int runmime(int argc, char **argv);
static int runhex(int argc, char **argv);
static int runpack(int argc, char **argv);
static int runtcbs(int argc, char **argv);
static int runzlib(int argc, char **argv);
static int runbzip(int argc, char **argv);
static int runxml(int argc, char **argv);
static int runcstr(int argc, char **argv);
static int runucs(int argc, char **argv);
static int runhash(int argc, char **argv);
static int runcipher(int argc, char **argv);
static int rundate(int argc, char **argv);
static int runtmpl(int argc, char **argv);
static int runconf(int argc, char **argv);
static int procurl(const char *ibuf, int isiz, bool dec, bool br, const char *base);
static int procbase(const char *ibuf, int isiz, bool dec);
static int procquote(const char *ibuf, int isiz, bool dec);
static int procmime(const char *ibuf, int isiz, bool dec, const char *ename, bool qb, bool on,
                    bool hd, bool bd, int part);
static int prochex(const char *ibuf, int isiz, bool dec);
static int procpack(const char *ibuf, int isiz, bool dec, bool bwt);
static int proctcbs(const char *ibuf, int isiz, bool dec);
static int proczlib(const char *ibuf, int isiz, bool dec, bool gz);
static int procbzip(const char *ibuf, int isiz, bool dec);
static int procxml(const char *ibuf, int isiz, bool dec, bool br);
static int proccstr(const char *ibuf, int isiz, bool dec, bool js);
static int procucs(const char *ibuf, int isiz, bool dec, bool un, const char *kw);
static int prochash(const char *ibuf, int isiz, bool crc, int ch);
static int proccipher(const char *ibuf, int isiz, const char *key);
static int procdate(const char *str, int jl, bool wf, bool rf);
static int proctmpl(const char *ibuf, int isiz, TCMAP *vars);
static int procconf(int mode);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "url")){
    rv = runurl(argc, argv);
  } else if(!strcmp(argv[1], "base")){
    rv = runbase(argc, argv);
  } else if(!strcmp(argv[1], "quote")){
    rv = runquote(argc, argv);
  } else if(!strcmp(argv[1], "mime")){
    rv = runmime(argc, argv);
  } else if(!strcmp(argv[1], "hex")){
    rv = runhex(argc, argv);
  } else if(!strcmp(argv[1], "pack")){
    rv = runpack(argc, argv);
  } else if(!strcmp(argv[1], "tcbs")){
    rv = runtcbs(argc, argv);
  } else if(!strcmp(argv[1], "zlib")){
    rv = runzlib(argc, argv);
  } else if(!strcmp(argv[1], "bzip")){
    rv = runbzip(argc, argv);
  } else if(!strcmp(argv[1], "xml")){
    rv = runxml(argc, argv);
  } else if(!strcmp(argv[1], "cstr")){
    rv = runcstr(argc, argv);
  } else if(!strcmp(argv[1], "ucs")){
    rv = runucs(argc, argv);
  } else if(!strcmp(argv[1], "hash")){
    rv = runhash(argc, argv);
  } else if(!strcmp(argv[1], "cipher")){
    rv = runcipher(argc, argv);
  } else if(!strcmp(argv[1], "date")){
    rv = rundate(argc, argv);
  } else if(!strcmp(argv[1], "tmpl")){
    rv = runtmpl(argc, argv);
  } else if(!strcmp(argv[1], "conf")){
    rv = runconf(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
static void usage(void){
  fprintf(stderr, "%s: popular encoders and decoders of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s url [-d] [-br] [-rs base] [file]\n", g_progname);
  fprintf(stderr, "  %s base [-d] [file]\n", g_progname);
  fprintf(stderr, "  %s quote [-d] [file]\n", g_progname);
  fprintf(stderr, "  %s mime [-d] [-en name] [-q] [-on] [-hd] [-bd] [-part num] [file]\n",
          g_progname);
  fprintf(stderr, "  %s hex [-d] [file]\n", g_progname);
  fprintf(stderr, "  %s pack [-d] [-bwt] [file]\n", g_progname);
  fprintf(stderr, "  %s tcbs [-d] [file]\n", g_progname);
  fprintf(stderr, "  %s zlib [-d] [-gz] [file]\n", g_progname);
  fprintf(stderr, "  %s bzip [-d] [file]\n", g_progname);
  fprintf(stderr, "  %s xml [-d] [-br] [file]\n", g_progname);
  fprintf(stderr, "  %s cstr [-d] [-js] [file]\n", g_progname);
  fprintf(stderr, "  %s ucs [-d] [-un] [file]\n", g_progname);
  fprintf(stderr, "  %s hash [-crc] [-ch num] [file]\n", g_progname);
  fprintf(stderr, "  %s cipher [-key str] [file]\n", g_progname);
  fprintf(stderr, "  %s date [-ds str] [-jl num] [-wf] [-rf]\n", g_progname);
  fprintf(stderr, "  %s tmpl [-var name val] [file]\n", g_progname);
  fprintf(stderr, "  %s conf [-v|-i|-l|-p]\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print formatted error string */
static void eprintf(const char *format, ...){
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s: ", g_progname);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}


/* parse arguments of url command */
static int runurl(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  bool br = false;
  char *base = NULL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else if(!strcmp(argv[i], "-br")){
        br = true;
      } else if(!strcmp(argv[i], "-rs")){
        if(++i >= argc) usage();
        base = argv[i];
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = procurl(ibuf, isiz, dec, br, base);
  if(path && path[0] == '@' && !br) printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of base command */
static int runbase(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = procbase(ibuf, isiz, dec);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of quote command */
static int runquote(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = procquote(ibuf, isiz, dec);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of mime command */
static int runmime(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  char *ename = NULL;
  bool qb = false;
  bool on = false;
  bool hd = false;
  bool bd = false;
  int part = -1;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else if(!strcmp(argv[i], "-en")){
        if(++i >= argc) usage();
        ename = argv[i];
      } else if(!strcmp(argv[i], "-q")){
        qb = true;
      } else if(!strcmp(argv[i], "-on")){
        on = true;
      } else if(!strcmp(argv[i], "-hd")){
        hd = true;
      } else if(!strcmp(argv[i], "-bd")){
        bd = true;
      } else if(!strcmp(argv[i], "-part")){
        if(++i >= argc) usage();
        part = tcatoix(argv[i]);
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  if(!ename) ename = "UTF-8";
  int rv = procmime(ibuf, isiz, dec, ename, qb, on, hd, bd, part);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of hex command */
static int runhex(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = prochex(ibuf, isiz, dec);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of pack command */
static int runpack(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  bool bwt = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else if(!strcmp(argv[i], "-bwt")){
        bwt = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = procpack(ibuf, isiz, dec, bwt);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of tcbs command */
static int runtcbs(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = proctcbs(ibuf, isiz, dec);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of zlib command */
static int runzlib(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  bool gz = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else if(!strcmp(argv[i], "-gz")){
        gz = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = proczlib(ibuf, isiz, dec, gz);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of bzip command */
static int runbzip(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = procbzip(ibuf, isiz, dec);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of xml command */
static int runxml(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  bool br = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else if(!strcmp(argv[i], "-br")){
        br = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = procxml(ibuf, isiz, dec, br);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of cstr command */
static int runcstr(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  bool js = false;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else if(!strcmp(argv[i], "-js")){
        js = true;
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = proccstr(ibuf, isiz, dec, js);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of ucs command */
static int runucs(int argc, char **argv){
  char *path = NULL;
  bool dec = false;
  bool un = false;
  char *kw = NULL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-d")){
        dec = true;
      } else if(!strcmp(argv[i], "-un")){
        un = true;
      } else if(!strcmp(argv[i], "-kw")){
        if(++i >= argc) usage();
        kw = argv[i];
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = procucs(ibuf, isiz, dec, un, kw);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of hash command */
static int runhash(int argc, char **argv){
  char *path = NULL;
  bool crc = false;
  int ch = 0;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-crc")){
        crc = true;
      } else if(!strcmp(argv[i], "-ch")){
        if(++i >= argc) usage();
        ch = tcatoix(argv[i]);
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = prochash(ibuf, isiz, crc, ch);
  tcfree(ibuf);
  return rv;
}


/* parse arguments of cipher command */
static int runcipher(int argc, char **argv){
  char *path = NULL;
  char *key = NULL;
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-key")){
        if(++i >= argc) usage();
        key = argv[i];
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = proccipher(ibuf, isiz, key);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of date command */
static int rundate(int argc, char **argv){
  char *str = NULL;
  int jl = INT_MAX;
  bool wf = false;
  bool rf = false;
  for(int i = 2; i < argc; i++){
    if(argv[i][0] == '-'){
      if(!strcmp(argv[i], "-ds")){
        if(++i >= argc) usage();
        str = argv[i];
      } else if(!strcmp(argv[i], "-jl")){
        if(++i >= argc) usage();
        jl = tcatoix(argv[i]);
      } else if(!strcmp(argv[i], "-wf")){
        wf = true;
      } else if(!strcmp(argv[i], "-rf")){
        rf = true;
      } else {
        usage();
      }
    } else {
      usage();
    }
  }
  int rv = procdate(str, jl, wf, rf);
  return rv;
}


/* parse arguments of tmpl command */
static int runtmpl(int argc, char **argv){
  char *path = NULL;
  TCMAP *vars = tcmpoolmapnew(tcmpoolglobal());
  for(int i = 2; i < argc; i++){
    if(!path && argv[i][0] == '-'){
      if(!strcmp(argv[i], "-var")){
        if(++i >= argc) usage();
        const char *name = argv[i];
        if(++i >= argc) usage();
        const char *value = argv[i];
        tcmapput2(vars, name, value);
      } else {
        usage();
      }
    } else if(!path){
      path = argv[i];
    } else {
      usage();
    }
  }
  char *ibuf;
  int isiz;
  if(path && path[0] == '@'){
    isiz = strlen(path) - 1;
    ibuf = tcmemdup(path + 1, isiz);
  } else {
    ibuf = tcreadfile(path, -1, &isiz);
  }
  if(!ibuf){
    eprintf("%s: cannot open", path ? path : "(stdin)");
    return 1;
  }
  int rv = proctmpl(ibuf, isiz, vars);
  if(path && path[0] == '@') printf("\n");
  tcfree(ibuf);
  return rv;
}


/* parse arguments of conf command */
static int runconf(int argc, char **argv){
  int mode = 0;
  for(int i = 2; i < argc; i++){
    if(argv[i][0] == '-'){
      if(!strcmp(argv[i], "-v")){
        mode = 'v';
      } else if(!strcmp(argv[i], "-i")){
        mode = 'i';
      } else if(!strcmp(argv[i], "-l")){
        mode = 'l';
      } else if(!strcmp(argv[i], "-p")){
        mode = 'p';
      } else {
        usage();
      }
    } else {
      usage();
    }
  }
  int rv = procconf(mode);
  return rv;
}


/* perform url command */
static int procurl(const char *ibuf, int isiz, bool dec, bool br, const char *base){
  if(base){
    char *obuf = tcurlresolve(base, ibuf);
    printf("%s", obuf);
    tcfree(obuf);
  } else if(br){
    TCMAP *elems = tcurlbreak(ibuf);
    const char *elem;
    if((elem = tcmapget2(elems, "self")) != NULL) printf("self: %s\n", elem);
    if((elem = tcmapget2(elems, "scheme")) != NULL) printf("scheme: %s\n", elem);
    if((elem = tcmapget2(elems, "host")) != NULL) printf("host: %s\n", elem);
    if((elem = tcmapget2(elems, "port")) != NULL) printf("port: %s\n", elem);
    if((elem = tcmapget2(elems, "authority")) != NULL) printf("authority: %s\n", elem);
    if((elem = tcmapget2(elems, "path")) != NULL) printf("path: %s\n", elem);
    if((elem = tcmapget2(elems, "file")) != NULL) printf("file: %s\n", elem);
    if((elem = tcmapget2(elems, "query")) != NULL) printf("query: %s\n", elem);
    if((elem = tcmapget2(elems, "fragment")) != NULL) printf("fragment: %s\n", elem);
    tcmapdel(elems);
  } else if(dec){
    int osiz;
    char *obuf = tcurldecode(ibuf, &osiz);
    fwrite(obuf, 1, osiz, stdout);
    tcfree(obuf);
  } else {
    char *obuf = tcurlencode(ibuf, isiz);
    fwrite(obuf, 1, strlen(obuf), stdout);
    tcfree(obuf);
  }
  return 0;
}


/* perform base command */
static int procbase(const char *ibuf, int isiz, bool dec){
  if(dec){
    int osiz;
    char *obuf = tcbasedecode(ibuf, &osiz);
    fwrite(obuf, 1, osiz, stdout);
    tcfree(obuf);
  } else {
    char *obuf = tcbaseencode(ibuf, isiz);
    fwrite(obuf, 1, strlen(obuf), stdout);
    tcfree(obuf);
  }
  return 0;
}


/* perform quote command */
static int procquote(const char *ibuf, int isiz, bool dec){
  if(dec){
    int osiz;
    char *obuf = tcquotedecode(ibuf, &osiz);
    fwrite(obuf, 1, osiz, stdout);
    tcfree(obuf);
  } else {
    char *obuf = tcquoteencode(ibuf, isiz);
    fwrite(obuf, 1, strlen(obuf), stdout);
    tcfree(obuf);
  }
  return 0;
}


/* perform mime command */
static int procmime(const char *ibuf, int isiz, bool dec, const char *ename, bool qb, bool on,
                    bool hd, bool bd, int part){
  if(hd || bd || part > 0){
    TCMAP *hmap = tcmapnew2(16);
    int osiz;
    char *obuf = tcmimebreak(ibuf, isiz, hmap, &osiz);
    if(part > 0){
      const char *value;
      if(!(value = tcmapget2(hmap, "TYPE")) || !tcstrifwm(value, "multipart/") ||
         !(value = tcmapget2(hmap, "BOUNDARY"))){
        eprintf("not multipart");
      } else {
        TCLIST *parts = tcmimeparts(obuf, osiz, value);
        if(part <= tclistnum(parts)){
          int bsiz;
          const char *body = tclistval(parts, part - 1, &bsiz);
          fwrite(body, 1, bsiz, stdout);
        } else {
          eprintf("no such part");
        }
        tclistdel(parts);
      }
    } else {
      if(hd){
        TCLIST *names = tcmapkeys(hmap);
        tclistsort(names);
        int num = tclistnum(names);
        for(int i = 0; i < num; i++){
          const char *name = tclistval2(names, i);
          printf("%s\t%s\n", name, tcmapget2(hmap, name));
        }
        tclistdel(names);
        if(bd) putchar('\n');
      }
      if(bd) fwrite(obuf, 1, osiz, stdout);
    }
    tcfree(obuf);
    tcmapdel(hmap);
  } else if(dec){
    char ebuf[32];
    char *obuf = tcmimedecode(ibuf, ebuf);
    if(on){
      fwrite(ebuf, 1, strlen(ebuf), stdout);
    } else {
      fwrite(obuf, 1, strlen(obuf), stdout);
    }
    tcfree(obuf);
  } else {
    char *obuf = tcmimeencode(ibuf, ename, !qb);
    fwrite(obuf, 1, strlen(obuf), stdout);
    tcfree(obuf);
  }
  return 0;
}


/* perform hex command */
static int prochex(const char *ibuf, int isiz, bool dec){
  if(dec){
    int osiz;
    char *obuf = tchexdecode(ibuf, &osiz);
    fwrite(obuf, 1, osiz, stdout);
    tcfree(obuf);
  } else {
    char *obuf = tchexencode(ibuf, isiz);
    fwrite(obuf, 1, strlen(obuf), stdout);
    tcfree(obuf);
  }
  return 0;
}


/* perform pack command */
static int procpack(const char *ibuf, int isiz, bool dec, bool bwt){
  if(dec){
    int osiz;
    char *obuf = tcpackdecode(ibuf, isiz, &osiz);
    if(bwt && osiz > 0){
      int idx, step;
      TCREADVNUMBUF(obuf, idx, step);
      char *tbuf = tcbwtdecode(obuf + step, osiz - step, idx);
      fwrite(tbuf, 1, osiz - step, stdout);
      tcfree(tbuf);
    } else {
      fwrite(obuf, 1, osiz, stdout);
    }
    tcfree(obuf);
  } else {
    char *tbuf = NULL;
    if(bwt){
      int idx;
      tbuf = tcbwtencode(ibuf, isiz, &idx);
      char vnumbuf[sizeof(int)+1];
      int step;
      TCSETVNUMBUF(step, vnumbuf, idx);
      tbuf = tcrealloc(tbuf, isiz + step + 1);
      memmove(tbuf + step, tbuf, isiz);
      memcpy(tbuf, vnumbuf, step);
      isiz += step;
      ibuf = tbuf;
    }
    int osiz;
    char *obuf = tcpackencode(ibuf, isiz, &osiz);
    fwrite(obuf, 1, osiz, stdout);
    tcfree(obuf);
    tcfree(tbuf);
  }
  return 0;
}


/* perform tcbs command */
static int proctcbs(const char *ibuf, int isiz, bool dec){
  if(dec){
    int osiz;
    char *obuf = tcbsdecode(ibuf, isiz, &osiz);
    fwrite(obuf, 1, osiz, stdout);
    tcfree(obuf);
  } else {
    int osiz;
    char *obuf = tcbsencode(ibuf, isiz, &osiz);
    fwrite(obuf, 1, osiz, stdout);
    tcfree(obuf);
  }
  return 0;
}


/* perform zlib command */
static int proczlib(const char *ibuf, int isiz, bool dec, bool gz){
  if(dec){
    int osiz;
    char *obuf = gz ? tcgzipdecode(ibuf, isiz, &osiz) : tcinflate(ibuf, isiz, &osiz);
    if(obuf){
      fwrite(obuf, 1, osiz, stdout);
      tcfree(obuf);
    } else {
      eprintf("inflate failure");
    }
  } else {
    int osiz;
    char *obuf = gz ? tcgzipencode(ibuf, isiz, &osiz) : tcdeflate(ibuf, isiz, &osiz);
    if(obuf){
      fwrite(obuf, 1, osiz, stdout);
      tcfree(obuf);
    } else {
      eprintf("deflate failure");
    }
  }
  return 0;
}


/* perform bzip command */
static int procbzip(const char *ibuf, int isiz, bool dec){
  if(dec){
    int osiz;
    char *obuf = tcbzipdecode(ibuf, isiz, &osiz);
    if(obuf){
      fwrite(obuf, 1, osiz, stdout);
      tcfree(obuf);
    } else {
      eprintf("inflate failure");
    }
  } else {
    int osiz;
    char *obuf = tcbzipencode(ibuf, isiz, &osiz);
    if(obuf){
      fwrite(obuf, 1, osiz, stdout);
      tcfree(obuf);
    } else {
      eprintf("deflate failure");
    }
  }
  return 0;
}


/* perform xml command */
static int procxml(const char *ibuf, int isiz, bool dec, bool br){
  if(br){
    TCLIST *elems = tcxmlbreak(ibuf);
    for(int i = 0; i < tclistnum(elems); i++){
      int esiz;
      const char *elem = tclistval(elems, i, &esiz);
      char *estr = tcmemdup(elem, esiz);
      tcstrsubchr(estr, "\t\n\r", "  ");
      tcstrtrim(estr);
      if(*estr != '\0'){
        if(*elem == '<'){
          if(tcstrfwm(estr, "<!--")){
            printf("COMMENT\t%s\n", estr);
          } else if(tcstrfwm(estr, "<![CDATA[")){
            printf("CDATA\t%s\n", estr);
          } else if(tcstrfwm(estr, "<!")){
            printf("DECL\t%s\n", estr);
          } else if(tcstrfwm(estr, "<?")){
            printf("XMLDECL\t%s\n", estr);
          } else {
            TCMAP *attrs = tcxmlattrs(estr);
            tcmapiterinit(attrs);
            const char *name;
            if((name = tcmapget2(attrs, "")) != NULL){
              if(tcstrfwm(estr, "</")){
                printf("END");
              } else if(tcstrbwm(estr, "/>")){
                printf("EMPTY");
              } else {
                printf("BEGIN");
              }
              printf("\t%s", name);
              while((name = tcmapiternext2(attrs)) != NULL){
                if(*name == '\0') continue;
                printf("\t%s\t%s", name, tcmapiterval2(name));
              }
              printf("\n");
            }
            tcmapdel(attrs);
          }
        } else {
          char *dstr = tcxmlunescape(estr);
          printf("TEXT\t%s\n", dstr);
          tcfree(dstr);
        }
      }
      tcfree(estr);
    }
    tclistdel(elems);
  } else if(dec){
    char *obuf = tcxmlunescape(ibuf);
    fwrite(obuf, 1, strlen(obuf), stdout);
    tcfree(obuf);
  } else {
    char *obuf = tcxmlescape(ibuf);
    fwrite(obuf, 1, strlen(obuf), stdout);
    tcfree(obuf);
  }
  return 0;
}


/* perform cstr command */
static int proccstr(const char *ibuf, int isiz, bool dec, bool js){
  if(js){
    if(dec){
      char *ostr = tcjsonunescape(ibuf);
      printf("%s", ostr);
      tcfree(ostr);
    } else {
      char *ostr = tcjsonescape(ibuf);
      printf("%s", ostr);
      tcfree(ostr);
    }
  } else {
    if(dec){
      char *ostr = tccstrunescape(ibuf);
      printf("%s", ostr);
      tcfree(ostr);
    } else {
      char *ostr = tccstrescape(ibuf);
      printf("%s", ostr);
      tcfree(ostr);
    }
  }
  return 0;
}


/* perform ucs command */
static int procucs(const char *ibuf, int isiz, bool dec, bool un, const char *kw){
  if(un){
    uint16_t *ary = tcmalloc(isiz * sizeof(uint16_t) + 1);
    int anum;
    tcstrutftoucs(ibuf, ary, &anum);
    anum = tcstrucsnorm(ary, anum, TCUNSPACE | TCUNLOWER | TCUNNOACC | TCUNWIDTH);
    char *str = tcmalloc(anum * 3 + 1);
    tcstrucstoutf(ary, anum, str);
    printf("%s", str);
    tcfree(str);
    tcfree(ary);
  } else if(kw){
    TCLIST *words = tcstrtokenize(kw);
    TCLIST *texts = tcstrkwic(ibuf, words, 10, TCKWMUTAB);
    for(int i = 0; i < tclistnum(texts); i++){
      printf("%s\n", tclistval2(texts, i));
    }
    tclistdel(texts);
    tclistdel(words);
  } else if(dec){
    uint16_t *ary = tcmalloc(isiz + 1);
    int anum = 0;
    for(int i = 0; i < isiz; i += 2){
      ary[anum++] = (((unsigned char *)ibuf)[i] << 8) + ((unsigned char *)ibuf)[i+1];
    }
    char *str = tcmalloc(isiz * 3 + 1);
    tcstrucstoutf(ary, anum, str);
    printf("%s", str);
    tcfree(str);
    tcfree(ary);
  } else {
    uint16_t *ary = tcmalloc(isiz * sizeof(uint16_t) + 1);
    int anum;
    tcstrutftoucs(ibuf, ary, &anum);
    for(int i = 0; i < anum; i++){
      int c = ary[i];
      putchar(c >> 8);
      putchar(c & 0xff);
    }
    tcfree(ary);
  }
  return 0;
}


/* perform hash command */
static int prochash(const char *ibuf, int isiz, bool crc, int ch){
  if(crc){
    printf("%08x\n", tcgetcrc(ibuf, isiz));
  } else if(ch > 0){
    TCCHIDX *chidx = tcchidxnew(ch);
    printf("%d\n", tcchidxhash(chidx, ibuf, isiz));
    tcchidxdel(chidx);
  } else {
    char buf[48];
    tcmd5hash(ibuf, isiz, buf);
    printf("%s\n", buf);
  }
  return 0;
}


/* perform cipher command */
static int proccipher(const char *ibuf, int isiz, const char *key){
  char *obuf = tcmalloc(isiz + 1);
  const char *kbuf = "";
  int ksiz = 0;
  if(key){
    kbuf = key;
    ksiz = strlen(key);
  }
  tcarccipher(ibuf, isiz, kbuf, ksiz, obuf);
  fwrite(obuf, 1, isiz, stdout);
  tcfree(obuf);
  return 0;
}


/* perform date command */
static int procdate(const char *str, int jl, bool wf, bool rf){
  int64_t t = str ? tcstrmktime(str) : time(NULL);
  if(wf){
    char buf[48];
    tcdatestrwww(t, jl, buf);
    printf("%s\n", buf);
  } else if(rf){
    char buf[48];
    tcdatestrhttp(t, jl, buf);
    printf("%s\n", buf);
  } else {
    printf("%lld\n", (long long int)t);
  }
  return 0;
}


/* perform tmpl command */
static int proctmpl(const char *ibuf, int isiz, TCMAP *vars){
  TCTMPL *tmpl = tctmplnew();
  tctmplload(tmpl, ibuf);
  char *str = tctmpldump(tmpl, vars);
  printf("%s", str);
  tcfree(str);
  tctmpldel(tmpl);
  return 0;
}


/* perform conf command */
static int procconf(int mode){
  switch(mode){
    case 'v':
      printf("%s\n", tcversion);
      break;
    case 'i':
      printf("%s\n", _TC_APPINC);
      break;
    case 'l':
      printf("%s\n", _TC_APPLIBS);
      break;
    case 'p':
      printf("%s\n", _TC_BINDIR);
      break;
    default:
      printf("myconf(version): %s\n", tcversion);
      printf("myconf(sysname): %s\n", TCSYSNAME);
      printf("myconf(libver): %d\n", _TC_LIBVER);
      printf("myconf(formatver): %s\n", _TC_FORMATVER);
      printf("myconf(prefix): %s\n", _TC_PREFIX);
      printf("myconf(includedir): %s\n", _TC_INCLUDEDIR);
      printf("myconf(libdir): %s\n", _TC_LIBDIR);
      printf("myconf(bindir): %s\n", _TC_BINDIR);
      printf("myconf(libexecdir): %s\n", _TC_LIBEXECDIR);
      printf("myconf(appinc): %s\n", _TC_APPINC);
      printf("myconf(applibs): %s\n", _TC_APPLIBS);
      printf("myconf(bigend): %d\n", TCBIGEND);
      printf("myconf(usezlib): %d\n", TCUSEZLIB);
      printf("myconf(usebzip): %d\n", TCUSEBZIP);
      printf("type(bool): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(bool), (int)_alignof(bool), TCALIGNOF(bool),
             (unsigned long long)true);
      printf("type(char): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(char), (int)_alignof(char), TCALIGNOF(char),
             (unsigned long long)CHAR_MAX);
      printf("type(short): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(short), (int)_alignof(short), TCALIGNOF(short),
             (unsigned long long)SHRT_MAX);
      printf("type(int): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(int), (int)_alignof(int), TCALIGNOF(int),
             (unsigned long long)INT_MAX);
      printf("type(long): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(long), (int)_alignof(long), TCALIGNOF(long),
             (unsigned long long)LONG_MAX);
      printf("type(long long): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(long long), (int)_alignof(long long), TCALIGNOF(long long),
             (unsigned long long)LLONG_MAX);
      printf("type(float): size=%d align=%d offset=%d max=%g\n",
             (int)sizeof(float), (int)_alignof(float), TCALIGNOF(float),
             (double)FLT_MAX);
      printf("type(double): size=%d align=%d offset=%d max=%g\n",
             (int)sizeof(double), (int)_alignof(double), TCALIGNOF(double),
             (double)DBL_MAX);
      printf("type(long double): size=%d align=%d offset=%d max=%Lg\n",
             (int)sizeof(long double), (int)_alignof(long double), TCALIGNOF(long double),
             (long double)LDBL_MAX);
      printf("type(void *): size=%d align=%d offset=%d\n",
             (int)sizeof(void *), (int)_alignof(void *), TCALIGNOF(void *));
      printf("type(intptr_t): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(intptr_t), (int)_alignof(intptr_t), TCALIGNOF(intptr_t),
             (unsigned long long)INTPTR_MAX);
      printf("type(size_t): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(size_t), (int)_alignof(size_t), TCALIGNOF(size_t),
             (unsigned long long)SIZE_MAX);
      printf("type(ptrdiff_t): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(ptrdiff_t), (int)_alignof(ptrdiff_t), TCALIGNOF(ptrdiff_t),
             (unsigned long long)PTRDIFF_MAX);
      printf("type(wchar_t): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(wchar_t), (int)_alignof(wchar_t), TCALIGNOF(wchar_t),
             (unsigned long long)WCHAR_MAX);
      printf("type(sig_atomic_t): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(sig_atomic_t), (int)_alignof(sig_atomic_t), TCALIGNOF(sig_atomic_t),
             (unsigned long long)SIG_ATOMIC_MAX);
      printf("type(time_t): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(time_t), (int)_alignof(time_t), TCALIGNOF(time_t),
             (unsigned long long)_maxof(time_t));
      printf("type(off_t): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(off_t), (int)_alignof(off_t), TCALIGNOF(off_t),
             (unsigned long long)_maxof(off_t));
      printf("type(ino_t): size=%d align=%d offset=%d max=%llu\n",
             (int)sizeof(ino_t), (int)_alignof(ino_t), TCALIGNOF(ino_t),
             (unsigned long long)_maxof(ino_t));
      printf("type(tcgeneric_t): size=%d align=%d offset=%d\n",
             (int)sizeof(tcgeneric_t), (int)_alignof(tcgeneric_t), TCALIGNOF(tcgeneric_t));
      printf("macro(RAND_MAX): %llu\n", (unsigned long long)RAND_MAX);
      printf("macro(PATH_MAX): %llu\n", (unsigned long long)PATH_MAX);
      printf("macro(NAME_MAX): %llu\n", (unsigned long long)NAME_MAX);
      printf("macro(P_tmpdir): %s\n", P_tmpdir);
      printf("sysconf(_SC_CLK_TCK): %ld\n", sysconf(_SC_CLK_TCK));
      printf("sysconf(_SC_OPEN_MAX): %ld\n", sysconf(_SC_OPEN_MAX));
      printf("sysconf(_SC_PAGESIZE): %ld\n", sysconf(_SC_PAGESIZE));
      TCMAP *info = tcsysinfo();
      if(info){
        tcmapiterinit(info);
        const char *name;
        while((name = tcmapiternext2(info)) != NULL){
          printf("sysinfo(%s): %s\n", name, tcmapiterval2(name));
        }
        tcmapdel(info);
      }
      struct stat sbuf;
      if(stat(MYCDIRSTR, &sbuf) == 0){
        printf("stat(st_uid): %d\n", (int)sbuf.st_uid);
        printf("stat(st_gid): %d\n", (int)sbuf.st_gid);
        printf("stat(st_blksize): %d\n", (int)sbuf.st_blksize);
      }
  }
  return 0;
}



// END OF FILE
