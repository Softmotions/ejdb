/*************************************************************************************************
 * The CGI utility of the abstract database API
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

#define MINIBNUM       31                // bucket number of map for trivial use
#define OUTBUFSIZ      (256*1024)        // size of the output buffer
#define UPLOADMAX      (256*1024*1024)   // maximum size of the upload data
#define DEFSHOWNUM     30                // default number of show result
#define FWMMAX         10000             // maximum number of forward matching
#define VALWIDTH       80                // maximum width of shown value
#define PAGETITLE      "Abstract Database Manager"  // page title
#define DBNAME         "casket"          // name of the database

#define XP(...) tcxstrprintf(obuf, __VA_ARGS__)  // syntex sugar of output setting

typedef struct {                         // type of structure of CGI parameters
  int action;                            // kind of the action
  const char *kbuf;                      // key buffer
  int ksiz;                              // key size
  const char *vbuf;                      // value buffer
  int vsiz;                              // value size
  int num;                               // number per page
  int page;                              // number of the page
} PARAMS;

enum {                                   // enumeration for error codes
  ACTLIST,                               // list records
  ACTLISTVAL,                            // list records with values
  ACTPUT,                                // put a record
  ACTOUT,                                // remove a record
  ACTGET                                 // get a record
};



/* global variables */
const char *g_scriptname;                // program name


/* function prototypes */
int main(int argc, char **argv);
static void readparameters(TCMAP *params);
static void dolist(PARAMS *params, TCADB *db);
static void doget(PARAMS *params, TCADB *db);
static void doerror(int code, const char *msg);
static void sethtmlheader(PARAMS *params, TCXSTR *obuf, TCADB *db);
static void sethtmlfooter(PARAMS *params, TCXSTR *obuf, TCADB *db);
static void sethtmlcomform(PARAMS *params, TCXSTR *obuf, TCADB *db);
static void sethtmlrecval(const char *kbuf, int ksiz, TCXSTR *obuf, TCADB *db);


/* main routine */
int main(int argc, char **argv){
  g_scriptname = getenv("SCRIPT_NAME");
  if(!g_scriptname) g_scriptname = argv[0];
  const char *rp = strrchr(g_scriptname, '/');
  if(rp) g_scriptname = rp + 1;
  TCMAP *pmap = tcmapnew2(MINIBNUM);
  readparameters(pmap);
  PARAMS params;
  params.action = ACTLIST;
  int size;
  const char *buf = tcmapget(pmap, "action", 6, &size);
  if(buf) params.action = tcatoix(buf);
  if(params.action < ACTLIST) params.action = ACTLIST;
  buf = tcmapget(pmap, "key", 3, &size);
  if(buf){
    params.kbuf = buf;
    params.ksiz = size;
  } else {
    params.kbuf = "";
    params.ksiz = 0;
  }
  buf = tcmapget(pmap, "value", 5, &size);
  if(buf){
    params.vbuf = buf;
    params.vsiz = size;
  } else {
    params.vbuf = "";
    params.vsiz = 0;
  }
  if(params.ksiz < 1){
    buf = tcmapget(pmap, "value_filename", 14, &size);
    if(buf){
      params.kbuf = buf;
      params.ksiz = size;
    }
  }
  params.num = 0;
  buf = tcmapget(pmap, "num", 3, &size);
  if(buf) params.num = tcatoix(buf);
  if(params.num < 1) params.num = DEFSHOWNUM;
  params.page = 1;
  buf = tcmapget(pmap, "page", 4, &size);
  if(buf) params.page = tcatoix(buf);
  if(params.page < 1) params.page = 1;
  bool wmode;
  switch(params.action){
    case ACTPUT:
    case ACTOUT:
      wmode = true;
      break;
    default:
      wmode = false;
      break;
  }
  TCADB *db = tcadbnew();
  char path[strlen(DBNAME)+16];
  sprintf(path, "%s.tch#mode=%s", DBNAME, wmode ? "w" : "r");
  if(!tcadbopen(db, path)){
    sprintf(path, "%s.tcb#mode=%s", DBNAME, wmode ? "w" : "r");
    if(!tcadbopen(db, path)){
      sprintf(path, "%s.tcf#mode=%s", DBNAME, wmode ? "w" : "r");
      tcadbopen(db, path);
    }
  }
  if(tcadbsize(db) > 0){
    if(wmode) tcadbtranbegin(db);
    switch(params.action){
      case ACTLIST:
      case ACTLISTVAL:
      case ACTPUT:
      case ACTOUT:
        dolist(&params, db);
        break;
      case ACTGET:
        doget(&params, db);
        break;
      default:
        doerror(400, "no such action");
        break;
    }
    if(wmode) tcadbtrancommit(db);
  } else {
    doerror(500, "the database file could not be opened");
  }
  tcadbdel(db);
  tcmapdel(pmap);
  return 0;
}


/* read CGI parameters */
static void readparameters(TCMAP *params){
  int maxlen = UPLOADMAX;
  char *buf = NULL;
  int len = 0;
  const char *rp;
  if((rp = getenv("REQUEST_METHOD")) != NULL && !strcmp(rp, "POST") &&
     (rp = getenv("CONTENT_LENGTH")) != NULL && (len = tcatoix(rp)) > 0){
    if(len > maxlen) len = maxlen;
    buf = tccalloc(len + 1, 1);
    if(fread(buf, 1, len, stdin) != len){
      tcfree(buf);
      buf = NULL;
    }
  } else if((rp = getenv("QUERY_STRING")) != NULL){
    buf = tcstrdup(rp);
    len = strlen(buf);
  }
  if(buf && len > 0) tcwwwformdecode2(buf, len, getenv("CONTENT_TYPE"), params);
  tcfree(buf);
}


/* perform the list action */
static void dolist(PARAMS *params, TCADB *db){
  printf("Content-Type: text/html\r\n");
  printf("\r\n");
  TCXSTR *obuf = tcxstrnew3(OUTBUFSIZ);
  sethtmlheader(params, obuf, db);
  if(params->action == ACTPUT){
    XP("<hr />\n");
    if(params->ksiz < 1){
      XP("<p>Error: the key should be specified.</p>\n");
    } else if(tcadbput(db, params->kbuf, params->ksiz, params->vbuf, params->vsiz)){
      XP("<p>Stored successfully!</p>\n");
    } else {
      XP("<p>Error: unknown error.</p>\n");
    }
    params->kbuf = "";
    params->ksiz = 0;
    params->action = ACTLIST;
  } else if(params->action == ACTOUT){
    XP("<hr />\n");
    if(params->ksiz < 1){
      XP("<p>Error: the key should be specified.</p>\n");
    } else if(tcadbout(db, params->kbuf, params->ksiz)){
      XP("<p>Removed successfully!</p>\n");
    } else if(tcadbvsiz(db, params->kbuf, params->ksiz) >= 0){
      XP("<p>Error: unknown error.</p>\n");
    } else {
      XP("<p>Error: no such record.</p>\n");
    }
    params->kbuf = "";
    params->ksiz = 0;
    params->action = ACTLIST;
  }
  sethtmlcomform(params, obuf, db);
  bool isnext = false;
  XP("<hr />\n");
  XP("<div id=\"list\">\n");
  int num = params->num;
  bool sv = params->action == ACTLISTVAL;
  if(params->ksiz > 0){
    TCLIST *keys = tcadbfwmkeys(db, params->kbuf, params->ksiz, FWMMAX);
    int knum = tclistnum(keys);
    int skip = params->num * (params->page - 1);
    int end = skip + params->num;
    for(int i = skip; i < knum && i < end; i++){
      int ksiz;
      const char *kbuf = tclistval(keys, i, &ksiz);
      XP("<div class=\"record\">\n");
      XP("<a href=\"%s?action=%d&amp;key=%?\" class=\"key\">%@</a>",
         g_scriptname, ACTGET, kbuf, kbuf);
      if(sv) sethtmlrecval(kbuf, ksiz, obuf, db);
      XP("</div>\n");
    }
    tclistdel(keys);
    isnext = knum > params->num * params->page;
  } else {
    tcadbiterinit(db);
    int ksiz;
    char *kbuf;
    int skip = params->num * (params->page - 1);
    for(int i = 0; i < skip && (kbuf = tcadbiternext(db, &ksiz)) != NULL; i++){
      tcfree(kbuf);
    }
    for(int i = 0; i < num && (kbuf = tcadbiternext(db, &ksiz)) != NULL; i++){
      XP("<div class=\"record\">\n");
      XP("<a href=\"%s?action=%d&amp;key=%?\" class=\"key\">%@</a>",
         g_scriptname, ACTGET, kbuf, kbuf);
      if(sv) sethtmlrecval(kbuf, ksiz, obuf, db);
      XP("</div>\n");
      tcfree(kbuf);
    }
    isnext = tcadbrnum(db) > params->num * params->page;
  }
  XP("</div>\n");
  XP("<hr />\n");
  XP("<form method=\"get\" action=\"%s\">\n", g_scriptname);
  XP("<div class=\"paging\">\n");
  if(params->page > 1){
    XP("<a href=\"%s?action=%d&amp;key=%?&amp;num=%d&amp;page=%d\" class=\"jump\">[PREV]</a>\n",
       g_scriptname, params->action, params->kbuf, params->num, params->page - 1);
  } else {
    XP("<span class=\"void\">[PREV]</span>\n");
  }
  if(isnext){
    XP("<a href=\"%s?action=%d&amp;key=%?&amp;num=%d&amp;page=%d\" class=\"jump\">[NEXT]</a>\n",
       g_scriptname, params->action, params->kbuf, params->num, params->page + 1);
  } else {
    XP("<span class=\"void\">[NEXT]</span>\n");
  }
  if(params->action == ACTLIST){
    XP("<a href=\"%s?action=%d&amp;key=%?&amp;num=%d&amp;page=%d\" class=\"jump\">[VALUE]</a>\n",
       g_scriptname, ACTLISTVAL, params->kbuf, params->num, params->page);
  } else {
    XP("<a href=\"%s?action=%d&amp;key=%?&amp;num=%d&amp;page=%d\" class=\"jump\">[NOVAL]</a>\n",
       g_scriptname, ACTLIST, params->kbuf, params->num, params->page);
  }
  XP("<select name=\"num\">\n");
  for(int i = 10; i <= 100; i += 10){
    XP("<option value=\"%d\"%s>%d records</option>\n",
       i, (i == params->num) ? " selected=\"selected\"" : "", i);
  }
  XP("</select>\n");
  XP("<input type=\"submit\" value=\"go\" />\n");
  XP("</div>\n");
  XP("</form>\n");
  sethtmlfooter(params, obuf, db);
  fwrite(tcxstrptr(obuf), 1, tcxstrsize(obuf), stdout);
  tcxstrdel(obuf);
}


/* perform the get action */
static void doget(PARAMS *params, TCADB *db){
  static char *types[] = {
    ".gz", "application/x-gzip", ".bz2", "application/x-bzip2", ".tar", "application/x-tar",
    ".zip", "application/zip", ".lzh", "application/octet-stream",
    ".pdf", "application/pdf", ".ps", "application/postscript",
    ".xml", "application/xml", ".html", "application/html", ".htm", "application/html",
    ".doc", "application/msword", ".xls", "application/vnd.ms-excel",
    ".ppt", "application/ms-powerpoint", ".swf", "application/x-shockwave-flash",
    ".png", "image/png", ".jpg", "image/jpeg", ".jpeg", "image/jpeg", ".gif", "image/gif",
    ".bmp", "image/bmp", ".tif", "image/tiff", ".tiff", "image/tiff", ".svg", "image/xml+svg",
    ".au", "audio/basic", ".snd", "audio/basic", ".mid", "audio/midi", ".midi", "audio/midi",
    ".mp3", "audio/mpeg", ".mp2", "audio/mpeg", ".wav", "audio/x-wav",
    ".tch", "application/x-tokyocabinet-hash", ".tcb", "application/x-tokyocabinet-btree",
    NULL
  };
  int vsiz;
  char *vbuf = tcadbget(db, params->kbuf, params->ksiz, &vsiz);
  if(vbuf){
    const char *type = "text/plain";
    for(int i = 0; types[i]; i++){
      if(tcstribwm(params->kbuf, types[i])){
        type = types[i+1];
        break;
      }
    }
    printf("Content-Type: %s\r\n", type);
    if(!strchr(params->kbuf, '\n') && !strchr(params->kbuf, '\r')){
      if(!strchr(params->kbuf, ' ') && !strchr(params->kbuf, ';')){
        printf("Content-Disposition: attachment; filename=%s\r\n", params->kbuf);
      } else {
        printf("Content-Disposition: attachment; filename=\"%s\"\r\n", params->kbuf);
      }
    }
    printf("\r\n");
    fwrite(vbuf, 1, vsiz, stdout);
    tcfree(vbuf);
  } else {
    doerror(404, "no such record");
  }
}


/* perform the error action */
static void doerror(int code, const char *msg){
  printf("Status: %d %s\r\n", code, msg);
  printf("Content-Type: text/plain\r\n");
  printf("\r\n");
  printf("%d: %s\n", code, msg);
}


/* set the header of HTML */
static void sethtmlheader(PARAMS *params, TCXSTR *obuf, TCADB *db){
  XP("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  XP("\n");
  XP("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\""
     " \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n");
  XP("\n");
  XP("<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n");
  XP("\n");
  XP("<head>\n");
  XP("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n");
  XP("<meta http-equiv=\"Content-Style-Type\" content=\"text/css\" />\n");
  XP("<title>%@</title>\n", PAGETITLE);
  XP("<style type=\"text/css\">html { margin: 0em; padding: 0em; }\n");
  XP("body { margin :0em; padding: 0.5em 1em; background: #eeeeee; color: #111111; }\n");
  XP("h1 { margin: 3px; padding: 0px; font-size: 125%%; }\n");
  XP("h1 a { color: #000000; }\n");
  XP("hr { margin: 0px 0px; height: 1px; border: none;"
     " background: #999999; color: #999999; }\n");
  XP("form { margin: 5px; padding: 0px; }\n");
  XP("#list { margin: 5px; padding: 0px; }\n");
  XP("p { margin: 5px; padding: 0px; }\n");
  XP("a { color: #1122ee; text-decoration: none; }\n");
  XP("a:hover { color: #2288ff; text-decoration: underline; }\n");
  XP("span.void { color: #888888; }\n");
  XP("span.value { font-size: 95%%; }\n");
  XP("i { color: #333333; font-size: 70%%; }\n");
  XP("</style>\n");
  XP("</head>\n");
  XP("\n");
  XP("<body>\n");
  XP("<h1><a href=\"%s\">%@</a></h1>\n", g_scriptname, PAGETITLE);
}


/* set the footer of HTML */
static void sethtmlfooter(PARAMS *params, TCXSTR *obuf, TCADB *db){
  XP("<hr />\n");
  XP("<div>record number: %lld</div>\n", (long long)tcadbrnum(db));
  XP("<div>size: %lld</div>\n", (long long)tcadbsize(db));
  XP("</body>\n");
  XP("\n");
  XP("</html>\n");
}


/* set the common form of HTML */
static void sethtmlcomform(PARAMS *params, TCXSTR *obuf, TCADB *db){
  XP("<hr />\n");
  XP("<form method=\"post\" action=\"%s\">\n", g_scriptname);
  XP("<div>\n");
  XP("<input type=\"text\" name=\"key\" value=\"\" size=\"24\" />\n");
  XP("<input type=\"text\" name=\"value\" value=\"\" size=\"24\" />\n");
  XP("<input type=\"submit\" value=\"store a new string record\" />\n");
  XP("<input type=\"hidden\" name=\"action\" value=\"%d\" />\n", ACTPUT);
  XP("</div>\n");
  XP("</form>\n");
  XP("<hr />\n");
  XP("<form method=\"post\" action=\"%s\" enctype=\"multipart/form-data\">\n", g_scriptname);
  XP("<div>\n");
  XP("<input type=\"text\" name=\"key\" value=\"\" size=\"24\" />\n");
  XP("<input type=\"file\" name=\"value\" size=\"24\" />\n");
  XP("<input type=\"submit\" value=\"store a new record from a file\" />\n");
  XP("<input type=\"hidden\" name=\"action\" value=\"%d\" />\n", ACTPUT);
  XP("</div>\n");
  XP("</form>\n");
  XP("<hr />\n");
  XP("<form method=\"post\" action=\"%s\">\n", g_scriptname);
  XP("<div>\n");
  XP("<input type=\"text\" name=\"key\" value=\"\" size=\"24\" />\n");
  XP("<input type=\"submit\" value=\"remove a record\" />\n");
  XP("<input type=\"hidden\" name=\"action\" value=\"%d\" />\n", ACTOUT);
  XP("</div>\n");
  XP("</form>\n");
  XP("<hr />\n");
  XP("<form method=\"post\" action=\"%s\">\n", g_scriptname);
  XP("<div>\n");
  XP("<input type=\"text\" name=\"key\" value=\"%@\" size=\"24\" />\n", params->kbuf);
  XP("<input type=\"submit\" value=\"get the value of a record\" />\n");
  XP("<input type=\"hidden\" name=\"action\" value=\"%d\" />\n", ACTGET);
  XP("</div>\n");
  XP("</form>\n");
  XP("<hr />\n");
  XP("<form method=\"post\" action=\"%s\">\n", g_scriptname);
  XP("<div>\n");
  XP("<input type=\"text\" name=\"key\" value=\"%@\" size=\"24\" />\n", params->kbuf);
  XP("<input type=\"submit\" value=\"forward matching list\" />\n");
  XP("<input type=\"hidden\" name=\"action\" value=\"%d\" />\n", ACTLIST);
  XP("</div>\n");
  XP("</form>\n");
}


/* set the value of a record */
static void sethtmlrecval(const char *kbuf, int ksiz, TCXSTR *obuf, TCADB *db){
  int vsiz;
  char *vbuf = tcadbget(db, kbuf, ksiz, &vsiz);
  if(!vbuf) return;
  XP(": <span class=\"value\">");
  bool hex = false;
  int width = VALWIDTH;
  for(int j = 0; j < vsiz; j++){
    int c = ((unsigned char *)vbuf)[j];
    if(c >= 0x20 && c <= 0x7e){
      if(hex) tcxstrcat(obuf, " ", 1);
      switch(c){
        case '<':
          tcxstrcat(obuf, "&lt;", 4);
          break;
        case '>':
          tcxstrcat(obuf, "&gt;", 4);
          break;
        case '&':
          tcxstrcat(obuf, "&amp;", 5);
          break;
        default:
          tcxstrcat(obuf, vbuf + j, 1);
          break;
      }
      width--;
      hex = false;
    } else {
      XP(" <i>%02X</i>", c);
      width -= 2;
      hex = true;
    }
    if(width < 1){
      XP(" <i>...</i>");
      break;
    }
  }
  XP("</span>");
  tcfree(vbuf);
}



// END OF FILE
