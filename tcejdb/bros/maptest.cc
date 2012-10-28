/*************************************************************************************************
 * Writing test of map utilities
 *************************************************************************************************/


#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <tcutil.h>
#include <map>
#include <set>
#include <tr1/unordered_map>
#include <google/sparse_hash_map>
#include <google/dense_hash_map>

#define RECBUFSIZ      32                // buffer for records

using namespace std;

struct stringhash {                      // hash function for string
  size_t operator()(const string& str) const {
    const char *ptr = str.data();
    int size = str.size();
    size_t idx = 19780211;
    while(size--){
      idx = idx * 37 + *(uint8_t *)ptr++;
    }
    return idx;
  }
};


// aliases of template instances
typedef map<string, string> stlmap;
typedef multimap<string, string> stlmmap;
typedef set<string> stlset;
typedef tr1::unordered_map<string, string, stringhash> trhash;
typedef google::dense_hash_map<string, string, stringhash> ggldh;
typedef google::sparse_hash_map<string, string, stringhash> gglsh;


// global variables
const char *g_progname;                  // program name


// function prototypes
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static int runtcmap(int argc, char **argv);
static int runtctree(int argc, char **argv);
static int runstlmap(int argc, char **argv);
static int runstlmmap(int argc, char **argv);
static int runstlset(int argc, char **argv);
static int runtrhash(int argc, char **argv);
static int runggldh(int argc, char **argv);
static int rungglsh(int argc, char **argv);
static int proctcmap(int rnum, bool rd);
static int proctctree(int rnum, bool rd);
static int procstlmap(int rnum, bool rd);
static int procstlmmap(int rnum, bool rd);
static int procstlset(int rnum, bool rd);
static int proctrhash(int rnum, bool rd);
static int procggldh(int rnum, bool rd);
static int procgglsh(int rnum, bool rd);


// main routine
int main(int argc, char **argv){
  g_progname = argv[0];
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "tcmap")){
    rv = runtcmap(argc, argv);
  } else if(!strcmp(argv[1], "tctree")){
    rv = runtctree(argc, argv);
  } else if(!strcmp(argv[1], "stlmap")){
    rv = runstlmap(argc, argv);
  } else if(!strcmp(argv[1], "stlmmap")){
    rv = runstlmmap(argc, argv);
  } else if(!strcmp(argv[1], "stlset")){
    rv = runstlset(argc, argv);
  } else if(!strcmp(argv[1], "trhash")){
    rv = runtrhash(argc, argv);
  } else if(!strcmp(argv[1], "ggldh")){
    rv = runggldh(argc, argv);
  } else if(!strcmp(argv[1], "gglsh")){
    rv = rungglsh(argc, argv);
  } else {
    usage();
  }
  return rv;
}


// print the usage and exit
static void usage(void){
  fprintf(stderr, "%s: speed checker of map utilities\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s tcmap [-rd] rnum\n", g_progname);
  fprintf(stderr, "  %s tctree [-rd] rnum\n", g_progname);
  fprintf(stderr, "  %s stlmap [-rd] rnum\n", g_progname);
  fprintf(stderr, "  %s stlmmap [-rd] rnum\n", g_progname);
  fprintf(stderr, "  %s stlset [-rd] rnum\n", g_progname);
  fprintf(stderr, "  %s trhash [-rd] rnum\n", g_progname);
  fprintf(stderr, "  %s ggldh [-rd] rnum\n", g_progname);
  fprintf(stderr, "  %s gglsh [-rd] rnum\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


// print formatted information string and flush the buffer
static void iprintf(const char *format, ...){
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  fflush(stdout);
  va_end(ap);
}


// parse arguments of tcmap command
static int runtcmap(int argc, char **argv){
  char *rstr = NULL;
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
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoi(rstr);
  if(rnum < 1) usage();
  int rv = proctcmap(rnum, rd);
  return rv;
}


// parse arguments of tctree command
static int runtctree(int argc, char **argv){
  char *rstr = NULL;
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
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoi(rstr);
  if(rnum < 1) usage();
  int rv = proctctree(rnum, rd);
  return rv;
}


// parse arguments of stlmap command
static int runstlmap(int argc, char **argv){
  char *rstr = NULL;
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
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoi(rstr);
  if(rnum < 1) usage();
  int rv = procstlmap(rnum, rd);
  return rv;
}


// parse arguments of stlmmap command
static int runstlmmap(int argc, char **argv){
  char *rstr = NULL;
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
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoi(rstr);
  if(rnum < 1) usage();
  int rv = procstlmmap(rnum, rd);
  return rv;
}


// parse arguments of stlset command
static int runstlset(int argc, char **argv){
  char *rstr = NULL;
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
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoi(rstr);
  if(rnum < 1) usage();
  int rv = procstlset(rnum, rd);
  return rv;
}


// parse arguments of trhash command
static int runtrhash(int argc, char **argv){
  char *rstr = NULL;
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
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoi(rstr);
  if(rnum < 1) usage();
  int rv = proctrhash(rnum, rd);
  return rv;
}


// parse arguments of ggldh command
static int runggldh(int argc, char **argv){
  char *rstr = NULL;
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
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoi(rstr);
  if(rnum < 1) usage();
  int rv = procggldh(rnum, rd);
  return rv;
}


// parse arguments of gglsh command
static int rungglsh(int argc, char **argv){
  char *rstr = NULL;
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
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = tcatoi(rstr);
  if(rnum < 1) usage();
  int rv = procgglsh(rnum, rd);
  return rv;
}


// perform tcmap command
static int proctcmap(int rnum, bool rd){
  iprintf("<Tokyo Cabinet Map Writing Test>\n  rnum=%d  rd=%d\n\n", rnum, rd);
  double stime = tctime();
  {
    TCMAP *mymap = tcmapnew2(rnum + 1);
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      int len = sprintf(buf, "%08d", i);
      tcmapput(mymap, buf, len, buf, len);
      if(rnum > 250 && i % (rnum / 250) == 0){
        putchar('.');
        fflush(stdout);
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
    if(rd){
      double itime = tctime();
      iprintf("time: %.3f\n", itime - stime);
      stime = itime;
      for(int i = 1; i <= rnum; i++){
        char buf[RECBUFSIZ];
        int len = sprintf(buf, "%08d", i);
        int vsiz;
        const void *vbuf = tcmapget(mymap, buf, len, &vsiz);
        if(!vbuf){
          iprintf("not found\n");
          break;
        }
        if(rnum > 250 && i % (rnum / 250) == 0){
          putchar('.');
          fflush(stdout);
          if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
        }
      }
    }
    iprintf("record number: %d\n", tcmaprnum(mymap));
    tcmapdel(mymap);
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


// perform tctree command
static int proctctree(int rnum, bool rd){
  iprintf("<Tokyo Cabinet Tree Writing Test>\n  rnum=%d  rd=%d\n\n", rnum, rd);
  double stime = tctime();
  {
    TCTREE *mytree = tctreenew();
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      int len = sprintf(buf, "%08d", i);
      tctreeput(mytree, buf, len, buf, len);
      if(rnum > 250 && i % (rnum / 250) == 0){
        putchar('.');
        fflush(stdout);
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
    if(rd){
      double itime = tctime();
      iprintf("time: %.3f\n", itime - stime);
      stime = itime;
      for(int i = 1; i <= rnum; i++){
        char buf[RECBUFSIZ];
        int len = sprintf(buf, "%08d", i);
        int vsiz;
        const void *vbuf = tctreeget(mytree, buf, len, &vsiz);
        if(!vbuf){
          iprintf("not found\n");
          break;
        }
        if(rnum > 250 && i % (rnum / 250) == 0){
          putchar('.');
          fflush(stdout);
          if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
        }
      }
    }
    iprintf("record number: %d\n", tctreernum(mytree));
    tctreedel(mytree);
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


// perform stlmap command
static int procstlmap(int rnum, bool rd){
  iprintf("<STL Map Writing Test>\n  rnum=%d  rd=%d\n\n", rnum, rd);
  double stime = tctime();
  {
    stlmap mymap;
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      sprintf(buf, "%08d", i);
      mymap.insert(stlmap::value_type(buf, buf));
      if(rnum > 250 && i % (rnum / 250) == 0){
        putchar('.');
        fflush(stdout);
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
    if(rd){
      double itime = tctime();
      iprintf("time: %.3f\n", itime - stime);
      stime = itime;
      for(int i = 1; i <= rnum; i++){
        char buf[RECBUFSIZ];
        sprintf(buf, "%08d", i);
        stlmap::const_iterator it = mymap.find(buf);
        if(it == mymap.end()){
          iprintf("not found\n");
          break;
        }
        if(rnum > 250 && i % (rnum / 250) == 0){
          putchar('.');
          fflush(stdout);
          if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
        }
      }
    }
    iprintf("record number: %d\n", mymap.size());
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


// perform multimap command
static int procstlmmap(int rnum, bool rd){
  iprintf("<STL Multi Map Writing Test>\n  rnum=%d  rd=%d\n\n", rnum, rd);
  double stime = tctime();
  {
    stlmmap mymap;
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      sprintf(buf, "%08d", i);
      mymap.insert(stlmmap::value_type(buf, buf));
      if(rnum > 250 && i % (rnum / 250) == 0){
        putchar('.');
        fflush(stdout);
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
    if(rd){
      double itime = tctime();
      iprintf("time: %.3f\n", itime - stime);
      stime = itime;
      for(int i = 1; i <= rnum; i++){
        char buf[RECBUFSIZ];
        sprintf(buf, "%08d", i);
        stlmmap::const_iterator it = mymap.find(buf);
        if(it == mymap.end()){
          iprintf("not found\n");
          break;
        }
        if(rnum > 250 && i % (rnum / 250) == 0){
          putchar('.');
          fflush(stdout);
          if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
        }
      }
    }
    iprintf("record number: %d\n", mymap.size());
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


// perform stlset command
static int procstlset(int rnum, bool rd){
  iprintf("<STL Set Writing Test>\n  rnum=%d  rd=%d\n\n", rnum, rd);
  double stime = tctime();
  {
    stlset mymap;
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      sprintf(buf, "%08d", i);
      mymap.insert(stlset::value_type(buf));
      if(rnum > 250 && i % (rnum / 250) == 0){
        putchar('.');
        fflush(stdout);
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
    if(rd){
      double itime = tctime();
      iprintf("time: %.3f\n", itime - stime);
      stime = itime;
      for(int i = 1; i <= rnum; i++){
        char buf[RECBUFSIZ];
        sprintf(buf, "%08d", i);
        stlset::const_iterator it = mymap.find(buf);
        if(it == mymap.end()){
          iprintf("not found\n");
          break;
        }
        if(rnum > 250 && i % (rnum / 250) == 0){
          putchar('.');
          fflush(stdout);
          if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
        }
      }
    }
    iprintf("record number: %d\n", mymap.size());
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


// perform trhash command
static int proctrhash(int rnum, bool rd){
  iprintf("<TR1 Hash Writing Test>\n  rnum=%d  rd=%d\n\n", rnum, rd);
  double stime = tctime();
  {
    trhash mymap;
    mymap.rehash(rnum + 1);
    mymap.max_load_factor(1.0);
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      sprintf(buf, "%08d", i);
      mymap.insert(trhash::value_type(buf, buf));
      if(rnum > 250 && i % (rnum / 250) == 0){
        putchar('.');
        fflush(stdout);
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
    if(rd){
      double itime = tctime();
      iprintf("time: %.3f\n", itime - stime);
      stime = itime;
      for(int i = 1; i <= rnum; i++){
        char buf[RECBUFSIZ];
        sprintf(buf, "%08d", i);
        trhash::const_iterator it = mymap.find(buf);
        if(it == mymap.end()){
          iprintf("not found\n");
          break;
        }
        if(rnum > 250 && i % (rnum / 250) == 0){
          putchar('.');
          fflush(stdout);
          if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
        }
      }
    }
    iprintf("record number: %d\n", mymap.size());
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


// perform ggldh command
static int procggldh(int rnum, bool rd){
  iprintf("<Google Dense Hash Writing Test>\n  rnum=%d  rd=%d\n\n", rnum, rd);
  double stime = tctime();
  {
    ggldh mymap;
    mymap.set_empty_key("");
    mymap.resize(rnum + 1);
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      sprintf(buf, "%08d", i);
      mymap.insert(ggldh::value_type(buf, buf));
      if(rnum > 250 && i % (rnum / 250) == 0){
        putchar('.');
        fflush(stdout);
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
    if(rd){
      double itime = tctime();
      iprintf("time: %.3f\n", itime - stime);
      stime = itime;
      for(int i = 1; i <= rnum; i++){
        char buf[RECBUFSIZ];
        sprintf(buf, "%08d", i);
        ggldh::const_iterator it = mymap.find(buf);
        if(it == mymap.end()){
          iprintf("not found\n");
          break;
        }
        if(rnum > 250 && i % (rnum / 250) == 0){
          putchar('.');
          fflush(stdout);
          if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
        }
      }
    }
    iprintf("record number: %d\n", mymap.size());
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


// perform gglsh command
static int procgglsh(int rnum, bool rd){
  iprintf("<Google Sparse Hash Writing Test>\n  rnum=%d  rd=%d\n\n", rnum, rd);
  double stime = tctime();
  {
    gglsh mymap;
    mymap.resize(rnum + 1);
    for(int i = 1; i <= rnum; i++){
      char buf[RECBUFSIZ];
      sprintf(buf, "%08d", i);
      mymap.insert(gglsh::value_type(buf, buf));
      if(rnum > 250 && i % (rnum / 250) == 0){
        putchar('.');
        fflush(stdout);
        if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
      }
    }
    if(rd){
      double itime = tctime();
      iprintf("time: %.3f\n", itime - stime);
      stime = itime;
      for(int i = 1; i <= rnum; i++){
        char buf[RECBUFSIZ];
        sprintf(buf, "%08d", i);
        gglsh::const_iterator it = mymap.find(buf);
        if(it == mymap.end()){
          iprintf("not found\n");
          break;
        }
        if(rnum > 250 && i % (rnum / 250) == 0){
          putchar('.');
          fflush(stdout);
          if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
        }
      }
    }
    iprintf("record number: %d\n", mymap.size());
  }
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}



// END OF FILE
