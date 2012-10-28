/*************************************************************************************************
 * The abstract database API of Tokyo Cabinet
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


#include "tcutil.h"
#include "tchdb.h"
#include "tcbdb.h"
#include "tcfdb.h"
#include "tctdb.h"
#include "tcadb.h"
#include "myconf.h"

#define ADBDIRMODE     00755             // permission of created directories
#define ADBMULPREFIX   "adbmul-"         // prefix of multiple database files

typedef struct {                         // type of structure for multiple database
  TCADB **adbs;                          // inner database objects
  int num;                               // number of inner databases
  int iter;                              // index of the iterator
  char *path;                            // path of the base directory
} ADBMUL;

typedef struct {                         // type of structure for mapper to B+ tree database
  TCADB *adb;                            // source database object
  TCBDB *bdb;                            // destination database object
  TCLIST *recs;                          // cached records
  int64_t rsiz;                          // total size of cached records
  int64_t csiz;                          // capacity of cached records
  ADBMAPPROC proc;                       // mapping function
  void *op;                              // opaque object for the mapping function
} ADBMAPBDB;


/* private function prototypes */
static ADBMUL *tcadbmulnew(int num);
static void tcadbmuldel(ADBMUL *mul);
static bool tcadbmulopen(ADBMUL *mul, const char *name);
static bool tcadbmulclose(ADBMUL *mul);
static bool tcadbmulput(ADBMUL *mul, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
static bool tcadbmulputkeep(ADBMUL *mul, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
static bool tcadbmulputcat(ADBMUL *mul, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
static bool tcadbmulout(ADBMUL *mul, const void *kbuf, int ksiz);
static void *tcadbmulget(ADBMUL *mul, const void *kbuf, int ksiz, int *sp);
static int tcadbmulvsiz(ADBMUL *mul, const void *kbuf, int ksiz);
static bool tcadbmuliterinit(ADBMUL *mul);
static void *tcadbmuliternext(ADBMUL *mul, int *sp);
static TCLIST *tcadbmulfwmkeys(ADBMUL *mul, const void *pbuf, int psiz, int max);
static int tcadbmuladdint(ADBMUL *mul, const void *kbuf, int ksiz, int num);
static double tcadbmuladddouble(ADBMUL *mul, const void *kbuf, int ksiz, double num);
static bool tcadbmulsync(ADBMUL *mul);
static bool tcadbmuloptimize(ADBMUL *mul, const char *params);
static bool tcadbmulvanish(ADBMUL *mul);
static bool tcadbmulcopy(ADBMUL *mul, const char *path);
static bool tcadbmultranbegin(ADBMUL *mul);
static bool tcadbmultrancommit(ADBMUL *mul);
static bool tcadbmultranabort(ADBMUL *mul);
static const char *tcadbmulpath(ADBMUL *mul);
static uint64_t tcadbmulrnum(ADBMUL *mul);
static uint64_t tcadbmulsize(ADBMUL *mul);
static TCLIST *tcadbmulmisc(ADBMUL *mul, const char *name, const TCLIST *args);
static bool tcadbmulputproc(ADBMUL *mul, const void *kbuf, int ksiz, const void *vbuf, int vsiz,
                            TCPDPROC proc, void *op);
static bool tcadbmulforeach(ADBMUL *mul, TCITER iter, void *op);
static int tcadbmulidx(ADBMUL *mul, const void *kbuf, int ksiz);
static bool tcadbmapbdbiter(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op);
static bool tcadbmapbdbdump(ADBMAPBDB *map);
static int tcadbmapreccmplexical(const TCLISTDATUM *a, const TCLISTDATUM *b);
static int tcadbmapreccmpdecimal(const TCLISTDATUM *a, const TCLISTDATUM *b);
static int tcadbmapreccmpint32(const TCLISTDATUM *a, const TCLISTDATUM *b);
static int tcadbmapreccmpint64(const TCLISTDATUM *a, const TCLISTDATUM *b);
static int tcadbtdbqrygetout(const void *pkbuf, int pksiz, TCMAP *cols, void *op);



/*************************************************************************************************
 * API
 *************************************************************************************************/


/* Create an abstract database object. */
TCADB *tcadbnew(void){
  TCADB *adb;
  TCMALLOC(adb, sizeof(*adb));
  adb->omode = ADBOVOID;
  adb->mdb = NULL;
  adb->ndb = NULL;
  adb->hdb = NULL;
  adb->bdb = NULL;
  adb->fdb = NULL;
  adb->tdb = NULL;
  adb->capnum = -1;
  adb->capsiz = -1;
  adb->capcnt = 0;
  adb->cur = NULL;
  adb->skel = NULL;
  return adb;
}


/* Delete an abstract database object. */
void tcadbdel(TCADB *adb){
  assert(adb);
  if(adb->omode != ADBOVOID) tcadbclose(adb);
  if(adb->skel){
    ADBSKEL *skel = adb->skel;
    if(skel->del) skel->del(skel->opq);
    TCFREE(skel);
  }
  TCFREE(adb);
}


/* Open an abstract database. */
bool tcadbopen(TCADB *adb, const char *name){
  assert(adb && name);
  if(adb->omode != ADBOVOID) return false;
  TCLIST *elems = tcstrsplit(name, "#");
  char *path = tclistshift2(elems);
  if(!path){
    tclistdel(elems);
    return false;
  }
  int dbgfd = -1;
  int64_t bnum = -1;
  int64_t capnum = -1;
  int64_t capsiz = -1;
  bool owmode = true;
  bool ocmode = true;
  bool otmode = false;
  bool onlmode = false;
  bool onbmode = false;
  int8_t apow = -1;
  int8_t fpow = -1;
  bool tlmode = false;
  bool tdmode = false;
  bool tbmode = false;
  bool ttmode = false;
  int32_t rcnum = -1;
  int64_t xmsiz = -1;
  int32_t dfunit = -1;
  int32_t lmemb = -1;
  int32_t nmemb = -1;
  int32_t lcnum = -1;
  int32_t ncnum = -1;
  int32_t width = -1;
  int64_t limsiz = -1;
  TCLIST *idxs = NULL;
  int ln = TCLISTNUM(elems);
  for(int i = 0; i < ln; i++){
    const char *elem = TCLISTVALPTR(elems, i);
    char *pv = strchr(elem, '=');
    if(!pv) continue;
    *(pv++) = '\0';
    if(!tcstricmp(elem, "dbgfd")){
      dbgfd = tcatoi(pv);
    } else if(!tcstricmp(elem, "bnum")){
      bnum = tcatoix(pv);
    } else if(!tcstricmp(elem, "capnum")){
      capnum = tcatoix(pv);
    } else if(!tcstricmp(elem, "capsiz")){
      capsiz = tcatoix(pv);
    } else if(!tcstricmp(elem, "mode")){
      owmode = strchr(pv, 'w') || strchr(pv, 'W');
      ocmode = strchr(pv, 'c') || strchr(pv, 'C');
      otmode = strchr(pv, 't') || strchr(pv, 'T');
      onlmode = strchr(pv, 'e') || strchr(pv, 'E');
      onbmode = strchr(pv, 'f') || strchr(pv, 'F');
    } else if(!tcstricmp(elem, "apow")){
      apow = tcatoix(pv);
    } else if(!tcstricmp(elem, "fpow")){
      fpow = tcatoix(pv);
    } else if(!tcstricmp(elem, "opts")){
      if(strchr(pv, 'l') || strchr(pv, 'L')) tlmode = true;
      if(strchr(pv, 'd') || strchr(pv, 'D')) tdmode = true;
      if(strchr(pv, 'b') || strchr(pv, 'B')) tbmode = true;
      if(strchr(pv, 't') || strchr(pv, 'T')) ttmode = true;
    } else if(!tcstricmp(elem, "rcnum")){
      rcnum = tcatoix(pv);
    } else if(!tcstricmp(elem, "xmsiz")){
      xmsiz = tcatoix(pv);
    } else if(!tcstricmp(elem, "dfunit")){
      dfunit = tcatoix(pv);
    } else if(!tcstricmp(elem, "lmemb")){
      lmemb = tcatoix(pv);
    } else if(!tcstricmp(elem, "nmemb")){
      nmemb = tcatoix(pv);
    } else if(!tcstricmp(elem, "lcnum")){
      lcnum = tcatoix(pv);
    } else if(!tcstricmp(elem, "ncnum")){
      ncnum = tcatoix(pv);
    } else if(!tcstricmp(elem, "width")){
      width = tcatoix(pv);
    } else if(!tcstricmp(elem, "limsiz")){
      limsiz = tcatoix(pv);
    } else if(!tcstricmp(elem, "idx")){
      if(!idxs) idxs = tclistnew();
      TCLISTPUSH(idxs, pv, strlen(pv));
    }
  }
  tclistdel(elems);
  adb->omode = ADBOVOID;
  if(adb->skel){
    ADBSKEL *skel = adb->skel;
    if(!skel->open || !skel->open(skel->opq, name)){
      if(idxs) tclistdel(idxs);
      TCFREE(path);
      return false;
    }
    adb->omode = ADBOSKEL;
  } else if(!tcstricmp(path, "*")){
    adb->mdb = bnum > 0 ? tcmdbnew2(bnum) : tcmdbnew();
    adb->capnum = capnum;
    adb->capsiz = capsiz;
    adb->capcnt = 0;
    adb->omode = ADBOMDB;
  } else if(!tcstricmp(path, "+")){
    adb->ndb = tcndbnew();
    adb->capnum = capnum;
    adb->capsiz = capsiz;
    adb->capcnt = 0;
    adb->omode = ADBONDB;
  } else if(tcstribwm(path, ".tch") || tcstribwm(path, ".hdb")){
    TCHDB *hdb = tchdbnew();
    if(dbgfd >= 0) tchdbsetdbgfd(hdb, dbgfd);
    tchdbsetmutex(hdb);
    int opts = 0;
    if(tlmode) opts |= HDBTLARGE;
    if(tdmode) opts |= HDBTDEFLATE;
    if(tbmode) opts |= HDBTBZIP;
    if(ttmode) opts |= HDBTTCBS;
    tchdbtune(hdb, bnum, apow, fpow, opts);
    tchdbsetcache(hdb, rcnum);
    if(xmsiz >= 0) tchdbsetxmsiz(hdb, xmsiz);
    if(dfunit >= 0) tchdbsetdfunit(hdb, dfunit);
    int omode = owmode ? HDBOWRITER : HDBOREADER;
    if(ocmode) omode |= HDBOCREAT;
    if(otmode) omode |= HDBOTRUNC;
    if(onlmode) omode |= HDBONOLCK;
    if(onbmode) omode |= HDBOLCKNB;
    if(!tchdbopen(hdb, path, omode)){
      tchdbdel(hdb);
      if(idxs) tclistdel(idxs);
      TCFREE(path);
      return false;
    }
    adb->hdb = hdb;
    adb->omode = ADBOHDB;
  } else if(tcstribwm(path, ".tcb") || tcstribwm(path, ".bdb")){
    TCBDB *bdb = tcbdbnew();
    if(dbgfd >= 0) tcbdbsetdbgfd(bdb, dbgfd);
    tcbdbsetmutex(bdb);
    int opts = 0;
    if(tlmode) opts |= BDBTLARGE;
    if(tdmode) opts |= BDBTDEFLATE;
    if(tbmode) opts |= BDBTBZIP;
    if(ttmode) opts |= BDBTTCBS;
    tcbdbtune(bdb, lmemb, nmemb, bnum, apow, fpow, opts);
    tcbdbsetcache(bdb, lcnum, ncnum);
    if(xmsiz >= 0) tcbdbsetxmsiz(bdb, xmsiz);
    if(dfunit >= 0) tcbdbsetdfunit(bdb, dfunit);
    if(capnum > 0) tcbdbsetcapnum(bdb, capnum);
    int omode = owmode ? BDBOWRITER : BDBOREADER;
    if(ocmode) omode |= BDBOCREAT;
    if(otmode) omode |= BDBOTRUNC;
    if(onlmode) omode |= BDBONOLCK;
    if(onbmode) omode |= BDBOLCKNB;
    if(!tcbdbopen(bdb, path, omode)){
      tcbdbdel(bdb);
      if(idxs) tclistdel(idxs);
      TCFREE(path);
      return false;
    }
    adb->bdb = bdb;
    adb->cur = tcbdbcurnew(bdb);
    adb->omode = ADBOBDB;
  } else if(tcstribwm(path, ".tcf") || tcstribwm(path, ".fdb")){
    TCFDB *fdb = tcfdbnew();
    if(dbgfd >= 0) tcfdbsetdbgfd(fdb, dbgfd);
    tcfdbsetmutex(fdb);
    tcfdbtune(fdb, width, limsiz);
    int omode = owmode ? FDBOWRITER : FDBOREADER;
    if(ocmode) omode |= FDBOCREAT;
    if(otmode) omode |= FDBOTRUNC;
    if(onlmode) omode |= FDBONOLCK;
    if(onbmode) omode |= FDBOLCKNB;
    if(!tcfdbopen(fdb, path, omode)){
      tcfdbdel(fdb);
      if(idxs) tclistdel(idxs);
      TCFREE(path);
      return false;
    }
    adb->fdb = fdb;
    adb->omode = ADBOFDB;
  } else if(tcstribwm(path, ".tct") || tcstribwm(path, ".tdb")){
    TCTDB *tdb = tctdbnew();
    if(dbgfd >= 0) tctdbsetdbgfd(tdb, dbgfd);
    tctdbsetmutex(tdb);
    int opts = 0;
    if(tlmode) opts |= TDBTLARGE;
    if(tdmode) opts |= TDBTDEFLATE;
    if(tbmode) opts |= TDBTBZIP;
    if(ttmode) opts |= TDBTTCBS;
    tctdbtune(tdb, bnum, apow, fpow, opts);
    tctdbsetcache(tdb, rcnum, lcnum, ncnum);
    if(xmsiz >= 0) tctdbsetxmsiz(tdb, xmsiz);
    if(dfunit >= 0) tctdbsetdfunit(tdb, dfunit);
    int omode = owmode ? TDBOWRITER : TDBOREADER;
    if(ocmode) omode |= TDBOCREAT;
    if(otmode) omode |= TDBOTRUNC;
    if(onlmode) omode |= TDBONOLCK;
    if(onbmode) omode |= TDBOLCKNB;
    if(!tctdbopen(tdb, path, omode)){
      tctdbdel(tdb);
      if(idxs) tclistdel(idxs);
      TCFREE(path);
      return false;
    }
    if(idxs){
      int xnum = TCLISTNUM(idxs);
      for(int i = 0; i < xnum; i++){
        const char *expr = TCLISTVALPTR(idxs, i);
        int type = TDBITLEXICAL;
        char *pv = strchr(expr, ':');
        if(pv){
          *(pv++) = '\0';
          type = tctdbstrtoindextype(pv);
        }
        if(type >= 0) tctdbsetindex(tdb, expr, type | TDBITKEEP);
      }
    }
    adb->tdb = tdb;
    adb->omode = ADBOTDB;
  }
  if(idxs) tclistdel(idxs);
  TCFREE(path);
  if(adb->omode == ADBOVOID) return false;
  return true;
}


/* Close an abstract database object. */
bool tcadbclose(TCADB *adb){
  assert(adb);
  int err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      tcmdbdel(adb->mdb);
      adb->mdb = NULL;
      break;
    case ADBONDB:
      tcndbdel(adb->ndb);
      adb->ndb = NULL;
      break;
    case ADBOHDB:
      if(!tchdbclose(adb->hdb)) err = true;
      tchdbdel(adb->hdb);
      adb->hdb = NULL;
      break;
    case ADBOBDB:
      tcbdbcurdel(adb->cur);
      if(!tcbdbclose(adb->bdb)) err = true;
      tcbdbdel(adb->bdb);
      adb->bdb = NULL;
      break;
    case ADBOFDB:
      if(!tcfdbclose(adb->fdb)) err = true;
      tcfdbdel(adb->fdb);
      adb->fdb = NULL;
      break;
    case ADBOTDB:
      if(!tctdbclose(adb->tdb)) err = true;
      tctdbdel(adb->tdb);
      adb->tdb = NULL;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->close){
        if(!skel->close(skel->opq)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  adb->omode = ADBOVOID;
  return !err;
}


/* Store a record into an abstract database object. */
bool tcadbput(TCADB *adb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  char numbuf[TCNUMBUFSIZ];
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      if(adb->capnum > 0 || adb->capsiz > 0){
        tcmdbput3(adb->mdb, kbuf, ksiz, vbuf, vsiz);
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum + 0x100)
            tcmdbcutfront(adb->mdb, 0x100);
          if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz)
            tcmdbcutfront(adb->mdb, 0x200);
        }
      } else {
        tcmdbput(adb->mdb, kbuf, ksiz, vbuf, vsiz);
      }
      break;
    case ADBONDB:
      tcndbput(adb->ndb, kbuf, ksiz, vbuf, vsiz);
      if(adb->capnum > 0 || adb->capsiz > 0){
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcndbrnum(adb->ndb) > adb->capnum + 0x100)
            tcndbcutfringe(adb->ndb, 0x100);
          if(adb->capsiz > 0 && tcndbmsiz(adb->ndb) > adb->capsiz)
            tcndbcutfringe(adb->ndb, 0x200);
        }
      }
      break;
    case ADBOHDB:
      if(!tchdbput(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbput(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbput2(adb->fdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOTDB:
      if(ksiz < 1){
        ksiz = sprintf(numbuf, "%lld", (long long)tctdbgenuid(adb->tdb));
        kbuf = numbuf;
      }
      if(!tctdbput2(adb->tdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->put){
        if(!skel->put(skel->opq, kbuf, ksiz, vbuf, vsiz)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Store a string record into an abstract object. */
bool tcadbput2(TCADB *adb, const char *kstr, const char *vstr){
  assert(adb && kstr && vstr);
  return tcadbput(adb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Store a new record into an abstract database object. */
bool tcadbputkeep(TCADB *adb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  char numbuf[TCNUMBUFSIZ];
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      if(tcmdbputkeep(adb->mdb, kbuf, ksiz, vbuf, vsiz)){
        if(adb->capnum > 0 || adb->capsiz > 0){
          adb->capcnt++;
          if((adb->capcnt & 0xff) == 0){
            if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum + 0x100)
              tcmdbcutfront(adb->mdb, 0x100);
            if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz)
              tcmdbcutfront(adb->mdb, 0x200);
          }
        }
      } else {
        err = true;
      }
      break;
    case ADBONDB:
      if(tcndbputkeep(adb->ndb, kbuf, ksiz, vbuf, vsiz)){
        if(adb->capnum > 0 || adb->capsiz > 0){
          adb->capcnt++;
          if((adb->capcnt & 0xff) == 0){
            if(adb->capnum > 0 && tcndbrnum(adb->ndb) > adb->capnum + 0x100)
              tcndbcutfringe(adb->ndb, 0x100);
            if(adb->capsiz > 0 && tcndbmsiz(adb->ndb) > adb->capsiz)
              tcndbcutfringe(adb->ndb, 0x200);
          }
        }
      } else {
        err = true;
      }
      break;
    case ADBOHDB:
      if(!tchdbputkeep(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbputkeep(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbputkeep2(adb->fdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOTDB:
      if(ksiz < 1){
        ksiz = sprintf(numbuf, "%lld", (long long)tctdbgenuid(adb->tdb));
        kbuf = numbuf;
      }
      if(!tctdbputkeep2(adb->tdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->putkeep){
        if(!skel->putkeep(skel->opq, kbuf, ksiz, vbuf, vsiz)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Store a new string record into an abstract database object. */
bool tcadbputkeep2(TCADB *adb, const char *kstr, const char *vstr){
  assert(adb && kstr && vstr);
  return tcadbputkeep(adb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Concatenate a value at the end of the existing record in an abstract database object. */
bool tcadbputcat(TCADB *adb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  char numbuf[TCNUMBUFSIZ];
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      if(adb->capnum > 0 || adb->capsiz > 0){
        tcmdbputcat3(adb->mdb, kbuf, ksiz, vbuf, vsiz);
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum + 0x100)
            tcmdbcutfront(adb->mdb, 0x100);
          if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz)
            tcmdbcutfront(adb->mdb, 0x200);
        }
      } else {
        tcmdbputcat(adb->mdb, kbuf, ksiz, vbuf, vsiz);
      }
      break;
    case ADBONDB:
      tcndbputcat(adb->ndb, kbuf, ksiz, vbuf, vsiz);
      if(adb->capnum > 0 || adb->capsiz > 0){
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcndbrnum(adb->ndb) > adb->capnum + 0x100)
            tcndbcutfringe(adb->ndb, 0x100);
          if(adb->capsiz > 0 && tcndbmsiz(adb->ndb) > adb->capsiz)
            tcndbcutfringe(adb->ndb, 0x200);
        }
      }
      break;
    case ADBOHDB:
      if(!tchdbputcat(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbputcat(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbputcat2(adb->fdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOTDB:
      if(ksiz < 1){
        ksiz = sprintf(numbuf, "%lld", (long long)tctdbgenuid(adb->tdb));
        kbuf = numbuf;
      }
      if(!tctdbputcat2(adb->tdb, kbuf, ksiz, vbuf, vsiz)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->putcat){
        if(!skel->putcat(skel->opq, kbuf, ksiz, vbuf, vsiz)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Concatenate a string value at the end of the existing record in an abstract database object. */
bool tcadbputcat2(TCADB *adb, const char *kstr, const char *vstr){
  assert(adb && kstr && vstr);
  return tcadbputcat(adb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Remove a record of an abstract database object. */
bool tcadbout(TCADB *adb, const void *kbuf, int ksiz){
  assert(adb && kbuf && ksiz >= 0);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      if(!tcmdbout(adb->mdb, kbuf, ksiz)) err = true;
      break;
    case ADBONDB:
      if(!tcndbout(adb->ndb, kbuf, ksiz)) err = true;
      break;
    case ADBOHDB:
      if(!tchdbout(adb->hdb, kbuf, ksiz)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbout(adb->bdb, kbuf, ksiz)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbout2(adb->fdb, kbuf, ksiz)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbout(adb->tdb, kbuf, ksiz)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->out){
        if(!skel->out(skel->opq, kbuf, ksiz)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Remove a string record of an abstract database object. */
bool tcadbout2(TCADB *adb, const char *kstr){
  assert(adb && kstr);
  return tcadbout(adb, kstr, strlen(kstr));
}


/* Retrieve a record in an abstract database object. */
void *tcadbget(TCADB *adb, const void *kbuf, int ksiz, int *sp){
  assert(adb && kbuf && ksiz >= 0 && sp);
  char *rv;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = tcmdbget(adb->mdb, kbuf, ksiz, sp);
      break;
    case ADBONDB:
      rv = tcndbget(adb->ndb, kbuf, ksiz, sp);
      break;
    case ADBOHDB:
      rv = tchdbget(adb->hdb, kbuf, ksiz, sp);
      break;
    case ADBOBDB:
      rv = tcbdbget(adb->bdb, kbuf, ksiz, sp);
      break;
    case ADBOFDB:
      rv = tcfdbget2(adb->fdb, kbuf, ksiz, sp);
      break;
    case ADBOTDB:
      rv = tctdbget2(adb->tdb, kbuf, ksiz, sp);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->get){
        rv = skel->get(skel->opq, kbuf, ksiz, sp);
      } else {
        rv = NULL;
      }
      break;
    default:
      rv = NULL;
      break;
  }
  return rv;
}


/* Retrieve a string record in an abstract database object. */
char *tcadbget2(TCADB *adb, const char *kstr){
  assert(adb && kstr);
  int vsiz;
  return tcadbget(adb, kstr, strlen(kstr), &vsiz);
}


/* Get the size of the value of a record in an abstract database object. */
int tcadbvsiz(TCADB *adb, const void *kbuf, int ksiz){
  assert(adb && kbuf && ksiz >= 0);
  int rv;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = tcmdbvsiz(adb->mdb, kbuf, ksiz);
      break;
    case ADBONDB:
      rv = tcndbvsiz(adb->ndb, kbuf, ksiz);
      break;
    case ADBOHDB:
      rv = tchdbvsiz(adb->hdb, kbuf, ksiz);
      break;
    case ADBOBDB:
      rv = tcbdbvsiz(adb->bdb, kbuf, ksiz);
      break;
    case ADBOFDB:
      rv = tcfdbvsiz2(adb->fdb, kbuf, ksiz);
      break;
    case ADBOTDB:
      rv = tctdbvsiz(adb->tdb, kbuf, ksiz);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->vsiz){
        rv = skel->vsiz(skel->opq, kbuf, ksiz);
      } else {
        rv = -1;
      }
      break;
    default:
      rv = -1;
      break;
  }
  return rv;
}


/* Get the size of the value of a string record in an abstract database object. */
int tcadbvsiz2(TCADB *adb, const char *kstr){
  assert(adb && kstr);
  return tcadbvsiz(adb, kstr, strlen(kstr));
}


/* Initialize the iterator of an abstract database object. */
bool tcadbiterinit(TCADB *adb){
  assert(adb);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      tcmdbiterinit(adb->mdb);
      break;
    case ADBONDB:
      tcndbiterinit(adb->ndb);
      break;
    case ADBOHDB:
      if(!tchdbiterinit(adb->hdb)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbcurfirst(adb->cur)){
        int ecode = tcbdbecode(adb->bdb);
        if(ecode != TCESUCCESS && ecode != TCEINVALID && ecode != TCEKEEP && ecode != TCENOREC)
          err = true;
      }
      break;
    case ADBOFDB:
      if(!tcfdbiterinit(adb->fdb)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbiterinit(adb->tdb)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->iterinit){
        if(!skel->iterinit(skel->opq)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Get the next key of the iterator of an abstract database object. */
void *tcadbiternext(TCADB *adb, int *sp){
  assert(adb && sp);
  char *rv;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = tcmdbiternext(adb->mdb, sp);
      break;
    case ADBONDB:
      rv = tcndbiternext(adb->ndb, sp);
      break;
    case ADBOHDB:
      rv = tchdbiternext(adb->hdb, sp);
      break;
    case ADBOBDB:
      rv = tcbdbcurkey(adb->cur, sp);
      tcbdbcurnext(adb->cur);
      break;
    case ADBOFDB:
      rv = tcfdbiternext2(adb->fdb, sp);
      break;
    case ADBOTDB:
      rv = tctdbiternext(adb->tdb, sp);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->iternext){
        rv = skel->iternext(skel->opq, sp);
      } else {
        rv = NULL;
      }
      break;
    default:
      rv = NULL;
      break;
  }
  return rv;
}


/* Get the next key string of the iterator of an abstract database object. */
char *tcadbiternext2(TCADB *adb){
  assert(adb);
  int vsiz;
  return tcadbiternext(adb, &vsiz);
}


/* Get forward matching keys in an abstract database object. */
TCLIST *tcadbfwmkeys(TCADB *adb, const void *pbuf, int psiz, int max){
  assert(adb && pbuf && psiz >= 0);
  TCLIST *rv;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = tcmdbfwmkeys(adb->mdb, pbuf, psiz, max);
      break;
    case ADBONDB:
      rv = tcndbfwmkeys(adb->ndb, pbuf, psiz, max);
      break;
    case ADBOHDB:
      rv = tchdbfwmkeys(adb->hdb, pbuf, psiz, max);
      break;
    case ADBOBDB:
      rv = tcbdbfwmkeys(adb->bdb, pbuf, psiz, max);
      break;
    case ADBOFDB:
      rv = tcfdbrange4(adb->fdb, pbuf, psiz, max);
      break;
    case ADBOTDB:
      rv = tctdbfwmkeys(adb->tdb, pbuf, psiz, max);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->fwmkeys){
        rv = skel->fwmkeys(skel->opq, pbuf, psiz, max);
      } else {
        rv = NULL;
      }
      break;
    default:
      rv = tclistnew();
      break;
  }
  return rv;
}


/* Get forward matching string keys in an abstract database object. */
TCLIST *tcadbfwmkeys2(TCADB *adb, const char *pstr, int max){
  assert(adb && pstr);
  return tcadbfwmkeys(adb, pstr, strlen(pstr), max);
}


/* Add an integer to a record in an abstract database object. */
int tcadbaddint(TCADB *adb, const void *kbuf, int ksiz, int num){
  assert(adb && kbuf && ksiz >= 0);
  int rv;
  char numbuf[TCNUMBUFSIZ];
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = tcmdbaddint(adb->mdb, kbuf, ksiz, num);
      if(adb->capnum > 0 || adb->capsiz > 0){
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum + 0x100)
            tcmdbcutfront(adb->mdb, 0x100);
          if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz)
            tcmdbcutfront(adb->mdb, 0x200);
        }
      }
      break;
    case ADBONDB:
      rv = tcndbaddint(adb->ndb, kbuf, ksiz, num);
      if(adb->capnum > 0 || adb->capsiz > 0){
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcndbrnum(adb->ndb) > adb->capnum + 0x100)
            tcndbcutfringe(adb->ndb, 0x100);
          if(adb->capsiz > 0 && tcndbmsiz(adb->ndb) > adb->capsiz)
            tcndbcutfringe(adb->ndb, 0x200);
        }
      }
      break;
    case ADBOHDB:
      rv = tchdbaddint(adb->hdb, kbuf, ksiz, num);
      break;
    case ADBOBDB:
      rv = tcbdbaddint(adb->bdb, kbuf, ksiz, num);
      break;
    case ADBOFDB:
      rv = tcfdbaddint(adb->fdb, tcfdbkeytoid(kbuf, ksiz), num);
      break;
    case ADBOTDB:
      if(ksiz < 1){
        ksiz = sprintf(numbuf, "%lld", (long long)tctdbgenuid(adb->tdb));
        kbuf = numbuf;
      }
      rv = tctdbaddint(adb->tdb, kbuf, ksiz, num);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->addint){
        rv = skel->addint(skel->opq, kbuf, ksiz, num);
      } else {
        rv = INT_MIN;
      }
      break;
    default:
      rv = INT_MIN;
      break;
  }
  return rv;
}


/* Add a real number to a record in an abstract database object. */
double tcadbadddouble(TCADB *adb, const void *kbuf, int ksiz, double num){
  assert(adb && kbuf && ksiz >= 0);
  double rv;
  char numbuf[TCNUMBUFSIZ];
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = tcmdbadddouble(adb->mdb, kbuf, ksiz, num);
      if(adb->capnum > 0 || adb->capsiz > 0){
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum + 0x100)
            tcmdbcutfront(adb->mdb, 0x100);
          if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz)
            tcmdbcutfront(adb->mdb, 0x200);
        }
      }
      break;
    case ADBONDB:
      rv = tcndbadddouble(adb->ndb, kbuf, ksiz, num);
      if(adb->capnum > 0 || adb->capsiz > 0){
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcndbrnum(adb->ndb) > adb->capnum + 0x100)
            tcndbcutfringe(adb->ndb, 0x100);
          if(adb->capsiz > 0 && tcndbmsiz(adb->ndb) > adb->capsiz)
            tcndbcutfringe(adb->ndb, 0x200);
        }
      }
      break;
    case ADBOHDB:
      rv = tchdbadddouble(adb->hdb, kbuf, ksiz, num);
      break;
    case ADBOBDB:
      rv = tcbdbadddouble(adb->bdb, kbuf, ksiz, num);
      break;
    case ADBOFDB:
      rv = tcfdbadddouble(adb->fdb, tcfdbkeytoid(kbuf, ksiz), num);
      break;
    case ADBOTDB:
      if(ksiz < 1){
        ksiz = sprintf(numbuf, "%lld", (long long)tctdbgenuid(adb->tdb));
        kbuf = numbuf;
      }
      rv = tctdbadddouble(adb->tdb, kbuf, ksiz, num);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->adddouble){
        rv = skel->adddouble(skel->opq, kbuf, ksiz, num);
      } else {
        rv = nan("");
      }
      break;
    default:
      rv = nan("");
      break;
  }
  return rv;
}


/* Synchronize updated contents of an abstract database object with the file and the device. */
bool tcadbsync(TCADB *adb){
  assert(adb);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      if(adb->capnum > 0){
        while(tcmdbrnum(adb->mdb) > adb->capnum){
          tcmdbcutfront(adb->mdb, 1);
        }
      }
      if(adb->capsiz > 0){
        while(tcmdbmsiz(adb->mdb) > adb->capsiz && tcmdbrnum(adb->mdb) > 0){
          tcmdbcutfront(adb->mdb, 1);
        }
      }
      adb->capcnt = 0;
      break;
    case ADBONDB:
      if(adb->capnum > 0 && tcndbrnum(adb->ndb) > adb->capnum)
        tcndbcutfringe(adb->ndb, tcndbrnum(adb->ndb) - adb->capnum);
      if(adb->capsiz > 0){
        while(tcndbmsiz(adb->ndb) > adb->capsiz && tcndbrnum(adb->ndb) > 0){
          tcndbcutfringe(adb->ndb, 0x100);
        }
      }
      adb->capcnt = 0;
      break;
    case ADBOHDB:
      if(!tchdbsync(adb->hdb)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbsync(adb->bdb)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbsync(adb->fdb)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbsync(adb->tdb)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->sync){
        if(!skel->sync(skel->opq)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Optimize the storage of an abstract database object. */
bool tcadboptimize(TCADB *adb, const char *params){
  assert(adb);
  TCLIST *elems = params ? tcstrsplit(params, "#") : tclistnew();
  int64_t bnum = -1;
  int64_t capnum = -1;
  int64_t capsiz = -1;
  int8_t apow = -1;
  int8_t fpow = -1;
  bool tdefault = true;
  bool tlmode = false;
  bool tdmode = false;
  bool tbmode = false;
  bool ttmode = false;
  int32_t lmemb = -1;
  int32_t nmemb = -1;
  int32_t width = -1;
  int64_t limsiz = -1;
  int ln = TCLISTNUM(elems);
  for(int i = 0; i < ln; i++){
    const char *elem = TCLISTVALPTR(elems, i);
    char *pv = strchr(elem, '=');
    if(!pv) continue;
    *(pv++) = '\0';
    if(!tcstricmp(elem, "bnum")){
      bnum = tcatoix(pv);
    } else if(!tcstricmp(elem, "capnum")){
      capnum = tcatoix(pv);
    } else if(!tcstricmp(elem, "capsiz")){
      capsiz = tcatoix(pv);
    } else if(!tcstricmp(elem, "apow")){
      apow = tcatoix(pv);
    } else if(!tcstricmp(elem, "fpow")){
      fpow = tcatoix(pv);
    } else if(!tcstricmp(elem, "opts")){
      tdefault = false;
      if(strchr(pv, 'l') || strchr(pv, 'L')) tlmode = true;
      if(strchr(pv, 'd') || strchr(pv, 'D')) tdmode = true;
      if(strchr(pv, 'b') || strchr(pv, 'B')) tbmode = true;
      if(strchr(pv, 't') || strchr(pv, 'T')) ttmode = true;
    } else if(!tcstricmp(elem, "lmemb")){
      lmemb = tcatoix(pv);
    } else if(!tcstricmp(elem, "nmemb")){
      nmemb = tcatoix(pv);
    } else if(!tcstricmp(elem, "width")){
      width = tcatoix(pv);
    } else if(!tcstricmp(elem, "limsiz")){
      limsiz = tcatoix(pv);
    }
  }
  tclistdel(elems);
  bool err = false;
  int opts;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      adb->capnum = capnum;
      adb->capsiz = capsiz;
      tcadbsync(adb);
      break;
    case ADBONDB:
      adb->capnum = capnum;
      adb->capsiz = capsiz;
      tcadbsync(adb);
      break;
    case ADBOHDB:
      opts = 0;
      if(tdefault){
        opts = UINT8_MAX;
      } else {
        if(tlmode) opts |= HDBTLARGE;
        if(tdmode) opts |= HDBTDEFLATE;
        if(tbmode) opts |= HDBTBZIP;
        if(ttmode) opts |= HDBTTCBS;
      }
      if(!tchdboptimize(adb->hdb, bnum, apow, fpow, opts)) err = true;
      break;
    case ADBOBDB:
      opts = 0;
      if(tdefault){
        opts = UINT8_MAX;
      } else {
        if(tlmode) opts |= BDBTLARGE;
        if(tdmode) opts |= BDBTDEFLATE;
        if(tbmode) opts |= BDBTBZIP;
        if(ttmode) opts |= BDBTTCBS;
      }
      if(!tcbdboptimize(adb->bdb, lmemb, nmemb, bnum, apow, fpow, opts)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdboptimize(adb->fdb, width, limsiz)) err = true;
      break;
    case ADBOTDB:
      opts = 0;
      if(tdefault){
        opts = UINT8_MAX;
      } else {
        if(tlmode) opts |= TDBTLARGE;
        if(tdmode) opts |= TDBTDEFLATE;
        if(tbmode) opts |= TDBTBZIP;
        if(ttmode) opts |= TDBTTCBS;
      }
      if(!tctdboptimize(adb->tdb, bnum, apow, fpow, opts)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->optimize){
        if(!skel->optimize(skel->opq, params)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Remove all records of an abstract database object. */
bool tcadbvanish(TCADB *adb){
  assert(adb);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      tcmdbvanish(adb->mdb);
      break;
    case ADBONDB:
      tcndbvanish(adb->ndb);
      break;
    case ADBOHDB:
      if(!tchdbvanish(adb->hdb)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbvanish(adb->bdb)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbvanish(adb->fdb)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbvanish(adb->tdb)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->vanish){
        if(!skel->vanish(skel->opq)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Copy the database file of an abstract database object. */
bool tcadbcopy(TCADB *adb, const char *path){
  assert(adb && path);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
    case ADBONDB:
      if(*path == '@'){
        char tsbuf[TCNUMBUFSIZ];
        sprintf(tsbuf, "%llu", (unsigned long long)(tctime() * 1000000));
        const char *args[2];
        args[0] = path + 1;
        args[1] = tsbuf;
        if(tcsystem(args, sizeof(args) / sizeof(*args)) != 0) err = true;
      } else {
        TCADB *tadb = tcadbnew();
        if(tcadbopen(tadb, path)){
          tcadbiterinit(adb);
          char *kbuf;
          int ksiz;
          while((kbuf = tcadbiternext(adb, &ksiz)) != NULL){
            int vsiz;
            char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
            if(vbuf){
              if(!tcadbput(tadb, kbuf, ksiz, vbuf, vsiz)) err = true;
              TCFREE(vbuf);
            }
            TCFREE(kbuf);
          }
          if(!tcadbclose(tadb)) err = true;
        } else {
          err = true;
        }
        tcadbdel(tadb);
      }
      break;
    case ADBOHDB:
      if(!tchdbcopy(adb->hdb, path)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbcopy(adb->bdb, path)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbcopy(adb->fdb, path)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbcopy(adb->tdb, path)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->copy){
        if(!skel->copy(skel->opq, path)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Begin the transaction of an abstract database object. */
bool tcadbtranbegin(TCADB *adb){
  assert(adb);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      err = true;
      break;
    case ADBONDB:
      err = true;
      break;
    case ADBOHDB:
      if(!tchdbtranbegin(adb->hdb)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbtranbegin(adb->bdb)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbtranbegin(adb->fdb)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbtranbegin(adb->tdb)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->tranbegin){
        if(!skel->tranbegin(skel->opq)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Commit the transaction of an abstract database object. */
bool tcadbtrancommit(TCADB *adb){
  assert(adb);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      err = true;
      break;
    case ADBONDB:
      err = true;
      break;
    case ADBOHDB:
      if(!tchdbtrancommit(adb->hdb)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbtrancommit(adb->bdb)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbtrancommit(adb->fdb)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbtrancommit(adb->tdb)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->trancommit){
        if(!skel->trancommit(skel->opq)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Abort the transaction of an abstract database object. */
bool tcadbtranabort(TCADB *adb){
  assert(adb);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      err = true;
      break;
    case ADBONDB:
      err = true;
      break;
    case ADBOHDB:
      if(!tchdbtranabort(adb->hdb)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbtranabort(adb->bdb)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbtranabort(adb->fdb)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbtranabort(adb->tdb)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->tranabort){
        if(!skel->tranabort(skel->opq)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Get the file path of an abstract database object. */
const char *tcadbpath(TCADB *adb){
  assert(adb);
  const char *rv;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = "*";
      break;
    case ADBONDB:
      rv = "+";
      break;
    case ADBOHDB:
      rv = tchdbpath(adb->hdb);
      break;
    case ADBOBDB:
      rv = tcbdbpath(adb->bdb);
      break;
    case ADBOFDB:
      rv = tcfdbpath(adb->fdb);
      break;
    case ADBOTDB:
      rv = tctdbpath(adb->tdb);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->path){
        rv = skel->path(skel->opq);
      } else {
        rv = NULL;
      }
      break;
    default:
      rv = NULL;
      break;
  }
  return rv;
}


/* Get the number of records of an abstract database object. */
uint64_t tcadbrnum(TCADB *adb){
  assert(adb);
  uint64_t rv;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = tcmdbrnum(adb->mdb);
      break;
    case ADBONDB:
      rv = tcndbrnum(adb->ndb);
      break;
    case ADBOHDB:
      rv = tchdbrnum(adb->hdb);
      break;
    case ADBOBDB:
      rv = tcbdbrnum(adb->bdb);
      break;
    case ADBOFDB:
      rv = tcfdbrnum(adb->fdb);
      break;
    case ADBOTDB:
      rv = tctdbrnum(adb->tdb);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->rnum){
        rv = skel->rnum(skel->opq);
      } else {
        rv = 0;
      }
      break;
    default:
      rv = 0;
      break;
  }
  return rv;
}


/* Get the size of the database of an abstract database object. */
uint64_t tcadbsize(TCADB *adb){
  assert(adb);
  uint64_t rv;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      rv = tcmdbmsiz(adb->mdb);
      break;
    case ADBONDB:
      rv = tcndbmsiz(adb->ndb);
      break;
    case ADBOHDB:
      rv = tchdbfsiz(adb->hdb);
      break;
    case ADBOBDB:
      rv = tcbdbfsiz(adb->bdb);
      break;
    case ADBOFDB:
      rv = tcfdbfsiz(adb->fdb);
      break;
    case ADBOTDB:
      rv = tctdbfsiz(adb->tdb);
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->size){
        rv = skel->size(skel->opq);
      } else {
        rv = 0;
      }
      break;
    default:
      rv = 0;
      break;
  }
  return rv;
}


/* Call a versatile function for miscellaneous operations of an abstract database object. */
TCLIST *tcadbmisc(TCADB *adb, const char *name, const TCLIST *args){
  assert(adb && name && args);
  int argc = TCLISTNUM(args);
  TCLIST *rv;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      if(!strcmp(name, "put") || !strcmp(name, "putkeep") || !strcmp(name, "putcat")){
        if(argc > 1){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          const char *vbuf;
          int vsiz;
          TCLISTVAL(vbuf, args, 1, vsiz);
          bool err = false;
          if(!strcmp(name, "put")){
            tcmdbput(adb->mdb, kbuf, ksiz, vbuf, vsiz);
          } else if(!strcmp(name, "putkeep")){
            if(!tcmdbputkeep(adb->mdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putcat")){
            tcmdbputcat(adb->mdb, kbuf, ksiz, vbuf, vsiz);
          }
          if(err){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "out")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          if(!tcmdbout(adb->mdb, kbuf, ksiz)){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "get")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int vsiz;
          char *vbuf = tcmdbget(adb->mdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          } else {
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "putlist")){
        rv = tclistnew2(1);
        argc--;
        for(int i = 0; i < argc; i += 2){
          const char *kbuf, *vbuf;
          int ksiz, vsiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          TCLISTVAL(vbuf, args, i + 1, vsiz);
          tcmdbput(adb->mdb, kbuf, ksiz, vbuf, vsiz);
        }
      } else if(!strcmp(name, "outlist")){
        rv = tclistnew2(1);
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          tcmdbout(adb->mdb, kbuf, ksiz);
        }
      } else if(!strcmp(name, "getlist")){
        rv = tclistnew2(argc * 2);
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          int vsiz;
          char *vbuf = tcmdbget(adb->mdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, kbuf, ksiz);
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          }
        }
      } else if(!strcmp(name, "getpart")){
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int off = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
          if(off < 0) off = 0;
          if(off > INT_MAX / 2 - 1) off = INT_MAX - 1;
          int len = argc > 2 ? tcatoi(TCLISTVALPTR(args, 2)) : -1;
          if(len < 0 || len > INT_MAX / 2) len = INT_MAX / 2;
          int vsiz;
          char *vbuf = tcmdbget(adb->mdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            if(off < vsiz){
              rv = tclistnew2(1);
              vsiz -= off;
              if(vsiz > len) vsiz = len;
              if(off > 0) memmove(vbuf, vbuf + off, vsiz);
              tclistpushmalloc(rv, vbuf, vsiz);
            } else {
              rv = NULL;
              TCFREE(vbuf);
            }
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "iterinit")){
        rv = tclistnew2(1);
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          tcmdbiterinit2(adb->mdb, kbuf, ksiz);
        } else {
          tcmdbiterinit(adb->mdb);
        }
      } else if(!strcmp(name, "iternext")){
        rv = tclistnew2(1);
        int ksiz;
        char *kbuf = tcmdbiternext(adb->mdb, &ksiz);
        if(kbuf){
          TCLISTPUSH(rv, kbuf, ksiz);
          int vsiz;
          char *vbuf = tcmdbget(adb->mdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          }
          TCFREE(kbuf);
        } else {
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "sync")){
        rv = tclistnew2(1);
        if(!tcadbsync(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "optimize")){
        rv = tclistnew2(1);
        const char *params = argc > 0 ? TCLISTVALPTR(args, 0) : NULL;
        if(!tcadboptimize(adb, params)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "vanish")){
        rv = tclistnew2(1);
        if(!tcadbvanish(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "regex")){
        if(argc > 0){
          const char *regex = TCLISTVALPTR(args, 0);
          int options = REG_EXTENDED | REG_NOSUB;
          if(*regex == '*'){
            options |= REG_ICASE;
            regex++;
          }
          regex_t rbuf;
          if(regcomp(&rbuf, regex, options) == 0){
            rv = tclistnew();
            int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
            if(max < 1) max = INT_MAX;
            tcmdbiterinit(adb->mdb);
            char *kbuf;
            int ksiz;
            while(max > 0 && (kbuf = tcmdbiternext(adb->mdb, &ksiz))){
              if(regexec(&rbuf, kbuf, 0, NULL, 0) == 0){
                int vsiz;
                char *vbuf = tcmdbget(adb->mdb, kbuf, ksiz, &vsiz);
                if(vbuf){
                  TCLISTPUSH(rv, kbuf, ksiz);
                  TCLISTPUSH(rv, vbuf, vsiz);
                  TCFREE(vbuf);
                  max--;
                }
              }
              TCFREE(kbuf);
            }
            regfree(&rbuf);
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else {
        rv = NULL;
      }
      break;
    case ADBONDB:
      if(!strcmp(name, "put") || !strcmp(name, "putkeep") || !strcmp(name, "putcat")){
        if(argc > 1){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          const char *vbuf;
          int vsiz;
          TCLISTVAL(vbuf, args, 1, vsiz);
          bool err = false;
          if(!strcmp(name, "put")){
            tcndbput(adb->ndb, kbuf, ksiz, vbuf, vsiz);
          } else if(!strcmp(name, "putkeep")){
            if(!tcndbputkeep(adb->ndb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putcat")){
            tcndbputcat(adb->ndb, kbuf, ksiz, vbuf, vsiz);
          }
          if(err){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "out")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          if(!tcndbout(adb->ndb, kbuf, ksiz)){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "get")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int vsiz;
          char *vbuf = tcndbget(adb->ndb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          } else {
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "putlist")){
        rv = tclistnew2(1);
        argc--;
        for(int i = 0; i < argc; i += 2){
          const char *kbuf, *vbuf;
          int ksiz, vsiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          TCLISTVAL(vbuf, args, i + 1, vsiz);
          tcndbput(adb->ndb, kbuf, ksiz, vbuf, vsiz);
        }
      } else if(!strcmp(name, "outlist")){
        rv = tclistnew2(1);
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          tcndbout(adb->ndb, kbuf, ksiz);
        }
      } else if(!strcmp(name, "getlist")){
        rv = tclistnew2(argc * 2);
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          int vsiz;
          char *vbuf = tcndbget(adb->ndb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, kbuf, ksiz);
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          }
        }
      } else if(!strcmp(name, "getpart")){
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int off = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
          if(off < 0) off = 0;
          if(off > INT_MAX / 2 - 1) off = INT_MAX - 1;
          int len = argc > 2 ? tcatoi(TCLISTVALPTR(args, 2)) : -1;
          if(len < 0 || len > INT_MAX / 2) len = INT_MAX / 2;
          int vsiz;
          char *vbuf = tcndbget(adb->ndb, kbuf, ksiz, &vsiz);
          if(vbuf){
            if(off < vsiz){
              rv = tclistnew2(1);
              vsiz -= off;
              if(vsiz > len) vsiz = len;
              if(off > 0) memmove(vbuf, vbuf + off, vsiz);
              tclistpushmalloc(rv, vbuf, vsiz);
            } else {
              rv = NULL;
              TCFREE(vbuf);
            }
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "iterinit")){
        rv = tclistnew2(1);
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          tcndbiterinit2(adb->ndb, kbuf, ksiz);
        } else {
          tcndbiterinit(adb->ndb);
        }
      } else if(!strcmp(name, "iternext")){
        rv = tclistnew2(1);
        int ksiz;
        char *kbuf = tcndbiternext(adb->ndb, &ksiz);
        if(kbuf){
          TCLISTPUSH(rv, kbuf, ksiz);
          int vsiz;
          char *vbuf = tcndbget(adb->ndb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          }
          TCFREE(kbuf);
        } else {
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "sync")){
        rv = tclistnew2(1);
        if(!tcadbsync(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "optimize")){
        rv = tclistnew2(1);
        const char *params = argc > 0 ? TCLISTVALPTR(args, 0) : NULL;
        if(!tcadboptimize(adb, params)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "vanish")){
        rv = tclistnew2(1);
        if(!tcadbvanish(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "regex")){
        if(argc > 0){
          const char *regex = TCLISTVALPTR(args, 0);
          int options = REG_EXTENDED | REG_NOSUB;
          if(*regex == '*'){
            options |= REG_ICASE;
            regex++;
          }
          regex_t rbuf;
          if(regcomp(&rbuf, regex, options) == 0){
            rv = tclistnew();
            int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
            if(max < 1) max = INT_MAX;
            tcndbiterinit(adb->ndb);
            char *kbuf;
            int ksiz;
            while(max > 0 && (kbuf = tcndbiternext(adb->ndb, &ksiz))){
              if(regexec(&rbuf, kbuf, 0, NULL, 0) == 0){
                int vsiz;
                char *vbuf = tcndbget(adb->ndb, kbuf, ksiz, &vsiz);
                if(vbuf){
                  TCLISTPUSH(rv, kbuf, ksiz);
                  TCLISTPUSH(rv, vbuf, vsiz);
                  TCFREE(vbuf);
                  max--;
                }
              }
              TCFREE(kbuf);
            }
            regfree(&rbuf);
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "range")){
        rv = tclistnew();
        int bksiz = 0;
        const char *bkbuf = NULL;
        if(argc > 0) TCLISTVAL(bkbuf, args, 0, bksiz);
        int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
        if(max < 1) max = INT_MAX;
        int eksiz = 0;
        const char *ekbuf = NULL;
        if(argc > 2) TCLISTVAL(ekbuf, args, 2, eksiz);
        if(bkbuf){
          tcndbiterinit2(adb->ndb, bkbuf, bksiz);
        } else {
          tcndbiterinit(adb->ndb);
        }
        char *kbuf;
        int ksiz;
        while(max > 0 && (kbuf = tcndbiternext(adb->ndb, &ksiz)) != NULL){
          if(ekbuf && tccmplexical(kbuf, ksiz, ekbuf, eksiz, NULL) >= 0){
            TCFREE(kbuf);
            break;
          }
          int vsiz;
          char *vbuf = tcndbget(adb->ndb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, kbuf, ksiz);
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
            max--;
          }
          TCFREE(kbuf);
        }
      } else {
        rv = NULL;
      }
      break;
    case ADBOHDB:
      if(!strcmp(name, "put") || !strcmp(name, "putkeep") || !strcmp(name, "putcat")){
        if(argc > 1){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          const char *vbuf;
          int vsiz;
          TCLISTVAL(vbuf, args, 1, vsiz);
          bool err = false;
          if(!strcmp(name, "put")){
            if(!tchdbput(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putkeep")){
            if(!tchdbputkeep(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putcat")){
            if(!tchdbputcat(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          }
          if(err){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "out")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          if(!tchdbout(adb->hdb, kbuf, ksiz)){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "get")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int vsiz;
          char *vbuf = tchdbget(adb->hdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          } else {
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "putlist")){
        rv = tclistnew2(1);
        bool err = false;
        argc--;
        for(int i = 0; i < argc; i += 2){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          int vsiz;
          const char *vbuf = tclistval(args, i + 1, &vsiz);
          if(!tchdbput(adb->hdb, kbuf, ksiz, vbuf, vsiz)){
            err = true;
            break;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "outlist")){
        rv = tclistnew2(1);
        bool err = false;
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          if(!tchdbout(adb->hdb, kbuf, ksiz) && tchdbecode(adb->hdb) != TCENOREC){
            err = true;
            break;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "getlist")){
        rv = tclistnew2(argc * 2);
        bool err = false;
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          int vsiz;
          char *vbuf = tchdbget(adb->hdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, kbuf, ksiz);
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          } else if(tchdbecode(adb->hdb) != TCENOREC){
            err = true;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "getpart")){
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int off = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
          if(off < 0) off = 0;
          if(off > INT_MAX / 2 - 1) off = INT_MAX - 1;
          int len = argc > 2 ? tcatoi(TCLISTVALPTR(args, 2)) : -1;
          if(len < 0 || len > INT_MAX / 2) len = INT_MAX / 2;
          int vsiz;
          char *vbuf = tchdbget(adb->hdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            if(off < vsiz){
              rv = tclistnew2(1);
              vsiz -= off;
              if(vsiz > len) vsiz = len;
              if(off > 0) memmove(vbuf, vbuf + off, vsiz);
              tclistpushmalloc(rv, vbuf, vsiz);
            } else {
              rv = NULL;
              TCFREE(vbuf);
            }
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "iterinit")){
        rv = tclistnew2(1);
        bool err = false;
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          if(!tchdbiterinit2(adb->hdb, kbuf, ksiz)) err = true;
        } else {
          if(!tchdbiterinit(adb->hdb)) err = true;
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "iternext")){
        rv = tclistnew2(1);
        int ksiz;
        char *kbuf = tchdbiternext(adb->hdb, &ksiz);
        if(kbuf){
          TCLISTPUSH(rv, kbuf, ksiz);
          int vsiz;
          char *vbuf = tchdbget(adb->hdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          }
          TCFREE(kbuf);
        } else {
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "sync")){
        rv = tclistnew2(1);
        if(!tcadbsync(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "optimize")){
        rv = tclistnew2(1);
        const char *params = argc > 0 ? TCLISTVALPTR(args, 0) : NULL;
        if(!tcadboptimize(adb, params)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "vanish")){
        rv = tclistnew2(1);
        if(!tcadbvanish(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "error")){
        rv = tclistnew2(1);
        int ecode = tchdbecode(adb->hdb);
        tclistprintf(rv, "%d: %s", ecode, tchdberrmsg(ecode));
        uint8_t flags = tchdbflags(adb->hdb);
        if(flags & HDBFFATAL) tclistprintf(rv, "fatal");
      } else if(!strcmp(name, "defrag")){
        rv = tclistnew2(1);
        int64_t step = argc > 0 ? tcatoi(TCLISTVALPTR(args, 0)) : -1;
        if(!tchdbdefrag(adb->hdb, step)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "cacheclear")){
        rv = tclistnew2(1);
        if(!tchdbcacheclear(adb->hdb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "regex")){
        if(argc > 0){
          const char *regex = TCLISTVALPTR(args, 0);
          int options = REG_EXTENDED | REG_NOSUB;
          if(*regex == '*'){
            options |= REG_ICASE;
            regex++;
          }
          regex_t rbuf;
          if(regcomp(&rbuf, regex, options) == 0){
            rv = tclistnew();
            int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
            if(max < 1) max = INT_MAX;
            tchdbiterinit(adb->hdb);
            TCXSTR *kxstr = tcxstrnew();
            TCXSTR *vxstr = tcxstrnew();
            while(max > 0 && tchdbiternext3(adb->hdb, kxstr, vxstr)){
              const char *kbuf = TCXSTRPTR(kxstr);
              if(regexec(&rbuf, kbuf, 0, NULL, 0) == 0){
                TCLISTPUSH(rv, kbuf, TCXSTRSIZE(kxstr));
                TCLISTPUSH(rv, TCXSTRPTR(vxstr), TCXSTRSIZE(vxstr));
                max--;
              }
            }
            tcxstrdel(vxstr);
            tcxstrdel(kxstr);
            regfree(&rbuf);
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else {
        rv = NULL;
      }
      break;
    case ADBOBDB:
      if(!strcmp(name, "put") || !strcmp(name, "putkeep") || !strcmp(name, "putcat") ||
         !strcmp(name, "putdup") || !strcmp(name, "putdupback")){
        if(argc > 1){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          const char *vbuf;
          int vsiz;
          TCLISTVAL(vbuf, args, 1, vsiz);
          bool err = false;
          if(!strcmp(name, "put")){
            if(!tcbdbput(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putkeep")){
            if(!tcbdbputkeep(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putcat")){
            if(!tcbdbputcat(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putdup")){
            if(!tcbdbputdup(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putdupback")){
            if(!tcbdbputdupback(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          }
          if(err){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "out")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          if(!tcbdbout(adb->bdb, kbuf, ksiz)){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "get")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          TCLIST *vals = tcbdbget4(adb->bdb, kbuf, ksiz);
          if(vals){
            tclistdel(rv);
            rv = vals;
          } else {
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "putlist")){
        rv = tclistnew2(1);
        bool err = false;
        argc--;
        for(int i = 0; i < argc; i += 2){
          const char *kbuf, *vbuf;
          int ksiz, vsiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          TCLISTVAL(vbuf, args, i + 1, vsiz);
          if(!tcbdbputdup(adb->bdb, kbuf, ksiz, vbuf, vsiz)){
            err = true;
            break;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "outlist")){
        rv = tclistnew2(1);
        bool err = false;
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          if(!tcbdbout3(adb->bdb, kbuf, ksiz) && tcbdbecode(adb->bdb) != TCENOREC){
            err = true;
            break;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "getlist")){
        rv = tclistnew2(argc * 2);
        bool err = false;
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          TCLIST *vals = tcbdbget4(adb->bdb, kbuf, ksiz);
          if(vals){
            int vnum = TCLISTNUM(vals);
            for(int j = 0; j < vnum; j++){
              TCLISTPUSH(rv, kbuf, ksiz);
              const char *vbuf;
              int vsiz;
              TCLISTVAL(vbuf, vals, j, vsiz);
              TCLISTPUSH(rv, vbuf, vsiz);
            }
            tclistdel(vals);
          } else if(tcbdbecode(adb->bdb) != TCENOREC){
            err = true;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "getpart")){
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int off = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
          if(off < 0) off = 0;
          if(off > INT_MAX / 2 - 1) off = INT_MAX - 1;
          int len = argc > 2 ? tcatoi(TCLISTVALPTR(args, 2)) : -1;
          if(len < 0 || len > INT_MAX / 2) len = INT_MAX / 2;
          int vsiz;
          char *vbuf = tcbdbget(adb->bdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            if(off < vsiz){
              rv = tclistnew2(1);
              vsiz -= off;
              if(vsiz > len) vsiz = len;
              if(off > 0) memmove(vbuf, vbuf + off, vsiz);
              tclistpushmalloc(rv, vbuf, vsiz);
            } else {
              rv = NULL;
              TCFREE(vbuf);
            }
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "iterinit")){
        rv = tclistnew2(1);
        bool err = false;
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          if(!tcbdbcurjump(adb->cur, kbuf, ksiz)) err = true;
        } else {
          if(!tcbdbcurfirst(adb->cur)) err = true;
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "iternext")){
        rv = tclistnew2(1);
        int ksiz;
        const char *kbuf = tcbdbcurkey3(adb->cur, &ksiz);
        if(kbuf){
          TCLISTPUSH(rv, kbuf, ksiz);
          int vsiz;
          const char *vbuf = tcbdbcurval3(adb->cur, &vsiz);
          if(vbuf) TCLISTPUSH(rv, vbuf, vsiz);
          tcbdbcurnext(adb->cur);
        } else {
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "sync")){
        rv = tclistnew2(1);
        if(!tcadbsync(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "optimize")){
        rv = tclistnew2(1);
        const char *params = argc > 0 ? TCLISTVALPTR(args, 0) : NULL;
        if(!tcadboptimize(adb, params)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "vanish")){
        rv = tclistnew2(1);
        if(!tcadbvanish(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "error")){
        rv = tclistnew2(1);
        int ecode = tcbdbecode(adb->bdb);
        tclistprintf(rv, "%d: %s", ecode, tcbdberrmsg(ecode));
        uint8_t flags = tcbdbflags(adb->bdb);
        if(flags & BDBFFATAL) tclistprintf(rv, "fatal");
      } else if(!strcmp(name, "defrag")){
        rv = tclistnew2(1);
        int64_t step = argc > 0 ? tcatoi(TCLISTVALPTR(args, 0)) : -1;
        if(!tcbdbdefrag(adb->bdb, step)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "cacheclear")){
        rv = tclistnew2(1);
        if(!tcbdbcacheclear(adb->bdb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "regex")){
        if(argc > 0){
          const char *regex = TCLISTVALPTR(args, 0);
          int options = REG_EXTENDED | REG_NOSUB;
          if(*regex == '*'){
            options |= REG_ICASE;
            regex++;
          }
          regex_t rbuf;
          if(regcomp(&rbuf, regex, options) == 0){
            rv = tclistnew();
            int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
            if(max < 1) max = INT_MAX;
            BDBCUR *cur = tcbdbcurnew(adb->bdb);
            tcbdbcurfirst(cur);
            TCXSTR *kxstr = tcxstrnew();
            TCXSTR *vxstr = tcxstrnew();
            while(max > 0 && tcbdbcurrec(cur, kxstr, vxstr)){
              const char *kbuf = TCXSTRPTR(kxstr);
              if(regexec(&rbuf, kbuf, 0, NULL, 0) == 0){
                TCLISTPUSH(rv, kbuf, TCXSTRSIZE(kxstr));
                TCLISTPUSH(rv, TCXSTRPTR(vxstr), TCXSTRSIZE(vxstr));
                max--;
              }
              tcbdbcurnext(cur);
            }
            tcxstrdel(vxstr);
            tcxstrdel(kxstr);
            tcbdbcurdel(cur);
            regfree(&rbuf);
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "range")){
        rv = tclistnew();
        int bksiz = 0;
        const char *bkbuf = NULL;
        if(argc > 0) TCLISTVAL(bkbuf, args, 0, bksiz);
        int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
        if(max < 1) max = INT_MAX;
        int eksiz = 0;
        const char *ekbuf = NULL;
        if(argc > 2) TCLISTVAL(ekbuf, args, 2, eksiz);
        TCCMP cmp = tcbdbcmpfunc(adb->bdb);
        void *cmpop = tcbdbcmpop(adb->bdb);
        BDBCUR *cur = tcbdbcurnew(adb->bdb);
        if(bkbuf){
          tcbdbcurjump(cur, bkbuf, bksiz);
        } else {
          tcbdbcurfirst(cur);
        }
        TCXSTR *kxstr = tcxstrnew();
        TCXSTR *vxstr = tcxstrnew();
        while(max > 0 && tcbdbcurrec(cur, kxstr, vxstr)){
          const char *kbuf = TCXSTRPTR(kxstr);
          int ksiz = TCXSTRSIZE(kxstr);
          if(ekbuf && cmp(kbuf, ksiz, ekbuf, eksiz, cmpop) >= 0) break;
          TCLISTPUSH(rv, kbuf, ksiz);
          TCLISTPUSH(rv, TCXSTRPTR(vxstr), TCXSTRSIZE(vxstr));
          max--;
          tcbdbcurnext(cur);
        }
        tcxstrdel(vxstr);
        tcxstrdel(kxstr);
        tcbdbcurdel(cur);
      } else {
        rv = NULL;
      }
      break;
    case ADBOFDB:
      if(!strcmp(name, "put") || !strcmp(name, "putkeep") || !strcmp(name, "putcat")){
        if(argc > 1){
          rv = tclistnew2(1);
          const char *kbuf, *vbuf;
          int ksiz, vsiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          TCLISTVAL(vbuf, args, 1, vsiz);
          bool err = false;
          if(!strcmp(name, "put")){
            if(!tcfdbput2(adb->fdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putkeep")){
            if(!tcfdbputkeep2(adb->fdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          } else if(!strcmp(name, "putcat")){
            if(!tcfdbputcat2(adb->fdb, kbuf, ksiz, vbuf, vsiz)) err = true;
          }
          if(err){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "out")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          if(!tcfdbout2(adb->fdb, kbuf, ksiz)){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "get")){
        if(argc > 0){
          rv = tclistnew2(1);
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int vsiz;
          char *vbuf = tcfdbget2(adb->fdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          } else {
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "putlist")){
        rv = tclistnew2(1);
        bool err = false;
        argc--;
        for(int i = 0; i < argc; i += 2){
          const char *kbuf, *vbuf;
          int ksiz, vsiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          TCLISTVAL(vbuf, args, i + 1, vsiz);
          if(!tcfdbput2(adb->fdb, kbuf, ksiz, vbuf, vsiz)){
            err = true;
            break;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "outlist")){
        rv = tclistnew2(1);
        bool err = false;
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          if(!tcfdbout2(adb->fdb, kbuf, ksiz) && tcfdbecode(adb->fdb) != TCENOREC){
            err = true;
            break;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "getlist")){
        rv = tclistnew2(argc * 2);
        bool err = false;
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          int vsiz;
          char *vbuf = tcfdbget2(adb->fdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, kbuf, ksiz);
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          } else if(tcfdbecode(adb->fdb) != TCENOREC){
            err = true;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "getpart")){
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int off = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
          if(off < 0) off = 0;
          if(off > INT_MAX / 2 - 1) off = INT_MAX - 1;
          int len = argc > 2 ? tcatoi(TCLISTVALPTR(args, 2)) : -1;
          if(len < 0 || len > INT_MAX / 2) len = INT_MAX / 2;
          int vsiz;
          char *vbuf = tcfdbget2(adb->fdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            if(off < vsiz){
              rv = tclistnew2(1);
              vsiz -= off;
              if(vsiz > len) vsiz = len;
              if(off > 0) memmove(vbuf, vbuf + off, vsiz);
              tclistpushmalloc(rv, vbuf, vsiz);
            } else {
              rv = NULL;
              TCFREE(vbuf);
            }
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "iterinit")){
        rv = tclistnew2(1);
        bool err = false;
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          if(!tcfdbiterinit3(adb->fdb, kbuf, ksiz)) err = true;
        } else {
          if(!tcfdbiterinit(adb->fdb)) err = true;
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "iternext")){
        rv = tclistnew2(1);
        int ksiz;
        char *kbuf = tcfdbiternext2(adb->fdb, &ksiz);
        if(kbuf){
          TCLISTPUSH(rv, kbuf, ksiz);
          int vsiz;
          char *vbuf = tcfdbget2(adb->fdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          }
          TCFREE(kbuf);
        } else {
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "sync")){
        rv = tclistnew2(1);
        if(!tcadbsync(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "optimize")){
        rv = tclistnew2(1);
        const char *params = argc > 0 ? TCLISTVALPTR(args, 0) : NULL;
        if(!tcadboptimize(adb, params)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "vanish")){
        rv = tclistnew2(1);
        if(!tcadbvanish(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "error")){
        rv = tclistnew2(1);
        int ecode = tcfdbecode(adb->fdb);
        tclistprintf(rv, "%d: %s", ecode, tcfdberrmsg(ecode));
        uint8_t flags = tcfdbflags(adb->fdb);
        if(flags & FDBFFATAL) tclistprintf(rv, "fatal");
      } else if(!strcmp(name, "regex")){
        if(argc > 0){
          const char *regex = TCLISTVALPTR(args, 0);
          int options = REG_EXTENDED | REG_NOSUB;
          if(*regex == '*'){
            options |= REG_ICASE;
            regex++;
          }
          regex_t rbuf;
          if(regcomp(&rbuf, regex, options) == 0){
            rv = tclistnew();
            int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
            if(max < 1) max = INT_MAX;
            tcfdbiterinit(adb->fdb);
            char *kbuf;
            int ksiz;
            while(max > 0 && (kbuf = tcfdbiternext2(adb->fdb, &ksiz))){
              if(regexec(&rbuf, kbuf, 0, NULL, 0) == 0){
                int vsiz;
                char *vbuf = tcfdbget2(adb->fdb, kbuf, ksiz, &vsiz);
                if(vbuf){
                  TCLISTPUSH(rv, kbuf, ksiz);
                  TCLISTPUSH(rv, vbuf, vsiz);
                  TCFREE(vbuf);
                  max--;
                }
              }
              TCFREE(kbuf);
            }
            regfree(&rbuf);
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "range")){
        rv = tclistnew();
        int bksiz = 0;
        const char *bkbuf = NULL;
        if(argc > 0) TCLISTVAL(bkbuf, args, 0, bksiz);
        int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
        if(max < 1) max = INT_MAX;
        int eksiz = 0;
        const char *ekbuf = NULL;
        if(argc > 2) TCLISTVAL(ekbuf, args, 2, eksiz);
        if(bkbuf){
          tcfdbiterinit3(adb->fdb, bkbuf, bksiz);
        } else {
          tcfdbiterinit(adb->fdb);
        }
        int64_t eid = ekbuf ? tcfdbkeytoid(ekbuf, eksiz) : -1;
        char *kbuf;
        int ksiz;
        while(max > 0 && (kbuf = tcfdbiternext2(adb->fdb, &ksiz)) != NULL){
          if(eid > 0 && tcatoi(kbuf) >= eid){
            TCFREE(kbuf);
            break;
          }
          int vsiz;
          char *vbuf = tcfdbget2(adb->fdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, kbuf, ksiz);
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
            max--;
          }
          TCFREE(kbuf);
        }
      } else {
        rv = NULL;
      }
      break;
    case ADBOTDB:
      if(!strcmp(name, "put") || !strcmp(name, "putkeep") || !strcmp(name, "putcat")){
        if(argc > 0){
          rv = tclistnew2(1);
          char *pkbuf;
          int pksiz;
          TCLISTVAL(pkbuf, args, 0, pksiz);
          argc--;
          TCMAP *cols = tcmapnew2(argc);
          for(int i = 1; i < argc; i += 2){
            const char *kbuf, *vbuf;
            int ksiz, vsiz;
            TCLISTVAL(kbuf, args, i, ksiz);
            TCLISTVAL(vbuf, args, i + 1, vsiz);
            tcmapput(cols, kbuf, ksiz, vbuf, vsiz);
          }
          bool err = false;
          if(!strcmp(name, "put")){
            if(!tctdbput(adb->tdb, pkbuf, pksiz, cols)) err = true;
          } else if(!strcmp(name, "putkeep")){
            if(!tctdbputkeep(adb->tdb, pkbuf, pksiz, cols)) err = true;
          } else if(!strcmp(name, "putcat")){
            if(!tctdbputcat(adb->tdb, pkbuf, pksiz, cols)) err = true;
          }
          tcmapdel(cols);
          if(err){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "out")){
        if(argc > 0){
          rv = tclistnew2(1);
          char *pkbuf;
          int pksiz;
          TCLISTVAL(pkbuf, args, 0, pksiz);
          if(!tctdbout(adb->tdb, pkbuf, pksiz)){
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "get")){
        if(argc > 0){
          rv = tclistnew2(1);
          char *pkbuf;
          int pksiz;
          TCLISTVAL(pkbuf, args, 0, pksiz);
          TCMAP *cols = tctdbget(adb->tdb, pkbuf, pksiz);
          if(cols){
            tcmapiterinit(cols);
            const char *kbuf;
            int ksiz;
            while((kbuf = tcmapiternext(cols, &ksiz)) != NULL){
              int vsiz;
              const char *vbuf = tcmapiterval(kbuf, &vsiz);
              TCLISTPUSH(rv, kbuf, ksiz);
              TCLISTPUSH(rv, vbuf, vsiz);
            }
            tcmapdel(cols);
          } else {
            tclistdel(rv);
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "putlist")){
        rv = tclistnew2(1);
        bool err = false;
        argc--;
        for(int i = 0; i < argc; i += 2){
          const char *kbuf, *vbuf;
          int ksiz, vsiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          TCLISTVAL(vbuf, args, i + 1, vsiz);
          if(!tctdbput2(adb->tdb, kbuf, ksiz, vbuf, vsiz)){
            err = true;
            break;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "outlist")){
        rv = tclistnew2(1);
        bool err = false;
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          if(!tctdbout(adb->tdb, kbuf, ksiz) && tctdbecode(adb->tdb) != TCENOREC){
            err = true;
            break;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "getlist")){
        rv = tclistnew2(argc * 2);
        bool err = false;
        for(int i = 0; i < argc; i++){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, i, ksiz);
          int vsiz;
          char *vbuf = tctdbget2(adb->tdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            TCLISTPUSH(rv, kbuf, ksiz);
            TCLISTPUSH(rv, vbuf, vsiz);
            TCFREE(vbuf);
          } else if(tctdbecode(adb->tdb) != TCENOREC){
            err = true;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "getpart")){
        if(argc > 0){
          const char *kbuf;
          int ksiz;
          TCLISTVAL(kbuf, args, 0, ksiz);
          int off = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
          if(off < 0) off = 0;
          if(off > INT_MAX / 2 - 1) off = INT_MAX - 1;
          int len = argc > 2 ? tcatoi(TCLISTVALPTR(args, 2)) : -1;
          if(len < 0 || len > INT_MAX / 2) len = INT_MAX / 2;
          int vsiz;
          char *vbuf = tctdbget2(adb->tdb, kbuf, ksiz, &vsiz);
          if(vbuf){
            if(off < vsiz){
              rv = tclistnew2(1);
              vsiz -= off;
              if(vsiz > len) vsiz = len;
              if(off > 0) memmove(vbuf, vbuf + off, vsiz);
              tclistpushmalloc(rv, vbuf, vsiz);
            } else {
              rv = NULL;
              TCFREE(vbuf);
            }
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "iterinit")){
        rv = tclistnew2(1);
        bool err = false;
        if(argc > 0){
          const char *pkbuf;
          int pksiz;
          TCLISTVAL(pkbuf, args, 0, pksiz);
          if(!tctdbiterinit2(adb->tdb, pkbuf, pksiz)) err = true;
        } else {
          if(!tctdbiterinit(adb->tdb)) err = true;
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "iternext")){
        rv = tclistnew2(1);
        int pksiz;
        char *pkbuf = tctdbiternext(adb->tdb, &pksiz);
        if(pkbuf){
          TCLISTPUSH(rv, pkbuf, pksiz);
          int csiz;
          char *cbuf = tctdbget2(adb->tdb, pkbuf, pksiz, &csiz);
          if(cbuf){
            TCLISTPUSH(rv, cbuf, csiz);
            TCFREE(cbuf);
          }
          TCFREE(pkbuf);
        } else {
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "sync")){
        rv = tclistnew2(1);
        if(!tcadbsync(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "optimize")){
        rv = tclistnew2(1);
        const char *params = argc > 0 ? TCLISTVALPTR(args, 0) : NULL;
        if(!tcadboptimize(adb, params)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "vanish")){
        rv = tclistnew2(1);
        if(!tcadbvanish(adb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "error")){
        rv = tclistnew2(1);
        int ecode = tctdbecode(adb->tdb);
        tclistprintf(rv, "%d: %s", ecode, tctdberrmsg(ecode));
        uint8_t flags = tctdbflags(adb->tdb);
        if(flags & TDBFFATAL) tclistprintf(rv, "fatal");
      } else if(!strcmp(name, "defrag")){
        rv = tclistnew2(1);
        int64_t step = argc > 0 ? tcatoi(TCLISTVALPTR(args, 0)) : -1;
        if(!tctdbdefrag(adb->tdb, step)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "cacheclear")){
        rv = tclistnew2(1);
        if(!tctdbcacheclear(adb->tdb)){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "regex")){
        if(argc > 0){
          const char *regex = TCLISTVALPTR(args, 0);
          int options = REG_EXTENDED | REG_NOSUB;
          if(*regex == '*'){
            options |= REG_ICASE;
            regex++;
          }
          regex_t rbuf;
          if(regcomp(&rbuf, regex, options) == 0){
            rv = tclistnew();
            int max = argc > 1 ? tcatoi(TCLISTVALPTR(args, 1)) : 0;
            if(max < 1) max = INT_MAX;
            tctdbiterinit(adb->tdb);
            char *kbuf;
            int ksiz;
            while(max > 0 && (kbuf = tctdbiternext(adb->tdb, &ksiz))){
              if(regexec(&rbuf, kbuf, 0, NULL, 0) == 0){
                int vsiz;
                char *vbuf = tctdbget2(adb->tdb, kbuf, ksiz, &vsiz);
                if(vbuf){
                  TCLISTPUSH(rv, kbuf, ksiz);
                  TCLISTPUSH(rv, vbuf, vsiz);
                  TCFREE(vbuf);
                  max--;
                }
              }
              TCFREE(kbuf);
            }
            regfree(&rbuf);
          } else {
            rv = NULL;
          }
        } else {
          rv = NULL;
        }
      } else if(!strcmp(name, "setindex")){
        rv = tclistnew2(1);
        bool err = false;
        argc--;
        for(int i = 0; i < argc; i += 2){
          const char *kbuf, *vbuf;
          kbuf = TCLISTVALPTR(args, i);
          vbuf = TCLISTVALPTR(args, i + 1);
          int type = tctdbstrtoindextype(vbuf);
          if(type >= 0){
            if(!tctdbsetindex(adb->tdb, kbuf, type)) err = true;
          } else {
            err = true;
          }
        }
        if(err){
          tclistdel(rv);
          rv = NULL;
        }
      } else if(!strcmp(name, "search") || !strcmp(name, "metasearch")){
        bool toout = false;
        bool tocnt = false;
        bool tohint = false;
        TDBQRY *qry = tctdbqrynew(adb->tdb);
        TDBQRY **qrys = NULL;
        int qnum = 0;
        int mstype = TDBMSUNION;
        TCLIST *cnames = NULL;
        for(int i = 0; i < argc; i++){
          const char *arg;
          int asiz;
          TCLISTVAL(arg, args, i, asiz);
          TCLIST *tokens = tcstrsplit2(arg, asiz);
          int tnum = TCLISTNUM(tokens);
          if(tnum > 0){
            const char *cmd = TCLISTVALPTR(tokens, 0);
            if((!strcmp(cmd, "addcond") || !strcmp(cmd, "cond")) && tnum > 3){
              const char *name = TCLISTVALPTR(tokens, 1);
              const char *opstr = TCLISTVALPTR(tokens, 2);
              const char *expr = TCLISTVALPTR(tokens, 3);
              int op = tctdbqrystrtocondop(opstr);
              if(op >= 0) tctdbqryaddcond(qry, name, op, expr);
            } else if((!strcmp(cmd, "setorder") || !strcmp(cmd, "order")) && tnum > 2){
              const char *name = TCLISTVALPTR(tokens, 1);
              const char *typestr = TCLISTVALPTR(tokens, 2);
              int type = tctdbqrystrtoordertype(typestr);
              if(type >= 0) tctdbqrysetorder(qry, name, type);
            } else if((!strcmp(cmd, "setlimit") || !strcmp(cmd, "limit") ||
                       !strcmp(cmd, "setmax") || !strcmp(cmd, "max") ) && tnum > 1){
              const char *maxstr = TCLISTVALPTR(tokens, 1);
              int max = tcatoi(maxstr);
              int skip = 0;
              if(tnum > 2){
                maxstr = TCLISTVALPTR(tokens, 2);
                skip = tcatoi(maxstr);
              }
              tctdbqrysetlimit(qry, max, skip);
            } else if(!strcmp(cmd, "get") || !strcmp(cmd, "columns")){
              if(!cnames) cnames = tclistnew();
              for(int j = 1; j < tnum; j++){
                const char *token;
                int tsiz;
                TCLISTVAL(token, tokens, j, tsiz);
                TCLISTPUSH(cnames, token, tsiz);
              }
            } else if(!strcmp(cmd, "next")){
              if(qrys){
                TCREALLOC(qrys, qrys, sizeof(*qrys) * (qnum + 1));
              } else {
                TCMALLOC(qrys, sizeof(*qrys) * 2);
                qrys[0] = qry;
                qnum = 1;
              }
              qry = tctdbqrynew(adb->tdb);
              qrys[qnum++] = qry;
            } else if(!strcmp(cmd, "mstype") && tnum > 1){
              const char *typestr = TCLISTVALPTR(tokens, 1);
              mstype = tctdbstrtometasearcytype(typestr);
              if(mstype < 0) mstype = TDBMSUNION;
            } else if(!strcmp(cmd, "out") || !strcmp(cmd, "remove")){
              toout = true;
            } else if(!strcmp(cmd, "count")){
              tocnt = true;
            } else if(!strcmp(cmd, "hint")){
              tohint = true;
            }
          }
          tclistdel(tokens);
        }
        if(toout){
          if(cnames){
            rv = tclistnew2(1);
            void *opq[2];
            opq[0] = rv;
            opq[1] = cnames;
            if(!tctdbqryproc2(qry, tcadbtdbqrygetout, opq)){
              tclistdel(rv);
              rv = NULL;
            }
          } else {
            if(tctdbqrysearchout2(qry)){
              rv = tclistnew2(1);
            } else {
              rv = NULL;
            }
          }
        } else {
          if(qrys){
            rv = tctdbmetasearch(qrys, qnum, mstype);
          } else {
            rv = tctdbqrysearch(qry);
          }
          if(cnames){
            int cnnum = TCLISTNUM(cnames);
            int rnum = TCLISTNUM(rv);
            TCLIST *nrv = tclistnew2(rnum);
            for(int i = 0; i < rnum; i++){
              const char *pkbuf;
              int pksiz;
              TCLISTVAL(pkbuf, rv, i, pksiz);
              TCMAP *cols = tctdbget(adb->tdb, pkbuf, pksiz);
              if(cols){
                tcmapput(cols, "", 0, pkbuf, pksiz);
                tcmapmove(cols, "", 0, true);
                if(cnnum > 0){
                  TCMAP *ncols = tcmapnew2(cnnum + 1);
                  for(int j = 0; j < cnnum; j++){
                    const char *cname;
                    int cnsiz;
                    TCLISTVAL(cname, cnames, j, cnsiz);
                    int cvsiz;
                    const char *cvalue = tcmapget(cols, cname, cnsiz, &cvsiz);
                    if(cvalue) tcmapput(ncols, cname, cnsiz, cvalue, cvsiz);
                  }
                  tcmapdel(cols);
                  cols = ncols;
                }
                int csiz;
                char *cbuf = tcstrjoin4(cols, &csiz);
                tclistpushmalloc(nrv, cbuf, csiz);
                tcmapdel(cols);
              }
            }
            tclistdel(rv);
            rv = nrv;
          }
        }
        if(tocnt && rv){
          tclistclear(rv);
          char numbuf[TCNUMBUFSIZ];
          int len = sprintf(numbuf, "%d", tctdbqrycount(qry));
          TCLISTPUSH(rv, numbuf, len);
        }
        if(tohint && rv){
          TCXSTR *hbuf = tcxstrnew();
          TCXSTRCAT(hbuf, "", 1);
          TCXSTRCAT(hbuf, "", 1);
          TCXSTRCAT(hbuf, "[[HINT]]\n", 9);
          const char *hint = tctdbqryhint(qrys ? qrys[0] : qry);
          TCXSTRCAT(hbuf, hint, strlen(hint));
          TCLISTPUSH(rv, TCXSTRPTR(hbuf), TCXSTRSIZE(hbuf));
          tcxstrdel(hbuf);
        }
        if(cnames) tclistdel(cnames);
        if(qrys){
          for(int i = 0; i < qnum; i++){
            tctdbqrydel(qrys[i]);
          }
          TCFREE(qrys);
        } else {
          tctdbqrydel(qry);
        }
      } else if(!strcmp(name, "genuid")){
        rv = tclistnew2(1);
        char numbuf[TCNUMBUFSIZ];
        int nsiz = sprintf(numbuf, "%lld", (long long)tctdbgenuid(adb->tdb));
        TCLISTPUSH(rv, numbuf, nsiz);
      } else {
        rv = NULL;
      }
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->misc){
        rv = skel->misc(skel->opq, name, args);
      } else {
        rv = NULL;
      }
      break;
    default:
      rv = NULL;
      break;
  }
  return rv;
}



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


/* Set an extra database sleleton to an abstract database object. */
bool tcadbsetskel(TCADB *adb, ADBSKEL *skel){
  assert(skel);
  if(adb->omode != ADBOVOID) return false;
  if(adb->skel) TCFREE(adb->skel);
  adb->skel = tcmemdup(skel, sizeof(*skel));
  return true;
}


/* Set the multiple database skeleton to an abstract database object. */
bool tcadbsetskelmulti(TCADB *adb, int num){
  assert(adb && num >= 0);
  if(adb->omode != ADBOVOID) return false;
  if(num < 1) return false;
  if(num > CHAR_MAX) num = CHAR_MAX;
  ADBMUL *mul = tcadbmulnew(num);
  ADBSKEL skel;
  memset(&skel, 0, sizeof(skel));
  skel.opq = mul;
  skel.del = (void (*)(void *))tcadbmuldel;
  skel.open = (bool (*)(void *, const char *))tcadbmulopen;
  skel.close = (bool (*)(void *))tcadbmulclose;
  skel.put = (bool (*)(void *, const void *, int, const void *, int))tcadbmulput;
  skel.putkeep = (bool (*)(void *, const void *, int, const void *, int))tcadbmulputkeep;
  skel.putcat = (bool (*)(void *, const void *, int, const void *, int))tcadbmulputcat;
  skel.out = (bool (*)(void *, const void *, int))tcadbmulout;
  skel.get = (void *(*)(void *, const void *, int, int *))tcadbmulget;
  skel.vsiz = (int (*)(void *, const void *, int))tcadbmulvsiz;
  skel.iterinit = (bool (*)(void *))tcadbmuliterinit;
  skel.iternext = (void *(*)(void *, int *))tcadbmuliternext;
  skel.fwmkeys = (TCLIST *(*)(void *, const void *, int, int))tcadbmulfwmkeys;
  skel.addint = (int (*)(void *, const void *, int, int))tcadbmuladdint;
  skel.adddouble = (double (*)(void *, const void *, int, double))tcadbmuladddouble;
  skel.sync = (bool (*)(void *))tcadbmulsync;
  skel.optimize = (bool (*)(void *, const char *))tcadbmuloptimize;
  skel.vanish = (bool (*)(void *))tcadbmulvanish;
  skel.copy = (bool (*)(void *, const char *))tcadbmulcopy;
  skel.tranbegin = (bool (*)(void *))tcadbmultranbegin;
  skel.trancommit = (bool (*)(void *))tcadbmultrancommit;
  skel.tranabort = (bool (*)(void *))tcadbmultranabort;
  skel.path = (const char *(*)(void *))tcadbmulpath;
  skel.rnum = (uint64_t (*)(void *))tcadbmulrnum;
  skel.size = (uint64_t (*)(void *))tcadbmulsize;
  skel.misc = (TCLIST *(*)(void *, const char *, const TCLIST *))tcadbmulmisc;
  skel.putproc =
    (bool (*)(void *, const void *, int, const void *, int, TCPDPROC, void *))tcadbmulputproc;
  skel.foreach = (bool (*)(void *, TCITER, void *))tcadbmulforeach;
  if(!tcadbsetskel(adb, &skel)){
    tcadbmuldel(mul);
    return false;
  }
  return true;
}


/* Get the open mode of an abstract database object. */
int tcadbomode(TCADB *adb){
  assert(adb);
  return adb->omode;
}


/* Get the concrete database object of an abstract database object. */
void *tcadbreveal(TCADB *adb){
  assert(adb);
  void *rv;
  switch(adb->omode){
    case ADBOMDB:
      rv = adb->mdb;
      break;
    case ADBONDB:
      rv = adb->ndb;
      break;
    case ADBOHDB:
      rv = adb->hdb;
      break;
    case ADBOBDB:
      rv = adb->bdb;
      break;
    case ADBOFDB:
      rv = adb->fdb;
      break;
    case ADBOTDB:
      rv = adb->tdb;
      break;
    case ADBOSKEL:
      rv = adb->skel;
      break;
    default:
      rv = NULL;
      break;
  }
  return rv;
}


/* Store a record into an abstract database object with a duplication handler. */
bool tcadbputproc(TCADB *adb, const void *kbuf, int ksiz, const void *vbuf, int vsiz,
                  TCPDPROC proc, void *op){
  assert(adb && kbuf && ksiz >= 0 && proc);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      if(tcmdbputproc(adb->mdb, kbuf, ksiz, vbuf, vsiz, proc, op)){
        if(adb->capnum > 0 || adb->capsiz > 0){
          adb->capcnt++;
          if((adb->capcnt & 0xff) == 0){
            if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum + 0x100)
              tcmdbcutfront(adb->mdb, 0x100);
            if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz)
              tcmdbcutfront(adb->mdb, 0x200);
          }
        }
      } else {
        err = true;
      }
      break;
    case ADBONDB:
      if(tcndbputproc(adb->ndb, kbuf, ksiz, vbuf, vsiz, proc, op)){
        if(adb->capnum > 0 || adb->capsiz > 0){
          adb->capcnt++;
          if((adb->capcnt & 0xff) == 0){
            if(adb->capnum > 0 && tcndbrnum(adb->ndb) > adb->capnum + 0x100)
              tcndbcutfringe(adb->ndb, 0x100);
            if(adb->capsiz > 0 && tcndbmsiz(adb->ndb) > adb->capsiz)
              tcndbcutfringe(adb->ndb, 0x200);
          }
        }
      } else {
        err = true;
      }
      break;
    case ADBOHDB:
      if(!tchdbputproc(adb->hdb, kbuf, ksiz, vbuf, vsiz, proc, op)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbputproc(adb->bdb, kbuf, ksiz, vbuf, vsiz, proc, op)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbputproc(adb->fdb, tcfdbkeytoid(kbuf, ksiz), vbuf, vsiz, proc, op)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbputproc(adb->tdb, kbuf, ksiz, vbuf, vsiz, proc, op)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->putproc){
        if(!skel->putproc(skel->opq, kbuf, ksiz, vbuf, vsiz, proc, op)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Process each record atomically of an abstract database object. */
bool tcadbforeach(TCADB *adb, TCITER iter, void *op){
  assert(adb && iter);
  bool err = false;
  ADBSKEL *skel;
  switch(adb->omode){
    case ADBOMDB:
      tcmdbforeach(adb->mdb, iter, op);
      break;
    case ADBONDB:
      tcndbforeach(adb->ndb, iter, op);
      break;
    case ADBOHDB:
      if(!tchdbforeach(adb->hdb, iter, op)) err = true;
      break;
    case ADBOBDB:
      if(!tcbdbforeach(adb->bdb, iter, op)) err = true;
      break;
    case ADBOFDB:
      if(!tcfdbforeach(adb->fdb, iter, op)) err = true;
      break;
    case ADBOTDB:
      if(!tctdbforeach(adb->tdb, iter, op)) err = true;
      break;
    case ADBOSKEL:
      skel = adb->skel;
      if(skel->foreach){
        if(!skel->foreach(skel->opq, iter, op)) err = true;
      } else {
        err = true;
      }
      break;
    default:
      err = true;
      break;
  }
  return !err;
}


/* Map records of an abstract database object into another B+ tree database. */
bool tcadbmapbdb(TCADB *adb, TCLIST *keys, TCBDB *bdb, ADBMAPPROC proc, void *op, int64_t csiz){
  assert(adb && bdb && proc);
  if(csiz < 0) csiz = 256LL << 20;
  TCLIST *recs = tclistnew2(tclmin(csiz / 64 + 256, INT_MAX / 4));
  ADBMAPBDB map;
  map.adb = adb;
  map.bdb = bdb;
  map.recs = recs;
  map.proc = proc;
  map.op = op;
  map.rsiz = 0;
  map.csiz = csiz;
  bool err = false;
  if(keys){
    int knum = TCLISTNUM(keys);
    for(int i = 0; i < knum && !err; i++){
      const char *kbuf;
      int ksiz;
      TCLISTVAL(kbuf, keys, i, ksiz);
      int vsiz;
      char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
      if(vbuf){
        if(!tcadbmapbdbiter(kbuf, ksiz, vbuf, vsiz, &map)) err = true;
        TCFREE(vbuf);
        if(map.rsiz > map.csiz && !tcadbmapbdbdump(&map)) err = true;
      }
      if(map.rsiz > 0 && !tcadbmapbdbdump(&map)) err = true;
    }
  } else {
    if(!tcadbforeach(adb, tcadbmapbdbiter, &map)) err = true;
  }
  if(map.rsiz > 0 && !tcadbmapbdbdump(&map)) err = true;
  tclistdel(recs);
  return !err;
}


/* Emit records generated by the mapping function into the result map. */
bool tcadbmapbdbemit(void *map, const char *kbuf, int ksiz, const char *vbuf, int vsiz){
  assert(map && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  ADBMAPBDB *mymap = map;
  int rsiz = sizeof(ksiz) + ksiz + vsiz;
  char stack[TCNUMBUFSIZ*8];
  char *rbuf;
  if(rsiz <= sizeof(stack)){
    rbuf = stack;
  } else {
    TCMALLOC(rbuf, rsiz);
  }
  bool err = false;
  char *wp = rbuf;
  memcpy(wp, &ksiz, sizeof(ksiz));
  wp += sizeof(ksiz);
  memcpy(wp, kbuf, ksiz);
  wp += ksiz;
  memcpy(wp, vbuf, vsiz);
  TCLISTPUSH(mymap->recs, rbuf, rsiz);
  mymap->rsiz += rsiz + sizeof(TCLISTDATUM);
  if(rbuf != stack) TCFREE(rbuf);
  if(mymap->rsiz > mymap->csiz && !tcadbmapbdbdump(map)) err = true;
  return !err;
}



/*************************************************************************************************
 * private features
 *************************************************************************************************/


/* Create a multiple database object.
   `num' specifies the number of inner databases.
   The return value is the new multiple database object. */
static ADBMUL *tcadbmulnew(int num){
  assert(num > 0);
  ADBMUL *mul;
  TCMALLOC(mul, sizeof(*mul));
  mul->adbs = NULL;
  mul->num = num;
  mul->iter = -1;
  mul->path = NULL;
  return mul;
}


/* Delete a multiple database object.
   `mul' specifies the multiple database object. */
static void tcadbmuldel(ADBMUL *mul){
  assert(mul);
  if(mul->adbs) tcadbmulclose(mul);
  TCFREE(mul);
}


/* Open a multiple database.
   `mul' specifies the multiple database object.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulopen(ADBMUL *mul, const char *name){
  assert(mul && name);
  if(mul->adbs) return false;
  mul->iter = -1;
  TCLIST *elems = tcstrsplit(name, "#");
  char *path = tclistshift2(elems);
  if(!path){
    tclistdel(elems);
    return false;
  }
  const char *ext = strrchr(path, MYEXTCHR);
  if(!ext) ext = "";
  const char *params = strchr(name, '#');
  if(!params) params = "";
  bool owmode = true;
  bool ocmode = true;
  bool otmode = false;
  int ln = TCLISTNUM(elems);
  for(int i = 0; i < ln; i++){
    const char *elem = TCLISTVALPTR(elems, i);
    char *pv = strchr(elem, '=');
    if(!pv) continue;
    *(pv++) = '\0';
    if(!tcstricmp(elem, "mode")){
      owmode = strchr(pv, 'w') || strchr(pv, 'W');
      ocmode = strchr(pv, 'c') || strchr(pv, 'C');
      otmode = strchr(pv, 't') || strchr(pv, 'T');
    }
  }
  tclistdel(elems);
  bool err = false;
  char *gpat = tcsprintf("%s%c%s*%s", path, MYPATHCHR, ADBMULPREFIX, ext);
  TCLIST *cpaths = tcglobpat(gpat);
  tclistsort(cpaths);
  int cnum = TCLISTNUM(cpaths);
  if(owmode){
    if(otmode){
      for(int i = 0; i < cnum; i++){
        const char *cpath = TCLISTVALPTR(cpaths, i);
        if(unlink(cpath) != 0) err = true;
      }
      tclistclear(cpaths);
      cnum = 0;
    }
    if(ocmode && cnum < 1){
      if(mkdir(path, ADBDIRMODE) != 0 && errno != EEXIST){
        err = true;
      } else {
        for(int i = 0; i < mul->num; i++){
          tclistprintf(cpaths, "%s%c%s%03d%s", path, MYPATHCHR, ADBMULPREFIX, i + 1, ext);
        }
        cnum = TCLISTNUM(cpaths);
      }
    }
  }
  if(!err && cnum > 0){
    TCADB **adbs;
    TCMALLOC(adbs, sizeof(*adbs) * cnum);
    for(int i = 0; i < cnum; i++){
      TCADB *adb = tcadbnew();
      const char *cpath = TCLISTVALPTR(cpaths, i);
      char *cname = tcsprintf("%s%s", cpath, params);
      if(!tcadbopen(adb, cname)) err = true;
      TCFREE(cname);
      adbs[i] = adb;
    }
    if(err){
      for(int i = cnum - 1; i >= 0; i--){
        tcadbdel(adbs[i]);
      }
      TCFREE(adbs);
    } else {
      mul->adbs = adbs;
      mul->num = cnum;
      mul->path = path;
      path = NULL;
    }
  }
  tclistdel(cpaths);
  TCFREE(gpat);
  TCFREE(path);
  return !err;
}


/* Close a multiple database object.
   `mul' specifies the multiple database object.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulclose(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return false;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = num - 1; i >= 0; i--){
    TCADB *adb = adbs[i];
    if(!tcadbclose(adb)) err = true;
    tcadbdel(adb);
  }
  TCFREE(mul->path);
  TCFREE(adbs);
  mul->adbs = NULL;
  mul->path = NULL;
  return !err;
}


/* Store a record into a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulput(ADBMUL *mul, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(mul && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  if(!mul->adbs) return false;
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbput(adb, kbuf, ksiz, vbuf, vsiz);
}


/* Store a new record into a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulputkeep(ADBMUL *mul, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(mul && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  if(!mul->adbs) return false;
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbputkeep(adb, kbuf, ksiz, vbuf, vsiz);
}


/* Concatenate a value at the end of the existing record in a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulputcat(ADBMUL *mul, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(mul && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  if(!mul->adbs) return false;
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbputcat(adb, kbuf, ksiz, vbuf, vsiz);
}


/* Remove a record of a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulout(ADBMUL *mul, const void *kbuf, int ksiz){
  assert(mul && kbuf && ksiz >= 0);
  if(!mul->adbs) return false;
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbout(adb, kbuf, ksiz);
}


/* Retrieve a record in a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the corresponding
   record. */
static void *tcadbmulget(ADBMUL *mul, const void *kbuf, int ksiz, int *sp){
  assert(mul && kbuf && ksiz >= 0 && sp);
  if(!mul->adbs) return false;
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbget(adb, kbuf, ksiz, sp);
}


/* Get the size of the value of a record in a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   If successful, the return value is the size of the value of the corresponding record, else,
   it is -1. */
static int tcadbmulvsiz(ADBMUL *mul, const void *kbuf, int ksiz){
  assert(mul && kbuf && ksiz >= 0);
  if(!mul->adbs) return false;
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbvsiz(adb, kbuf, ksiz);
}


/* Initialize the iterator of a multiple database object.
   `mul' specifies the multiple database object.
   If successful, the return value is true, else, it is false. */
static bool tcadbmuliterinit(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return false;
  mul->iter = -1;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = 0; i < num; i++){
    if(!tcadbiterinit(adbs[i])) err = true;
  }
  if(err) return false;
  mul->iter = 0;
  return true;
}


/* Get the next key of the iterator of a multiple database object.
   `mul' specifies the multiple database object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the next key, else, it is
   `NULL'. */
static void *tcadbmuliternext(ADBMUL *mul, int *sp){
  assert(mul && sp);
  if(!mul->adbs || mul->iter < 0) return false;
  while(mul->iter < mul->num){
    TCADB *adb = mul->adbs[mul->iter];
    char *rv = tcadbiternext(adb, sp);
    if(rv) return rv;
    mul->iter++;
  }
  mul->iter = -1;
  return NULL;
}


/* Get forward matching keys in a multiple database object.
   `mul' specifies the multiple database object.
   `pbuf' specifies the pointer to the region of the prefix.
   `psiz' specifies the size of the region of the prefix.
   `max' specifies the maximum number of keys to be fetched.  If it is negative, no limit is
   specified.
   The return value is a list object of the corresponding keys. */
static TCLIST *tcadbmulfwmkeys(ADBMUL *mul, const void *pbuf, int psiz, int max){
  assert(mul && pbuf && psiz >= 0);
  if(!mul->adbs) return tclistnew2(1);
  if(max < 0) max = INT_MAX;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  TCLIST *rv = tclistnew();
  for(int i = 0; i < num && TCLISTNUM(rv) < max; i++){
    TCLIST *res = tcadbfwmkeys(adbs[i], pbuf, psiz, max);
    int rnum = TCLISTNUM(res);
    for(int j = 0; j < rnum && TCLISTNUM(rv) < max; j++){
      const char *vbuf;
      int vsiz;
      TCLISTVAL(vbuf, res, j, vsiz);
      TCLISTPUSH(rv, vbuf, vsiz);
    }
    tclistdel(res);
  }
  return rv;
}


/* Add an integer to a record in a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `num' specifies the additional value.
   If successful, the return value is the summation value, else, it is `INT_MIN'. */
static int tcadbmuladdint(ADBMUL *mul, const void *kbuf, int ksiz, int num){
  assert(mul && kbuf && ksiz >= 0);
  if(!mul->adbs) return INT_MIN;
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbaddint(adb, kbuf, ksiz, num);
}


/* Add a real number to a record in a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `num' specifies the additional value.
   If successful, the return value is the summation value, else, it is Not-a-Number. */
static double tcadbmuladddouble(ADBMUL *mul, const void *kbuf, int ksiz, double num){
  assert(mul && kbuf && ksiz >= 0);
  if(!mul->adbs) return nan("");
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbadddouble(adb, kbuf, ksiz, num);
}


/* Synchronize updated contents of a multiple database object with the file and the device.
   `mul' specifies the multiple database object.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulsync(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return false;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = 0; i < num; i++){
    if(!tcadbsync(adbs[i])) err = true;
  }
  return !err;
}


/* Optimize the storage of a multiple database object.
   `mul' specifies the multiple database object.
   `params' specifies the string of the tuning parameters, which works as with the tuning
   of parameters the function `tcadbmulopen'.
   If successful, the return value is true, else, it is false. */
static bool tcadbmuloptimize(ADBMUL *mul, const char *params){
  assert(mul);
  if(!mul->adbs) return false;
  mul->iter = -1;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = 0; i < num; i++){
    if(!tcadboptimize(adbs[i], params)) err = true;
  }
  return !err;
}


/* Remove all records of a multiple database object.
   `mul' specifies the multiple database object.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulvanish(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return false;
  mul->iter = -1;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = 0; i < num; i++){
    if(!tcadbvanish(adbs[i])) err = true;
  }
  return !err;
}


/* Copy the database file of a multiple database object.
   `mul' specifies the multiple database object.
   `path' specifies the path of the destination file.
   If successful, the return value is true, else, it is false.  False is returned if the executed
   command returns non-zero code. */
static bool tcadbmulcopy(ADBMUL *mul, const char *path){
  assert(mul && path);
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  if(*path == '@'){
    for(int i = 0; i < num; i++){
      if(!tcadbcopy(adbs[i], path)) err = true;
    }
  } else {
    if(mkdir(path, ADBDIRMODE) == -1 && errno != EEXIST) return false;
    for(int i = 0; i < num; i++){
      TCADB *adb = adbs[i];
      const char *cpath = tcadbpath(adb);
      if(cpath){
        const char *cname = strrchr(cpath, MYPATHCHR);
        cname = cname ? cname + 1 : cpath;
        const char *ext = strrchr(cname, MYEXTCHR);
        if(!ext) ext = "";
        char *npath = tcsprintf("%s%c%s%03d%s", path, MYPATHCHR, ADBMULPREFIX, i + 1, ext);
        if(!tcadbcopy(adb, npath)) err = true;
        TCFREE(npath);
      } else {
        err = true;
      }
    }
  }
  return !err;
}


/* Begin the transaction of a multiple database object.
   `mul' specifies the multiple database object.
   If successful, the return value is true, else, it is false. */
static bool tcadbmultranbegin(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return false;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = 0; i < num; i++){
    if(!tcadbtranbegin(adbs[i])){
      while(--i >= 0){
        tcadbtranabort(adbs[i]);
      }
      err = true;
      break;
    }
  }
  return !err;
}


/* Commit the transaction of a multiple database object.
   `mul' specifies the multiple database object.
   If successful, the return value is true, else, it is false. */
static bool tcadbmultrancommit(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return false;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = num - 1; i >= 0; i--){
    if(!tcadbtrancommit(adbs[i])) err = true;
  }
  return !err;
}


/* Abort the transaction of a multiple database object.
   `mul' specifies the multiple database object.
   If successful, the return value is true, else, it is false. */
static bool tcadbmultranabort(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return false;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = num - 1; i >= 0; i--){
    if(!tcadbtranabort(adbs[i])) err = true;
  }
  return !err;
}


/* Get the file path of a multiple database object.
   `mul' specifies the multiple database object.
   The return value is the path of the database file or `NULL' if the object does not connect to
   any database. */
static const char *tcadbmulpath(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return NULL;
  return mul->path;
}


/* Get the number of records of a multiple database object.
   `mul' specifies the multiple database object.
   The return value is the number of records or 0 if the object does not connect to any database
   instance. */
static uint64_t tcadbmulrnum(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return 0;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  uint64_t rnum = 0;
  for(int i = 0; i < num; i++){
    rnum += tcadbrnum(adbs[i]);
  }
  return rnum;
}


/* Get the size of the database of a multiple database object.
   `mul' specifies the multiple database object.
   The return value is the size of the database or 0 if the object does not connect to any
   database instance. */
static uint64_t tcadbmulsize(ADBMUL *mul){
  assert(mul);
  if(!mul->adbs) return 0;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  uint64_t size = 0;
  for(int i = 0; i < num; i++){
    size += tcadbsize(adbs[i]);
  }
  return size;
}


/* Call a versatile function for miscellaneous operations of a multiple database object.
   `mul' specifies the multiple database object.
   `name' specifies the name of the function.
   `args' specifies a list object containing arguments.
   If successful, the return value is a list object of the result. */
static TCLIST *tcadbmulmisc(ADBMUL *mul, const char *name, const TCLIST *args){
  assert(mul && name);
  if(!mul->adbs) return NULL;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  TCLIST *rv = tclistnew();
  if(*name == '@'){
    name++;
    int anum = TCLISTNUM(args) - 1;
    TCLIST *targs = tclistnew2(2);
    for(int i = 0; i < anum; i++){
      const char *kbuf;
      int ksiz;
      TCLISTVAL(kbuf, args, i, ksiz);
      tclistclear(targs);
      TCLISTPUSH(targs, kbuf, ksiz);
      int idx = tcadbmulidx(mul, kbuf, ksiz);
      TCADB *adb = mul->adbs[idx];
      TCLIST *res = tcadbmisc(adb, name, targs);
      if(res){
        int rnum = TCLISTNUM(res);
        for(int j = 0; j < rnum; j++){
          const char *vbuf;
          int vsiz;
          TCLISTVAL(vbuf, res, j, vsiz);
          TCLISTPUSH(rv, vbuf, vsiz);
        }
        tclistdel(res);
      }
    }
    tclistdel(targs);
  } else if(*name == '%'){
    name++;
    int anum = TCLISTNUM(args) - 1;
    TCLIST *targs = tclistnew2(2);
    for(int i = 0; i < anum; i += 2){
      const char *kbuf, *vbuf;
      int ksiz, vsiz;
      TCLISTVAL(kbuf, args, i, ksiz);
      TCLISTVAL(vbuf, args, i + 1, vsiz);
      tclistclear(targs);
      TCLISTPUSH(targs, kbuf, ksiz);
      TCLISTPUSH(targs, vbuf, vsiz);
      int idx = tcadbmulidx(mul, kbuf, ksiz);
      TCADB *adb = mul->adbs[idx];
      TCLIST *res = tcadbmisc(adb, name, targs);
      if(res){
        int rnum = TCLISTNUM(res);
        for(int j = 0; j < rnum; j++){
          TCLISTVAL(vbuf, res, j, vsiz);
          TCLISTPUSH(rv, vbuf, vsiz);
        }
        tclistdel(res);
      }
    }
    tclistdel(targs);
  } else {
    for(int i = 0; i < num; i++){
      TCLIST *res = tcadbmisc(adbs[i], name, args);
      if(res){
        int rnum = TCLISTNUM(res);
        for(int j = 0; j < rnum; j++){
          const char *vbuf;
          int vsiz;
          TCLISTVAL(vbuf, res, j, vsiz);
          TCLISTPUSH(rv, vbuf, vsiz);
        }
        tclistdel(res);
      } else {
        tclistdel(rv);
        rv = NULL;
        break;
      }
    }
  }
  return rv;
}


/* Store a record into a multiple database object with a duplication handler.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   `proc' specifies the pointer to the callback function to process duplication.
   `op' specifies an arbitrary pointer to be given as a parameter of the callback function.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulputproc(ADBMUL *mul, const void *kbuf, int ksiz, const void *vbuf, int vsiz,
                            TCPDPROC proc, void *op){
  assert(mul && kbuf && ksiz >= 0 && proc);
  if(!mul->adbs) return false;
  int idx = tcadbmulidx(mul, kbuf, ksiz);
  TCADB *adb = mul->adbs[idx];
  return tcadbputproc(adb, kbuf, ksiz, vbuf, vsiz, proc, op);
}


/* Process each record atomically of a multiple database object.
   `mul' specifies the multiple database object.
   `iter' specifies the pointer to the iterator function called for each record.
   `op' specifies an arbitrary pointer to be given as a parameter of the iterator function.
   If successful, the return value is true, else, it is false. */
static bool tcadbmulforeach(ADBMUL *mul, TCITER iter, void *op){
  assert(mul && iter);
  if(!mul->adbs) return false;
  TCADB **adbs = mul->adbs;
  int num = mul->num;
  bool err = false;
  for(int i = 0; i < num; i++){
    if(!tcadbforeach(adbs[i], iter, op)){
      err = true;
      break;
    }
  }
  return !err;
}


/* Get the database index of a multiple database object.
   `mul' specifies the multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   The return value is the bucket index. */
static int tcadbmulidx(ADBMUL *mul, const void *kbuf, int ksiz){
  assert(mul && kbuf && ksiz >= 0);
  uint32_t hash = 20090810;
  const char *rp = (char *)kbuf + ksiz;
  while(ksiz--){
    hash = (hash * 29) ^ *(uint8_t *)--rp;
  }
  return hash % mul->num;
}


/* Call the mapping function for every record of a multiple database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   `op' specifies the pointer to the optional opaque object.
   The return value is true to continue iteration or false to stop iteration. */
static bool tcadbmapbdbiter(const void *kbuf, int ksiz, const void *vbuf, int vsiz, void *op){
  assert(kbuf && ksiz >= 0 && vbuf && vsiz >= 0 && op);
  ADBMAPBDB *map = op;
  bool err = false;
  if(!map->proc(map, kbuf, ksiz, vbuf, vsiz, map->op)) err = true;
  return !err;
}


/* Dump all cached records into the B+ tree database.
   `map' specifies the mapper object for the B+ tree database.
   The return value is true if successful, else, it is false. */
static bool tcadbmapbdbdump(ADBMAPBDB *map){
  assert(map);
  TCBDB *bdb = map->bdb;
  TCLIST *recs = map->recs;
  int rnum = TCLISTNUM(recs);
  TCCMP cmp = tcbdbcmpfunc(bdb);
  if(cmp == tccmplexical){
    tclistsortex(recs, tcadbmapreccmplexical);
  } else if(cmp == tccmpdecimal){
    tclistsortex(recs, tcadbmapreccmpdecimal);
  } else if(cmp == tccmpint32){
    tclistsortex(recs, tcadbmapreccmpint32);
  } else if(cmp == tccmpint64){
    tclistsortex(recs, tcadbmapreccmpint64);
  }
  bool err = false;
  for(int i = 0; i < rnum; i++){
    const char *rbuf;
    int rsiz;
    TCLISTVAL(rbuf, recs, i, rsiz);
    int ksiz;
    memcpy(&ksiz, rbuf, sizeof(ksiz));
    const char *kbuf = rbuf + sizeof(ksiz);
    if(!tcbdbputdup(bdb, kbuf, ksiz, kbuf + ksiz, rsiz - sizeof(ksiz) - ksiz)){
      err = true;
      break;
    }
  }
  tclistclear(recs);
  map->rsiz = 0;
  return !err;
}


/* Compare two list elements by lexical order for mapping.
   `a' specifies the pointer to one element.
   `b' specifies the pointer to the other element.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tcadbmapreccmplexical(const TCLISTDATUM *a, const TCLISTDATUM *b){
  assert(a && b);
  unsigned char *ao = (unsigned char *)((TCLISTDATUM *)a)->ptr;
  unsigned char *bo = (unsigned char *)((TCLISTDATUM *)b)->ptr;
  int size = (((TCLISTDATUM *)a)->size < ((TCLISTDATUM *)b)->size) ?
    ((TCLISTDATUM *)a)->size : ((TCLISTDATUM *)b)->size;
  for(int i = sizeof(int); i < size; i++){
    if(ao[i] > bo[i]) return 1;
    if(ao[i] < bo[i]) return -1;
  }
  return ((TCLISTDATUM *)a)->size - ((TCLISTDATUM *)b)->size;
}


/* Compare two keys as decimal strings of real numbers for mapping.
   `a' specifies the pointer to one element.
   `b' specifies the pointer to the other element.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tcadbmapreccmpdecimal(const TCLISTDATUM *a, const TCLISTDATUM *b){
  assert(a && b);
  return tccmpdecimal(((TCLISTDATUM *)a)->ptr + sizeof(int), a->size - sizeof(int),
                      ((TCLISTDATUM *)b)->ptr + sizeof(int), b->size - sizeof(int), NULL);
}


/* Compare two list elements as 32-bit integers in the native byte order for mapping.
   `a' specifies the pointer to one element.
   `b' specifies the pointer to the other element.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tcadbmapreccmpint32(const TCLISTDATUM *a, const TCLISTDATUM *b){
  assert(a && b);
  return tccmpint32(((TCLISTDATUM *)a)->ptr + sizeof(int), a->size - sizeof(int),
                    ((TCLISTDATUM *)b)->ptr + sizeof(int), b->size - sizeof(int), NULL);
}


/* Compare two list elements as 64-bit integers in the native byte order for mapping.
   `a' specifies the pointer to one element.
   `b' specifies the pointer to the other element.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tcadbmapreccmpint64(const TCLISTDATUM *a, const TCLISTDATUM *b){
  assert(a && b);
  return tccmpint64(((TCLISTDATUM *)a)->ptr + sizeof(int), a->size - sizeof(int),
                    ((TCLISTDATUM *)b)->ptr + sizeof(int), b->size - sizeof(int), NULL);
}


/* Retrieve and remove each record corresponding to a query object.
   `pkbuf' specifies the pointer to the region of the primary key.
   `pksiz' specifies the size of the region of the primary key.
   `cols' specifies a map object containing columns.
   `op' specifies the pointer to the optional opaque object.
   The return value is flags of the post treatment by bitwise-or.
   If successful, the return value is true, else, it is false. */
static int tcadbtdbqrygetout(const void *pkbuf, int pksiz, TCMAP *cols, void *op){
  TCLIST *rv = ((void **)op)[0];
  TCLIST *cnames = ((void **)op)[1];
  int cnnum = TCLISTNUM(cnames);
  tcmapput(cols, "", 0, pkbuf, pksiz);
  tcmapmove(cols, "", 0, true);
  if(cnnum > 0){
    TCMAP *ncols = tcmapnew2(cnnum + 1);
    for(int j = 0; j < cnnum; j++){
      const char *cname;
      int cnsiz;
      TCLISTVAL(cname, cnames, j, cnsiz);
      int cvsiz;
      const char *cvalue = tcmapget(cols, cname, cnsiz, &cvsiz);
      if(cvalue) tcmapput(ncols, cname, cnsiz, cvalue, cvsiz);
    }
    int csiz;
    char *cbuf = tcstrjoin4(ncols, &csiz);
    tclistpushmalloc(rv, cbuf, csiz);
    tcmapdel(ncols);
  } else {
    int csiz;
    char *cbuf = tcstrjoin4(cols, &csiz);
    tclistpushmalloc(rv, cbuf, csiz);
  }
  return TDBQPOUT;
}



// END OF FILE
