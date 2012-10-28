#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cdb.h>

#undef TRUE
#define TRUE           1                 /* boolean true */
#undef FALSE
#define FALSE          0                 /* boolean false */

#define RECBUFSIZ      32                /* buffer for records */


/* global variables */
const char *progname;                    /* program name */
int showprgr;                            /* whether to show progression */


/* function prototypes */
int main(int argc, char **argv);
void usage(void);
int runwrite(int argc, char **argv);
int runread(int argc, char **argv);
int dowrite(char *name, int rnum);
int doread(char *name, int rnum);


/* main routine */
int main(int argc, char **argv){
  int rv;
  progname = argv[0];
  showprgr = TRUE;
  if(getenv("HIDEPRGR")) showprgr = FALSE;
  if(argc < 2) usage();
  rv = 0;
  if(!strcmp(argv[1], "write")){
    rv = runwrite(argc, argv);
  } else if(!strcmp(argv[1], "read")){
    rv = runread(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
void usage(void){
  fprintf(stderr, "%s: test cases for Constant Database\n", progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write name rnum\n", progname);
  fprintf(stderr, "  %s read name rnum\n", progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* parse arguments of write command */
int runwrite(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  for(i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  rnum = atoi(rstr);
  if(rnum < 1) usage();
  rv = dowrite(name, rnum);
  return rv;
}


/* parse arguments of read command */
int runread(int argc, char **argv){
  char *name, *rstr;
  int i, rnum, rv;
  name = NULL;
  rstr = NULL;
  rnum = 0;
  for(i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  rnum = atoi(rstr);
  if(rnum < 1) usage();
  rv = doread(name, rnum);
  return rv;
}


/* perform write command */
int dowrite(char *name, int rnum){
  struct cdb_make cdb;
  int i, fd, err, len;
  char buf[32];
  if(showprgr) printf("<Writing Test>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  if((fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1){
    perror("open");
    return 1;
  }
  if(cdb_make_start(&cdb, fd) == -1){
    perror("cdb_make_start");
    close(fd);
    return 1;
  }
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* store a record */
    len = sprintf(buf, "%08d", i);
    if(cdb_make_add(&cdb, buf, len, buf, len) == -1){
      perror("cdb_add");
      err = TRUE;
      break;
    }
    /* print progression */
    if(showprgr && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0){
        printf(" (%08d)\n", i);
        fflush(stdout);
      }
    }
  }
  /* close the database */
  if(cdb_make_finish(&cdb) == -1){
    perror("cdb_make_finish");
    close(fd);
    return 1;
  }
  if(close(fd) == -1){
    perror("close");
    return 1;
  }
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}


/* perform read command */
int doread(char *name, int rnum){
  struct cdb cdb;
  int i, fd, err, len;
  char buf[RECBUFSIZ], *val;
  if(showprgr) printf("<Reading Test>\n  name=%s  rnum=%d\n\n", name, rnum);
  /* open a database */
  if((fd = open(name, O_RDONLY, 0644)) == -1){
    perror("open");
    return 1;
  }
  cdb_init(&cdb, fd);
  err = FALSE;
  /* loop for each record */
  for(i = 1; i <= rnum; i++){
    /* retrieve a record */
    len = sprintf(buf, "%08d", i);
    if(cdb_find(&cdb, buf, len) == 0){
      perror("cdb_find");
      err = TRUE;
      break;
    }
    len = cdb_datalen(&cdb);
    if(!(val = malloc(len + 1))){
      perror("malloc");
      err = TRUE;
      break;
    }
    cdb_read(&cdb, val, len, cdb_datapos(&cdb));
    free(val);
    /* print progression */
    if(showprgr && rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0){
        printf(" (%08d)\n", i);
        fflush(stdout);
      }
    }
  }
  /* close the database */
  cdb_free(&cdb);
  if(close(fd) == -1){
    perror("close");
    return 1;
  }
  if(showprgr && !err) printf("ok\n\n");
  return err ? 1 : 0;
}



/* END OF FILE */
