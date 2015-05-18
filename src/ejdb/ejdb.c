/**************************************************************************************************
 *  EJDB database library http://ejdb.org
 *  Copyright (C) 2012-2015 Softmotions Ltd <info@softmotions.com>
 *
 *  This file is part of EJDB.
 *  EJDB is free software; you can redistribute it and/or modify it under the terms of
 *  the GNU Lesser General Public License as published by the Free Software Foundation; either
 *  version 2.1 of the License or any later version.  EJDB is distributed in the hope
 *  that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *  You should have received a copy of the GNU Lesser General Public License along with EJDB;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA.
 *************************************************************************************************/

#include "myconf.h"
#include "bson.h"
#include "ejdb_private.h"
#include "ejdbutl.h"

/* private macros */
#define JBLOCKMETHOD(JB_ejdb, JB_wr)                            \
    ((JB_ejdb)->mmtx ? _ejdblockmethod((JB_ejdb), (JB_wr)) : true)
#define JBUNLOCKMETHOD(JB_ejdb)                         \
    ((JB_ejdb)->mmtx ? _ejdbunlockmethod(JB_ejdb) : true)

#define JBCLOCKMETHOD(JB_col, JB_wr)                            \
    ((JB_col)->mmtx ? _ejcollockmethod((JB_col), (JB_wr)) : true)
#define JBCUNLOCKMETHOD(JB_col)                         \
    ((JB_col)->mmtx ? _ejcollunlockmethod(JB_col) : true)

#define JBISOPEN(JB_jb) (((JB_jb) && (JB_jb)->metadb && (JB_jb)->metadb->open) ? true : false)

#define JBISVALCOLNAME(JB_cname) ((JB_cname) && \
                                  strlen(JB_cname) < JBMAXCOLNAMELEN && \
                                  !strchr((JB_cname), '.') && \
                                  !strchr((JB_cname), '$'))

#define JBENSUREOPENLOCK(JB_jb, JB_lock, JB_ret)  \
    assert(JB_jb); \
    if (!JBLOCKMETHOD((JB_jb), (JB_lock))) return JB_ret; \
    if (!JBISOPEN(JB_jb)) { \
        _ejdbsetecode((JB_jb), TCEINVALID, __FILE__, __LINE__, __func__); \
        JBUNLOCKMETHOD(JB_jb); \
        return JB_ret; \
    }

/* Default size of stack allocated buffer for string conversions eg. tcicaseformat() */
#define JBSTRINOPBUFFERSZ 512

/* Default size (16K) of tmp bson buffer on stack for field stripping in _pushstripbson() */
#define JBSBUFFERSZ 16384

#define JBFILEMODE 00644             // permission of created files

/* string processing/conversion flags */
typedef enum {
    JBICASE = 1
} txtflags_t;

/* ejdb number */
typedef union {
    int64_t inum;
    double dnum;
} _EJDBNUM;

/* opaque data for `_bsonipathrowldr()` and `_bsonfpathrowldr()` functions */
typedef struct {
    EJCOLL *coll; //current collection
    bool icase; //ignore case normalization
} _BSONIPATHROWLDR;


/* Maximum number of objects keeped to update deffered indexes */
#define JBMAXDEFFEREDIDXNUM 512

/* context of deffered index updates. See `_updatebsonidx()` */
typedef struct {
    bson_oid_t oid;
    TCMAP *rmap;
    TCMAP *imap;
} _DEFFEREDIDXCTX;

/* query execution context. See `_qryexecute()`*/
typedef struct {
    bool imode;     //if true ifields are included otherwise excluded
    int qflags;     //query flags (JBQRYCOUNT|JBQRYFINDONE) passed into `ejdbqryexecute()` method
    EJCOLL *coll;   //collection
    EJQ *q;         //query object
    EJQF *mqf;      //main indexed query condition if exists
    TCMAP *ifields; //include/exclude $fields
    TCMAP *dfields; //$do fields
    TCLIST *res;    //result set
    TCXSTR *log;    //query debug log buffer
    TCLIST *didxctx; //deffered indexes context
} _QRYCTX;


/* private function prototypes */
static void _ejdbsetecode(EJDB *jb, int ecode, const char *filename, int line, const char *func);
static void _ejdbsetecode2(EJDB *jb, int ecode, const char *filename, int line, 
                           const char *func, bool notfatal);
static bool _ejdbsetmutex(EJDB *ejdb);
EJDB_INLINE bool _ejdblockmethod(EJDB *ejdb, bool wr);
EJDB_INLINE bool _ejdbunlockmethod(EJDB *ejdb);
EJDB_INLINE bool _ejdbcolsetmutex(EJCOLL *coll);
EJDB_INLINE bool _ejcollockmethod(EJCOLL *coll, bool wr);
EJDB_INLINE bool _ejcollunlockmethod(EJCOLL *coll);
static bson_type _bsonoidkey(bson *bs, bson_oid_t *oid);
static char* _bsonitstrval(EJDB *jb, bson_iterator *it, int *vsz, TCLIST *tokens, txtflags_t flags);
static char* _bsonipathrowldr(TCLIST *tokens, const char *pkbuf, int pksz, 
                              const char *rowdata, int rowdatasz,
                              const char *ipath, int ipathsz, void *op, int *vsz);
static char* _bsonfpathrowldr(TCLIST *tokens, const char *rowdata, int rowdatasz,
                              const char *fpath, int fpathsz, void *op, int *vsz);
static bool _createcoldb(const char *colname, EJDB *jb, EJCOLLOPTS *opts, TCTDB** res);
static bool _addcoldb0(const char *colname, EJDB *jb, EJCOLLOPTS *opts, EJCOLL **res);
static void _delcoldb(EJCOLL *cdb);
static void _delqfdata(const EJQ *q, const EJQF *ejqf);
static bool _ejdbsavebsonimpl(EJCOLL *coll, bson *bs, bson_oid_t *oid, bool merge);
static bool _updatebsonidx(EJCOLL *coll, const bson_oid_t *oid, const bson *bs,
                           const void *obsdata, int obsdatasz, TCLIST *dlist);
static bool _metasetopts(EJDB *jb, const char *colname, EJCOLLOPTS *opts);
static bool _metagetopts(EJDB *jb, const char *colname, EJCOLLOPTS *opts);
static bson* _metagetbson(EJDB *jb, const char *colname, int colnamesz, const char *mkey);
static bson* _metagetbson2(EJCOLL *coll, const char *mkey) __attribute__((unused));
static bool _metasetbson(EJDB *jb, const char *colname, int colnamesz,
                         const char *mkey, bson *val, bool merge, bool mergeoverwrt);
static bool _metasetbson2(EJCOLL *coll, const char *mkey, bson *val, bool merge, bool mergeoverwrt);
static bson* _imetaidx(EJCOLL *coll, const char *ipath);
static bool _qrypreprocess(_QRYCTX *ctx);
static TCLIST* _parseqobj(EJDB *jb, EJQ *q, bson *qspec);
static TCLIST* _parseqobj2(EJDB *jb, EJQ *q, const void *qspecbsdata);
static int _parse_qobj_impl(EJDB *jb, EJQ *q, bson_iterator *it, TCLIST *qlist, 
                            TCLIST *pathStack, EJQF *pqf, int mgrp);
static int _ejdbsoncmp(const TCLISTDATUM *d1, const TCLISTDATUM *d2, void *opaque);
static bool _qrycondcheckstrand(const char *vbuf, const TCLIST *tokens);
static bool _qrycondcheckstror(const char *vbuf, const TCLIST *tokens);
static bool _qrybsvalmatch(const EJQF *qf, bson_iterator *it, bool expandarrays, int *arridx);
static bool _qrybsmatch(EJQF *qf, const void *bsbuf, int bsbufsz);
static bool _qry_and_or_match(EJCOLL *coll, EJQ *ejq, const void *pkbuf, int pkbufsz);
static bool _qryormatch2(EJCOLL *coll, EJQ *ejq, const void *bsbuf, int bsbufsz);
static bool _qryormatch3(EJCOLL *coll, EJQ *ejq, EJQ *oq, const void *bsbuf, int bsbufsz);
static bool _qryandmatch2(EJCOLL *coll, EJQ *ejq, const void *bsbuf, int bsbufsz);
static bool _qryallcondsmatch(EJQ *ejq, int anum, EJCOLL *coll, EJQF **qfs, 
                              int qfsz, const void *pkbuf, int pkbufsz);
static EJQ* _qryaddand(EJDB *jb, EJQ *q, const void *andbsdata);
static bool _qrydup(const EJQ *src, EJQ *target, uint32_t qflags);
static void _qrydel(EJQ *q, bool freequery);
static bool _pushprocessedbson(_QRYCTX *ctx, const void *bsbuf, int bsbufsz);
static bool _exec_do(_QRYCTX *ctx, const void *bsbuf, bson *bsout);
static void _qryctxclear(_QRYCTX *ctx);
static TCLIST* _qryexecute(EJCOLL *coll, const EJQ *q, uint32_t *count, int qflags, TCXSTR *log);
EJDB_INLINE void _nufetch(_EJDBNUM *nu, const char *sval, bson_type bt);
EJDB_INLINE int _nucmp(_EJDBNUM *nu, const char *sval, bson_type bt);
EJDB_INLINE int _nucmp2(_EJDBNUM *nu1, _EJDBNUM *nu2, bson_type bt);
static EJCOLL* _getcoll(EJDB *jb, const char *colname);
static bool _exportcoll(EJCOLL *coll, const char *dpath, int flags, TCXSTR *log);
static bool _importcoll(EJDB *jb, const char *bspath, TCLIST *cnames, int flags, TCXSTR *log);
static EJCOLL* _createcollimpl(EJDB *jb, const char *colname, EJCOLLOPTS *opts);
static bool _rmcollimpl(EJDB *jb, EJCOLL *coll, bool unlinkfile);
static bool _setindeximpl(EJCOLL *coll, const char *fpath, int flags, bool nolock);

extern const char *utf8proc_errmsg(ssize_t errcode);

static const bool yes = true;

const char *ejdbversion() {
    return tcversion;
}

uint32_t ejdbformatversion(EJDB *jb) {
    return JBISOPEN(jb) ? jb->fversion : 0;
}

uint8_t ejdbformatversionmajor(EJDB *jb) {
   return (JBISOPEN(jb) && jb->fversion) ? jb->fversion / 100000 : 0; 
}

uint16_t ejdbformatversionminor(EJDB *jb) {
    if (!JBISOPEN(jb) || !jb->fversion) {
        return 0;
    }
    int major = jb->fversion / 100000;
    return (jb->fversion - major * 100000) / 1000;
}

uint16_t ejdbformatversionpatch(EJDB *jb) {
   if (!JBISOPEN(jb) || !jb->fversion) {
        return 0;
    }
    int major = jb->fversion / 100000;
    int minor = (jb->fversion - major * 100000) / 1000;
    return (jb->fversion - major * 100000 - minor * 1000);
}

const char* ejdberrmsg(int ecode) {
    if (ecode > -6 && ecode < 0) { //Hook for negative error codes of utf8proc library
        return utf8proc_errmsg(ecode);
    }
    switch (ecode) {
        case JBEINVALIDCOLNAME:
            return "invalid collection name";
        case JBEINVALIDBSON:
            return "invalid bson object";
        case JBEQINVALIDQCONTROL:
            return "invalid query control field starting with '$'";
        case JBEQINOPNOTARRAY:
            return "$strand, $stror, $in, $nin, $bt keys require not empty array value";
        case JBEMETANVALID:
            return "inconsistent database metadata";
        case JBEFPATHINVALID:
            return "invalid JSEJDB_EXPORT const char *ejdbversion();ON field path value";
        case JBEQINVALIDQRX:
            return "invalid query regexp value";
        case JBEQRSSORTING:
            return "result set sorting error";
        case JBEQERROR:
            return "invalid query";
        case JBEQUPDFAILED:
            return "bson record update failed";
        case JBEINVALIDBSONPK:
            return "invalid bson _id field";
        case JBEQONEEMATCH:
            return "only one $elemMatch allowed in the fieldpath";
        case JBEQINCEXCL:
            return "$fields hint cannot mix include and exclude fields";
        case JBEQACTKEY:
            return "action key in $do block can be one of: $join, $slice";
        case JBEMAXNUMCOLS:
            return "exceeded the maximum number of collections per database: 1024";
        case JBEEJSONPARSE:
            return "JSON parsing failed";
        case JBEEI:
            return "data export/import failed";
        case JBETOOBIGBSON:
            return "bson size exceeds the maximum allowed size limit";
        case JBEINVALIDCMD:
            return "invalid ejdb command specified";
        default:
            return tcerrmsg(ecode);
    }
}

bool ejdbisvalidoidstr(const char *oid) {
    if (!oid) {
        return false;
    }
    int i = 0;
    for (; oid[i] != '\0' &&
            ((oid[i] >= 0x30 && oid[i] <= 0x39) || /* 1 - 9 */
             (oid[i] >= 0x61 && oid[i] <= 0x66)); ++i); /* a - f */
    return (i == 24);
}

/* Get the last happened error code of a database object. */
int ejdbecode(EJDB *jb) {
    assert(jb && jb->metadb);
    return tctdbecode(jb->metadb);
}

EJDB* ejdbnew(void) {
    EJDB *jb;
    TCCALLOC(jb, 1, sizeof (*jb));
    jb->metadb = tctdbnew();
    jb->fversion = 0;
    tctdbsetmutex(jb->metadb);
    tctdbsetcache(jb->metadb, 1024, 0, 0);
    if (!_ejdbsetmutex(jb)) {
        tctdbdel(jb->metadb);
        TCFREE(jb);
        return NULL;
    }
    return jb;
}

void ejdbdel(EJDB *jb) {
    assert(jb && jb->metadb);
    if (JBISOPEN(jb)) ejdbclose(jb);
    for (int i = 0; i < jb->cdbsnum; ++i) {
        assert(jb->cdbs[i]);
        _delcoldb(jb->cdbs[i]);
        TCFREE(jb->cdbs[i]);
        jb->cdbs[i] = NULL;
    }
    jb->cdbsnum = 0;
    jb->fversion = 0;
    if (jb->mmtx) {
        pthread_rwlock_destroy(jb->mmtx);
        TCFREE(jb->mmtx);
    }
    tctdbdel(jb->metadb);
    TCFREE(jb);
}

bool ejdbclose(EJDB *jb) {
    JBENSUREOPENLOCK(jb, true, false);
    bool rv = true;
    for (int i = 0; i < jb->cdbsnum; ++i) {
        assert(jb->cdbs[i]);
        JBCLOCKMETHOD(jb->cdbs[i], true);
        if (!tctdbclose(jb->cdbs[i]->tdb)) {
            rv = false;
        }
        JBCUNLOCKMETHOD(jb->cdbs[i]);
    }
    if (!tctdbclose(jb->metadb)) {
        rv = false;
    }
    jb->fversion = 0;
    JBUNLOCKMETHOD(jb);
    return rv;
}

bool ejdbisopen(EJDB *jb) {
    return JBISOPEN(jb);
}

bool ejdbopen(EJDB *jb, const char *path, int mode) {
    assert(jb && path && jb->metadb);
    if (!JBLOCKMETHOD(jb, true)) return false;
    if (JBISOPEN(jb)) {
        _ejdbsetecode(jb, TCEINVALID, __FILE__, __LINE__, __func__);
        JBUNLOCKMETHOD(jb);
        return false;
    }
    bool rv = tctdbopen(jb->metadb, path, mode);
    if (!rv) {
        goto finish;
    }
    
    TCTDB *mdb = jb->metadb;
    char *colname = NULL;
    uint64_t mbuf;
    jb->cdbsnum = 0;
    
    if (!(rv = tctdbiterinit(mdb))) {
        goto finish;
    }
    //check ejdb format version
    if (tctdbreadopaque(mdb, &mbuf, 0, sizeof(mbuf)) != sizeof(mbuf)) {
        rv = false;
        _ejdbsetecode(jb, JBEMETANVALID, __FILE__, __LINE__, __func__);
        goto finish;
    }
    for (int i = 0; rv && i < mdb->hdb->rnum && (colname = tctdbiternext2(mdb)) != NULL; ++i) {
        if ((rv = tcisvalidutf8str(colname, strlen(colname)))) {
            EJCOLL *cdb;
            EJCOLLOPTS opts;
            if ((rv = _metagetopts(jb, colname, &opts))) {
                rv = _addcoldb0(colname, jb, &opts, &cdb);
            }
        } else {
            _ejdbsetecode(jb, JBEMETANVALID, __FILE__, __LINE__, __func__);
        }
        TCFREE(colname);
    }
    
finish:
    if (rv) {
        mbuf = TCITOHLL(mbuf);
        uint16_t magic = (uint16_t) mbuf;
        jb->fversion = (uint32_t) (mbuf >> 16);
        
        if (!mbuf && (mode & (JBOWRITER | JBOTRUNC))) { //write ejdb format info opaque data
            magic = EJDB_MAGIC;
            jb->fversion = 100000 * EJDB_VERSION_MAJOR + 1000 * EJDB_VERSION_MINOR + EJDB_VERSION_PATCH;
            mbuf |= jb->fversion;
            mbuf = (mbuf << 16) | ((uint64_t) magic & 0xffff);
            mbuf = TCHTOILL(mbuf);
            if (tctdbwriteopaque(mdb, &mbuf, 0, sizeof(mbuf)) != sizeof(mbuf)) {
                rv = false;
                _ejdbsetecode(jb, TCEWRITE, __FILE__, __LINE__, __func__);
            } else {
                tctdbsync(jb->metadb);
            }
        } else if (magic && (magic != EJDB_MAGIC)) {
            rv = false;
           _ejdbsetecode(jb, JBEMETANVALID, __FILE__, __LINE__, __func__); 
        }
    }
    JBUNLOCKMETHOD(jb);
    return rv;
}

EJCOLL* ejdbgetcoll(EJDB *jb, const char *colname) {
    assert(colname);
    EJCOLL *coll = NULL;
    JBENSUREOPENLOCK(jb, false, NULL);
    coll = _getcoll(jb, colname);
    JBUNLOCKMETHOD(jb);
    return coll;
}

TCLIST* ejdbgetcolls(EJDB *jb) {
    assert(jb);
    EJCOLL *coll = NULL;
    JBENSUREOPENLOCK(jb, false, NULL);
    TCLIST *ret = tclistnew2(jb->cdbsnum);
    for (int i = 0; i < jb->cdbsnum; ++i) {
        coll = jb->cdbs[i];
        TCLISTPUSH(ret, coll, sizeof (*coll));
    }
    JBUNLOCKMETHOD(jb);
    return ret;
}

EJCOLL* ejdbcreatecoll(EJDB *jb, const char *colname, EJCOLLOPTS *opts) {
    assert(colname);
    EJCOLL *coll = ejdbgetcoll(jb, colname);
    if (coll) {
        return coll;
    }
    JBENSUREOPENLOCK(jb, true, NULL);
    coll = _createcollimpl(jb, colname, opts);
    JBUNLOCKMETHOD(jb);
    return coll;
}

bool ejdbrmcoll(EJDB *jb, const char *colname, bool unlinkfile) {
    assert(colname);
    JBENSUREOPENLOCK(jb, true, false);
    bool rv = true;
    EJCOLL *coll = _getcoll(jb, colname);
    if (!coll) {
        goto finish;
    }
    if (!JBCLOCKMETHOD(coll, true)) return false;
    rv = _rmcollimpl(jb, coll, unlinkfile);
    JBCUNLOCKMETHOD(coll);
    _delcoldb(coll);
    TCFREE(coll);
finish:
    JBUNLOCKMETHOD(jb);
    return rv;
}

/* Save/Update BSON  */
bool ejdbsavebson(EJCOLL *coll, bson *bs, bson_oid_t *oid) {
    return ejdbsavebson2(coll, bs, oid, false);
}

bool ejdbsavebson2(EJCOLL *coll, bson *bs, bson_oid_t *oid, bool merge) {
    assert(coll);
    if (!bs || bs->err || !bs->finished) {
        _ejdbsetecode(coll->jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        return false;
    }
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    if (!JBCLOCKMETHOD(coll, true)) return false;
    bool rv = _ejdbsavebsonimpl(coll, bs, oid, merge);
    JBCUNLOCKMETHOD(coll);
    return rv;
}

bool ejdbsavebson3(EJCOLL *coll, const void *bsdata, bson_oid_t *oid, bool merge) {
    bson bs;
    bson_init_with_data(&bs, bsdata);
    return ejdbsavebson2(coll, &bs, oid, merge);
}

bool ejdbrmbson(EJCOLL *coll, bson_oid_t *oid) {
    assert(coll && oid);
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    JBCLOCKMETHOD(coll, true);
    bool rv = true;
    const void *olddata;
    int olddatasz = 0;
    TCMAP *rmap = tctdbget(coll->tdb, oid, sizeof (*oid));
    if (!rmap) {
        goto finish;
    }
    olddata = tcmapget3(rmap, JDBCOLBSON, JDBCOLBSONL, &olddatasz);
    if (!_updatebsonidx(coll, oid, NULL, olddata, olddatasz, NULL) ||
            !tctdbout(coll->tdb, oid, sizeof (*oid))) {
        rv = false;
    }
finish:
    JBCUNLOCKMETHOD(coll);
    if (rmap) {
        tcmapdel(rmap);
    }
    return rv;
}

bson* ejdbloadbson(EJCOLL *coll, const bson_oid_t *oid) {
    assert(coll && oid);
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return NULL;
    }
    JBCLOCKMETHOD(coll, false);
    bson *ret = NULL;
    int datasz;
    void *cdata = tchdbget(coll->tdb->hdb, oid, sizeof (*oid), &datasz);
    if (!cdata) {
        goto finish;
    }
    void *bsdata = tcmaploadone(cdata, datasz, JDBCOLBSON, JDBCOLBSONL, &datasz);
    if (!bsdata) {
        goto finish;
    }
    if (datasz <= 4) {
        TCFREE(bsdata);
        goto finish;
    }
    ret = bson_create();
    bson_init_finished_data(ret, bsdata);
finish:
    JBCUNLOCKMETHOD(coll);
    if (cdata) {
        TCFREE(cdata);
    }
    return ret;
}

EJQ* ejdbcreatequery(EJDB *jb, bson *qobj, bson *orqobjs, int orqobjsnum, bson *hints) {
    assert(jb);
    if (!qobj || qobj->err || !qobj->finished) {
        _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        return NULL;
    }
    EJQ *q;
    TCCALLOC(q, 1, sizeof (*q));
    if (qobj) {
        q->qflist = _parseqobj(jb, q, qobj);
        if (!q->qflist) {
            goto error;
        }
    }
    if (orqobjs && orqobjsnum > 0) {
        for (int i = 0; i < orqobjsnum; ++i) {
            bson *oqb = (orqobjs + i);
            assert(oqb);
            if (ejdbqueryaddor(jb, q, bson_data(oqb)) == NULL) {
                goto error;
            }
        }
    }
    if (hints) {
        if (hints->err || !hints->finished) {
            _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
            return NULL;
        }
        q->hints = bson_create();
        if (bson_copy(q->hints, hints)) {
            goto error;
        }
    }
    return q;
error:
    ejdbquerydel(q);
    return NULL;
}

EJQ* ejdbcreatequery2(EJDB *jb, const void *qbsdata) {
    assert(jb);
    if (!qbsdata) {
        _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        return NULL;
    }
    EJQ *q;
    TCCALLOC(q, 1, sizeof (*q));
    q->qflist = _parseqobj2(jb, q, qbsdata);
    if (!q->qflist) {
        goto error;
    }
    return q;
error:
    ejdbquerydel(q);
    return NULL;
}

EJQ* ejdbqueryaddor(EJDB *jb, EJQ *q, const void *orbsdata) {
    assert(jb && q && orbsdata);
    if (!orbsdata) {
        _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        return NULL;
    }
    EJQ *oq = ejdbcreatequery2(jb, orbsdata);
    if (oq == NULL) {
        return NULL;
    }
    if (q->orqlist == NULL) {
        q->orqlist = tclistnew2(TCLISTINYNUM);
    }
    TCLISTPUSH(q->orqlist, &oq, sizeof(oq));
    return q;
}

EJQ* ejdbqueryhints(EJDB *jb, EJQ *q, const void *hintsbsdata) {
    assert(jb && q);
    if (!hintsbsdata) {
        _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        return NULL;
    }
    bson_iterator it;
    BSON_ITERATOR_FROM_BUFFER(&it, hintsbsdata);
    bson *bs = bson_create_from_iterator(&it);
    if (bs->err) {
        bson_del(bs);
        _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        return NULL;
    }
    if (q->hints) {
        bson_del(q->hints);
        q->hints = NULL;
    }
    if (q->ifields) {
        tcmapdel(q->ifields);
        q->ifields = NULL;
    }
    q->hints = bs;
    return q;
}

void ejdbquerydel(EJQ *q) {
    _qrydel(q, true);
}

/** Set index */
bool ejdbsetindex(EJCOLL *coll, const char *fpath, int flags) {
    return _setindeximpl(coll, fpath, flags, false);
}

uint32_t ejdbupdate(EJCOLL *coll, bson *qobj, 
                    bson *orqobjs, int orqobjsnum, 
                    bson *hints, TCXSTR *log) {
    assert(coll);
    uint32_t count = 0;
    EJQ *q = ejdbcreatequery(coll->jb, qobj, orqobjs, orqobjsnum, hints);
    if (q == NULL) {
        return count;
    }
    ejdbqryexecute(coll, q, &count, JBQRYCOUNT, log);
    ejdbquerydel(q);
    return count;
}

EJQRESULT ejdbqryexecute(EJCOLL *coll, const EJQ *q, 
                         uint32_t *count, int qflags, 
                         TCXSTR *log) {
    assert(coll && q && q->qflist);
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return NULL;
    }
    JBCLOCKMETHOD(coll, (q->flags & EJQUPDATING) ? true : false);
    _ejdbsetecode(coll->jb, TCESUCCESS, __FILE__, __LINE__, __func__);
    if (ejdbecode(coll->jb) != TCESUCCESS) { // We are not in fatal state
        JBCUNLOCKMETHOD(coll);
        return NULL;
    }
    TCLIST *res = _qryexecute(coll, q, count, qflags, log);
    JBCUNLOCKMETHOD(coll);
    return res;
}

bson* ejdbqrydistinct(EJCOLL *coll, const char *fpath, bson *qobj, 
                      bson *orqobjs, int orqobjsnum, 
                      uint32_t *count, TCXSTR *log) {
    assert(coll);
    uint32_t icount = 0;
    
    bson *rqobj = qobj;
    bson hints;
    bson_init_as_query(&hints);
    bson_append_start_object(&hints, "$fields");
    bson_append_int(&hints, fpath, 1);
    bson_append_finish_object(&hints);
    bson_append_start_object(&hints, "$orderby");
    bson_append_int(&hints, fpath, 1);
    bson_append_finish_object(&hints);
    bson_finish(&hints);

    EJQ *q = NULL;
    bson *rres = NULL;
    *count = 0;

    if (!rqobj) {
        rqobj = bson_create();
        bson_init(rqobj);
        bson_finish(rqobj);
    }
    q = ejdbcreatequery(coll->jb, rqobj, orqobjs, orqobjsnum, &hints);
    if (q == NULL) {
        goto fail;
    }
    if (q->flags & EJQUPDATING) {
        _ejdbsetecode(coll->jb, JBEQERROR, __FILE__, __LINE__, __func__);
        goto fail;
    }
    TCLIST *res = ejdbqryexecute(coll, q, &icount, 0, log);
    rres = bson_create();
    bson_init(rres);

    bson_iterator bsi[2];
    bson_iterator *prev = NULL, *cur;
    int biind = 0;
    char biindstr[TCNUMBUFSIZ];
    memset(biindstr, '\0', 32);
    int fplen = strlen(fpath);
    for(int i = 0; i < TCLISTNUM(res); ++i) {
        cur = bsi + (biind & 1);
        BSON_ITERATOR_FROM_BUFFER(cur, TCLISTVALPTR(res, i));
        bson_type bt = bson_find_fieldpath_value2(fpath, fplen, cur);
        if (bt == BSON_EOO) {
            continue;
        }
        
        if (prev == NULL || bson_compare_it_current(prev, cur)) {
            bson_numstrn(biindstr, TCNUMBUFSIZ, biind);
            bson_append_field_from_iterator2(biindstr, cur, rres);
            prev = cur;
            biind++;
        }
    }
    bson_finish(rres);
    tclistdel(res);
    
    *count = biind;
    
fail:
    if (q) {
        ejdbquerydel(q);
    }
    if (rqobj != qobj) {
        bson_del(rqobj);
    }
    bson_destroy(&hints);
    return rres;
}

int ejdbqresultnum(EJQRESULT qr) {
    return qr ? tclistnum(qr) : 0;
}

const void* ejdbqresultbsondata(EJQRESULT qr, int pos, int *size) {
    if (!qr || pos < 0) {
        *size = 0;
        return NULL;
    }
    const void *bsdata = tclistval2(qr, pos);
    *size = (bsdata != NULL) ? bson_size2(bsdata) : 0;
    return bsdata;
}

void ejdbqresultdispose(EJQRESULT qr) {
    if (qr) {
        tclistdel(qr);
    }
}

bool ejdbsyncoll(EJCOLL *coll) {
    assert(coll);
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    bool rv = false;
    if (!JBCLOCKMETHOD(coll, true)) return false;
    rv = tctdbsync(coll->tdb);
    JBCUNLOCKMETHOD(coll);
    return rv;
}

bool ejdbsyncdb(EJDB *jb) {
    assert(jb);
    JBENSUREOPENLOCK(jb, true, false);
    bool rv = tctdbsync(jb->metadb);
    if (!rv) {
        return rv;
    }
    for (int i = 0; i < jb->cdbsnum; ++i) {
        assert(jb->cdbs[i]);
        rv = JBCLOCKMETHOD(jb->cdbs[i], true);
        if (!rv) break;
        rv = tctdbsync(jb->cdbs[i]->tdb);
        JBCUNLOCKMETHOD(jb->cdbs[i]);
        if (!rv) break;
    }
    JBUNLOCKMETHOD(jb);
    return rv;
}

bool ejdbtranbegin(EJCOLL *coll) {
    assert(coll);
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    for (double wsec = 1.0 / sysconf_SC_CLK_TCK; true; wsec *= 2) {
        if (!JBCLOCKMETHOD(coll, true)) return false;
        if (!coll->tdb->open || !coll->tdb->wmode) {
            _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
            JBCUNLOCKMETHOD(coll);
            return false;
        }
        if (!coll->tdb->tran) break;
        JBCUNLOCKMETHOD(coll);
        if (wsec > 1.0) wsec = 1.0;
        tcsleep(wsec);
    }
    if (!tctdbtranbeginimpl(coll->tdb)) {
        JBCUNLOCKMETHOD(coll);
        return false;
    }
    coll->tdb->tran = true;
    JBCUNLOCKMETHOD(coll);
    return true;
}

bool ejdbtrancommit(EJCOLL *coll) {
    assert(coll);
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    if (!JBCLOCKMETHOD(coll, true)) return false;
    if (!coll->tdb->open || !coll->tdb->wmode || !coll->tdb->tran) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        JBCUNLOCKMETHOD(coll);
        return false;
    }
    coll->tdb->tran = false;
    bool err = false;
    if (!tctdbtrancommitimpl(coll->tdb)) err = true;
    JBCUNLOCKMETHOD(coll);
    return !err;
}

bool ejdbtranabort(EJCOLL *coll) {
    assert(coll);
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    if (!JBCLOCKMETHOD(coll, true)) return false;
    if (!coll->tdb->open || !coll->tdb->wmode || !coll->tdb->tran) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        JBCUNLOCKMETHOD(coll);
        return false;
    }
    coll->tdb->tran = false;
    bool err = false;
    if (!tctdbtranabortimpl(coll->tdb)) err = true;
    JBCUNLOCKMETHOD(coll);
    return !err;
}

bool ejdbtranstatus(EJCOLL *coll, bool *txactive) {
    assert(coll && txactive);
    if (!JBISOPEN(coll->jb)) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    if (!JBCLOCKMETHOD(coll, true)) return false;
    *txactive = coll->tdb->tran;
    JBCUNLOCKMETHOD(coll);
    return true;
}

static int _cmpcolls(const TCLISTDATUM *d1, const TCLISTDATUM *d2) {
    EJCOLL *c1 = (EJCOLL*) d1->ptr;
    EJCOLL *c2 = (EJCOLL*) d2->ptr;
    return memcmp(c1->cname, c2->cname, MIN(c1->cnamesz, c2->cnamesz));
}

bson* ejdbmeta(EJDB *jb) {
    JBENSUREOPENLOCK(jb, false, NULL);
    char nbuff[TCNUMBUFSIZ];
    bson *bs = bson_create();
    bson_init(bs);
    bson_append_string(bs, "file", jb->metadb->hdb->path);
    bson_append_start_array(bs, "collections"); //collections

    TCLIST *cols = ejdbgetcolls(jb);
    tclistsortex(cols, _cmpcolls);

    for (int i = 0; i < TCLISTNUM(cols); ++i) {
        EJCOLL *coll = (EJCOLL*) TCLISTVALPTR(cols, i);
        if (!JBCLOCKMETHOD(coll, false)) {
            tclistdel(cols);
            bson_del(bs);
            JBUNLOCKMETHOD(jb);
            return NULL;
        }
        bson_numstrn(nbuff, TCNUMBUFSIZ, i);
        bson_append_start_object(bs, nbuff); // coll obj
        bson_append_string_n(bs, "name", coll->cname, coll->cnamesz);
        bson_append_string(bs, "file", coll->tdb->hdb->path);
        bson_append_long(bs, "records", coll->tdb->hdb->rnum);

        bson_append_start_object(bs, "options"); // coll.options
        bson_append_long(bs, "buckets", coll->tdb->hdb->bnum);
        bson_append_long(bs, "cachedrecords", coll->tdb->hdb->rcnum);
        bson_append_bool(bs, "large", (coll->tdb->opts & TDBTLARGE));
        bson_append_bool(bs, "compressed", (coll->tdb->opts & TDBTDEFLATE));
        bson_append_finish_object(bs); // eof coll.options

        bson_append_start_array(bs, "indexes"); // coll.indexes[]
        for (int j = 0; j < coll->tdb->inum; ++j) {
            TDBIDX *idx = (coll->tdb->idxs + j);
            if (idx->type != TDBITLEXICAL &&
                    idx->type != TDBITDECIMAL &&
                    idx->type != TDBITTOKEN) {
                continue;
            }
            bson_numstrn(nbuff, TCNUMBUFSIZ, j);
            bson_append_start_object(bs, nbuff); // coll.indexes.index
            bson_append_string(bs, "field", idx->name + 1);
            bson_append_string(bs, "iname", idx->name);
            switch (idx->type) {
                case TDBITLEXICAL:
                    bson_append_string(bs, "type", "lexical");
                    break;
                case TDBITDECIMAL:
                    bson_append_string(bs, "type", "decimal");
                    break;
                case TDBITTOKEN:
                    bson_append_string(bs, "type", "token");
                    break;
            }
            TCBDB *idb = (TCBDB*) idx->db;
            if (idb) {
                bson_append_long(bs, "records", idb->rnum);
                bson_append_string(bs, "file", idb->hdb->path);
            }
            bson_append_finish_object(bs); // eof coll.indexes.index
        }
        bson_append_finish_array(bs); // eof coll.indexes[]
        bson_append_finish_object(bs); // eof coll
        JBCUNLOCKMETHOD(coll);
    }
    bson_append_finish_array(bs); //eof collections
    bson_finish(bs);
    tclistdel(cols);
    JBUNLOCKMETHOD(jb);
    if (bs->err) {
        _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        bson_del(bs);
        bs = NULL;
    }
    return bs;
}

bool ejdbexport(EJDB *jb, const char *path, TCLIST *cnames, int flags, TCXSTR *log) {
    assert(jb && path);
    bool err = false;
    bool isdir = false;
    tcstatfile(path, &isdir, NULL, NULL);
    if (!isdir) {
        if (mkdir(path, 00755)) {
            _ejdbsetecode2(jb, TCEMKDIR, __FILE__, __LINE__, __func__, true);
            return false;
        }
    }
    JBENSUREOPENLOCK(jb, false, false);
    TCLIST *_cnames = cnames;
    if (_cnames == NULL) {
        _cnames = tclistnew2(jb->cdbsnum);
        for (int i = 0; i < jb->cdbsnum; ++i) {
            EJCOLL *c = jb->cdbs[i];
            TCLISTPUSH(_cnames, c->cname, c->cnamesz);
        }
    }
    for (int i = 0; i < TCLISTNUM(_cnames); ++i) {
        const char *cn = TCLISTVALPTR(_cnames, i);
        assert(cn);
        EJCOLL *coll = _getcoll(jb, cn);
        if (!coll) continue;
        if (!JBCLOCKMETHOD(coll, false)) {
            err = true;
            goto finish;
        }
        if (!_exportcoll(coll, path, flags, log)) {
            err = true;
        }
        JBCUNLOCKMETHOD(coll);
    }
finish:
    JBUNLOCKMETHOD(jb);
    if (_cnames != cnames) {
        tclistdel(_cnames);
    }
    return !err;
}

bool ejdbimport(EJDB *jb, const char *path, TCLIST *cnames, int flags, TCXSTR *log) {
    assert(jb && path);
    bool err = false;
    bool isdir = false;
    if (flags == 0) {
        flags |= JBIMPORTUPDATE;
    }
    if (!tcstatfile(path, &isdir, NULL, NULL) || !isdir) {
        _ejdbsetecode2(jb, TCENOFILE, __FILE__, __LINE__, __func__, true);
        return false;
    }
    bool tail = (path[0] != '\0' && path[strlen(path) - 1] == MYPATHCHR);
    char *bsonpat = tail ? tcsprintf("%s*.bson") : tcsprintf("%s%c*.bson", path, MYPATHCHR);
    TCLIST *bspaths = tcglobpat(bsonpat);
    JBENSUREOPENLOCK(jb, true, false);
    for (int i = 0; i < TCLISTNUM(bspaths); ++i) {
        const char* bspath = TCLISTVALPTR(bspaths, i);
        if (!_importcoll(jb, bspath, cnames, flags, log)) {
            err = true;
            goto finish;
        }
    }
finish:
    JBUNLOCKMETHOD(jb);
    if (bsonpat) {
        TCFREE(bsonpat);
    }
    if (bspaths) {
        tclistdel(bspaths);
    }
    return !err;
}

bson* ejdbcommand2(EJDB *jb, void *cmdbsondata) {
    bson cmd;
    bson_init_with_data(&cmd, cmdbsondata);
    bson *bret = ejdbcommand(jb, &cmd);
    return bret;
}

bson* ejdbcommand(EJDB *jb, bson *cmd) {
    bson *ret = bson_create();
    int ecode = 0;
    const char *err = NULL;
    TCXSTR *xlog = NULL;
    TCLIST *cnames = NULL;
    bool rv = true;

    bson_init(ret);
    bson_type bt;
    bson_iterator it;
    BSON_ITERATOR_INIT(&it, cmd);

    while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
        const char *key = BSON_ITERATOR_KEY(&it);
        if (!strcmp("export", key) || !strcmp("import", key)) {
            xlog = tcxstrnew();
            char *path = NULL;
            int flags = 0;
            bson_iterator sit;
            BSON_ITERATOR_SUBITERATOR(&it, &sit);
            if (bson_find_fieldpath_value("path", &sit) == BSON_STRING) {
                path = strdup(bson_iterator_string(&sit));
            }
            BSON_ITERATOR_SUBITERATOR(&it, &sit);
            if (bson_find_fieldpath_value("mode", &sit) == BSON_INT) {
                flags = bson_iterator_int(&sit);
            }
            BSON_ITERATOR_SUBITERATOR(&it, &sit);
            if (bson_find_fieldpath_value("cnames", &sit) == BSON_ARRAY) {
                bson_iterator ait;
                BSON_ITERATOR_SUBITERATOR(&sit, &ait);
                while ((bt = bson_iterator_next(&ait)) != BSON_EOO) {
                    if (bt == BSON_STRING) {
                        if (cnames == NULL) {
                            cnames = tclistnew();
                        }
                        const char *sv = bson_iterator_string(&ait);
                        TCLISTPUSH(cnames, sv, strlen(sv));
                    }
                }
            }
            if (path == NULL) {
                err = "Missing required 'path' field";
                ecode = JBEINVALIDCMD;
                goto finish;
            }
            if (!strcmp("export", key)) {
                rv = ejdbexport(jb, path, cnames, flags, xlog);
            } else { //import
                rv = ejdbimport(jb, path, cnames, flags, xlog);
            }
            if (!rv) {
                ecode = ejdbecode(jb);
                err = ejdberrmsg(ecode);
            }
            TCFREE(path);
        } else if (!strcmp("ping", key)) {
            xlog = tcxstrnew();
            tcxstrprintf(xlog, "pong");
        } else {
            err = "Unknown command";
            ecode = JBEINVALIDCMD;
            goto finish;
        }
    }
finish:
    if (err) {
        bson_append_string(ret, "error", err);
        bson_append_int(ret, "errorCode", ecode);
    }
    if (xlog) {
        bson_append_string(ret, "log", TCXSTRPTR(xlog));
        tcxstrdel(xlog);
    }
    if (cnames) {
        tclistdel(cnames);
    }
    bson_finish(ret);
    return ret;
}

/*************************************************************************************************
 * private features
 *************************************************************************************************/

static bool _setindeximpl(EJCOLL *coll, const char *fpath, int flags, bool nolock) {
    assert(coll && fpath);
    bool rv = true;
    bson *imeta = NULL;
    bson_iterator it;
    int tcitype = 0; //TCDB index type
    int oldiflags = 0; //Old index flags stored in meta
    bool ibld = (flags & JBIDXREBLD);
    if (ibld) {
        flags &= ~JBIDXREBLD;
    }
    bool idrop = (flags & JBIDXDROP);
    if (idrop) {
        flags &= ~JBIDXDROP;
    }
    bool idropall = (flags & JBIDXDROPALL);
    if (idropall) {
        idrop = true;
        flags &= ~JBIDXDROPALL;
    }
    bool iop = (flags & JBIDXOP);
    if (iop) {
        flags &= ~JBIDXOP;
    }
    char ipath[BSON_MAX_FPATH_LEN + 2]; // Add 2 bytes for one char prefix and '\0'term
    char ikey[BSON_MAX_FPATH_LEN + 2]; // Add 2 bytes for one char prefix and '\0'term
    int fpathlen = strlen(fpath);
    if (fpathlen > BSON_MAX_FPATH_LEN) {
        _ejdbsetecode(coll->jb, JBEFPATHINVALID, __FILE__, __LINE__, __func__);
        rv = false;
        goto finish;
    }
    memmove(ikey + 1, fpath, fpathlen + 1);
    ikey[0] = 'i';
    memmove(ipath + 1, fpath, fpathlen + 1);
    ipath[0] = 's'; // Defaulting to string index type

    if (!nolock) {
        JBENSUREOPENLOCK(coll->jb, true, false);
    }
    imeta = _imetaidx(coll, fpath);
    if (!imeta) {
        if (idrop) { // Cannot drop/optimize not existent index;
            if (!nolock) {
                JBUNLOCKMETHOD(coll->jb);
            }
            goto finish;
        }
        if (iop) {
            iop = false; // New index will be created
        }
        imeta = bson_create();
        bson_init(imeta);
        bson_append_string(imeta, "ipath", fpath);
        bson_append_int(imeta, "iflags", flags);
        bson_finish(imeta);
        rv = _metasetbson2(coll, ikey, imeta, false, false);
        if (!rv) {
            if (!nolock) {
                JBUNLOCKMETHOD(coll->jb);
            }
            goto finish;
        }
    } else {
        if (bson_find(&it, imeta, "iflags") != BSON_EOO) {
            oldiflags = bson_iterator_int(&it);
        }
        if (!idrop && oldiflags != flags) { // Update index meta
            bson imetadelta;
            bson_init(&imetadelta);
            bson_append_int(&imetadelta, "iflags", (flags | oldiflags));
            bson_finish(&imetadelta);
            rv = _metasetbson2(coll, ikey, &imetadelta, true, true);
            bson_destroy(&imetadelta);
            if (!rv) {
                if (!nolock) {
                    JBUNLOCKMETHOD(coll->jb);
                }
                goto finish;
            }
        }
    }
    if (!nolock) {
        JBUNLOCKMETHOD(coll->jb);
    }
    if (idrop) {
        tcitype = TDBITVOID;
        if (idropall && oldiflags) {
            flags = oldiflags; // Drop index only for existing types
        }
    } else if (iop) {
        tcitype = TDBITOPT;
        if (oldiflags) {
            flags = oldiflags; // Optimize index for all existing types
        }
    }

    if (!nolock) {
        if (!JBCLOCKMETHOD(coll, true)) {
            rv = false;
            goto finish;
        }
    }
    _BSONIPATHROWLDR op;
    op.icase = false;
    op.coll = coll;
    if (tcitype) {
        if (flags & JBIDXSTR) {
            ipath[0] = 's';
            rv = tctdbsetindexrldr(coll->tdb, ipath, tcitype, _bsonipathrowldr, &op);
        }
        if (flags & JBIDXISTR) {
            ipath[0] = 'i';
            op.icase = true;
            rv = tctdbsetindexrldr(coll->tdb, ipath, tcitype, _bsonipathrowldr, &op);
        }
        if (rv && (flags & JBIDXNUM)) {
            ipath[0] = 'n';
            rv = tctdbsetindexrldr(coll->tdb, ipath, tcitype, _bsonipathrowldr, &op);
        }
        if (rv && (flags & JBIDXARR)) {
            ipath[0] = 'a';
            rv = tctdbsetindexrldr(coll->tdb, ipath, tcitype, _bsonipathrowldr, &op);
        }
        if (idrop) { // Update index meta on drop
            oldiflags &= ~flags;
            if (oldiflags) { // Index dropped only for some types
                bson imetadelta;
                bson_init(&imetadelta);
                bson_append_int(&imetadelta, "iflags", oldiflags);
                bson_finish(&imetadelta);
                rv = _metasetbson2(coll, ikey, &imetadelta, true, true);
                bson_destroy(&imetadelta);
            } else { // Index dropped completely
                rv = _metasetbson2(coll, ikey, NULL, false, false);
            }
        }
    } else {
        if ((flags & JBIDXSTR) && (ibld || !(oldiflags & JBIDXSTR))) {
            ipath[0] = 's';
            rv = tctdbsetindexrldr(coll->tdb, ipath, TDBITLEXICAL, _bsonipathrowldr, &op);
        }
        if ((flags & JBIDXISTR) && (ibld || !(oldiflags & JBIDXISTR))) {
            ipath[0] = 'i';
            op.icase = true;
            rv = tctdbsetindexrldr(coll->tdb, ipath, TDBITLEXICAL, _bsonipathrowldr, &op);
        }
        if (rv && (flags & JBIDXNUM) && (ibld || !(oldiflags & JBIDXNUM))) {
            ipath[0] = 'n';
            rv = tctdbsetindexrldr(coll->tdb, ipath, TDBITDECIMAL, _bsonipathrowldr, &op);
        }
        if (rv && (flags & JBIDXARR) && (ibld || !(oldiflags & JBIDXARR))) {
            ipath[0] = 'a';
            rv = tctdbsetindexrldr(coll->tdb, ipath, TDBITTOKEN, _bsonipathrowldr, &op);
        }
    }
    if (!nolock) {
        JBCUNLOCKMETHOD(coll);
    }
finish:
    if (imeta) {
        bson_del(imeta);
    }
    return rv;
}

/**
 * In order to cleanup resources you need:
 *      _delcoldb(coll);
 *      TCFREE(coll);
 * after completion of this method with any return status.
 */
static bool _rmcollimpl(EJDB *jb, EJCOLL *coll, bool unlinkfile) {
    assert(jb && coll);
    bool rv = true;
    tctdbout(jb->metadb, coll->cname, coll->cnamesz);
    tctdbvanish(coll->tdb);
    TCLIST *paths = tclistnew2(10);
    tclistpush2(paths, coll->tdb->hdb->path);
    for (int j = 0; j < coll->tdb->inum; ++j) {
        TDBIDX *idx = coll->tdb->idxs + j;
        const char *ipath = tcbdbpath(idx->db);
        if (ipath) {
            tclistpush2(paths, ipath);
        }
    }
    tctdbclose(coll->tdb);
    if (unlinkfile) {
        for (int i = 0; i < TCLISTNUM(paths); ++i) {
            unlink(tclistval2(paths, i));
        }
    }
    tclistdel(paths);
    jb->cdbsnum--;
    for (int i = 0; i < EJDB_MAX_COLLECTIONS; ++i) {
        if (jb->cdbs[i] == coll) {
            jb->cdbs[i] = NULL;
            break;
        }
    }
    // Collapse the NULL hole
    for (int i = 0; i < EJDB_MAX_COLLECTIONS - 1; ++i) {
        if (!jb->cdbs[i] && jb->cdbs[i + 1]) {
            jb->cdbs[i] = jb->cdbs[i + 1];
            jb->cdbs[i + 1] = NULL;
        }
    }
    return rv;
}

static EJCOLL* _createcollimpl(EJDB *jb, const char *colname, EJCOLLOPTS *opts) {
    EJCOLL *coll = NULL;
    if (!JBISVALCOLNAME(colname)) {
        _ejdbsetecode(jb, JBEINVALIDCOLNAME, __FILE__, __LINE__, __func__);
        return NULL;
    }
    TCTDB *meta = jb->metadb;
    char *row = tcsprintf("name\t%s", colname);
    if (!tctdbput3(meta, colname, row)) {
        goto finish;
    }
    if (!_addcoldb0(colname, jb, opts, &coll)) {
        tctdbout2(meta, colname); // Cleaning
        goto finish;
    }
    _metasetopts(jb, colname, opts);
finish:
    if (row) {
        TCFREE(row);
    }
    return coll;
}

static bool _importcoll(EJDB *jb, const char *bspath, 
                        TCLIST *cnames, int flags, TCXSTR *log) {
    if (log) {
        tcxstrprintf(log, "\n\nReading '%s'", bspath);
    }
    bool err = false;
    // /foo/bar.bson
    char *dp = strrchr(bspath, '.');
    char *pp = strrchr(bspath, MYPATHCHR);
    if (!dp || !pp || pp > dp) {
        if (log) {
            tcxstrprintf(log, "\nERROR: Invalid file path: '%s'", bspath);
        }
        _ejdbsetecode2(jb, JBEEI, __FILE__, __LINE__, __func__, true);
        return false;
    }
    char *lastsep = pp;
    int i = 0;
    char *cname, *mjson = NULL;
    bson_type bt;
    bson *mbson = NULL; // Meta bson
    bson_iterator mbsonit;
    int sp;
    EJCOLL *coll;

    TCMALLOC(cname, dp - pp + 1);
    TCXSTR *xmetapath = tcxstrnew();

    while (++pp != dp) {
        cname[i++] = *pp;
    }
    cname[i] = '\0';
    if (cnames != NULL) {
        if (tclistlsearch(cnames, cname, i) == -1) {
            goto finish;
        }
    }
    tcxstrcat(xmetapath, bspath, lastsep - bspath + 1);
    tcxstrprintf(xmetapath, "%s-meta.json", cname);
    mjson = tcreadfile(tcxstrptr(xmetapath), 0, &sp);
    if (!mjson) {
        err = true;
        if (log) {
            tcxstrprintf(log, "\nERROR: Error reading the file: '%s'", tcxstrptr(xmetapath));
        }
        _ejdbsetecode2(jb, TCEREAD, __FILE__, __LINE__, __func__, true);
        goto finish;
    }
    mbson = json2bson(mjson);
    if (!mbson) {
        err = true;
        if (log) {
            tcxstrprintf(log, "\nERROR: Invalid JSON in the file: '%s'", tcxstrptr(xmetapath));
        }
        _ejdbsetecode2(jb, JBEEJSONPARSE, __FILE__, __LINE__, __func__, true);
    }
    coll = _getcoll(jb, cname);
    if (coll && (flags & JBIMPORTREPLACE)) {
        if (log) {
            tcxstrprintf(log, "\nReplacing all data in '%s'", cname);
        }
        err = !(_rmcollimpl(jb, coll, true));
        _delcoldb(coll);
        TCFREE(coll);
        coll = NULL;
        if (err) {
            if (log) {
                tcxstrprintf(log, "\nERROR: Failed to remove collection: '%s'", cname);
            }
            goto finish;
        }
    }
    if (!coll) {
        // Build collection options
        BSON_ITERATOR_INIT(&mbsonit, mbson);
        EJCOLLOPTS cops = {0};
        if (bson_find_fieldpath_value("opts", &mbsonit) == BSON_OBJECT) {
            bson_iterator sit;
            BSON_ITERATOR_SUBITERATOR(&mbsonit, &sit);
            while ((bt = bson_iterator_next(&sit)) != BSON_EOO) {
                const char *key = BSON_ITERATOR_KEY(&sit);
                if (strcmp("compressed", key) == 0 && bt == BSON_BOOL) {
                    cops.compressed = bson_iterator_bool(&sit);
                } else if (strcmp("large", key) == 0 && bt == BSON_BOOL) {
                    cops.large = bson_iterator_bool(&sit);
                } else if (strcmp("cachedrecords", key) == 0 && BSON_IS_NUM_TYPE(bt)) {
                    cops.cachedrecords = bson_iterator_int(&sit);
                } else if (strcmp("records", key) == 0 && BSON_IS_NUM_TYPE(bt)) {
                    cops.records = bson_iterator_long(&sit);
                }
            }
        }
        coll = _createcollimpl(jb, cname, &cops);
        if (!coll) {
            err = true;
            if (log) {
                tcxstrprintf(log, "\nERROR: Error creating collection: '%s'", cname);
            }
            goto finish;
        }
    }

    // Register collection indexes
    BSON_ITERATOR_INIT(&mbsonit, mbson);
    while ((bt = bson_iterator_next(&mbsonit)) != BSON_EOO) {
        const char *key = BSON_ITERATOR_KEY(&mbsonit);
        if (bt != BSON_OBJECT || strlen(key) < 2 || key[0] != 'i') {
            continue;
        }
        char *ipath = NULL;
        int iflags = 0;
        bson_iterator sit;

        BSON_ITERATOR_SUBITERATOR(&mbsonit, &sit);
        bt = bson_find_fieldpath_value("ipath", &sit);
        if (bt == BSON_STRING) {
            ipath = strdup(bson_iterator_string(&sit));
        }

        BSON_ITERATOR_SUBITERATOR(&mbsonit, &sit);
        bt = bson_find_fieldpath_value("iflags", &sit);
        if (bt == BSON_INT || bt == BSON_LONG) {
            iflags = bson_iterator_int(&sit);
        }
        if (ipath) {
            if (!_setindeximpl(coll, ipath, iflags, true)) {
                err = true;
                if (log) {
                    tcxstrprintf(log, "\nERROR: Error creating collection index."
                                      " Collection: '%s' Field: '%s'", cname, ipath);
                }
                TCFREE(ipath);
            }
            TCFREE(ipath);
        }
    }

    int fd = open(bspath, O_RDONLY, TCFILEMODE);
    if (fd == -1) {
        err = true;
        if (log) {
            tcxstrprintf(log, "\nERROR: Error reading file: '%s'", bspath);
        }
        _ejdbsetecode2(jb, TCEREAD, __FILE__, __LINE__, __func__, true);
        goto finish;
    }
    if (!JBCLOCKMETHOD(coll, true)) {
        err = true;
        goto finish;
    }
    int32_t maxdocsiz = 0, docsiz = 0, numdocs = 0;
    char *docbuf;
    TCMALLOC(docbuf, 4);
    while (true) {
        sp = read(fd, docbuf, 4);
        if (sp < 4) {
            break;
        }
        memcpy(&docsiz, docbuf, 4);
        docsiz = TCHTOIL(docsiz);
        if (docsiz > EJDB_MAX_IMPORTED_BSON_SIZE) {
            err = true;
            if (log) {
                tcxstrprintf(log, "\nERROR: BSON document size: %d exceeds the maximum" 
                                  " allowed size limit: %d for import operation",
                             docsiz, EJDB_MAX_IMPORTED_BSON_SIZE);
            }
            _ejdbsetecode2(jb, JBETOOBIGBSON, __FILE__, __LINE__, __func__, true);
            break;
        }
        if (docsiz < 5) {
            break;
        }
        if (maxdocsiz < docsiz) {
            maxdocsiz = docsiz;
            TCREALLOC(docbuf, docbuf, maxdocsiz);
        }
        sp = read(fd, docbuf + 4, docsiz - 4);
        if (sp < docsiz - 4) {
            break;
        }
        bson_oid_t oid;
        bson savebs;
        bson_init_with_data(&savebs, docbuf);
        if (docbuf[docsiz - 1] != '\0' || savebs.err || !savebs.finished) {
            err = true;
            _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
            break;
        }
        if (!_ejdbsavebsonimpl(coll, &savebs, &oid, false)) {
            err = true;
            break;
        }
        ++numdocs;
    }
    if (docbuf) {
        TCFREE(docbuf);
    }
    if (!tctdbsync(coll->tdb)) {
        err = true;
    }
    if (!err && log) {
        tcxstrprintf(log, "\n%d objects imported into '%s'", numdocs, cname);
    }
    JBCUNLOCKMETHOD(coll);
finish:
    if (mbson) {
        bson_del(mbson);
    }
    if (mjson) {
        TCFREE(mjson);
    }
    tcxstrdel(xmetapath);
    if (err && log) {
        tcxstrprintf(log, "\nERROR: Importing data into: '%s' failed with error: '%s'", 
                          cname, (ejdbecode(jb) != 0) ? ejdberrmsg(ejdbecode(jb)) : "Unknown");
    }
    TCFREE(cname);
    return !err;
}

static bool _exportcoll(EJCOLL *coll, const char *dpath, int flags, TCXSTR *log) {
    bool err = false;
    char *fpath = tcsprintf("%s%c%s%s", dpath, MYPATHCHR, coll->cname, 
                           (flags & JBJSONEXPORT) ? ".json" : ".bson");
    char *fpathm = tcsprintf("%s%c%s%s", dpath, MYPATHCHR, coll->cname, "-meta.json");
    TCHDB *hdb = coll->tdb->hdb;
    TCXSTR *skbuf = tcxstrnew3(sizeof (bson_oid_t) + 1);
    TCXSTR *colbuf = tcxstrnew3(1024);
    TCXSTR *bsbuf = tcxstrnew3(1024);
    int sz = 0;
#ifndef _WIN32
    HANDLE fd = open(fpath, O_RDWR | O_CREAT | O_TRUNC, JBFILEMODE);
    HANDLE fdm = open(fpathm, O_RDWR | O_CREAT | O_TRUNC, JBFILEMODE);
#else
    HANDLE fd = CreateFile(fpath, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    HANDLE fdm = CreateFile(fpathm, GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
    if (INVALIDHANDLE(fd) || INVALIDHANDLE(fdm)) {
        _ejdbsetecode2(coll->jb, JBEEI, __FILE__, __LINE__, __func__, true);
        err = true;
        goto finish;
    }
    TCHDBITER *it = tchdbiter2init(hdb);
    if (!it) {
        goto finish;
    }
    while (!err && tchdbiter2next(hdb, it, skbuf, colbuf)) {
        sz = tcmaploadoneintoxstr(TCXSTRPTR(colbuf), TCXSTRSIZE(colbuf), 
                                  JDBCOLBSON, JDBCOLBSONL, bsbuf);
        if (sz > 0) {
            char *wbuf = NULL;
            int wsiz;
            if (flags & JBJSONEXPORT) {
                if (bson2json(TCXSTRPTR(bsbuf), &wbuf, &wsiz) != BSON_OK) {
                    _ejdbsetecode2(coll->jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__, true);
                    goto wfinish;
                }
            } else {
                wbuf = TCXSTRPTR(bsbuf);
                wsiz = TCXSTRSIZE(bsbuf);
            }
            if (!tcwrite(fd, wbuf, wsiz)) {
                _ejdbsetecode2(coll->jb, JBEEI, __FILE__, __LINE__, __func__, true);
                goto wfinish;
            }
wfinish:
            if (wbuf && wbuf != TCXSTRPTR(bsbuf)) {
                TCFREE(wbuf);
            }
        }
        tcxstrclear(skbuf);
        tcxstrclear(colbuf);
        tcxstrclear(bsbuf);
    }

    if (!err) { // Export collection meta
        TCMAP *cmeta = tctdbget(coll->jb->metadb, coll->cname, coll->cnamesz);
        if (!cmeta) {
            goto finish;
        }
        bson mbs;
        bson_init(&mbs);
        tcmapiterinit(cmeta);
        const char *mkey = NULL;
        while ((mkey = tcmapiternext2(cmeta)) != NULL) {
            if (!mkey || (*mkey != 'i' && strcmp(mkey, "opts"))) {
                continue; // Only index & opts meta bsons allowing
            }
            bson *bs = _metagetbson(coll->jb, coll->cname, coll->cnamesz, mkey);
            if (bs) {
                bson_append_bson(&mbs, mkey, bs);
                bson_del(bs);
            }
        }
        tcmapdel(cmeta);

        bson_finish(&mbs);
        char *wbuf = NULL;
        int wsiz;
        if (bson2json(bson_data(&mbs), &wbuf, &wsiz) != BSON_OK) {
            err = true;
            _ejdbsetecode2(coll->jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__, true);
            bson_destroy(&mbs);
            goto finish;
        }
        bson_destroy(&mbs);
        if (!tcwrite(fdm, wbuf, wsiz)) {
            err = true;
            _ejdbsetecode2(coll->jb, JBEEI, __FILE__, __LINE__, __func__, true);
        }
        TCFREE(wbuf);
    }

finish:
    if (!INVALIDHANDLE(fd) && !CLOSEFH(fd)) {
        _ejdbsetecode2(coll->jb, JBEEI, __FILE__, __LINE__, __func__, true);
        err = true;
    }
    if (!INVALIDHANDLE(fdm) && !CLOSEFH(fdm)) {
        _ejdbsetecode2(coll->jb, JBEEI, __FILE__, __LINE__, __func__, true);
        err = true;
    }
    tcxstrdel(skbuf);
    tcxstrdel(colbuf);
    tcxstrdel(bsbuf);
    TCFREE(fpath);
    TCFREE(fpathm);
    return !err;
}

/* Set the error code of a table database object. */
static void _ejdbsetecode(EJDB *jb, int ecode, const char *filename, int line, const char *func) {
    _ejdbsetecode2(jb, ecode, filename, line, func, false);
}

static void _ejdbsetecode2(EJDB *jb, int ecode, const char *filename, int line, 
                           const char *func, bool notfatal) {
    assert(jb && filename && line >= 1 && func);
    tctdbsetecode2(jb->metadb, ecode, filename, line, func, notfatal);
}

static EJCOLL* _getcoll(EJDB *jb, const char *colname) {
    assert(colname);
    for (int i = 0; i < jb->cdbsnum; ++i) {
        assert(jb->cdbs[i]);
        if (!strcmp(colname, jb->cdbs[i]->cname)) {
            return jb->cdbs[i];
        }
    }
    return NULL;
}

/* Set mutual exclusion control of a table database object for threading. */
static bool _ejdbsetmutex(EJDB *ejdb) {
    assert(ejdb);
    if (ejdb->mmtx || JBISOPEN(ejdb)) {
        _ejdbsetecode(ejdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    TCMALLOC(ejdb->mmtx, sizeof (pthread_rwlock_t));
    bool err = false;
    if (pthread_rwlock_init(ejdb->mmtx, NULL) != 0) err = true;
    if (err) {
        TCFREE(ejdb->mmtx);
        ejdb->mmtx = NULL;
        return false;
    }
    return true;
}

/* Lock a method of the table database object.
   `tdb' specifies the table database object.
   `wr' specifies whether the lock is writer or not.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool _ejdblockmethod(EJDB *ejdb, bool wr) {
    assert(ejdb);
    if (wr ? pthread_rwlock_wrlock(ejdb->mmtx) != 0 : pthread_rwlock_rdlock(ejdb->mmtx) != 0) {
        _ejdbsetecode(ejdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Unlock a method of the table database object.
   `tdb' specifies the table database object.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool _ejdbunlockmethod(EJDB *ejdb) {
    assert(ejdb);
    if (pthread_rwlock_unlock(ejdb->mmtx) != 0) {
        _ejdbsetecode(ejdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

EJDB_INLINE bool _ejdbcolsetmutex(EJCOLL *coll) {
    assert(coll && coll->jb);
    if (coll->mmtx) {
        _ejdbsetecode(coll->jb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    TCMALLOC(coll->mmtx, sizeof (pthread_rwlock_t));
    bool err = false;
    if (pthread_rwlock_init(coll->mmtx, NULL) != 0) err = true;
    if (err) {
        TCFREE(coll->mmtx);
        coll->mmtx = NULL;
        return false;
    }
    return true;
}

EJDB_INLINE bool _ejcollockmethod(EJCOLL *coll, bool wr) {
    assert(coll && coll->jb);
    if (wr ? pthread_rwlock_wrlock(coll->mmtx) != 0 : pthread_rwlock_rdlock(coll->mmtx) != 0) {
        _ejdbsetecode(coll->jb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return (coll->tdb && coll->tdb->open);
}

EJDB_INLINE bool _ejcollunlockmethod(EJCOLL *coll) {
    assert(coll && coll->jb);
    if (pthread_rwlock_unlock(coll->mmtx) != 0) {
        _ejdbsetecode(coll->jb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

bool ejcollockmethod(EJCOLL *coll, bool wr) {
    return _ejcollockmethod(coll, wr);
}

bool ejcollunlockmethod(EJCOLL *coll) {
    return _ejcollunlockmethod(coll);
}

static void _qrydel(EJQ *q, bool freequery) {
    if (!q) {
        return;
    }
    const EJQF *qf = NULL;
    if (q->qflist) {
        for (int i = 0; i < TCLISTNUM(q->qflist); ++i) {
            qf = TCLISTVALPTR(q->qflist, i);
            assert(qf);
            _delqfdata(q, qf);
        }
        tclistdel(q->qflist);
        q->qflist = NULL;
    }

    if (q->orqlist) {
        for (int i = 0; i < TCLISTNUM(q->orqlist); ++i) {
            EJQ *oq = *((EJQ**) TCLISTVALPTR(q->orqlist, i));
            _qrydel(oq, true);
        }
        tclistdel(q->orqlist);
        q->orqlist = NULL;
    }

    if (q->andqlist) {
        for (int i = 0; i < TCLISTNUM(q->andqlist); ++i) {
            EJQ *aq = *((EJQ**) TCLISTVALPTR(q->andqlist, i));
            _qrydel(aq, true);
        }
        tclistdel(q->andqlist);
        q->andqlist = NULL;
    }

    if (q->hints) {
        bson_del(q->hints);
        q->hints = NULL;
    }
    if (q->ifields) {
        tcmapdel(q->ifields);
        q->ifields = NULL;
    }
    if (q->colbuf) {
        tcxstrdel(q->colbuf);
        q->colbuf = NULL;
    }
    if (q->bsbuf) {
        tcxstrdel(q->bsbuf);
        q->bsbuf = NULL;
    }
    if (q->tmpbuf) {
        tcxstrdel(q->tmpbuf);
        q->tmpbuf = NULL;
    }
    if (q->allqfields) {
        TCFREE(q->allqfields);
        q->allqfields = NULL;
    }
    if (freequery) {
        TCFREE(q);
    }
}

static bool _qrybsvalmatch(const EJQF *qf, bson_iterator *it, bool expandarrays, int *arridx) {
    if (qf->tcop == TDBQTRUE) {
        return (true == !qf->negate);
    }
    bool rv = false;
    bson_type bt = BSON_ITERATOR_TYPE(it);
    const char *expr = qf->expr;
    int exprsz = qf->exprsz;
    char sbuf[JBSTRINOPBUFFERSZ]; // Buffer for icase comparisons
    char oidbuf[25]; // OID buffer
    char *cbuf = NULL;
    int cbufstrlen = 0;
    int fvalsz;
    int sp;
    const char *fval;

	// Feature #129: Handle BSON_SYMBOL like BSON_STRING
#define _FETCHSTRFVAL() \
    do { \
        fvalsz = (BSON_IS_STRING_TYPE(bt)) ? bson_iterator_string_len(it) : 1; \
        fval = (BSON_IS_STRING_TYPE(bt)) ? bson_iterator_string(it) : ""; \
        if (bt == BSON_OID) { \
            bson_oid_to_string(bson_iterator_oid(it), oidbuf); \
            fvalsz = 25; \
            fval = oidbuf; \
        } \
    } while(false)


    if (bt == BSON_ARRAY && expandarrays) { // Iterate over array
        bson_iterator sit;
        BSON_ITERATOR_SUBITERATOR(it, &sit);
        int c = 0;
        while ((bt = bson_iterator_next(&sit)) != BSON_EOO) {
            if (_qrybsvalmatch(qf, &sit, false, arridx)) {
                if (arridx) {
                    *arridx = c;
                }
                return true;
            }
            ++c;
        }
        return (false == !qf->negate);
    }
    switch (qf->tcop) {
        case TDBQCSTREQ: {
            _FETCHSTRFVAL();
            if ((qf->flags & EJCONDICASE) && (bt == BSON_STRING)) {
                cbufstrlen = tcicaseformat(fval, fvalsz - 1, sbuf, JBSTRINOPBUFFERSZ, &cbuf);
                if (cbufstrlen < 0) {
                    _ejdbsetecode(qf->jb, cbufstrlen, __FILE__, __LINE__, __func__);
                    rv = false;
                } else {
                    rv = (exprsz == cbufstrlen) && (exprsz == 0 || !memcmp(expr, cbuf, exprsz));
                }
                if (cbuf && cbuf != sbuf) {
                    TCFREE(cbuf);
                }
            } else {
                rv = (exprsz == fvalsz - 1) && (exprsz == 0 || !memcmp(expr, fval, exprsz));
            }
            break;
        }
        case TDBQCSTRINC: {
            _FETCHSTRFVAL();
            if ((qf->flags & EJCONDICASE) && (bt == BSON_STRING)) {
                cbufstrlen = tcicaseformat(fval, fvalsz - 1, sbuf, JBSTRINOPBUFFERSZ, &cbuf);
                if (cbufstrlen < 0) {
                    _ejdbsetecode(qf->jb, cbufstrlen, __FILE__, __LINE__, __func__);
                    rv = false;
                } else {
                    rv = (exprsz <= cbufstrlen) && strstr(cbuf, expr);
                }
                if (cbuf && cbuf != sbuf) {
                    TCFREE(cbuf);
                }
            } else {
                rv = (exprsz <= fvalsz) && strstr(fval, expr);
            }
            break;
        }
        case TDBQCSTRBW: {
            _FETCHSTRFVAL();
            if ((qf->flags & EJCONDICASE) && (bt == BSON_STRING)) {
                cbufstrlen = tcicaseformat(fval, fvalsz - 1, sbuf, JBSTRINOPBUFFERSZ, &cbuf);
                if (cbufstrlen < 0) {
                    _ejdbsetecode(qf->jb, cbufstrlen, __FILE__, __LINE__, __func__);
                    rv = false;
                } else {
                    rv = tcstrfwm(cbuf, expr);
                }
                if (cbuf && cbuf != sbuf) {
                    TCFREE(cbuf);
                }
            } else {
                rv = tcstrfwm(fval, expr);
            }
            break;
        }
        case TDBQCSTREW: {
            _FETCHSTRFVAL();
            if ((qf->flags & EJCONDICASE) && (bt == BSON_STRING)) {
                cbufstrlen = tcicaseformat(fval, fvalsz - 1, sbuf, JBSTRINOPBUFFERSZ, &cbuf);
                if (cbufstrlen < 0) {
                    _ejdbsetecode(qf->jb, cbufstrlen, __FILE__, __LINE__, __func__);
                    rv = false;
                } else {
                    rv = tcstrbwm(cbuf, expr);
                }
                if (cbuf && cbuf != sbuf) {
                    TCFREE(cbuf);
                }
            } else {
                rv = tcstrbwm(fval, expr);
            }
            break;
        }
        case TDBQCSTRAND: {
            TCLIST *tokens = qf->exprlist;
            assert(tokens);
            _FETCHSTRFVAL();
            if ((qf->flags & EJCONDICASE) && (bt == BSON_STRING)) {
                cbufstrlen = tcicaseformat(fval, fvalsz - 1, sbuf, JBSTRINOPBUFFERSZ, &cbuf);
                if (cbufstrlen < 0) {
                    _ejdbsetecode(qf->jb, cbufstrlen, __FILE__, __LINE__, __func__);
                    rv = false;
                } else {
                    rv = _qrycondcheckstrand(cbuf, tokens);
                }
                if (cbuf && cbuf != sbuf) {
                    TCFREE(cbuf);
                }
            } else {
                rv = _qrycondcheckstrand(fval, tokens);
            }
            break;
        }
        case TDBQCSTROR: {
            TCLIST *tokens = qf->exprlist;
            assert(tokens);
            _FETCHSTRFVAL();
            if ((qf->flags & EJCONDICASE) && (bt == BSON_STRING)) {
                cbufstrlen = tcicaseformat(fval, fvalsz - 1, sbuf, JBSTRINOPBUFFERSZ, &cbuf);
                if (cbufstrlen < 0) {
                    _ejdbsetecode(qf->jb, cbufstrlen, __FILE__, __LINE__, __func__);
                    rv = false;
                } else {
                    rv = _qrycondcheckstror(cbuf, tokens);
                }
                if (cbuf && cbuf != sbuf) {
                    TCFREE(cbuf);
                }
            } else {
                rv = _qrycondcheckstror(fval, tokens);
            }
            break;
        }
        case TDBQCSTROREQ: {
            TCLIST *tokens = qf->exprlist;
            assert(tokens);
            _FETCHSTRFVAL();
            if ((qf->flags & EJCONDICASE) && (bt == BSON_STRING)) {
                cbufstrlen = tcicaseformat(fval, fvalsz - 1, sbuf, JBSTRINOPBUFFERSZ, &cbuf);
                if (cbufstrlen < 0) {
                    _ejdbsetecode(qf->jb, cbufstrlen, __FILE__, __LINE__, __func__);
                    rv = false;
                } else {
                    if (qf->exprmap) {
                        if (tcmapget(qf->exprmap, cbuf, cbufstrlen, &sp) != NULL) {
                            rv = true;
                            break;
                        }
                    } else {
                        for (int i = 0; i < TCLISTNUM(tokens); ++i) {
                            const char *token = TCLISTVALPTR(tokens, i);
                            int tokensz = TCLISTVALSIZ(tokens, i);
                            if (tokensz == cbufstrlen && !strncmp(token, cbuf, tokensz)) {
                                rv = true;
                                break;
                            }
                        }
                    }
                }
                if (cbuf && cbuf != sbuf) {
                    TCFREE(cbuf);
                }
            } else {
                if (qf->exprmap) {
                    if (tcmapget3(qf->exprmap, fval, (fvalsz - 1), &sp) != NULL) {
                        rv = true;
                        break;
                    }
                } else {
                    for (int i = 0; i < TCLISTNUM(tokens); ++i) {
                        const char *token = TCLISTVALPTR(tokens, i);
                        int tokensz = TCLISTVALSIZ(tokens, i);
                        if (tokensz == (fvalsz - 1) && !strncmp(token, fval, tokensz)) {
                            rv = true;
                            break;
                        }
                    }
                }
            }
            break;
        }
        case TDBQCSTRORBW: {
            TCLIST *tokens = qf->exprlist;
            assert(tokens);
            _FETCHSTRFVAL();
            if ((qf->flags & EJCONDICASE) && (bt == BSON_STRING)) {
                cbufstrlen = tcicaseformat(fval, fvalsz - 1, sbuf, JBSTRINOPBUFFERSZ, &cbuf);
                if (cbufstrlen < 0) {
                    _ejdbsetecode(qf->jb, cbufstrlen, __FILE__, __LINE__, __func__);
                    rv = false;
                } else {
                    for (int i = 0; i < TCLISTNUM(tokens); ++i) {
                        const char *token = TCLISTVALPTR(tokens, i);
                        int tokensz = TCLISTVALSIZ(tokens, i);
                        if (tokensz <= cbufstrlen && !strncmp(token, cbuf, tokensz)) {
                            rv = true;
                            break;
                        }
                    }
                }
                if (cbuf && cbuf != sbuf) {
                    TCFREE(cbuf);
                }
            } else {
                for (int i = 0; i < TCLISTNUM(tokens); ++i) {
                    const char *token = TCLISTVALPTR(tokens, i);
                    int tokensz = TCLISTVALSIZ(tokens, i);
                    if (tokensz <= (fvalsz - 1) && !strncmp(token, fval, tokensz)) {
                        rv = true;
                        break;
                    }
                }
            }
            break;
        }
        case TDBQCSTRRX: {
            _FETCHSTRFVAL();
            rv = qf->regex && (regexec((regex_t *) qf->regex, fval, 0, NULL, 0) == 0);
            break;
        }
        case TDBQCNUMEQ: {
            if (bt == BSON_DOUBLE) {
                rv = (qf->exprdblval == bson_iterator_double_raw(it));
            } else if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
                rv = (qf->exprlongval == bson_iterator_long(it));
            } else {
                rv = false;
            }
            break;
        }
        case TDBQCNUMGT: {
            if (bt == BSON_DOUBLE) {
                rv = (qf->exprdblval < bson_iterator_double_raw(it));
            } else if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
                rv = (qf->exprlongval < bson_iterator_long(it));
            } else {
                rv = false;
            }
            break;
        }
        case TDBQCNUMGE: {
            if (bt == BSON_DOUBLE) {
                rv = (qf->exprdblval <= bson_iterator_double_raw(it));
            } else if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
                rv = (qf->exprlongval <= bson_iterator_long(it));
            } else {
                rv = false;
            }
            break;
        }
        case TDBQCNUMLT: {
            if (bt == BSON_DOUBLE) {
                rv = (qf->exprdblval > bson_iterator_double_raw(it));
            } else if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
                rv = (qf->exprlongval > bson_iterator_long(it));
            } else {
                rv = false;
            }
            break;
        }
        case TDBQCNUMLE: {
            if (bt == BSON_DOUBLE) {
                rv = (qf->exprdblval >= bson_iterator_double_raw(it));
            } else if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
                rv = (qf->exprlongval >= bson_iterator_long(it));
            } else {
                rv = false;
            }
            break;
        }
        case TDBQCNUMBT: {
            assert(qf->ftype == BSON_ARRAY);
            TCLIST *tokens = qf->exprlist;
            assert(tokens);
            assert(TCLISTNUM(tokens) == 2);
            if (BSON_ITERATOR_TYPE(it) == BSON_DOUBLE) {
                double v1 = tcatof(tclistval2(tokens, 0));
                double v2 = tcatof(tclistval2(tokens, 1));
                double val = bson_iterator_double(it);
                rv = (v2 > v1) ? (v2 >= val && v1 <= val) : (v2 <= val && v1 >= val);
            } else {
                int64_t v1 = tcatoi(tclistval2(tokens, 0));
                int64_t v2 = tcatoi(tclistval2(tokens, 1));
                int64_t val = bson_iterator_long(it);
                rv = (v2 > v1) ? (v2 >= val && v1 <= val) : (v2 <= val && v1 >= val);
            }
            break;
        }
        case TDBQCNUMOREQ: {
            TCLIST *tokens = qf->exprlist;
            assert(tokens);
            if (bt == BSON_DOUBLE) {
                double nval = bson_iterator_double_raw(it);
                for (int i = 0; i < TCLISTNUM(tokens); ++i) {
                    if (tcatof(TCLISTVALPTR(tokens, i)) == nval) {
                        rv = true;
                        break;
                    }
                }
            } else if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
                int64_t nval = bson_iterator_long(it);
                for (int i = 0; i < TCLISTNUM(tokens); ++i) {
                    if (tcatoi(TCLISTVALPTR(tokens, i)) == nval) {
                        rv = true;
                        break;
                    }
                }
            }
            break;
        }
    }
    return (rv == !qf->negate);

#undef _FETCHSTRFVAL
}


// Fills `ffpctx` and `qf->uslots`
static void _qrysetarrayidx(FFPCTX *ffpctx, EJQF *qf, int dpos, int mpos) {
    if (ffpctx->dpos == dpos && ffpctx->mpos == -1) { //single ctx matching
        ffpctx->mpos = mpos;
    }
    if (qf->uslots) {
        for (int i = 0; i < TCLISTNUM(qf->uslots); ++i) {
            USLOT *us = TCLISTVALPTR(qf->uslots, i);
            assert(us);
            if (us->dpos == dpos && us->mpos == -1) {
                us->mpos = mpos;
            }
        }
    }
}

static bool _qrybsrecurrmatch(EJQF *qf, FFPCTX *ffpctx, int currpos) {
    assert(qf && ffpctx && ffpctx->stopnestedarr);
    bson_type bt = bson_find_fieldpath_value3(ffpctx);
    if (bt == BSON_ARRAY && ffpctx->stopos < ffpctx->fplen) { 
        // A bit of complicated code  in this case :-)
        // we just stepped in some array in middle of our fieldpath, 
        // so have to perform recursive nested iterations.
        // $elemMatch active in this context
        while (ffpctx->fpath[ffpctx->stopos] == '.' && ffpctx->stopos < ffpctx->fplen) {
            ffpctx->stopos++;
        }
        ffpctx->fplen = ffpctx->fplen - ffpctx->stopos;
        assert(ffpctx->fplen > 0);
        ffpctx->fpath = ffpctx->fpath + ffpctx->stopos;
        currpos += ffpctx->stopos; //adjust cumulative field position

        bson_iterator sit;
        BSON_ITERATOR_SUBITERATOR(ffpctx->input, &sit);
        for (int arr_idx = 0;(bt = bson_iterator_next(&sit)) != BSON_EOO; ++arr_idx) {
            if (bt != BSON_OBJECT && bt != BSON_ARRAY)
                continue;

            bson_iterator sit2;
            BSON_ITERATOR_SUBITERATOR(&sit, &sit2);

            ffpctx->input = &sit2;

            // Match using context initialised above.
            if (_qrybsrecurrmatch(qf, ffpctx, currpos) == qf->negate) {
                continue;
            }

            bool ret = true;
            if (qf->elmatchgrp > 0 && qf->elmatchpos == currpos) { 
                // $elemMatch matching group exists at right place
                // Match all sub-queries on current field pos. Early exit (break) on failure.
                for (int i = TCLISTNUM(qf->q->qflist) - 1; i >= 0; --i) {
                    EJQF *eqf = TCLISTVALPTR(qf->q->qflist, i);
                    if (eqf == qf || (eqf->mflags & EJFEXCLUDED) || eqf->elmatchgrp != qf->elmatchgrp) {
                        continue;
                    }
                    eqf->mflags |= EJFEXCLUDED;
                    BSON_ITERATOR_SUBITERATOR(&sit, &sit2);
                    FFPCTX nffpctx = *ffpctx;
                    nffpctx.fplen = eqf->fpathsz - eqf->elmatchpos;
                    if (nffpctx.fplen <= 0) { //should never happen if query construction is correct
                        assert(false);
                        ret = false;
                        break;
                    }
                    nffpctx.fpath = eqf->fpath + eqf->elmatchpos;
                    nffpctx.input = &sit2;
                    nffpctx.stopos = 0;

                    // Match sub-query at current field pos.
                    // Ignores outer negate (qf) on inner query (eqf).
                    if (_qrybsrecurrmatch(eqf, &nffpctx, eqf->elmatchpos) == qf->negate) {
                        // Skip all remaining sub-queries on this field. Go to next element, if any.
                        ret = false;
                        break;
                    }
                }
            }
            if (ret) {
                _qrysetarrayidx(ffpctx, qf, (currpos - 1), arr_idx);
                // Only return success at this point.
                // An failure here may precede a later success so proceed to next element, if any.
                return ret != qf->negate;
            }
        }
        return qf->negate;
    } else {
        if (bt == BSON_EOO || bt == BSON_UNDEFINED || bt == BSON_NULL) {
            return qf->negate; //Field missing
        } else if (qf->tcop == TDBQCEXIST) {
            return !qf->negate;
        }
        int mpos = -1;
        bool ret = _qrybsvalmatch(qf, ffpctx->input, true, &mpos);
        if (ret && currpos == 0 && bt == BSON_ARRAY) { //save $(projection)
            _qrysetarrayidx(ffpctx, qf, ffpctx->fplen, mpos);
        }
        return ret;
    }
}

static bool _qrybsmatch(EJQF *qf, const void *bsbuf, int bsbufsz) {
    if (qf->tcop == TDBQTRUE) {
        return !qf->negate;
    }
    bson_iterator it;
    BSON_ITERATOR_FROM_BUFFER(&it, bsbuf);
    FFPCTX ffpctx = {
        .fpath = qf->fpath,
        .fplen = qf->fpathsz,
        .input = &it,
        .stopnestedarr = true,
        .stopos = 0,
        .dpos = -1,
        .mpos = -1
    };
    if (qf->uslots) {
        for (int i = 0; i < TCLISTNUM(qf->uslots); ++i) {
            ((USLOT*) (TCLISTVALPTR(qf->uslots, i)))->mpos = -1;
        }
    }
    return _qrybsrecurrmatch(qf, &ffpctx, 0);
}

static bool _qry_and_or_match(EJCOLL *coll, EJQ *ejq, const void *pkbuf, int pkbufsz) {
    bool isor = (ejq->orqlist && TCLISTNUM(ejq->orqlist) > 0);
    bool isand = (ejq->andqlist && TCLISTNUM(ejq->andqlist) > 0);
    if (!isor && !isand) {
        return true;
    }
    void *bsbuf = TCXSTRPTR(ejq->bsbuf);
    int bsbufsz = TCXSTRSIZE(ejq->bsbuf);

    if (!bsbuf || bsbufsz <= 0) {
        tcxstrclear(ejq->colbuf);
        tcxstrclear(ejq->bsbuf);
        if (tchdbgetintoxstr(coll->tdb->hdb, pkbuf, pkbufsz, ejq->colbuf) <= 0) {
            return false;
        }
        if (tcmaploadoneintoxstr(TCXSTRPTR(ejq->colbuf), TCXSTRSIZE(ejq->colbuf), 
                                 JDBCOLBSON, JDBCOLBSONL, ejq->bsbuf) <= 0) {
            return false;
        }
        bsbufsz = TCXSTRSIZE(ejq->bsbuf);
        bsbuf = TCXSTRPTR(ejq->bsbuf);
    }
    if (isand && !_qryandmatch2(coll, ejq, bsbuf, bsbufsz)) {
        return false;
    } else if (isor) {
        return _qryormatch2(coll, ejq, bsbuf, bsbufsz);
    } else {
        return true;
    }
}

static bool _qryormatch3(EJCOLL *coll, EJQ *ejq, EJQ *oq, const void *bsbuf, int bsbufsz) {
    int j = 0;
    int jm = TCLISTNUM(oq->qflist);
    for (; j < jm; ++j) {
        EJQF *qf = TCLISTVALPTR(oq->qflist, j);
        assert(qf);
        qf->mflags = qf->flags;
        if (qf->mflags & EJFEXCLUDED) {
            continue;
        }
        if (!_qrybsmatch(qf, bsbuf, bsbufsz)) {
            break;
        }
    }
    if (j == jm) { //all fields in oq are matched
        if (oq->andqlist && TCLISTNUM(oq->andqlist) > 0 &&
                !_qryandmatch2(coll, oq, bsbuf, bsbufsz)) { //we have nested $and fields
            return false;
        }
        if (oq->orqlist && TCLISTNUM(oq->orqlist) &&
                !_qryormatch2(coll, oq, bsbuf, bsbufsz)) { //we have nested $or fields
            return false;
        }
        return true;
    }
    return false;
}

static bool _qryormatch2(EJCOLL *coll, EJQ *ejq, const void *bsbuf, int bsbufsz) {
    if (!ejq->orqlist || TCLISTNUM(ejq->orqlist) < 1) {
        return true;
    }
    if (ejq->lastmatchedorq && _qryormatch3(coll, ejq, ejq->lastmatchedorq, bsbuf, bsbufsz)) {
        return true;
    }
    for (int i = 0; i < TCLISTNUM(ejq->orqlist); ++i) {
        EJQ *oq = *((EJQ**) TCLISTVALPTR(ejq->orqlist, i));
        assert(oq && oq->qflist);
        if (ejq->lastmatchedorq == oq) {
            continue;
        }
        if (_qryormatch3(coll, ejq, oq, bsbuf, bsbufsz)) {
            ejq->lastmatchedorq = oq;
            return true;
        }
    }
    return false;
}

static bool _qryandmatch2(EJCOLL *coll, EJQ *ejq, const void *bsbuf, int bsbufsz) {
    if (!ejq->andqlist || TCLISTNUM(ejq->andqlist) < 1) {
        return true;
    }
    for (int i = 0; i < TCLISTNUM(ejq->andqlist); ++i) {
        EJQ *aq = *((EJQ**) TCLISTVALPTR(ejq->andqlist, i));
        assert(aq && aq->qflist);
        for (int j = 0; j < TCLISTNUM(aq->qflist); ++j) {
            EJQF *qf = TCLISTVALPTR(aq->qflist, j);
            assert(qf);
            qf->mflags = qf->flags;
            if (qf->mflags & EJFEXCLUDED) {
                continue;
            }
            if (!_qrybsmatch(qf, bsbuf, bsbufsz)) {
                return false;
            }
        }
        if (aq->andqlist && TCLISTNUM(aq->andqlist) > 0 &&
                !_qryandmatch2(coll, aq, bsbuf, bsbufsz)) { //we have nested $and fields
            return false;
        }
        if (aq->orqlist && TCLISTNUM(aq->orqlist) > 0 &&
                !_qryormatch2(coll, aq, bsbuf, bsbufsz)) { //we have nested $or fields
            return false;
        }
    }
    return true;
}

/** Return true if all main query conditions matched */
static bool _qryallcondsmatch(
    EJQ *ejq, int anum,
    EJCOLL *coll, EJQF **qfs, int qfsz,
    const void *pkbuf, int pkbufsz) {
    assert(ejq->colbuf && ejq->bsbuf);
    if (!(ejq->flags & EJQUPDATING) && (ejq->flags & EJQONLYCOUNT) && anum < 1) {
        return true;
    }
    tcxstrclear(ejq->colbuf);
    tcxstrclear(ejq->bsbuf);
    if (tchdbgetintoxstr(coll->tdb->hdb, pkbuf, pkbufsz, ejq->colbuf) <= 0) {
        return false;
    }
    if (tcmaploadoneintoxstr(TCXSTRPTR(ejq->colbuf), TCXSTRSIZE(ejq->colbuf), 
                             JDBCOLBSON, JDBCOLBSONL, ejq->bsbuf) <= 0) {
        return false;
    }
    if (anum < 1) {
        return true;
    }
    for (int i = 0; i < qfsz; ++i) qfs[i]->mflags = qfs[i]->flags; //reset matching flags
    for (int i = 0; i < qfsz; ++i) {
        EJQF *qf = qfs[i];
        if (qf->mflags & EJFEXCLUDED) continue;
        if (!_qrybsmatch(qf, TCXSTRPTR(ejq->bsbuf), TCXSTRSIZE(ejq->bsbuf))) {
            return false;
        }
    }
    return true;
}

static EJQ* _qryaddand(EJDB *jb, EJQ *q, const void *andbsdata) {
    assert(jb && q && andbsdata);
    if (!andbsdata) {
        _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        return NULL;
    }
    EJQ *oq = ejdbcreatequery2(jb, andbsdata);
    if (oq == NULL) {
        return NULL;
    }
    if (q->andqlist == NULL) {
        q->andqlist = tclistnew2(TCLISTINYNUM);
    }
    tclistpush(q->andqlist, &oq, sizeof(oq));
    return q;
}

typedef struct {
    EJQF **ofs;
    int ofsz;
} _EJBSORTCTX;

/* RS sorting comparison func */
static int _ejdbsoncmp(const TCLISTDATUM *d1, const TCLISTDATUM *d2, void *opaque) {
    _EJBSORTCTX *ctx = opaque;
    assert(ctx);
    int res = 0;
    for (int i = 0; !res && i < ctx->ofsz; ++i) {
        const EJQF *qf = ctx->ofs[i];
        if (qf->flags & EJFORDERUSED) {
            continue;
        }
        res = bson_compare(d1->ptr, d2->ptr, qf->fpath, qf->fpathsz) * (qf->order >= 0 ? 1 : -1);
    }
    return res;
}

EJDB_INLINE void _nufetch(_EJDBNUM *nu, const char *sval, bson_type bt) {
    if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
        nu->inum = tcatoi(sval);
    } else if (bt == BSON_DOUBLE) {
        nu->dnum = tcatof(sval);
    } else {
        nu->inum = 0;
        assert(0);
    }
}

EJDB_INLINE int _nucmp(_EJDBNUM *nu, const char *sval, bson_type bt) {
    if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
        int64_t v = tcatoi(sval);
        return (nu->inum > v) ? 1 : (nu->inum < v ? -1 : 0);
    } else if (bt == BSON_DOUBLE) {
        double v = tcatof(sval);
        return (nu->dnum > v) ? 1 : (nu->dnum < v ? -1 : 0);
    } else {
        assert(0);
    }
    return 0;
}

EJDB_INLINE int _nucmp2(_EJDBNUM *nu1, _EJDBNUM *nu2, bson_type bt) {
    if (bt == BSON_INT || bt == BSON_LONG || bt == BSON_BOOL || bt == BSON_DATE) {
        return (nu1->inum > nu2->inum) ? 1 : (nu1->inum < nu2->inum ? -1 : 0);
    } else if (bt == BSON_DOUBLE) {
        return (nu1->dnum > nu2->dnum) ? 1 : (nu1->dnum < nu2->dnum ? -1 : 0);
    } else {
        assert(0);
    }
    return 0;
}

static void _qryfieldup(const EJQF *src, EJQF *target, uint32_t qflags) {
    assert(src && target);
    memset(target, 0, sizeof (*target));
    target->exprdblval = src->exprdblval;
    target->exprlongval = src->exprlongval;
    target->flags = src->flags;
    target->ftype = src->ftype;
    target->negate = src->negate;
    target->order = src->order;
    target->orderseq = src->orderseq;
    target->tcop = src->tcop;
    target->elmatchgrp = src->elmatchgrp;
    target->elmatchpos = src->elmatchpos;

    if (src->expr) {
        TCMEMDUP(target->expr, src->expr, src->exprsz);
        target->exprsz = src->exprsz;
    }
    if (src->fpath) {
        TCMEMDUP(target->fpath, src->fpath, src->fpathsz);
        target->fpathsz = src->fpathsz;
    }
    if (src->regex && (EJQINTERNAL & qflags)) {
        //We cannot do deep copy of regex_t so do shallow copy only for internal query objects
        target->regex = src->regex;
    }
    if (src->exprlist) {
        target->exprlist = tclistdup(src->exprlist);
    }
    if (src->exprmap) {
        target->exprmap = tcmapdup(src->exprmap);
    }
    if (src->updateobj) {
        target->updateobj = bson_dup(src->updateobj);
    }
    if (src->ufields) {
        target->ufields = tclistdup(src->ufields);
    }
    if (src->uslots) {
        target->uslots = tclistdup(src->uslots);
    }
}

/* Clone query object */
static bool _qrydup(const EJQ *src, EJQ *target, uint32_t qflags) {
    assert(src && target);
    memset(target, 0, sizeof (*target));
    target->flags = src->flags | qflags;
    target->max = src->max;
    target->skip = src->skip;
    if (src->qflist) {
        target->qflist = tclistnew2(TCLISTNUM(src->qflist));
        for (int i = 0; i < TCLISTNUM(src->qflist); ++i) {
            EJQF qf;
            _qryfieldup(TCLISTVALPTR(src->qflist, i), &qf, qflags);
            qf.q = target;
            TCLISTPUSH(target->qflist, &qf, sizeof (qf));
        }
    }
    if (src->hints) {
        target->hints = bson_dup(src->hints);
    }
    if (src->ifields) {
        target->ifields = tcmapdup(src->ifields);
    }
    if (src->orqlist) {
        target->orqlist = tclistnew2(TCLISTNUM(src->orqlist));
        for (int i = 0; i < TCLISTNUM(src->orqlist); ++i) {
            EJQ *q;
            TCMALLOC(q, sizeof(*q));
            if (_qrydup(*((EJQ**) TCLISTVALPTR(src->orqlist, i)), q, qflags)) {
                TCLISTPUSH(target->orqlist, &q, sizeof (q));
            } else {
                _qrydel(q, true);
            }
        }
    }
    if (src->andqlist) {
        target->andqlist = tclistnew2(TCLISTNUM(src->andqlist));
        for (int i = 0; i < TCLISTNUM(src->andqlist); ++i) {
            EJQ *q;
            TCMALLOC(q, sizeof(*q));
            if (_qrydup(*((EJQ**) TCLISTVALPTR(src->andqlist, i)), q, qflags)) {
                tclistpush(target->andqlist, &q, sizeof (q));
            } else {
                _qrydel(q, true);
            }
        }
    }
    return true;
}

typedef struct { /**> $do action visitor context */
    EJQ *q;
    EJDB *jb;
    TCMAP *dfields;
    bson *sbson;
} _BSONDOVISITORCTX;

static bson_visitor_cmd_t _bsondovisitor(const char *ipath, int ipathlen,
                                         const char *key, int keylen,
                                         const bson_iterator *it, 
                                         bool after, void *op) {

    assert(op);
    _BSONDOVISITORCTX *ictx = op;
    EJCOLL *coll;
    TCMAP *dfields = ictx->dfields;
    bson_type lbt = BSON_ITERATOR_TYPE(it), bt;
    bson_iterator doit, bufit, sit;
    int sp;
    const char *sval;
    const EJQF *dofield;
    bson_visitor_cmd_t rv = BSON_VCMD_SKIP_AFTER;

    switch (lbt) {
        case BSON_OBJECT: {
            bson_append_field_from_iterator(it, ictx->sbson);
            rv = (BSON_VCMD_SKIP_AFTER | BSON_VCMD_SKIP_NESTED);
            break;
        }
        case BSON_STRING:
        case BSON_OID:
        case BSON_ARRAY: {
            dofield = tcmapget(dfields, ipath, ipathlen, &sp);
            if (!dofield) {
                if (lbt == BSON_ARRAY) {
                    bson_append_field_from_iterator(it, ictx->sbson);
                    rv = (BSON_VCMD_SKIP_AFTER | BSON_VCMD_SKIP_NESTED);
                } else {
                    bson_append_field_from_iterator(it, ictx->sbson);
                }
                break;
            }
            assert(dofield->updateobj && (dofield->flags & EJCONDOIT));
            BSON_ITERATOR_INIT(&doit, dofield->updateobj);
            while ((bt = bson_iterator_next(&doit)) != BSON_EOO) {
				const char *dofname = BSON_ITERATOR_KEY(&doit);
				
				if (bt == BSON_STRING && !strcmp("$join", dofname)) {
					coll = _getcoll(ictx->jb, bson_iterator_string(&doit));
					if (!coll) break;
					bson_oid_t loid;
					if (lbt == BSON_STRING) {
						sval = bson_iterator_string(it);
						if (!ejdbisvalidoidstr(sval)) break;
						bson_oid_from_string(&loid, sval);
					} else if (lbt == BSON_OID) {
						loid = *(bson_iterator_oid(it));
					}
					if (lbt == BSON_STRING || lbt == BSON_OID) {
						tcxstrclear(ictx->q->colbuf);
						tcxstrclear(ictx->q->tmpbuf);
						if (!tchdbgetintoxstr(coll->tdb->hdb, &loid, sizeof (loid), ictx->q->colbuf) ||
								!tcmaploadoneintoxstr(TCXSTRPTR(ictx->q->colbuf), TCXSTRSIZE(ictx->q->colbuf),
													  JDBCOLBSON, JDBCOLBSONL, ictx->q->tmpbuf)) {
							break;
						}
						BSON_ITERATOR_FROM_BUFFER(&bufit, TCXSTRPTR(ictx->q->tmpbuf));
						bson_append_object_from_iterator(BSON_ITERATOR_KEY(it), &bufit, ictx->sbson);
						break;
					}
					if (lbt == BSON_ARRAY) {
						BSON_ITERATOR_SUBITERATOR(it, &sit);
						bson_append_start_array(ictx->sbson, BSON_ITERATOR_KEY(it));
						while ((bt = bson_iterator_next(&sit)) != BSON_EOO) {
							if (bt != BSON_STRING && bt != BSON_OID) {
								bson_append_field_from_iterator(&sit, ictx->sbson);
								continue;
							}
							if (bt == BSON_STRING) {
								sval = bson_iterator_string(&sit);
								if (!ejdbisvalidoidstr(sval)) break;
								bson_oid_from_string(&loid, sval);
							} else if (bt == BSON_OID) {
								loid = *(bson_iterator_oid(&sit));
							}
							tcxstrclear(ictx->q->colbuf);
							tcxstrclear(ictx->q->tmpbuf);
							if (!tchdbgetintoxstr(coll->tdb->hdb, &loid, sizeof (loid), ictx->q->colbuf) ||
									!tcmaploadoneintoxstr(TCXSTRPTR(ictx->q->colbuf), TCXSTRSIZE(ictx->q->colbuf),
														  JDBCOLBSON, JDBCOLBSONL, ictx->q->tmpbuf)) {
								bson_append_field_from_iterator(&sit, ictx->sbson);
								continue;
							}
							BSON_ITERATOR_FROM_BUFFER(&bufit, TCXSTRPTR(ictx->q->tmpbuf));
							bson_append_object_from_iterator(BSON_ITERATOR_KEY(&sit), &bufit, ictx->sbson);
						}
						bson_append_finish_array(ictx->sbson);
						rv = (BSON_VCMD_SKIP_AFTER | BSON_VCMD_SKIP_NESTED);
					}
					
				} else if (lbt == BSON_ARRAY && 
                           (bt == BSON_ARRAY || BSON_IS_NUM_TYPE(bt)) && 
                           !strcmp("$slice", dofname)) {
								
					bson_append_start_array(ictx->sbson, BSON_ITERATOR_KEY(it));
					int skip = 0, limit, idx = 0, i;
					char nbuff[TCNUMBUFSIZ];
					
					if (bt == BSON_ARRAY) { // $slice : [skip, limit]
						bson_type bt2;
						bson_iterator sit2;
						BSON_ITERATOR_SUBITERATOR(&doit, &sit2);
						
						bt2 = bson_find_fieldpath_value2("0", 1, &sit2);
						if (!BSON_IS_NUM_TYPE(bt2)) {
							bson_append_field_from_iterator(it, ictx->sbson);
							break;
						}
						skip = bson_iterator_int(&sit2);
						
						bt2 = bson_find_fieldpath_value2("1", 1, &sit2);
						if (!BSON_IS_NUM_TYPE(bt2)) {
							bson_append_field_from_iterator(it, ictx->sbson);
							break;
						}
						limit = abs(bson_iterator_int(&sit2));
					} else { // $slice : limit
						limit = abs(bson_iterator_int(&doit));
					}
					
					if (skip < 0) {
						int cnt = 0;
						BSON_ITERATOR_SUBITERATOR(it, &sit);
						while (bson_iterator_next(&sit) != BSON_EOO) ++cnt;
						skip = cnt + skip % cnt;
						if (skip == cnt) {
							skip = 0;
						}
					}
					
					limit = (limit <= INT_MAX - skip) ? limit + skip : INT_MAX;
					BSON_ITERATOR_SUBITERATOR(it, &sit);
					for (i = 0; idx < limit && (bt = bson_iterator_next(&sit)) != BSON_EOO; ++idx) {
						if (idx >= skip) {
							bson_numstrn(nbuff, TCNUMBUFSIZ, i++);
							bson_append_field_from_iterator2(nbuff, &sit, ictx->sbson);
						}
					}
					bson_append_finish_array(ictx->sbson);
					rv = (BSON_VCMD_SKIP_AFTER | BSON_VCMD_SKIP_NESTED);
				}
			}
            break;
        }
        default:
            bson_append_field_from_iterator(it, ictx->sbson);

    }
    return rv;
}

static bool _pushprocessedbson(_QRYCTX *ctx, const void *bsbuf, int bsbufsz) {
    assert(bsbuf && bsbufsz);
    if (!ctx->dfields && !ctx->ifields && !ctx->q->ifields) { 
        // Trivial case: no $do operations or $fields
        tclistpush(ctx->res, bsbuf, bsbufsz);
        return true;
    }
    bool rv = true;
    EJDB *jb = ctx->coll->jb;
    EJQ *q = ctx->q;
    TCMAP *ifields = ctx->ifields;
    int sp;
    char bstack[JBSBUFFERSZ];
    bson bsout;
    bson_init_on_stack(&bsout, bstack, bsbufsz, JBSBUFFERSZ);

    if (ctx->dfields) { // $do fields exists
        rv = _exec_do(ctx, bsbuf, &bsout);
    }

    if (rv && (ifields || q->ifields)) { // $fields hints
    
        TCMAP *_ifields = ifields;
        TCMAP *_fkfields = NULL; // Fields with overriden keys
        char* inbuf = (bsout.finished) ? bsout.data : (char*) bsbuf;
        if (bsout.finished) {
            bson_init_size(&bsout, bson_size(&bsout));
        }
        if (q->ifields) { // We have positional $(projection)
            assert(ctx->imode == true); // Ensure we are in include mode
            if (!_ifields) {
                _ifields = tcmapnew2(q->ifields->bnum);
            } else {
                _ifields = tcmapdup(ifields);
            }
            _fkfields = tcmapnew2(TCMAPTINYBNUM);
            for (int i = 0; i < TCLISTNUM(q->qflist); ++i) {
                EJQF *qf = TCLISTVALPTR(q->qflist, i);
                const char *dfpath = tcmapget(q->ifields, qf->fpath, qf->fpathsz, &sp);
                if (dfpath) {
                    TCXSTR *ifield = tcxstrnew3(sp + 10);
                    bson_iterator it;
                    BSON_ITERATOR_FROM_BUFFER(&it, inbuf);
                    FFPCTX ctx = {
                        .fpath = qf->fpath,
                        .fplen = qf->fpathsz,
                        .input = &it,
                        .stopnestedarr = true,
                        .stopos = 0,
                        .mpos = -1,
                        .dpos = -1
                    };
                    const char *dpos = strchr(dfpath, '$');
                    assert(dpos);
                    ctx.dpos = (dpos - dfpath) - 1;
                    qf->mflags = (qf->flags & ~EJFEXCLUDED);
                    if (!_qrybsrecurrmatch(qf, &ctx, 0)) {
                        assert(false); // Something went wrong, it should never be happen
                    } else if (ctx.mpos >= 0) {
                        tcxstrcat(ifield, dfpath, (dpos - dfpath));
                        tcxstrprintf(ifield, "%d", ctx.mpos);
                        tcmapput(_fkfields, TCXSTRPTR(ifield), TCXSTRSIZE(ifield), "0", strlen("0"));
                        tcxstrcat(ifield, dpos + 1, sp - (dpos - dfpath) - 1);
                        tcmapput(_ifields, TCXSTRPTR(ifield), TCXSTRSIZE(ifield), &yes, sizeof (yes));
                    } else {
                        assert(false); // Something went wrong, it should never be happen
                    }
                    tcxstrdel(ifield);
                }
            }
        }
        
        BSONSTRIPCTX sctx = {
            .ifields = _ifields,
            .fkfields = _fkfields,
            .imode = ctx->imode,
            .bsbuf = inbuf,
            .bsout = &bsout,
            .matched = 0
        };
        
        if (bson_strip2(&sctx) != BSON_OK) {
            _ejdbsetecode(jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        }
        if (inbuf != bsbuf && inbuf != bstack) {
            TCFREE(inbuf);
        }
        if (_ifields != ifields) {
            tcmapdel(_ifields);
        }
        if (_fkfields) {
            tcmapdel(_fkfields);
        }
    }

    if (rv) {
        assert(bsout.finished);
        if (bsout.flags & BSON_FLAG_STACK_ALLOCATED) {
            TCLISTPUSH(ctx->res, bsout.data, bson_size(&bsout));
        } else {
            tclistpushmalloc(ctx->res, bsout.data, bson_size(&bsout));
        }
    } else {
        bson_destroy(&bsout);
    }
    return rv;
}

static bool _exec_do(_QRYCTX *ctx, const void *bsbuf, bson *bsout) {
    assert(ctx && ctx->dfields);
    
    _BSONDOVISITORCTX ictx = {
        .q = ctx->q,
        .jb = ctx->coll->jb,
        .dfields = ctx->dfields,
        .sbson = bsout
    };
    
    bson_iterator it;
    BSON_ITERATOR_FROM_BUFFER(&it, bsbuf);
    bson_visit_fields(&it, 0, _bsondovisitor, &ictx);
    if (bson_finish(bsout) != BSON_OK) {
        _ejdbsetecode(ctx->coll->jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
        return false;
    }
    return true;
}

// Create update BSON object for $set/$unset/$inc operations
static bson* _qfgetupdateobj(const EJQF *qf) {
    assert(qf->updateobj);
    if (!qf->ufields || TCLISTNUM(qf->ufields) < 1) { // We do not ref $(query) fields.
        return qf->updateobj;
    }
    const EJQ *q = qf->q;
    char pbuf[BSON_MAX_FPATH_LEN + 1];
    int ppos = 0;
    bson_iterator it;
    bson_type bt;
    bson *ret =  bson_create();
    bson_init(ret);
    for (int i = 0; i < TCLISTNUM(qf->ufields); ++i) {
        const char *uf = TCLISTVALPTR(qf->ufields, i);
        for (int j = 0; *(q->allqfields + j) != '\0'; ++j) {
            const EJQF *kqf = *(q->allqfields + j);
            if (kqf == qf || kqf->uslots == NULL || TCLISTNUM(kqf->uslots) < 1) {
                continue;
            }
            for (int k = 0; k < TCLISTNUM(kqf->uslots); ++k) {
                USLOT *uslot = TCLISTVALPTR(kqf->uslots, k);
                if (uslot->op == uf && uslot->mpos >= 0) {
                    char *dp = strchr(uf, '$');
                    assert(dp);
                    ppos = (dp - uf);
                    assert(ppos == uslot->dpos + 1);
                    if (ppos < 1 || ppos >= BSON_MAX_FPATH_LEN - 1) {
                        break;
                    }
                    memcpy(pbuf, uf, ppos);
                    int wl = bson_numstrn(pbuf + ppos, (BSON_MAX_FPATH_LEN - ppos), uslot->mpos);
                    if (wl >= BSON_MAX_FPATH_LEN - ppos) { // Output is truncated
                        break;
                    }
                    ppos += wl;
                    // Copy suffix
                    for (int fpos = (dp - uf) + 1; ppos < BSON_MAX_FPATH_LEN && *(uf + fpos) != '\0';) {
                        pbuf[ppos++] = *(uf + fpos++);
                    }
                    assert(ppos <= BSON_MAX_FPATH_LEN);
                    pbuf[ppos] = '\0';

                    bt = bson_find(&it, qf->updateobj, uf);
                    if (bt == BSON_EOO) {
                        assert(false);
                        break;
                    }
                    bson_append_field_from_iterator2(pbuf, &it, ret);
                    break;
                }
            }
        }
    }
    BSON_ITERATOR_INIT(&it, qf->updateobj);
    while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
        const char *key = bson_iterator_key(&it);
        if (strchr(key, '$') == NULL) {
            bson_append_field_from_iterator2(key, &it, ret);
        }
    }
    bson_finish(ret);
    return ret;
}

static bool _qryupdate(_QRYCTX *ctx, void *bsbuf, int bsbufsz) {
    assert(ctx && ctx->q && (ctx->q->flags & EJQUPDATING) && bsbuf && ctx->didxctx);

    bool rv = true;
    int update = 0;
    EJCOLL *coll = ctx->coll;
    EJQ *q = ctx->q;
    bson_oid_t *oid;
    bson_type bt, bt2;
    bson_iterator it, it2;
    TCMAP *rowm = NULL;

    if (q->flags & EJQDROPALL) { // Records will be dropped
        bt = bson_find_from_buffer(&it, bsbuf, JDBIDKEYNAME);
        if (bt != BSON_OID) {
            _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
            return false;
        }
        oid = bson_iterator_oid(&it);
        assert(oid);
        if (ctx->log) {
            char xoid[25];
            bson_oid_to_string(oid, xoid);
            tcxstrprintf(ctx->log, "$DROPALL ON: %s\n", xoid);
        }
        const void *olddata;
        int olddatasz = 0;
        TCMAP *rmap = tctdbget(coll->tdb, oid, sizeof (*oid));
        if (rmap) {
            olddata = tcmapget3(rmap, JDBCOLBSON, JDBCOLBSONL, &olddatasz);
            if (!_updatebsonidx(coll, oid, NULL, olddata, olddatasz, ctx->didxctx) ||
                    !tctdbout(coll->tdb, oid, sizeof (*oid))) {
                rv = false;
            }
            tcmapdel(rmap);
        }
        return rv;
    }
    
    //Apply update operation
    bson bsout;
    bson_init_size(&bsout, 0);

    const EJQF *setqf = NULL;                   // $set 
    const EJQF *unsetqf = NULL;                 // $unset
    const EJQF *incqf = NULL;                   // $inc
	const EJQF *renameqf = NULL;                // $rename
    const EJQF *addsetqf[2] = {NULL};           // $addToSet, $addToSetAll
    const EJQF *pushqf[2] = {NULL};             // $push, $pushAll
    const EJQF *pullqf[2] = {NULL};             // $pull, $pullAll

    for (int i = 0; i < TCLISTNUM(q->qflist); ++i) {
        
        const EJQF *qf = TCLISTVALPTR(q->qflist, i);
        const uint32_t flags = qf->flags;
        
        if (qf->updateobj == NULL) {
            continue;
        } 
        if (flags & EJCONDSET) {                // $set
            setqf = qf;
        } else if (flags & EJCONDUNSET) {       // $unset
            unsetqf = qf;
        } else if (flags & EJCONDINC) {         // $inc
            incqf = qf;
        } else if (flags & EJCONDRENAME) {      // $rename
			renameqf = qf;
		} else if (flags & EJCONDADDSET) {      // $addToSet, $addToSetAll
            if (flags & EJCONDALL) {
                addsetqf[1] = qf;
            } else {
                addsetqf[0] = qf;
            }
        } else if (flags & EJCONDPUSH) {        // $push, $pushAll
            if (flags & EJCONDALL) {
                pushqf[1] = qf;
            } else {
                pushqf[0] = qf;
            }
        } else if (flags & EJCONDPULL) {        // $pull, $pullAll
            if (flags & EJCONDALL) {
                pullqf[1] = qf;
            } else {
                pullqf[0] = qf;
            }
        }
    }
    
	if (renameqf) {
        char *inbuf = (bsout.finished) ? bsout.data : bsbuf;
        if (bsout.finished) {
            // Reinit `bsout`, `inbuf` already points to `bsout.data` and will be freed later
            bson_init_size(&bsout, bson_size(&bsout));
        } else {
            assert(bsout.data == NULL);
            bson_init_size(&bsout, bsbufsz);
        }
		bson *updobj = _qfgetupdateobj(renameqf);
        TCMAP *rfields = tcmapnew2(TCMAPTINYBNUM);
        BSON_ITERATOR_INIT(&it, updobj);
        while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
            if (bt != BSON_STRING) {
                continue;
            }
            const char *nfpath = bson_iterator_string(&it);
            int nlen = bson_iterator_string_len(&it);
            if (nlen == 0) {
                continue;
            }
            tcmapputkeep(rfields, 
                         BSON_ITERATOR_KEY(&it), strlen(BSON_ITERATOR_KEY(&it)), 
                         nfpath, nlen);
        }
        int rencnt;
        if (bson_rename(rfields, inbuf, &bsout, &rencnt) != BSON_OK) {
            rv = false;
            _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);    
        }
        if (rencnt > 0) {
            update++;
        }
        tcmapdel(rfields);
        bson_finish(&bsout);
        if (inbuf != bsbuf) {
            TCFREE(inbuf);
        }
        if (updobj != renameqf->updateobj) {
            bson_del(updobj);
        }
        if (!rv) {
            goto finish;
        }
	}
    
    if (unsetqf) { //$unset
        char *inbuf = (bsout.finished) ? bsout.data : bsbuf;
        if (bsout.finished) {
            bson_init_size(&bsout, bson_size(&bsout));
        } else {
            assert(bsout.data == NULL);
            bson_init_size(&bsout, bsbufsz);
        }
        int matched;
        bson *updobj = _qfgetupdateobj(unsetqf);
        TCMAP *ifields = tcmapnew2(TCMAPTINYBNUM);
        BSON_ITERATOR_INIT(&it, updobj);
        while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
            const char *fpath = BSON_ITERATOR_KEY(&it);
            tcmapput(ifields, fpath, strlen(fpath), &yes, sizeof(yes));
        }
        if (bson_strip(ifields, false, inbuf, &bsout, &matched) != BSON_OK) {
            rv = false;
            _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
        }
        if (matched > 0) {
            update++;
        }
        tcmapdel(ifields);
        bson_finish(&bsout);
        if (inbuf != bsbuf) {
            TCFREE(inbuf);
        }
        if (updobj != unsetqf->updateobj) {
            bson_del(updobj);
        }
        if (!rv) {
            goto finish;
        }
    }

    if (setqf) { //$set
        update++;
        bson *updobj = _qfgetupdateobj(setqf);
        char *inbuf = (bsout.finished) ? bsout.data : bsbuf;
        if (bsout.finished) {
            bson_init_size(&bsout, bson_size(&bsout));
        } else {
            assert(bsout.data == NULL);
            bson_init_size(&bsout, bsbufsz);
        }
        int err = bson_merge_fieldpaths(inbuf, bson_data(updobj), &bsout);
        if (err) {
            rv = false;
            _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
        }
        bson_finish(&bsout);
        if (inbuf != bsbuf) {
            TCFREE(inbuf);
        }
        if (updobj != setqf->updateobj) {
            bson_del(updobj);
        }
        if (!rv) {
            goto finish;
        }
    }

    if (incqf) { // $inc
        bson bsmerge; // holds missing $inc fieldpaths in object
        bson_init_size(&bsmerge, 0);
        bson *updobj = _qfgetupdateobj(incqf);
        if (!bsout.data) {
            bson_create_from_buffer2(&bsout, bsbuf, bsbufsz);
        }
        BSON_ITERATOR_INIT(&it, updobj);
        while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
            if (!BSON_IS_NUM_TYPE(bt)) {
                continue;
            }
            BSON_ITERATOR_INIT(&it2, &bsout);
            bt2 = bson_find_fieldpath_value(BSON_ITERATOR_KEY(&it), &it2);
            if (bt2 == BSON_EOO) { // found a missing field
                if (!bsmerge.data) {
                    bson_init_size(&bsmerge, bson_size(updobj));
                }
                if (bt == BSON_DOUBLE) {
                    bson_append_double(&bsmerge, BSON_ITERATOR_KEY(&it), bson_iterator_double(&it));
                } else {
                    bson_append_long(&bsmerge, BSON_ITERATOR_KEY(&it), bson_iterator_long(&it));
                }
            }
            if (!BSON_IS_NUM_TYPE(bt2)) {
                continue;
            }
            if (bt2 == BSON_DOUBLE) {
                double v = bson_iterator_double(&it2);
                if (bt == BSON_DOUBLE) {
                    v += bson_iterator_double(&it);
                } else {
                    v += bson_iterator_long(&it);
                }
                if (bson_inplace_set_double(&it2, v)) {
                    rv = false;
                    _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
                    break;
                }
                update++;
            } else {
                int64_t v = bson_iterator_long(&it2);
                v += bson_iterator_long(&it);
                if (bson_inplace_set_long(&it2, v)) {
                    rv = false;
                    _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
                    break;
                }
                update++;
            }
        }
        
        if (rv && 
            bsmerge.data && 
            bson_finish(&bsmerge) == BSON_OK) {
            
            bson_finish(&bsout);
            char *inbuf = bsout.data;
            bson_init_size(&bsout, bson_size(&bsout));
            if (bson_merge_fieldpaths(inbuf, bsmerge.data, &bsout) != BSON_OK) {
                rv = false;
                 _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
            } else {
                update++;
            }
            if (inbuf != bsbuf) {
                TCFREE(inbuf);
            }
        }
        
        bson_finish(&bsout);
        bson_destroy(&bsmerge);
        if (updobj != incqf->updateobj) {
            bson_del(updobj);
        }
        if (!rv) {
            goto finish;
        }
    }

    for (int i = 0; i < 2; ++i) { // $pull $pullAll
        const EJQF *qf = pullqf[i];
        if (!qf) continue;
        char *inbuf = (bsout.finished) ? bsout.data : bsbuf;
        if (bson_find_merged_arrays(bson_data(qf->updateobj), inbuf, (qf->flags & EJCONDALL))) {
            if (bsout.finished) {
                bson_init_size(&bsout, bson_size(&bsout));
            } else {
                assert(bsout.data == NULL);
                bson_init_size(&bsout, bsbufsz);
            }
            // $pull $pullAll merge
            if (bson_merge_arrays(bson_data(qf->updateobj), inbuf, 
                                  BSON_MERGE_ARRAY_PULL, (qf->flags & EJCONDALL), &bsout)) {
                rv = false;
                _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
            }
            if (inbuf != bsbuf) {
                TCFREE(inbuf);
            }
            bson_finish(&bsout);
            update++;
        }
        if (!rv) {
            goto finish;
        }
    }
    
    for (int i = 0; i < 2; ++i) { // $push $pushAll
        const EJQF *qf = pushqf[i];
        if (!qf) continue;
        char *inbuf = (bsout.finished) ? bsout.data : bsbuf;
        if (bsout.finished) {
            bson_init_size(&bsout, bson_size(&bsout));
        } else {
            assert(bsout.data == NULL);
            bson_init_size(&bsout, bsbufsz);
        }
        // $push $pushAll merge
        if (bson_merge_arrays(bson_data(qf->updateobj), inbuf, 
                              BSON_MERGE_ARRAY_PUSH, (qf->flags & EJCONDALL), &bsout)) {
            rv = false;
            _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
        }
        if (inbuf != bsbuf) {
            TCFREE(inbuf);
        }
        bson_finish(&bsout);
        update++;
    }

    for (int i = 0; i < 2; ++i) { // $addToSet $addToSetAll
        const EJQF *qf = addsetqf[i];
        if (!qf) continue;
        char *inbuf = (bsout.finished) ? bsout.data : bsbuf;
        if ((qf->flags & EJCONDALL) || bson_find_unmerged_arrays(bson_data(qf->updateobj), inbuf)) {
            // Missing $addToSet element in some array field found
            if (bsout.finished) {
                bson_init_size(&bsout, bson_size(&bsout));
            } else {
                assert(bsout.data == NULL);
                bson_init_size(&bsout, bsbufsz);
            }
            // $addToSet $addToSetAll merge
            if (bson_merge_arrays(bson_data(qf->updateobj), inbuf, 
                                  BSON_MERGE_ARRAY_ADDSET, (qf->flags & EJCONDALL), &bsout)) {
                rv = false;
                _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
            }
            if (inbuf != bsbuf) {
                TCFREE(inbuf);
            }
            bson_finish(&bsout);
            update++;
        }
        if (!rv) {
            goto finish;
        }
    }
    
    // Finishing document update
    if (!update || !rv) {
        goto finish;
    }
    if (bsout.err) { // Resulting BSON is OK?
        rv = false;
        _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
        goto finish;
    }
    if (bson_size(&bsout) == 0) { // Record was not updated
        goto finish;
    }
    // Perform updating
    bt = bson_find_from_buffer(&it, bsbuf, JDBIDKEYNAME);
    if (bt != BSON_OID) {
        rv = false;
        _ejdbsetecode(coll->jb, JBEQUPDFAILED, __FILE__, __LINE__, __func__);
        goto finish;
    }
    oid = bson_iterator_oid(&it);
    rowm = tcmapnew2(TCMAPTINYBNUM);
    tcmapput(rowm, JDBCOLBSON, JDBCOLBSONL, bson_data(&bsout), bson_size(&bsout));
    rv = tctdbput(coll->tdb, oid, sizeof (*oid), rowm);
    if (rv) {
        rv = _updatebsonidx(coll, oid, &bsout, bsbuf, bsbufsz, ctx->didxctx);
    }

finish:
    bson_destroy(&bsout);
    if (rowm) {
        tcmapdel(rowm);
    }
    return rv;
}

/** Query */
static TCLIST* _qryexecute(EJCOLL *coll, const EJQ *_q, 
                           uint32_t *outcount, 
                           int qflags, TCXSTR *log) {
                               
    assert(coll && coll->tdb && coll->tdb->hdb);
    *outcount = 0;

    _QRYCTX ctx = {NULL};
    EJQ *q; //Clone the query object
    TCMALLOC(q, sizeof (*q));
    if (!_qrydup(_q, q, EJQINTERNAL)) {
        TCFREE(q);
        return NULL;
    }
    ctx.log = log;
    ctx.q = q;
    ctx.qflags = qflags;
    ctx.coll = coll;
    if (!_qrypreprocess(&ctx)) {
        _qryctxclear(&ctx);
        return NULL;
    }
    bool all = false; // If True we need all records to fetch (sorting)
    TCHDB *hdb = coll->tdb->hdb;
    TCLIST *res = ctx.res;
    EJQF *mqf = ctx.mqf;

    int sz = 0;         // Generic size var
    int anum = 0;       // Number of active conditions
    int ofsz = 0;       // Order fields count
    int aofsz = 0;      // Active order fields count
    const int qfsz = TCLISTNUM(q->qflist); // Number of all condition fields
    EJQF **ofs = NULL;  // Order fields
    EJQF **qfs = NULL;  // Condition fields array
    if (qfsz > 0) {
        TCMALLOC(qfs, qfsz * sizeof (EJQF*));
    }

    const void *kbuf;
    int kbufsz;
    const void *vbuf;
    int vbufsz;

    uint32_t count = 0; // Current count
    uint32_t max = (q->max > 0) ? q->max : UINT_MAX;
    uint32_t skip = q->skip;
    const TDBIDX *midx = mqf ? mqf->idx : NULL;

    if (midx) { // Main index used for ordering
        if (mqf->orderseq == 1 &&
                !(mqf->tcop == TDBQCSTRAND || 
                mqf->tcop == TDBQCSTROR || mqf->tcop == TDBQCSTRNUMOR)) {
                    
            mqf->flags |= EJFORDERUSED;
        }
    }
    for (int i = 0; i < qfsz; ++i) {
        EJQF *qf = TCLISTVALPTR(q->qflist, i);
        assert(qf);
        if (log) {
            if (qf->exprmap) {
                tcxstrprintf(log, "USING HASH TOKENS IN: %s\n", qf->fpath);
            }
            if (qf->flags & EJCONDOIT) {
                tcxstrprintf(log, "FIELD: %s HAS $do OPERATION\n", qf->fpath);
            }
        }
        qf->jb = coll->jb;
        qfs[i] = qf;
        if (qf->fpathsz > 0 && !(qf->flags & EJFEXCLUDED)) {
            anum++;
        }
        if (qf->orderseq) {
            ofsz++;
            if (q->flags & EJQONLYCOUNT) {
                qf->flags |= EJFORDERUSED;
            }
        }
    }
    if (ofsz > 0) { // Collect order fields array
        TCMALLOC(ofs, ofsz * sizeof (EJQF*));
        for (int i = 0; i < ofsz; ++i) {
            for (int j = 0; j < qfsz; ++j) {
                if (qfs[j]->orderseq == i + 1) { // orderseq starts with 1
                    ofs[i] = qfs[j];
                    if (!(ofs[i]->flags & EJFORDERUSED)) {
                        aofsz++;
                        if (ctx.ifields) { // Force order field to be included in result set
                            if (ctx.imode) { // add field to the included set
                                tcmapputkeep(ctx.ifields, ofs[i]->fpath, 
                                             ofs[i]->fpathsz, &yes, sizeof (yes));
                            } else { // remove field from excluded
                                tcmapout(ctx.ifields, ofs[i]->fpath, ofs[i]->fpathsz);
                            }
                        }
                    }
                    break;
                }
            }
        }
        for (int i = 0; i < ofsz; ++i) assert(ofs[i] != NULL);
    }

    if ((q->flags & EJQONLYCOUNT) && qfsz == 0 &&
            (q->orqlist == NULL || TCLISTNUM(q->orqlist) < 1) &&
            (q->andqlist == NULL || TCLISTNUM(q->andqlist) < 1)) { // primitive count(*) query
        count = coll->tdb->hdb->rnum;
        if (log) {
            tcxstrprintf(log, "SIMPLE COUNT(*): %u\n", count);
        }
        goto finish;
    }

    if (!(q->flags & EJQONLYCOUNT) && aofsz > 0 && (!midx || mqf->orderseq != 1)) { 
        // Main index is not the main order field
        all = true; // Need all records for ordering for some other fields
    }

    if (log) {
        tcxstrprintf(log, "UPDATING MODE: %s\n", (q->flags & EJQUPDATING) ? "YES" : "NO");
        tcxstrprintf(log, "MAX: %u\n", max);
        tcxstrprintf(log, "SKIP: %u\n", skip);
        tcxstrprintf(log, "COUNT ONLY: %s\n", (q->flags & EJQONLYCOUNT) ? "YES" : "NO");
        tcxstrprintf(log, "MAIN IDX: '%s'\n", midx ? midx->name : "NONE");
        tcxstrprintf(log, "ORDER FIELDS: %d\n", ofsz);
        tcxstrprintf(log, "ACTIVE CONDITIONS: %d\n", anum);
        tcxstrprintf(log, "ROOT $OR QUERIES: %d\n", ((q->orqlist) ? TCLISTNUM(q->orqlist) : 0));
        tcxstrprintf(log, "ROOT $AND QUERIES: %d\n", ((q->andqlist) ? TCLISTNUM(q->andqlist) : 0));
        tcxstrprintf(log, "FETCH ALL: %s\n", all ? "YES" : "NO");
    }
    if (max < UINT_MAX - skip) {
        max += skip;
    }
    if (max == 0) {
        goto finish;
    }
    if (!midx && (!mqf || !(mqf->flags & EJFPKMATCHING))) { 
        // Missing main index & no PK matching
        goto fullscan;
    }
    if (log) {
        tcxstrprintf(log, "MAIN IDX TCOP: %d\n", mqf->tcop);
    }

#define JBQREGREC(_pkbuf, _pkbufsz, _bsbuf, _bsbufsz)   \
    ++count; \
    if (q->flags & EJQUPDATING) { \
        _qryupdate(&ctx, (_bsbuf), (_bsbufsz)); \
    } \
    if (!(q->flags & EJQONLYCOUNT) && (all || count > skip)) { \
        _pushprocessedbson(&ctx, (_bsbuf), (_bsbufsz)); \
    }
    // eof #define JBQREGREC

    bool trim = (midx && *midx->name != '\0');
    if (anum > 0 && !(mqf->flags & EJFEXCLUDED) && !(mqf->uslots && TCLISTNUM(mqf->uslots) > 0)) {
        anum--;
        mqf->flags |= EJFEXCLUDED;
    }

    if (mqf->flags & EJFPKMATCHING) { // PK matching
        if (log) {
            tcxstrprintf(log, "PRIMARY KEY MATCHING: TRUE\n");
        }
        assert(mqf->expr);
        if (mqf->tcop == TDBQCSTREQ) {
            do {
                bson_oid_t oid;
                bson_oid_from_string(&oid, mqf->expr);
                tcxstrclear(q->colbuf);
                tcxstrclear(q->bsbuf);
                sz = tchdbgetintoxstr(coll->tdb->hdb, &oid, sizeof (oid), q->colbuf);
                if (sz <= 0) {
                    break;
                }
                sz = tcmaploadoneintoxstr(TCXSTRPTR(q->colbuf), TCXSTRSIZE(q->colbuf), 
                                          JDBCOLBSON, JDBCOLBSONL, q->bsbuf);
                if (sz <= 0) {
                    break;
                }
                bool matched = true;
                for (int i = 0; i < qfsz; ++i) qfs[i]->mflags = qfs[i]->flags;
                for (int i = 0; i < qfsz; ++i) {
                    EJQF *qf = qfs[i];
                    if (qf->mflags & EJFEXCLUDED) continue;
                    if (!_qrybsmatch(qf, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf))) {
                        matched = false;
                        break;
                    }
                }
                if (matched && _qry_and_or_match(coll, q, &oid, sizeof (oid))) {
                    JBQREGREC(&oid, sizeof (oid), TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                }
            } while (false);
        } else if (mqf->tcop == TDBQCSTROREQ) {
            TCLIST *tokens = mqf->exprlist;
            assert(tokens);
            tclistsort(tokens);
            for (int i = 1; i < TCLISTNUM(tokens); i++) {
                if (!strcmp(TCLISTVALPTR(tokens, i), TCLISTVALPTR(tokens, i - 1))) {
                    TCFREE(tclistremove2(tokens, i));
                    i--;
                }
            }
            int tnum = TCLISTNUM(tokens);
            for (int i = 0; (all || count < max) && i < tnum; i++) {
                bool matched = true;
                bson_oid_t oid;
                const char *token;
                int tsiz;
                TCLISTVAL(token, tokens, i, tsiz);
                if (tsiz < 1) {
                    continue;
                }
                bson_oid_from_string(&oid, token);
                tcxstrclear(q->bsbuf);
                tcxstrclear(q->colbuf);
                sz = tchdbgetintoxstr(coll->tdb->hdb, &oid, sizeof (oid), q->colbuf);
                if (sz <= 0) {
                    continue;
                }
                sz = tcmaploadoneintoxstr(TCXSTRPTR(q->colbuf), TCXSTRSIZE(q->colbuf), 
                                          JDBCOLBSON, JDBCOLBSONL, q->bsbuf);
                if (sz <= 0) {
                    continue;
                }
                for (int i = 0; i < qfsz; ++i) qfs[i]->mflags = qfs[i]->flags;
                for (int i = 0; i < qfsz; ++i) {
                    EJQF *qf = qfs[i];
                    if (qf->mflags & EJFEXCLUDED) continue;
                    if (!_qrybsmatch(qf, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf))) {
                        matched = false;
                        break;
                    }
                }
                if (matched && _qry_and_or_match(coll, q, &oid, sizeof (oid))) {
                    JBQREGREC(&oid, sizeof (oid), TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                }
            }
        } else {
            assert(0);
        }
    } else if (mqf->tcop == TDBQTRUE) {
        BDBCUR *cur = tcbdbcurnew(midx->db);
        if (mqf->order >= 0) {
            tcbdbcurfirst(cur);
        } else {
            tcbdbcurlast(cur);
        }
        while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
            if (trim) kbufsz -= 3;
            vbuf = tcbdbcurval3(cur, &vbufsz);
            if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                    
                JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
            }
            if (mqf->order >= 0) {
                tcbdbcurnext(cur);
            } else {
                tcbdbcurprev(cur);
            }
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCSTREQ) { /* String is equal to */
        assert(midx->type == TDBITLEXICAL);
        char *expr = mqf->expr;
        int exprsz = mqf->exprsz;
        BDBCUR *cur = tcbdbcurnew(midx->db);
        tcbdbcurjump(cur, expr, exprsz + trim);
        while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
            if (trim) kbufsz -= 3;
            if (kbufsz == exprsz && !memcmp(kbuf, expr, exprsz)) {
                vbuf = tcbdbcurval3(cur, &vbufsz);
                if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                    _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                        
                    JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                }
            } else {
                break;
            }
            tcbdbcurnext(cur);
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCSTRBW) { /* String begins with */
        assert(midx->type == TDBITLEXICAL);
        char *expr = mqf->expr;
        int exprsz = mqf->exprsz;
        BDBCUR *cur = tcbdbcurnew(midx->db);
        tcbdbcurjump(cur, expr, exprsz + trim);
        while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
            if (trim) kbufsz -= 3;
            if (kbufsz >= exprsz && !memcmp(kbuf, expr, exprsz)) {
                vbuf = tcbdbcurval3(cur, &vbufsz);
                if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                    _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                        
                    JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                }
            } else {
                break;
            }
            tcbdbcurnext(cur);
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCSTRORBW) { /* String begins with one token in */
        assert(mqf->ftype == BSON_ARRAY);
        assert(midx->type == TDBITLEXICAL);
        BDBCUR *cur = tcbdbcurnew(midx->db);
        TCLIST *tokens = mqf->exprlist;
        assert(tokens);
        tclistsort(tokens);
        for (int i = 1; i < TCLISTNUM(tokens); i++) {
            if (!strcmp(TCLISTVALPTR(tokens, i), TCLISTVALPTR(tokens, i - 1))) {
                TCFREE(tclistremove2(tokens, i));
                i--;
            }
        }
        if (mqf->order < 0 && (mqf->flags & EJFORDERUSED)) {
            tclistinvert(tokens);
        }
        int tnum = TCLISTNUM(tokens);
        for (int i = 0; (all || count < max) && i < tnum; i++) {
            const char *token;
            int tsiz;
            TCLISTVAL(token, tokens, i, tsiz);
            if (tsiz < 1) continue;
            tcbdbcurjump(cur, token, tsiz + trim);
            while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
                if (trim) kbufsz -= 3;
                if (kbufsz >= tsiz && !memcmp(kbuf, token, tsiz)) {
                    vbuf = tcbdbcurval3(cur, &vbufsz);
                    if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                        _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                            
                        JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                    }
                } else {
                    break;
                }
                tcbdbcurnext(cur);
            }
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCSTROREQ) { /* String is equal to at least one token in */
        assert(mqf->ftype == BSON_ARRAY);
        assert(midx->type == TDBITLEXICAL);
        BDBCUR *cur = tcbdbcurnew(midx->db);
        TCLIST *tokens = mqf->exprlist;
        assert(tokens);
        tclistsort(tokens);
        for (int i = 1; i < TCLISTNUM(tokens); i++) {
            if (!strcmp(TCLISTVALPTR(tokens, i), TCLISTVALPTR(tokens, i - 1))) {
                TCFREE(tclistremove2(tokens, i));
                i--;
            }
        }
        if (mqf->order < 0 && (mqf->flags & EJFORDERUSED)) {
            tclistinvert(tokens);
        }
        int tnum = TCLISTNUM(tokens);
        for (int i = 0; (all || count < max) && i < tnum; i++) {
            const char *token;
            int tsiz;
            TCLISTVAL(token, tokens, i, tsiz);
            if (tsiz < 1) continue;
            tcbdbcurjump(cur, token, tsiz + trim);
            while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
                if (trim) kbufsz -= 3;
                if (kbufsz == tsiz && !memcmp(kbuf, token, tsiz)) {
                    vbuf = tcbdbcurval3(cur, &vbufsz);
                    if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                        _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                            
                        JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                    }
                } else {
                    break;
                }
                tcbdbcurnext(cur);
            }
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCNUMEQ) { /* Number is equal to */
        assert(midx->type == TDBITDECIMAL);
        char *expr = mqf->expr;
        int exprsz = mqf->exprsz;
        BDBCUR *cur = tcbdbcurnew(midx->db);
        _EJDBNUM num;
        _nufetch(&num, expr, mqf->ftype);
        tctdbqryidxcurjumpnum(cur, expr, exprsz, true);
        while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
            if (_nucmp(&num, kbuf, mqf->ftype) == 0) {
                vbuf = tcbdbcurval3(cur, &vbufsz);
                if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                    _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                        
                    JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                }
            } else {
                break;
            }
            tcbdbcurnext(cur);
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCNUMGT || mqf->tcop == TDBQCNUMGE) {
        /* Number is greater than | number is greater than or equal to */
        assert(midx->type == TDBITDECIMAL);
        char *expr = mqf->expr;
        int exprsz = mqf->exprsz;
        BDBCUR *cur = tcbdbcurnew(midx->db);
        _EJDBNUM xnum;
        _nufetch(&xnum, expr, mqf->ftype);
        if (mqf->order < 0 && (mqf->flags & EJFORDERUSED)) { //DESC
            tcbdbcurlast(cur);
            while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
                _EJDBNUM knum;
                _nufetch(&knum, kbuf, mqf->ftype);
                int cmp = _nucmp2(&knum, &xnum, mqf->ftype);
                if (cmp < 0) break;
                if (cmp > 0 || (mqf->tcop == TDBQCNUMGE && cmp >= 0)) {
                    vbuf = tcbdbcurval3(cur, &vbufsz);
                    if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                        _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                            
                        JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                    }
                }
                tcbdbcurprev(cur);
            }
        } else { // ASC
            tctdbqryidxcurjumpnum(cur, expr, exprsz, true);
            while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
                _EJDBNUM knum;
                _nufetch(&knum, kbuf, mqf->ftype);
                int cmp = _nucmp2(&knum, &xnum, mqf->ftype);
                if (cmp > 0 || (mqf->tcop == TDBQCNUMGE && cmp >= 0)) {
                    vbuf = tcbdbcurval3(cur, &vbufsz);
                    if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                        _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                            
                        JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                    }
                }
                tcbdbcurnext(cur);
            }
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCNUMLT || mqf->tcop == TDBQCNUMLE) {
        /* Number is less than | number is less than or equal to */
        assert(midx->type == TDBITDECIMAL);
        char *expr = mqf->expr;
        int exprsz = mqf->exprsz;
        BDBCUR *cur = tcbdbcurnew(midx->db);
        _EJDBNUM xnum;
        _nufetch(&xnum, expr, mqf->ftype);
        if (mqf->order >= 0) { //ASC
            tcbdbcurfirst(cur);
            while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
                _EJDBNUM knum;
                _nufetch(&knum, kbuf, mqf->ftype);
                int cmp = _nucmp2(&knum, &xnum, mqf->ftype);
                if (cmp > 0) break;
                if (cmp < 0 || (cmp <= 0 && mqf->tcop == TDBQCNUMLE)) {
                    vbuf = tcbdbcurval3(cur, &vbufsz);
                    if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                        _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                            
                        JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                    }
                }
                tcbdbcurnext(cur);
            }
        } else {
            tctdbqryidxcurjumpnum(cur, expr, exprsz, false);
            while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
                _EJDBNUM knum;
                _nufetch(&knum, kbuf, mqf->ftype);
                int cmp = _nucmp2(&knum, &xnum, mqf->ftype);
                if (cmp < 0 || (cmp <= 0 && mqf->tcop == TDBQCNUMLE)) {
                    vbuf = tcbdbcurval3(cur, &vbufsz);
                    if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                        _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                            
                        JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                    }
                }
                tcbdbcurprev(cur);
            }
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCNUMBT) { /* Number is between two tokens of */
        assert(mqf->ftype == BSON_ARRAY);
        assert(midx->type == TDBITDECIMAL);
        assert(mqf->exprlist);
        TCLIST *tokens = mqf->exprlist;
        assert(TCLISTNUM(tokens) == 2);
        const char *expr;
        int exprsz;
        long double lower = tcatof2(tclistval2(tokens, 0));
        long double upper = tcatof2(tclistval2(tokens, 1));
        expr = tclistval2(tokens, (lower > upper) ? 1 : 0);
        exprsz = strlen(expr);
        if (lower > upper) {
            long double swap = lower;
            lower = upper;
            upper = swap;
        }
        BDBCUR *cur = tcbdbcurnew(midx->db);
        tctdbqryidxcurjumpnum(cur, expr, exprsz, true);
        while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
            if (tcatof2(kbuf) > upper) break;
            vbuf = tcbdbcurval3(cur, &vbufsz);
            if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                    
                JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
            }
            tcbdbcurnext(cur);
        }
        tcbdbcurdel(cur);
        if (!all && !(q->flags & EJQONLYCOUNT) && 
            mqf->order < 0 && (mqf->flags & EJFORDERUSED)) { //DESC
            
            tclistinvert(res);
        }
    } else if (mqf->tcop == TDBQCNUMOREQ) { /* Number is equal to at least one token in */
        assert(mqf->ftype == BSON_ARRAY);
        assert(midx->type == TDBITDECIMAL);
        BDBCUR *cur = tcbdbcurnew(midx->db);
        TCLIST *tokens = mqf->exprlist;
        assert(tokens);
        tclistsortex(tokens, tdbcmppkeynumasc);
        for (int i = 1; i < TCLISTNUM(tokens); i++) {
            if (tcatof2(TCLISTVALPTR(tokens, i)) == tcatof2(TCLISTVALPTR(tokens, i - 1))) {
                TCFREE(tclistremove2(tokens, i));
                i--;
            }
        }
        if (mqf->order < 0 && (mqf->flags & EJFORDERUSED)) {
            tclistinvert(tokens);
        }
        int tnum = TCLISTNUM(tokens);
        for (int i = 0; (all || count < max) && i < tnum; i++) {
            const char *token;
            int tsiz;
            TCLISTVAL(token, tokens, i, tsiz);
            if (tsiz < 1) continue;
            long double xnum = tcatof2(token);
            tctdbqryidxcurjumpnum(cur, token, tsiz, true);
            while ((all || count < max) && (kbuf = tcbdbcurkey3(cur, &kbufsz)) != NULL) {
                if (tcatof2(kbuf) == xnum) {
                    vbuf = tcbdbcurval3(cur, &vbufsz);
                    if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, vbuf, vbufsz) && 
                        _qry_and_or_match(coll, q, vbuf, vbufsz)) {
                            
                        JBQREGREC(vbuf, vbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                    }
                } else {
                    break;
                }
                tcbdbcurnext(cur);
            }
        }
        tcbdbcurdel(cur);
    } else if (mqf->tcop == TDBQCSTRAND || mqf->tcop == TDBQCSTROR || mqf->tcop == TDBQCSTRNUMOR) {
        /* String includes all tokens in | string includes at least one token in */
        assert(midx->type == TDBITTOKEN);
        assert(mqf->ftype == BSON_ARRAY);
        TCLIST *tokens = mqf->exprlist;
        assert(tokens);
        if (mqf->tcop == TDBQCSTRNUMOR) {
            tclistsortex(tokens, tdbcmppkeynumasc);
            for (int i = 1; i < TCLISTNUM(tokens); i++) {
                if (tcatof2(TCLISTVALPTR(tokens, i)) == tcatof2(TCLISTVALPTR(tokens, i - 1))) {
                    TCFREE(tclistremove2(tokens, i));
                    i--;
                }
            }
        } else {
            tclistsort(tokens);
            for (int i = 1; i < TCLISTNUM(tokens); i++) {
                if (!strcmp(TCLISTVALPTR(tokens, i), TCLISTVALPTR(tokens, i - 1))) {
                    TCFREE(tclistremove2(tokens, i));
                    i--;
                }
            }
        }
        TCMAP *tres = tctdbidxgetbytokens(coll->tdb, midx, tokens, mqf->tcop, log);
        tcmapiterinit(tres);
        while ((all || count < max) && (kbuf = tcmapiternext(tres, &kbufsz)) != NULL) {
            if (_qryallcondsmatch(q, anum, coll, qfs, qfsz, kbuf, kbufsz) && 
                _qry_and_or_match(coll, q, kbuf, kbufsz)) {
                    
                JBQREGREC(kbuf, kbufsz, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
            }
        }
        tcmapdel(tres);
    }

    if (q->flags & EJQONLYCOUNT) {
        goto finish;
    } else {
        goto sorting;
    }

fullscan: /* Full scan */
    assert(count == 0);
    assert(!res || TCLISTNUM(res) == 0);

    if ((q->flags & EJQDROPALL) && (q->flags & EJQONLYCOUNT)) {
        // If we are in primitive $dropall case. Query: {$dropall:true}
        if (qfsz == 1 && qfs[0]->tcop == TDBQTRUE) { //single $dropall field
            if (log) {
                tcxstrprintf(log, "VANISH WHOLE COLLECTION ON $dropall\n");
            }
            // Write lock already acquired so use impl
            count = coll->tdb->hdb->rnum;
            if (!tctdbvanish(coll->tdb)) {
                count = 0;
            }
            goto finish;
        }
    }

    if (log) {
        tcxstrprintf(log, "RUN FULLSCAN\n");
    }
    TCMAP *updkeys = (q->flags & EJQUPDATING) ? tcmapnew2(100 * 1024) : NULL;
    TCHDBITER *hdbiter = tchdbiter2init(hdb);
    if (!hdbiter) {
        goto finish;
    }
    TCXSTR *skbuf = tcxstrnew3(sizeof (bson_oid_t) + 1);
    tcxstrclear(q->colbuf);
    tcxstrclear(q->bsbuf);
    int rows = 0;
    while ((all || count < max) && tchdbiter2next(hdb, hdbiter, skbuf, q->colbuf)) {
        ++rows;
        sz = tcmaploadoneintoxstr(TCXSTRPTR(q->colbuf), TCXSTRSIZE(q->colbuf), 
                                  JDBCOLBSON, JDBCOLBSONL, q->bsbuf);
        if (sz <= 0) {
            goto wfinish;
        }
        bool matched = true;
        for (int i = 0; i < qfsz; ++i) qfs[i]->mflags = qfs[i]->flags;
        for (int i = 0; i < qfsz; ++i) {
            EJQF *qf = qfs[i];
            if (qf->mflags & EJFEXCLUDED) {
                continue;
            }
            if (!_qrybsmatch(qf, TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf))) {
                matched = false;
                break;
            }
        }
        if (matched && _qry_and_or_match(coll, q, TCXSTRPTR(skbuf), TCXSTRSIZE(skbuf))) {
            if (updkeys) { // We are in updating mode
                if (tcmapputkeep(updkeys, TCXSTRPTR(skbuf), 
                                 TCXSTRSIZE(skbuf), &yes, sizeof (yes))) {
                                     
                    JBQREGREC(TCXSTRPTR(skbuf), TCXSTRSIZE(skbuf), 
                              TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
                }
            } else {
                JBQREGREC(TCXSTRPTR(skbuf), TCXSTRSIZE(skbuf), 
                          TCXSTRPTR(q->bsbuf), TCXSTRSIZE(q->bsbuf));
            }
        }
wfinish:
        tcxstrclear(skbuf);
        tcxstrclear(q->colbuf);
        tcxstrclear(q->bsbuf);
    }
    tchdbiter2dispose(hdb, hdbiter);
    tcxstrdel(skbuf);
    if (updkeys) {
        tcmapdel(updkeys);
    }

sorting: /* Sorting resultset */
    if (!res || aofsz <= 0) { // No sorting needed
        goto finish;
    }
    _EJBSORTCTX sctx; // Sorting context
    sctx.ofs = ofs;
    sctx.ofsz = ofsz;
    ejdbqsortlist(res, _ejdbsoncmp, &sctx);

finish:
    // Check $upsert operation
    if (count == 0 && (q->flags & EJQUPDATING)) { // Finding the $upsert qf if no updates maden
        for (int i = 0; i < qfsz; ++i) {
            if (qfs[i]->flags & EJCONDUPSERT) {
                bson *updateobj = qfs[i]->updateobj;
                assert(updateobj);
                bson_oid_t oid;
                if (_ejdbsavebsonimpl(coll, updateobj, &oid, false)) {
                    bson *nbs = bson_create();
                    bson_init_size(nbs, 
                                   bson_size(updateobj) + 
                                   (strlen(JDBIDKEYNAME) + 1/*key*/ + 1/*type*/ + sizeof (oid)));
                    bson_append_oid(nbs, JDBIDKEYNAME, &oid);
                    bson_ensure_space(nbs, bson_size(updateobj) - 4);
                    bson_append(nbs, bson_data(updateobj) + 4, 
                                bson_size(updateobj) - (4 + 1/*BSON_EOO*/));
                    bson_finish(nbs);
                    if (nbs->err) {
                        _ejdbsetecode(coll->jb, JBEINVALIDBSON, __FILE__, __LINE__, __func__);
                        break;
                    }
                    ++count;
                    if (!(q->flags & EJQONLYCOUNT) && (all || count > skip)) {
                        _pushprocessedbson(&ctx, bson_data(nbs), bson_size(nbs));
                    }
                    bson_del(nbs);
                }
                break;
            }
        }
    } // EOF $upsert

    // Revert max
    if (max < UINT_MAX && max > skip) {
        max = max - skip;
    }
    if (res) {
        if (all) { // Skipping results after full sorting with skip > 0
            for (int i = 0; i < skip && res->num > 0; ++i) {
                TCFREE(res->array[res->start].ptr);
                ++(res->start);
                --(res->num);
            }
        }
        if ((res->start & 0xff) == 0 && res->start > (res->num >> 1)) {
            memmove(res->array, res->array + res->start, res->num * sizeof (res->array[0]));
            res->start = 0;
        }
        if (TCLISTNUM(res) > max) { // truncate results if max specified
            int end = res->start + res->num;
            TCLISTDATUM *array = res->array;
            for (int i = (res->start + max); i < end; i++) {
                TCFREE(array[i].ptr);
                --(res->num);
            }
        }
    }
    count = (skip < count) ? count - skip : 0;
    if (count > max) {
        count = max;
    }
    *outcount = count;
    if (log) {
        tcxstrprintf(log, "RS COUNT: %u\n", count);
        tcxstrprintf(log, "RS SIZE: %d\n", (res ? TCLISTNUM(res) : 0));
        tcxstrprintf(log, "FINAL SORTING: %s\n", 
                           ((q->flags & EJQONLYCOUNT) || aofsz <= 0) ? "NO" : "YES");
    }

    // Apply deffered index changes
    if (ctx.didxctx) {
        for (int i = TCLISTNUM(ctx.didxctx) - 1; i >= 0; --i) {
            _DEFFEREDIDXCTX *di = TCLISTVALPTR(ctx.didxctx, i);
            assert(di);
            if (di->rmap) {
                tctdbidxout2(coll->tdb, &(di->oid), sizeof (di->oid), di->rmap);
                tcmapdel(di->rmap);
            }
            if (di->imap) {
                tctdbidxput2(coll->tdb, &(di->oid), sizeof (di->oid), di->imap);
                tcmapdel(di->imap);
            }
        }
    }
    // Cleanup
    if (qfs) {
        TCFREE(qfs);
    }
    if (ofs) {
        TCFREE(ofs);
    }
    ctx.res = NULL; // Save res from deleting in `_qryctxclear()`
    _qryctxclear(&ctx);
#undef JBQREGREC
    return res;
}

static void _qryctxclear(_QRYCTX *ctx) {
    if (ctx->dfields) {
        tcmapdel(ctx->dfields);
    }
    if (ctx->ifields) {
        tcmapdel(ctx->ifields);
    }
    if (ctx->q) {
        ejdbquerydel(ctx->q);
    }
    if (ctx->res) {
        tclistdel(ctx->res);
    }
    if (ctx->didxctx) {
        tclistdel(ctx->didxctx);
    }
    memset(ctx, 0, sizeof(*ctx));
}

static TDBIDX* _qryfindidx(EJCOLL *coll, EJQF *qf, bson *idxmeta) {
    TCTDB *tdb = coll->tdb;
    char p = '\0';
    switch (qf->tcop) {
        case TDBQCSTREQ:
        case TDBQCSTRBW:
        case TDBQCSTROREQ:
        case TDBQCSTRORBW:
            p = (qf->flags & EJCONDICASE) ? 'i' : 's'; // lexical string index
            break;
        case TDBQCNUMEQ:
        case TDBQCNUMGT:
        case TDBQCNUMGE:
        case TDBQCNUMLT:
        case TDBQCNUMLE:
        case TDBQCNUMBT:
        case TDBQCNUMOREQ:
            p = 'n'; // number index
            break;
        case TDBQCSTRAND:
        case TDBQCSTROR:
            p = 'a'; // token index
            break;
        case TDBQTRUE:
            p = 'o'; // take first appropriate index
            break;
    }
    if (p == '\0' || !qf->fpath || !qf->fpathsz) {
        return NULL;
    }
    for (int i = 0; i < tdb->inum; ++i) {
        TDBIDX *idx = tdb->idxs + i;
        assert(idx);
        if (p == 'o') {
            if (*idx->name == 'a' || *idx->name == 'i') { 
                // token index or icase index not the best solution here
                continue;
            }
        } else if (*idx->name != p) {
            continue;
        }
        if (!strcmp(qf->fpath, idx->name + 1)) {
            return idx;
        }
    }
    // No direct operation index. Any alternatives?
    if (idxmeta &&
            !(qf->flags & EJCONDICASE) && // Not a case insensitive query
            (
                qf->tcop == TDBQCSTREQ ||
                qf->tcop == TDBQCSTROREQ ||
                qf->tcop == TDBQCNUMOREQ ||
                qf->tcop == TDBQCNUMEQ)
       ) {
        bson_iterator it;
        bson_type bt = bson_find(&it, idxmeta, "iflags");
        if (bt != BSON_INT) {
            return NULL;
        }
        int iflags = bson_iterator_int(&it);
        if (iflags & JBIDXARR) { // Array token index exists so convert qf into TDBQCSTROR
            for (int i = 0; i < tdb->inum; ++i) {
                TDBIDX *idx = tdb->idxs + i;
                if (!strcmp(qf->fpath, idx->name + 1)) {
                    if (qf->tcop == TDBQCSTREQ) {
                        qf->tcop = TDBQCSTROR;
                        qf->exprlist = tclistnew2(1);
                        TCLISTPUSH(qf->exprlist, qf->expr, qf->exprsz);
                        if (qf->expr) TCFREE(qf->expr);
                        qf->expr = tclistdump(qf->exprlist, &qf->exprsz);
                        qf->ftype = BSON_ARRAY;
                        return idx;
                    } else if (qf->tcop == TDBQCNUMEQ) {
                        qf->tcop = TDBQCSTRNUMOR;
                        qf->exprlist = tclistnew2(1);
                        TCLISTPUSH(qf->exprlist, qf->expr, qf->exprsz);
                        if (qf->expr) TCFREE(qf->expr);
                        qf->expr = tclistdump(qf->exprlist, &qf->exprsz);
                        qf->ftype = BSON_ARRAY;
                        return idx;
                    } else if (qf->tcop == TDBQCSTROREQ) {
                        assert(qf->ftype == BSON_ARRAY);
                        qf->tcop = TDBQCSTROR;
                        return idx;
                    } else if (qf->tcop == TDBQCNUMOREQ) {
                        assert(qf->ftype == BSON_ARRAY);
                        qf->tcop = TDBQCSTRNUMOR;
                        return idx;
                    }
                }
            }
        }
    }
    return NULL;
}

static void _registerallqfields(TCLIST *reg, EJQ *q) {
    for (int i = 0; i < TCLISTNUM(q->qflist); ++i) {
        EJQF *qf = TCLISTVALPTR(q->qflist, i);
        assert(qf);
        tclistpush(reg, &qf, sizeof(qf));
    }
    for (int i = 0; q->andqlist && i < TCLISTNUM(q->andqlist); ++i) {
        _registerallqfields(reg, *((EJQ**) TCLISTVALPTR(q->andqlist, i)));
    }
    for (int i = 0; q->orqlist && i < TCLISTNUM(q->orqlist); ++i) {
        _registerallqfields(reg, *((EJQ**) TCLISTVALPTR(q->orqlist, i)));
    }
}

static bool _qrypreprocess(_QRYCTX *ctx) {
    assert(ctx->coll && ctx->q && ctx->q->qflist);
    EJQ *q = ctx->q;

    // Fill the NULL terminated registry of all *EQF fields including all $and $or QF
    assert(!q->allqfields);
    TCLIST *alist = tclistnew2(TCLISTINYNUM);
    _registerallqfields(alist, q);
    TCMALLOC(q->allqfields, sizeof(EJQF*) * (TCLISTNUM(alist) + 1));
    for (int i = 0; i < TCLISTNUM(alist); ++i) {
        EJQF **qfp = TCLISTVALPTR(alist, i);
        q->allqfields[i] = *qfp; //*EJQF
    }
    q->allqfields[TCLISTNUM(alist)] = NULL;
    tclistdel(alist);
    alist = NULL;

    if (ctx->qflags & JBQRYCOUNT) { // Sync the user JBQRYCOUNT flag with internal
        q->flags |= EJQONLYCOUNT;
    }
    EJQF *oqf = NULL; // Order condition
    TCLIST *qflist = q->qflist;
    bson_iterator it;
    bson_type bt;

    if (q->hints) {
        bson_type bt;
        bson_iterator it, sit;
        // Process $orderby
        bt = bson_find(&it, q->hints, "$orderby");
        if (bt == BSON_OBJECT) {
            int orderseq = 1;
            BSON_ITERATOR_SUBITERATOR(&it, &sit);
            while ((bt = bson_iterator_next(&sit)) != BSON_EOO) {
                if (!BSON_IS_NUM_TYPE(bt)) {
                    continue;
                }
                const char *ofield = BSON_ITERATOR_KEY(&sit);
                int odir = bson_iterator_int(&sit);
                odir = (odir > 0) ? 1 : (odir < 0 ? -1 : 0);
                if (!odir) {
                    continue;
                }
                EJQF nqf;
                EJQF *qf = NULL;
                for (int i = 0; i < TCLISTNUM(qflist); ++i) {
                    if (!strcmp(ofield, ((EJQF*) TCLISTVALPTR(qflist, i))->fpath)) {
                        qf = TCLISTVALPTR(qflist, i);
                        assert(qf);
                        break;
                    }
                }
                if (qf == NULL) { // Create syntetic query field for orderby ops
                    memset(&nqf, 0, sizeof (EJQF));
                    nqf.fpath = tcstrdup(ofield);
                    nqf.fpathsz = strlen(nqf.fpath);
                    nqf.expr = tcstrdup("");
                    nqf.exprsz = 0;
                    nqf.tcop = TDBQTRUE; // Disable any TC matching operation
                    nqf.ftype = BSON_OBJECT;
                    nqf.orderseq = orderseq++;
                    nqf.order = odir;
                    nqf.flags |= EJFEXCLUDED; // Field excluded from matching
                    qf = &nqf;
                    TCLISTPUSH(qflist, qf, sizeof (*qf));
                } else {
                    qf->orderseq = orderseq++;
                    qf->order = odir;
                }
            }
        }
        bt = bson_find(&it, q->hints, "$skip");
        if (BSON_IS_NUM_TYPE(bt)) {
            int64_t v = bson_iterator_long(&it);
            q->skip = (uint32_t) ((v < 0) ? 0 : v);
        }
        bt = bson_find(&it, q->hints, "$max");
        if (ctx->qflags & JBQRYFINDONE) {
            q->max = (uint32_t) 1;
        } else if (BSON_IS_NUM_TYPE(bt)) {
            int64_t v = bson_iterator_long(&it);
            q->max = (uint32_t) ((v < 0) ? 0 : v);
        }
        if (!(ctx->qflags & JBQRYCOUNT)) {
            bt = bson_find(&it, q->hints, "$fields"); // Collect required fields
            if (bt == BSON_OBJECT) {
                TCMAP *fmap = tcmapnew2(TCMAPTINYBNUM);
                BSON_ITERATOR_SUBITERATOR(&it, &sit);
                for (int i = 0; (bt = bson_iterator_next(&sit)) != BSON_EOO; ++i) {
                    if (!BSON_IS_NUM_TYPE(bt)) {
                        continue;
                    }
                    bool inc = (bson_iterator_int(&sit) > 0 ? true : false);
                    if (i > 0 && inc != ctx->imode) { 
                        // $fields hint cannot mix include and exclude fields
                        tcmapdel(fmap);
                        _ejdbsetecode(ctx->coll->jb, JBEQINCEXCL, __FILE__, __LINE__, __func__);
                        return false;
                    }
                    ctx->imode = inc;
                    const char *key = BSON_ITERATOR_KEY(&sit);
                    char *pptr;
                    // Checking the $(projection) operator.
                    if (inc && (pptr = strstr(key, ".$")) && 
                        (*(pptr + 2) == '\0' || *(pptr + 2) == '.')) { // '.$' || '.$.'
                        
                        int dc = 0;
                        for (int i = 0; *(key + i) != '\0'; ++i) {
                            if (*(key + i) == '$' && (dc++ > 0)) break;
                        }
                        if (dc != 1) { // More than one '$' chars in projection, it is invalid
                            continue;
                        }
                        for (int i = 0; i < TCLISTNUM(qflist); ++i) {
                            EJQF *qf = TCLISTVALPTR(qflist, i);
                            int j;
                            for (j = 0; *(key + j) != '\0' && *(key + j) == *(qf->fpath + j); ++j);
                            if (key + j == pptr || key + j == pptr + 1) { 
                                // Existing QF matched the $(projection) prefix
                                if (!q->ifields) {
                                    q->ifields = tcmapnew2(TCMAPTINYBNUM);
                                }
                                tcmapput(q->ifields, qf->fpath, qf->fpathsz, key, strlen(key));
                                break;
                            }
                        }
                        continue; // Skip registering this fields in the fmap
                    }
                    tcmapputkeep(fmap, key, strlen(key), &yes, sizeof (yes));
                }
                if (TCMAPRNUM(fmap) == 0) { // if {$fields : {}} we will force {$fields : {_id:1}}
                    ctx->imode = true;
                    tcmapputkeep(fmap, JDBIDKEYNAME, JDBIDKEYNAMEL, &yes, sizeof (yes));
                }
                ctx->ifields = fmap;
            }
        }
    } //eof hints

    const int scoreexact = 100;
    const int scoregtlt = 50;
    int maxiscore = 0; //Maximum index score
    int maxselectivity = 0;

    uint32_t skipflags = (//skip field flags
                             EJFNOINDEX |
                             EJCONDSET |
                             EJCONDINC |
                             EJCONDADDSET |
                             EJCONDPULL |
                             EJCONDUPSERT |
                             EJCONDOIT);

    for (int i = 0; i < TCLISTNUM(qflist); ++i) {
        int iscore = 0;
        EJQF *qf = (EJQF*) TCLISTVALPTR(qflist, i);
        assert(qf && qf->fpath);

        if (qf->flags & EJCONDOIT) { //$do field
            TCMAP *dmap = ctx->dfields;
            if (!dmap) {
                dmap = tcmapnew2(TCMAPTINYBNUM);
                ctx->dfields = dmap;
            }
            tcmapputkeep(dmap, qf->fpath, qf->fpathsz, qf, sizeof (*qf));
        }

        if (qf->flags & skipflags) {
            continue;
        }
        
        // OID PK matching
        if (!qf->negate && 
            (qf->tcop == TDBQCSTREQ || qf->tcop == TDBQCSTROREQ) && 
            !strcmp(JDBIDKEYNAME, qf->fpath)) {
                
            qf->flags |= EJFPKMATCHING;
            ctx->mqf = qf;
            break;
        }

        bool firstorderqf = false;
        qf->idxmeta = _imetaidx(ctx->coll, qf->fpath);
        qf->idx = _qryfindidx(ctx->coll, qf, qf->idxmeta);
        if (qf->order && qf->orderseq == 1) { // Index for first 'orderby' exists
            oqf = qf;
            firstorderqf = true;
        }
        if (!qf->idx || !qf->idxmeta) {
            if (qf->idxmeta) {
                bson_del(qf->idxmeta);
            }
            qf->idx = NULL;
            qf->idxmeta = NULL;
            continue;
        }
        if (qf->tcop == TDBQTRUE || qf->negate) {
            continue;
        }
        int avgreclen = -1;
        int selectivity = -1;
        bt = bson_find(&it, qf->idxmeta, "selectivity");
        if (bt == BSON_DOUBLE) {
            selectivity = (int) ((double) bson_iterator_double(&it) * 100); // Selectivity percent
        }
        bt = bson_find(&it, qf->idxmeta, "avgreclen");
        if (bt == BSON_DOUBLE) {
            avgreclen = (int) bson_iterator_double(&it);
        }
        if (selectivity > 0) {
            if (selectivity <= 20) { // Not using index at all if selectivity lesser than 20%
                continue;
            }
            iscore += selectivity;
        }
        if (firstorderqf) {
            iscore += (maxselectivity - selectivity) / 2;
        }
        if (selectivity > maxselectivity) {
            maxselectivity = selectivity;
        }
        switch (qf->tcop) {
            case TDBQCSTREQ:
            case TDBQCSTROR:
            case TDBQCNUMEQ:
            case TDBQCNUMBT:
                iscore += scoreexact;
                break;
            case TDBQCSTRBW:
            case TDBQCSTREW:
                if (avgreclen > 0 && qf->exprsz > avgreclen) {
                    iscore += scoreexact;
                }
                break;
            case TDBQCNUMGT:
            case TDBQCNUMGE:
            case TDBQCNUMLT:
            case TDBQCNUMLE:
                if (firstorderqf) {
                    iscore += scoreexact;
                } else {
                    iscore += scoregtlt;
                }
                break;
        }
        if (iscore >= maxiscore) {
            ctx->mqf = qf;
            maxiscore = iscore;
        }
    }
    if (ctx->mqf == NULL && (oqf && oqf->idx && !oqf->negate)) {
        ctx->mqf = oqf;
    }

    if (q->flags & EJQHASUQUERY) { 
        // Check update $(query) projection then sync inter-qf refs #91
        
        for (int i = 0; *(q->allqfields + i) != '\0'; ++i) {
            EJQF *qf = q->allqfields[i];
            if (!qf->ufields) continue;
            TCLIST *uflist = qf->ufields;
            for (int j = 0; j < TCLISTNUM(uflist); ++j) {
                const char *ukey = TCLISTVALPTR(uflist, j);
                char *pptr = strstr(ukey, ".$");
                assert(pptr);
                for (int k = 0; *(q->allqfields + k) != '\0'; ++k) {
                    int l;
                    EJQF *kqf = q->allqfields[k];
                    if (kqf == qf || !kqf->fpath) { // Do not process itself
                        continue;
                    }
                    for (l = 0; *(ukey + l) != '\0' && *(ukey + l) == *(kqf->fpath + l); ++l);
                    if (ukey + l == pptr || ukey + l == pptr + 1) { 
                        // Existing QF matched the $(query) prefix
                        if (!kqf->uslots) {
                            kqf->uslots = tclistnew2(TCLISTINYNUM);
                        }
                        USLOT uslot = {
                            .mpos = -1,
                            .dpos = (pptr - ukey),
                            .op = ukey
                        };
                        tclistpush(kqf->uslots, &uslot, sizeof(uslot));
                    }
                }
            }
        }
    }

    // Init query processing buffers
    assert(!q->colbuf && !q->bsbuf);
    q->colbuf = tcxstrnew3(1024);
    q->bsbuf = tcxstrnew3(1024);
    q->tmpbuf = tcxstrnew();
    ctx->didxctx = (q->flags & EJQUPDATING) ? tclistnew() : NULL;
    ctx->res = (q->flags & EJQONLYCOUNT) ? NULL : tclistnew2(4096);
    return true;
}

static bool _metasetopts(EJDB *jb, const char *colname, EJCOLLOPTS *opts) {
    bool rv = true;
    if (!opts) {
        return _metasetbson(jb, colname, strlen(colname), "opts", NULL, false, false);
    }
    bson *bsopts = bson_create();
    bson_init(bsopts);
    bson_append_bool(bsopts, "compressed", opts->compressed);
    bson_append_bool(bsopts, "large", opts->large);
    bson_append_int(bsopts, "cachedrecords", opts->cachedrecords);
    bson_append_int(bsopts, "records", opts->records);
    bson_finish(bsopts);
    rv = _metasetbson(jb, colname, strlen(colname), "opts", bsopts, false, false);
    bson_del(bsopts);
    return rv;
}

static bool _metagetopts(EJDB *jb, const char *colname, EJCOLLOPTS *opts) {
    assert(opts);
    memset(opts, 0, sizeof (*opts));
    bson *bsopts = _metagetbson(jb, colname, strlen(colname), "opts");
    if (!bsopts) {
        return true;
    }
    if (bson_validate(bsopts, true, true) != BSON_OK) {
        _ejdbsetecode(jb, JBEMETANVALID, __FILE__, __LINE__, __func__);
        bson_del(bsopts);
        return false;
    }
    bson_iterator it;
    bson_type bt = bson_find(&it, bsopts, "compressed");
    if (bt == BSON_BOOL) {
        opts->compressed = bson_iterator_bool(&it);
    }
    bt = bson_find(&it, bsopts, "large");
    if (bt == BSON_BOOL) {
        opts->large = bson_iterator_bool(&it);
    }
    bt = bson_find(&it, bsopts, "cachedrecords");
    if (BSON_IS_NUM_TYPE(bt)) {
        opts->cachedrecords = bson_iterator_long(&it);
    }
    bt = bson_find(&it, bsopts, "records");
    if (BSON_IS_NUM_TYPE(bt)) {
        opts->records = bson_iterator_long(&it);
    }
    bson_del(bsopts);
    return true;
}

static bool _metasetbson(EJDB *jb, const char *colname, int colnamesz,
                         const char *mkey, bson *val, bool merge, bool mergeoverwrt) {
    assert(jb && colname && mkey);
    bool rv = true;
    bson *bsave = NULL;
    bson *oldval = NULL;
    bson mresult;

    TCMAP *cmeta = tctdbget(jb->metadb, colname, colnamesz);
    if (!cmeta) {
        _ejdbsetecode(jb, JBEMETANVALID, __FILE__, __LINE__, __func__);
        rv = false;
        goto finish;
    }
    if (!val) {
        if (tcmapout2(cmeta, mkey)) {
            rv = tctdbput(jb->metadb, colname, colnamesz, cmeta);
        }
        goto finish;
    }
    assert(val);
    if (merge) { // Merged
        oldval = _metagetbson(jb, colname, colnamesz, mkey);
        if (oldval) {
            bson_init(&mresult);
            bson_merge(oldval, val, mergeoverwrt, &mresult);
            bson_finish(&mresult);
            bsave = &mresult;
        } else {
            bsave = val;
        }
    } else { // Rewrited
        bsave = val;
    }

    assert(bsave);
    tcmapput(cmeta, mkey, strlen(mkey), bson_data(bsave), bson_size(bsave));
    rv = tctdbput(jb->metadb, colname, colnamesz, cmeta);
finish:
    if (oldval) {
        if (merge) {
            bson_destroy(bsave);
        }
        bson_del(oldval);
    }
    if (cmeta) {
        tcmapdel(cmeta);
    }
    tctdbsync(jb->metadb);
    return rv;
}

static bool _metasetbson2(EJCOLL *coll, const char *mkey, 
                          bson *val, bool merge, bool mergeoverwrt) {
    assert(coll);
    return _metasetbson(coll->jb, coll->cname, coll->cnamesz, mkey, val, merge, mergeoverwrt);
}

/** Returned meta BSON data must be freed by 'bson_del' */
static bson* _metagetbson(EJDB *jb, const char *colname, int colnamesz, const char *mkey) {
    assert(jb && colname && mkey);
    bson *rv = NULL;
    TCMAP *cmeta = tctdbget(jb->metadb, colname, colnamesz);
    if (!cmeta) {
        _ejdbsetecode(jb, JBEMETANVALID, __FILE__, __LINE__, __func__);
        return NULL;
    }
    int bsz;
    const void *raw = tcmapget(cmeta, mkey, strlen(mkey), &bsz);
    if (!raw || bsz == 0) {
        goto finish;
    }
    rv = bson_create();
    bson_init_size(rv, bsz);
    bson_ensure_space(rv, bsz - 4);
    bson_append(rv, ((char*) raw) + 4, bsz - (4 + 1/*BSON_EOO*/));
    bson_finish(rv);
finish:
    tcmapdel(cmeta);
    return rv;
}

static bson* _metagetbson2(EJCOLL *coll, const char *mkey) {
    assert(coll);
    return _metagetbson(coll->jb, coll->cname, coll->cnamesz, mkey);
}

/** Returned index meta if not NULL it must be freed by 'bson_del' */
static bson* _imetaidx(EJCOLL *coll, const char *ipath) {
    assert(coll && ipath);
    if (*ipath == '\0') {
        return NULL;
    }
    bson *rv = NULL;
    char fpathkey[BSON_MAX_FPATH_LEN + 1];
    TCMAP *cmeta = tctdbget(coll->jb->metadb, coll->cname, coll->cnamesz);
    if (!cmeta) {
        _ejdbsetecode(coll->jb, JBEMETANVALID, __FILE__, __LINE__, __func__);
        goto finish;
    }
    
    // 'i' prefix for all columns with index meta
    int klen = snprintf(fpathkey, BSON_MAX_FPATH_LEN + 1, "i%s", ipath); 
    if (klen > BSON_MAX_FPATH_LEN) {
        _ejdbsetecode(coll->jb, JBEFPATHINVALID, __FILE__, __LINE__, __func__);
        goto finish;
    }
    int bsz;
    const void *bsdata = tcmapget(cmeta, fpathkey, klen, &bsz);
    if (bsdata) {
        rv = bson_create();
        bson_init_size(rv, bsz);
        bson_ensure_space(rv, bsz - 4);
        bson_append(rv, ((char*) bsdata) + 4, bsz - (4 + 1));
        bson_finish(rv);
    }
    
finish:
    if (cmeta) {
        tcmapdel(cmeta);
    }
    return rv;
}

/** Free EJQF field **/
static void _delqfdata(const EJQ *q, const EJQF *qf) {
    assert(q && qf);
    if (qf->expr) {
        TCFREE(qf->expr);
    }
    if (qf->fpath) {
        TCFREE(qf->fpath);
    }
    if (qf->idxmeta) {
        bson_del(qf->idxmeta);
    }
    if (qf->updateobj) {
        bson_del(qf->updateobj);
    }
    if (qf->ufields) {
        tclistdel(qf->ufields);
    }
    if (qf->uslots) {
        tclistdel(qf->uslots);
    }
    if (qf->regex && !(EJQINTERNAL & q->flags)) {
        // We do not clear regex_t data because it not deep copy in internal queries
        regfree((regex_t *) qf->regex);
        TCFREE(qf->regex);
    }
    if (qf->exprlist) {
        tclistdel(qf->exprlist);
    }
    if (qf->exprmap) {
        tcmapdel(qf->exprmap);
    }
}

static bool _ejdbsavebsonimpl(EJCOLL *coll, bson *bs, bson_oid_t *oid, bool merge) {
    bool rv = false;
    bson *nbs = NULL;
    bson_type oidt = _bsonoidkey(bs, oid);
    if (oidt == BSON_EOO) { // Missing _id so generate a new _id
        bson_oid_gen(oid);
        nbs = bson_create();
        bson_init_size(nbs, bson_size(bs) + (strlen(JDBIDKEYNAME) + 
                            1/*key*/ + 1/*type*/ + sizeof (*oid)));
        bson_append_oid(nbs, JDBIDKEYNAME, oid);
        bson_ensure_space(nbs, bson_size(bs) - 4);
        bson_append(nbs, bson_data(bs) + 4, bson_size(bs) - (4 + 1/*BSON_EOO*/));
        bson_finish(nbs);
        assert(!nbs->err);
        bs = nbs;
    } else if (oidt != BSON_OID) { // _oid presented by it is not BSON_OID
        _ejdbsetecode(coll->jb, JBEINVALIDBSONPK, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTDB *tdb = coll->tdb;
    TCMAP *rowm = (tdb->hdb->rnum > 0) ? tctdbget(tdb, oid, sizeof (*oid)) : NULL;
    char *obsdata = NULL; // Old bson
    int obsdatasz = 0;
    if (rowm) { // Save the copy of old bson data
        const void *obs = tcmapget(rowm, JDBCOLBSON, JDBCOLBSONL, &obsdatasz);
        if (obs && obsdatasz > 0) {
            TCMALLOC(obsdata, obsdatasz);
            memmove(obsdata, obs, obsdatasz);
        }
    } else {
        rowm = tcmapnew2(TCMAPTINYBNUM);
    }
    if (merge && !nbs && obsdata) {
        nbs = bson_create();
        bson_init_size(nbs, MAX(obsdatasz, bson_size(bs)));
        bson_merge2(obsdata, bson_data(bs), true, nbs);
        bson_finish(nbs);
        assert(!nbs->err);
        bs = nbs;
    }
    tcmapput(rowm, JDBCOLBSON, JDBCOLBSONL, bson_data(bs), bson_size(bs));
    if (!tctdbput(tdb, oid, sizeof (*oid), rowm)) {
        goto finish;
    }
    // Update indexes
    rv = _updatebsonidx(coll, oid, bs, obsdata, obsdatasz, NULL);
finish:
    if (rowm) {
        tcmapdel(rowm);
    }
    if (obsdata) {
        TCFREE(obsdata);
    }
    if (nbs) {
        bson_del(nbs);
    }
    return rv;
}

/**
 * Copy BSON array into new TCLIST. TCLIST must be freed by 'tclistdel'.
 * @param it BSON iterator
 * @param type[out] Detected BSON type of last element
 */
static TCLIST* _fetch_bson_str_array(EJDB *jb, bson_iterator *it, 
                                     bson_type *type, txtflags_t tflags) {
                                         
    TCLIST *res = tclistnew();
    *type = BSON_EOO;
    bson_type ftype;
    for (int i = 0; (ftype = bson_iterator_next(it)) != BSON_EOO; ++i) {
        switch (ftype) {
            case BSON_STRING:
                *type = ftype;
                if (tflags & JBICASE) { //ignore case
                    char *buf = NULL;
                    char sbuf[JBSTRINOPBUFFERSZ];
                    int len = tcicaseformat(bson_iterator_string(it), 
                                            bson_iterator_string_len(it) - 1, 
                                            sbuf, JBSTRINOPBUFFERSZ, &buf);
                    if (len < 0) {
                        _ejdbsetecode(jb, len, __FILE__, __LINE__, __func__);
                        break;
                    }
                    tclistpush2(res, buf);
                    if (buf && buf != sbuf) {
                        TCFREE(buf);
                    }
                } else {
                    tclistpush2(res, bson_iterator_string(it));
                }
                break;
            case BSON_INT:
            case BSON_LONG:
            case BSON_BOOL:
            case BSON_DATE:
                *type = ftype;
                tclistprintf(res, "%" PRId64, bson_iterator_long(it));
                break;
            case BSON_DOUBLE:
                *type = ftype;
                tclistprintf(res, "%f", bson_iterator_double(it));
                break;
            case BSON_OID:
                *type = ftype;
                char xoid[25];
                bson_oid_to_string(bson_iterator_oid(it), xoid);
                tclistprintf(res, "%s", xoid);
                break;
            default:
                break;
        }
    }
    return res;
}

/** Result must be freed by TCFREE */
static char* _fetch_bson_str_array2(EJDB *jb, bson_iterator *it, 
                                    bson_type *type, txtflags_t tflags) {
                                        
    TCLIST *res = _fetch_bson_str_array(jb, it, type, tflags);
    char *tokens = tcstrjoin(res, ',');
    tclistdel(res);
    return tokens;
}

static int _parse_qobj_impl(EJDB *jb, EJQ *q, bson_iterator *it, TCLIST *qlist, 
                            TCLIST *pathStack, EJQF *pqf, int elmatchgrp) {
                                
    assert(it && qlist && pathStack);
    int ret = 0;
    bson_type ftype, bt;

    while ((ftype = bson_iterator_next(it)) != BSON_EOO) {
        const char *fkey = BSON_ITERATOR_KEY(it);
        bool isckey = (*fkey == '$'); // Key is a control key: $in, $nin, $not, $all, ...
        EJQF qf;
        memset(&qf, 0, sizeof (qf));
        qf.q = q;
        if (pqf) {
            qf.elmatchgrp = pqf->elmatchgrp;
            qf.elmatchpos = pqf->elmatchpos;
            qf.flags = pqf->flags;
            if(elmatchgrp > 0)
                qf.negate = pqf->negate;
        }

        if (!isckey) {
            //Push key on top of path stack
            tclistpush2(pathStack, fkey);
            qf.ftype = ftype;
        } else {
            if (!strcmp("$or", fkey) || //Both levels operators.
                    !strcmp("$and", fkey)) {
                //nop
            } else if (!strcmp("$set", fkey) ||
                       !strcmp("$inc", fkey) ||
                       !strcmp("$dropall", fkey) ||
                       !strcmp("$addToSet", fkey) ||
                       !strcmp("$addToSetAll", fkey) ||
                       !strcmp("$push", fkey) ||
                       !strcmp("$pushAll", fkey) ||
                       !strcmp("$pull", fkey) ||
                       !strcmp("$pullAll", fkey) ||
                       !strcmp("$upsert", fkey) ||
                       !strcmp("$do", fkey) ||
                       !strcmp("$unset", fkey) ||
                       !strcmp("$rename", fkey)
                      ) {
                if (pqf) { // Top level ops
                    ret = JBEQERROR;
                    _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                    break;
                }
            } else {
                if (!pqf) { // Require parent query object
                    ret = JBEQERROR;
                    _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                    break;
                }
                qf = *pqf;
                if (!strcmp("$not", fkey)) {
                    qf.negate = !qf.negate;
                } else if (!strcmp("$gt", fkey)) {
                    qf.flags |= EJCOMPGT;
                } else if (!strcmp("$gte", fkey)) {
                    qf.flags |= EJCOMPGTE;
                } else if (!strcmp("$lt", fkey)) {
                    qf.flags |= EJCOMPLT;
                } else if (!strcmp("$lte", fkey)) {
                    qf.flags |= EJCOMPLTE;
                } else if (!strcmp("$begin", fkey)) {
                    qf.flags |= EJCONDSTARTWITH;
                } else if (!strcmp("$icase", fkey)) {
                    qf.flags |= EJCONDICASE;
                }
            }
        }

        switch (ftype) {
            case BSON_ARRAY: {
                if (isckey) {
                    if (!strcmp("$and", fkey)) {
                        bson_iterator sit;
                        BSON_ITERATOR_SUBITERATOR(it, &sit);
                        while ((bt = bson_iterator_next(&sit)) != BSON_EOO) {
                            if (bt != BSON_OBJECT) {
                                continue;
                            }
                            if (_qryaddand(jb, q, bson_iterator_value(&sit)) == NULL) {
                                break;
                            }
                        }
                        break;
                    } else if (!strcmp("$or", fkey)) {
                        bson_iterator sit;
                        BSON_ITERATOR_SUBITERATOR(it, &sit);
                        while ((bt = bson_iterator_next(&sit)) != BSON_EOO) {
                            if (bt != BSON_OBJECT) {
                                continue;
                            }
                            if (ejdbqueryaddor(jb, q, bson_iterator_value(&sit)) == NULL) {
                                break;
                            }
                        }
                        break;
                    } else {
                        bson_iterator sit;
                        BSON_ITERATOR_SUBITERATOR(it, &sit);
                        bson_type atype = 0;
                        TCLIST *tokens = 
                                         _fetch_bson_str_array(jb, &sit, &atype, 
                                                             (qf.flags & EJCONDICASE) ? JBICASE : 0);
                        if (atype == 0) {
                            ret = JBEQINOPNOTARRAY;
                            _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                            tclistdel(tokens);
                            break;
                        }
                        assert(!qf.expr && !qf.fpath);
                        qf.ftype = BSON_ARRAY;
                        qf.expr = tclistdump(tokens, &qf.exprsz);
                        qf.exprlist = tokens;
                        if (!strcmp("$in", fkey) || !strcmp("$nin", fkey)) {
                            if (!strcmp("$nin", fkey)) {
                                qf.negate = true;
                            }
                            if (BSON_IS_NUM_TYPE(atype) || 
                                atype == BSON_BOOL || atype == BSON_DATE) {
                                qf.tcop = TDBQCNUMOREQ;
                            } else {
                                qf.tcop = TDBQCSTROREQ;
                                if (TCLISTNUM(tokens) >= JBINOPTMAPTHRESHOLD) {
                                    assert(!qf.exprmap);
                                    qf.exprmap = tcmapnew2(TCLISTNUM(tokens));
                                    for (int i = 0; i < TCLISTNUM(tokens); ++i) {
                                        tcmapputkeep(qf.exprmap, TCLISTVALPTR(tokens, i), 
                                                     TCLISTVALSIZ(tokens, i), &yes, sizeof (yes));
                                    }
                                }
                            }
                        } else if (!strcmp("$bt", fkey)) { //between
                            qf.tcop = TDBQCNUMBT;
                            if (TCLISTNUM(tokens) != 2) {
                                ret = JBEQINOPNOTARRAY;
                                _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                                TCFREE(qf.expr);
                                tclistdel(qf.exprlist);
                                break;
                            }
                        } else if (!strcmp("$strand", fkey)) { //$strand
                            qf.tcop = TDBQCSTRAND;
                        } else if (!strcmp("$stror", fkey)) { //$stror
                            qf.tcop = TDBQCSTROR;
                        } else if (qf.flags & EJCONDSTARTWITH) { //$begin with some token
                            qf.tcop = TDBQCSTRORBW;
                        } else {
                            ret = JBEQINVALIDQCONTROL;
                            _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                            TCFREE(qf.expr);
                            tclistdel(qf.exprlist);
                            break;
                        }
                        qf.fpath = tcstrjoin(pathStack, '.');
                        qf.fpathsz = strlen(qf.fpath);
                        TCLISTPUSH(qlist, &qf, sizeof (qf));
                        break;
                    }
                } else {
                    bson_iterator sit;
                    BSON_ITERATOR_SUBITERATOR(it, &sit);
                    ret = _parse_qobj_impl(jb, q, &sit, qlist, pathStack, &qf, elmatchgrp);
                    break;
                }
            }

            case BSON_OBJECT: {
                if (isckey) {
                    if (!strcmp("$inc", fkey)) {
                        qf.flags |= EJCONDINC;
                    } else if (!pqf) { //top level op
                        if (!strcmp("$do", fkey)) {
                            qf.flags |= EJCONDOIT;
                        } else if (!strcmp("$set", fkey)) { //top level set OP
                            qf.flags |= EJCONDSET;
                        } else if (!strcmp("$addToSet", fkey)) {
                            qf.flags |= EJCONDADDSET;
                        } else if (!strcmp("$addToSetAll", fkey)) {
                            qf.flags |= EJCONDADDSET;
                            qf.flags |= EJCONDALL;
                        } else if (!strcmp("$push", fkey)) {
                            qf.flags |= EJCONDPUSH;
                        } else if (!strcmp("$pushAll", fkey)) {
                            qf.flags |= EJCONDPUSH;
                            qf.flags |= EJCONDALL;
                        } else if (!strcmp("$pull", fkey)) {
                            qf.flags |= EJCONDPULL;
                        } else if (!strcmp("$pullAll", fkey)) {
                            qf.flags |= EJCONDPULL;
                            qf.flags |= EJCONDALL;
                        } else if (!strcmp("$upsert", fkey)) {
                            qf.flags |= EJCONDSET;
                            qf.flags |= EJCONDUPSERT;
                        } else if (!strcmp("$unset", fkey)) {
                            qf.flags |= EJCONDUNSET;
                        } else if (!strcmp("$rename", fkey)) {
                            qf.flags |= EJCONDRENAME;
                        }
                    }

                    if ((qf.flags & 
                          (EJCONDSET | EJCONDINC | EJCONDADDSET | EJCONDPULL | EJCONDPUSH | 
                           EJCONDUNSET | EJCONDRENAME))) {
                               
                        assert(qf.updateobj == NULL);
                        qf.q->flags |= EJQUPDATING;
                        qf.updateobj = bson_create();
                        bson_init_as_query(qf.updateobj);
                        bson_type sbt;
                        bson_iterator sit;
                        BSON_ITERATOR_SUBITERATOR(it, &sit);
                        while ((sbt = bson_iterator_next(&sit)) != BSON_EOO) {
                            if ((qf.flags & EJCONDALL) && sbt != BSON_ARRAY) {
                                // addToSet, push, pull accept only an arrays
                                continue;
                            }
                            if (!(qf.flags & EJCONDALL) && 
                                 (qf.flags & (EJCONDSET | EJCONDINC | EJCONDUNSET | EJCONDRENAME))) { 
                                // Checking the $(query) positional operator.
                                const char* ukey = BSON_ITERATOR_KEY(&sit);
                                char *pptr;
                                if ((pptr = strstr(ukey, ".$")) && pptr && 
                                    (*(pptr + 2) == '\0' || *(pptr + 2) == '.')) {// '.$' || '.$.'
                                    
                                    int dc = 0;
                                    for (int i = 0; *(ukey + i) != '\0'; ++i) {
                                        if (*(ukey + i) == '$' && (dc++ > 0)) break;
                                    }
                                    if (dc != 1) { 
                                        // More than one '$' chars in projection, it is invalid
                                        continue;
                                    }
                                    // Now just store only [$(query) key] into the qf->ufields
                                    if (!qf.ufields) {
                                        qf.ufields = tclistnew2(TCLISTINYNUM);
                                    }
                                    q->flags |= EJQHASUQUERY;
                                    tclistpush(qf.ufields, ukey, strlen(ukey));
                                }
                            }
                            bson_append_field_from_iterator(&sit, qf.updateobj);
                        }
                        bson_finish(qf.updateobj);
                        if (qf.updateobj->err) {
                            ret = JBEQERROR;
                            _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                            break;
                        }
                        qf.fpath = strdup(fkey);
                        qf.fpathsz = strlen(qf.fpath);
                        qf.tcop = TDBQTRUE;
                        qf.flags |= EJFEXCLUDED;
                        TCLISTPUSH(qlist, &qf, sizeof (qf));
                        break;
                    }

                    if (!strcmp("$elemMatch", fkey)) {
                        if (qf.elmatchgrp) { // Only one $elemMatch allowed in query field
                            ret = JBEQERROR;
                            _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                            break;
                        }
                        qf.elmatchgrp = ++elmatchgrp;
                        char *fpath = tcstrjoin(pathStack, '.');
                        qf.elmatchpos = strlen(fpath) + 1; //+ 1 to skip next dot '.'
                        free(fpath);
                    }
                } else {
                    if (qf.flags & EJCONDOIT) {
                        qf.updateobj = bson_create();
                        bson_init_as_query(qf.updateobj);
                        bson_type sbt;
                        bson_iterator sit;
                        BSON_ITERATOR_SUBITERATOR(it, &sit);
                        int ac = 0;
                        while ((sbt = bson_iterator_next(&sit)) != BSON_EOO) {
                            const char *akey = BSON_ITERATOR_KEY(&sit);
                            if (!strcmp("$join", akey) || !strcmp("$slice", akey)) {
                                bson_append_field_from_iterator(&sit, qf.updateobj);
                                ++ac;
                            }
                        }
                        bson_finish(qf.updateobj);
                        if (qf.updateobj->err) {
                            ret = JBEQERROR;
                            _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                            break;
                        }
                        if (ac == 0) {
                            ret = JBEQACTKEY;
                            _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                            break;
                        }
                        qf.fpath = strdup(fkey);
                        qf.fpathsz = strlen(qf.fpath);
                        qf.tcop = TDBQTRUE;
                        qf.flags |= EJFEXCLUDED;
                        TCLISTPUSH(qlist, &qf, sizeof (qf));
                        break;
                    }
                }
                bson_iterator sit;
                BSON_ITERATOR_SUBITERATOR(it, &sit);
                ret = _parse_qobj_impl(jb, q, &sit, qlist, pathStack, &qf, elmatchgrp);
                break;
            }
            case BSON_OID: {

                assert(!qf.fpath && !qf.expr);
                qf.ftype = ftype;
                TCMALLOC(qf.expr, 25 * sizeof (char));
                bson_oid_to_string(bson_iterator_oid(it), qf.expr);
                qf.exprsz = 24;
                qf.fpath = tcstrjoin(pathStack, '.');
                qf.fpathsz = strlen(qf.fpath);
                qf.tcop = TDBQCSTREQ;
                TCLISTPUSH(qlist, &qf, sizeof (qf));
                break;
            }
            case BSON_STRING: {
                assert(!qf.fpath && !qf.expr);
                qf.ftype = ftype;
                if (qf.flags & EJCONDICASE) {
                    qf.exprsz = tcicaseformat(bson_iterator_string(it), 
                                              bson_iterator_string_len(it) - 1, 
                                              NULL, 0, &qf.expr);
                    if (qf.exprsz < 0) {
                        ret = qf.exprsz;
                        _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                        qf.exprsz = 0;
                        break;
                    }
                } else {
                    qf.expr = tcstrdup(bson_iterator_string(it));
                    qf.exprsz = strlen(qf.expr);
                }

                qf.fpath = tcstrjoin(pathStack, '.');
                qf.fpathsz = strlen(qf.fpath);
                if (qf.flags & EJCONDSTARTWITH) {
                    qf.tcop = TDBQCSTRBW;
                } else {
                    qf.tcop = TDBQCSTREQ;
                    if (!strcmp(JDBIDKEYNAME, fkey)) { //_id
                        qf.ftype = BSON_OID;
                    }
                }
                TCLISTPUSH(qlist, &qf, sizeof (qf));
                break;
            }
            case BSON_LONG:
            case BSON_DOUBLE:
            case BSON_INT:
            case BSON_DATE: {
                assert(!qf.fpath && !qf.expr);
                qf.ftype = ftype;
                qf.fpath = tcstrjoin(pathStack, '.');
                qf.fpathsz = strlen(qf.fpath);
                if (ftype == BSON_LONG || ftype == BSON_INT || ftype == BSON_DATE) {
                    qf.exprlongval = bson_iterator_long(it);
                    qf.exprdblval = qf.exprlongval;
                    // 2015-04-14: Change to use standard format string for int64_t
                    qf.expr = tcsprintf("%" PRId64, qf.exprlongval);
                } else {
                    qf.exprdblval = bson_iterator_double(it);
                    qf.exprlongval = (int64_t) qf.exprdblval;
                    qf.expr = tcsprintf("%f", qf.exprdblval);
                }
                qf.exprsz = strlen(qf.expr);
                if (qf.flags & EJCOMPGT) {
                    qf.tcop = TDBQCNUMGT;
                } else if (qf.flags & EJCOMPGTE) {
                    qf.tcop = TDBQCNUMGE;
                } else if (qf.flags & EJCOMPLT) {
                    qf.tcop = TDBQCNUMLT;
                } else if (qf.flags & EJCOMPLTE) {
                    qf.tcop = TDBQCNUMLE;
                } else {
                    qf.tcop = TDBQCNUMEQ;
                }
                TCLISTPUSH(qlist, &qf, sizeof (qf));
                break;
            }
            case BSON_REGEX: {
                assert(!qf.fpath && !qf.expr);
                qf.ftype = ftype;
                qf.tcop = TDBQCSTRRX;
                char *re = tcstrdup(bson_iterator_regex(it));
                const char *opts = bson_iterator_regex_opts(it);
                qf.fpath = tcstrjoin(pathStack, '.');
                qf.fpathsz = strlen(qf.fpath);
                qf.expr = re;
                qf.exprsz = strlen(qf.expr);
                const char *rxstr = qf.expr;
                regex_t rxbuf;
                int rxopt = REG_EXTENDED | REG_NOSUB;
                if (strchr(opts, 'i')) {
                    rxopt |= REG_ICASE;
                }
                if (regcomp(&rxbuf, rxstr, rxopt) == 0) {
                    TCMALLOC(qf.regex, sizeof (rxbuf));
                    memcpy(qf.regex, &rxbuf, sizeof (rxbuf));
                } else {
                    ret = JBEQINVALIDQRX;
                    _ejdbsetecode(jb, ret, __FILE__, __LINE__, __func__);
                    TCFREE(qf.fpath);
                    TCFREE(qf.expr);
                    break;
                }
                TCLISTPUSH(qlist, &qf, sizeof (qf));
                break;
            }
            case BSON_NULL:
            case BSON_UNDEFINED:
                qf.ftype = ftype;
                qf.tcop = TDBQCEXIST;
                qf.negate = !qf.negate;
                qf.expr = tcstrdup(""); //Empty string as expr
                qf.exprsz = 0;
                qf.fpath = tcstrjoin(pathStack, '.');
                qf.fpathsz = strlen(qf.fpath);
                TCLISTPUSH(qlist, &qf, sizeof (qf));
                break;

            case BSON_BOOL: { //boolean converted into number
                bool bv = bson_iterator_bool_raw(it);
                if (isckey) {
                    if (!strcmp("$dropall", fkey) && bv) {
                        qf.flags |= EJFEXCLUDED;
                        qf.fpath = tcstrjoin(pathStack, '.');
                        qf.fpathsz = strlen(qf.fpath);
                        qf.tcop = TDBQTRUE;
                        qf.q->flags |= EJQUPDATING;
                        qf.q->flags |= EJQDROPALL;
                        qf.expr = tcstrdup(""); //Empty string as expr
                        qf.exprsz = 0;
                        TCLISTPUSH(qlist, &qf, sizeof (qf));
                        break;
                    }
                    if (!strcmp("$exists", fkey)) {
                        qf.tcop = TDBQCEXIST;
                        qf.fpath = tcstrjoin(pathStack, '.');
                        qf.fpathsz = strlen(qf.fpath);
                        qf.expr = tcstrdup(""); //Empty string as expr
                        qf.exprsz = 0;
                        if (!bv) {
                            qf.negate = !qf.negate;
                        }
                        TCLISTPUSH(qlist, &qf, sizeof (qf));
                        break;
                    }
                }
                qf.tcop = TDBQCNUMEQ;
                qf.fpath = tcstrjoin(pathStack, '.');
                qf.fpathsz = strlen(qf.fpath);
                qf.exprlongval = (bv ? 1 : 0);
                qf.exprdblval = qf.exprlongval;
                qf.expr = strdup(bv ? "1" : "0");
                qf.exprsz = 1;
                TCLISTPUSH(qlist, &qf, sizeof (qf));
                break;
            }
            default:
                break;
        };

        if (!isckey) {
            assert(pathStack->num > 0);
            TCFREE(tclistpop2(pathStack));
        }
        if (ret) { //cleanup on error condition
            break;
        }
    }
    return ret;
}

/**
 * Convert bson query spec into field path -> EJQF instance.
 *  Created map instance must be freed by `tcmapdel`.
 *  Each element of map must be freed by `ejdbquerydel`.
 */
static TCLIST* _parseqobj(EJDB *jb, EJQ *q, bson *qspec) {
    assert(qspec);
    return _parseqobj2(jb, q, bson_data(qspec));
}

static TCLIST* _parseqobj2(EJDB *jb, EJQ *q, const void *qspecbsdata) {
    assert(qspecbsdata);
    int rv = 0;
    TCLIST *res = tclistnew2(TCLISTINYNUM);
    TCLIST *pathStack = tclistnew2(TCLISTINYNUM);
    bson_iterator it;
    BSON_ITERATOR_FROM_BUFFER(&it, qspecbsdata);
    rv = _parse_qobj_impl(jb, q, &it, res, pathStack, NULL, 0);
    if (rv) {
        tclistdel(res);
        res = NULL;
    }
    assert(!pathStack->num);
    tclistdel(pathStack);
    return res;
}

/**
 * Get OID value from the '_id' field of specified bson object.
 * @param bson[in] BSON object
 * @param oid[out] Pointer to OID type
 * @return True if OID value is found int _id field of bson object otherwise False.
 */
static bson_type _bsonoidkey(bson *bs, bson_oid_t *oid) {
    bson_iterator it;
    bson_type bt = bson_find(&it, bs, JDBIDKEYNAME);
    if (bt == BSON_OID) {
        *oid = *bson_iterator_oid(&it);
    }
    return bt;
}

/**
 * Return string value representation of value pointed by 'it'.
 * Resulting value size stored into 'vsz'.
 * If returned value is not NULL it must be freed by TCFREE.
 */
static char* _bsonitstrval(EJDB *jb, bson_iterator *it, int *vsz, 
                           TCLIST *tokens, txtflags_t tflags) {
                               
    int retlen = 0;
    char *ret = NULL;
    bson_type btype = BSON_ITERATOR_TYPE(it);
    if (btype == BSON_STRING) {
        if (tokens) { // Split string into tokens and push it into 'tokens' list
            const unsigned char *sp = (unsigned char *) bson_iterator_string(it);
            while (*sp != '\0') {
                while ((*sp != '\0' && *sp <= ' ') || *sp == ',') {
                    sp++;
                }
                const unsigned char *ep = sp;
                while (*ep > ' ' && *ep != ',') {
                    ep++;
                }
                if (ep > sp) {
                    if (tflags & JBICASE) { // Ignore case mode
                        char *buf = NULL;
                        char sbuf[JBSTRINOPBUFFERSZ];
                        int len = tcicaseformat((const char*) sp, ep - sp, 
                                                sbuf, JBSTRINOPBUFFERSZ, &buf);
                        if (len >= 0) { // Success
                            TCLISTPUSH(tokens, buf, len);
                        } else {
                            _ejdbsetecode(jb, len, __FILE__, __LINE__, __func__);
                        }
                        if (buf && buf != sbuf) {
                            TCFREE(buf);
                        }
                    } else {
                        TCLISTPUSH(tokens, sp, ep - sp);
                    }
                }
                sp = ep;
            }
        } else {
            retlen = bson_iterator_string_len(it) - 1;
            if (tflags & JBICASE) {
                retlen = tcicaseformat(bson_iterator_string(it), retlen, NULL, 0, &ret);
            } else {
                ret = tcmemdup(bson_iterator_string(it), retlen);
            }
        }
    } else if (BSON_IS_NUM_TYPE(btype) || btype == BSON_BOOL || btype == BSON_DATE) {
        char nbuff[TCNUMBUFSIZ];
        if (btype == BSON_INT || btype == BSON_LONG || btype == BSON_BOOL || btype == BSON_DATE) {
            retlen = bson_numstrn(nbuff, TCNUMBUFSIZ, bson_iterator_long(it));
            if (retlen >= TCNUMBUFSIZ) {
                retlen = TCNUMBUFSIZ - 1;
            }
        } else if (btype == BSON_DOUBLE) {
            retlen = tcftoa(bson_iterator_double(it), nbuff, TCNUMBUFSIZ, 6);
            if (retlen >= TCNUMBUFSIZ) {
                retlen = TCNUMBUFSIZ - 1;
            }
        }
        if (tflags & JBICASE) {
            retlen = tcicaseformat(nbuff, retlen, NULL, 0, &ret);
        } else {
            ret = tcmemdup(nbuff, retlen);
        }
    } else if (btype == BSON_ARRAY) {
        bson_type eltype; // Last element bson type
        bson_iterator sit;
        BSON_ITERATOR_SUBITERATOR(it, &sit);
        if (tokens) {
            while ((eltype = bson_iterator_next(&sit)) != BSON_EOO) {
                int vz = 0;
                char *v = _bsonitstrval(jb, &sit, &vz, NULL, tflags);
                if (v) {
                    TCLISTPUSH(tokens, v, vz);
                    TCFREE(v);
                }
            }
        } else {
            // Array elements are joined with ',' delimeter.
            ret = _fetch_bson_str_array2(jb, &sit, &eltype, tflags);
            retlen = strlen(ret);
        }
    }
    if (retlen < 0) {
        _ejdbsetecode(jb, retlen, __FILE__, __LINE__, __func__);
        ret = NULL;
        retlen = 0;
    }
    *vsz = retlen;
    return ret;
}

static char* _bsonipathrowldr(
    TCLIST *tokens,
    const char *pkbuf, int pksz,
    const char *rowdata, int rowdatasz,
    const char *ipath, int ipathsz, void *op, int *vsz) {
    assert(op);
    char *res = NULL;
    if (ipath && *ipath == '\0') { // PK TODO review
        if (tokens) {
            const unsigned char *sp = (unsigned char *) pkbuf;
            while (*sp != '\0') {
                while ((*sp != '\0' && *sp <= ' ') || *sp == ',') {
                    sp++;
                }
                const unsigned char *ep = sp;
                while (*ep > ' ' && *ep != ',') {
                    ep++;
                }
                if (ep > sp) TCLISTPUSH(tokens, sp, ep - sp);
                sp = ep;
            }
            *vsz = 0;
            return NULL;
        } else {
            TCMEMDUP(res, pkbuf, pksz);
            *vsz = pksz;
            return res;
        }
    }
    if (!ipath || ipathsz < 2 || *(ipath + 1) == '\0' || strchr("snai", *ipath) == NULL) {
        return NULL;
    }
    // Skip index type prefix char with (fpath + 1)
    res = _bsonfpathrowldr(tokens, rowdata, rowdatasz, ipath + 1, ipathsz - 1, op, vsz);
    if (*vsz == 0) { // Do not allow empty strings for index opration
        if (res) TCFREE(res);
        res = NULL;
    }
    return res;
}

static char* _bsonfpathrowldr(TCLIST *tokens, const char *rowdata, int rowdatasz,
                              const char *fpath, int fpathsz, void *op, int *vsz) {
    _BSONIPATHROWLDR *odata = (_BSONIPATHROWLDR*) op;
    assert(odata && odata->coll);
    char *ret = NULL;
    int bsize;
    bson_iterator it;
    char *bsdata = tcmaploadone(rowdata, rowdatasz, JDBCOLBSON, JDBCOLBSONL, &bsize);
    if (!bsdata) {
        *vsz = 0;
        return NULL;
    }
    BSON_ITERATOR_FROM_BUFFER(&it, bsdata);
    bson_find_fieldpath_value2(fpath, fpathsz, &it);
    ret = _bsonitstrval(odata->coll->jb, &it, vsz, tokens, (odata->icase ? JBICASE : 0));
    TCFREE(bsdata);
    return ret;
}

static bool _updatebsonidx(EJCOLL *coll, const bson_oid_t *oid, const bson *bs,
                           const void *obsdata, int obsdatasz, TCLIST *dlist) {
    bool rv = true;
    TCMAP *cmeta = tctdbget(coll->jb->metadb, coll->cname, coll->cnamesz);
    if (!cmeta) {
        _ejdbsetecode(coll->jb, JBEMETANVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    TCMAP *imap = NULL; // New index map
    TCMAP *rimap = NULL; // Remove index map
    bson_type mt = BSON_EOO;
    bson_type ft = BSON_EOO;
    bson_type oft = BSON_EOO;
    bson_iterator fit, oit, mit;
    int bsz;
    char ikey[BSON_MAX_FPATH_LEN + 2];
    const char *mkey;
    int mkeysz;

    tcmapiterinit(cmeta);
    while ((mkey = tcmapiternext(cmeta, &mkeysz)) != NULL && mkeysz > 0) {
        if (*mkey != 'i' || mkeysz > BSON_MAX_FPATH_LEN + 1) {
            continue;
        }
        const void *mraw = tcmapget(cmeta, mkey, mkeysz, &bsz);
        if (!mraw || !bsz || (mt = bson_find_from_buffer(&mit, mraw, "iflags")) != BSON_INT) {
            continue;
        }
        int iflags = bson_iterator_int(&mit);
        //OK then process index keys
        memcpy(ikey + 1, mkey + 1, mkeysz - 1);
        ikey[mkeysz] = '\0';

        int fvaluesz = 0;
        char *fvalue = NULL;
        int ofvaluesz = 0;
        char *ofvalue = NULL;
        txtflags_t textflags = (iflags & JBIDXISTR) ? JBICASE : 0;

        if (obsdata && obsdatasz > 0) {
            BSON_ITERATOR_FROM_BUFFER(&oit, obsdata);
            oft = bson_find_fieldpath_value2(mkey + 1, mkeysz - 1, &oit);
            TCLIST *tokens = (oft == BSON_ARRAY || (oft == BSON_STRING && (iflags & JBIDXARR))) ? 
                             tclistnew() : NULL;
                             
            ofvalue = BSON_IS_IDXSUPPORTED_TYPE(oft) ? 
                      _bsonitstrval(coll->jb, &oit, &ofvaluesz, tokens, textflags) : NULL;
                      
            if (tokens) {
                oft = BSON_ARRAY;
                ofvalue = tclistdump(tokens, &ofvaluesz);
                tclistdel(tokens);
            }
        }
        if (bs) {
            BSON_ITERATOR_INIT(&fit, bs);
            ft = bson_find_fieldpath_value2(mkey + 1, mkeysz - 1, &fit);
            TCLIST *tokens = (ft == BSON_ARRAY || (ft == BSON_STRING && (iflags & JBIDXARR))) ? 
                              tclistnew() : NULL;
            fvalue = BSON_IS_IDXSUPPORTED_TYPE(ft) ? 
                     _bsonitstrval(coll->jb, &fit, &fvaluesz, tokens, textflags) : NULL;
            if (tokens) {
                ft = BSON_ARRAY;
                fvalue = tclistdump(tokens, &fvaluesz);
                tclistdel(tokens);
            }
        }
        if (!fvalue && !ofvalue) {
            continue;
        }
        if (imap == NULL) {
            imap = tcmapnew2(TCMAPTINYBNUM);
            rimap = tcmapnew2(TCMAPTINYBNUM);
        }
        for (int i = 4; i <= 7; ++i) { /* JBIDXNUM, JBIDXSTR, JBIDXARR, JBIDXISTR */
            bool rm = false;
            int itype = (1 << i);
            if (itype == JBIDXNUM && (JBIDXNUM & iflags)) {
                ikey[0] = 'n';
            } else if (itype == JBIDXSTR && (JBIDXSTR & iflags)) {
                ikey[0] = 's';
            } else if (itype == JBIDXISTR && (JBIDXISTR & iflags)) {
                ikey[0] = 'i';
            } else if (itype == JBIDXARR && (JBIDXARR & iflags)) {
                ikey[0] = 'a';
                if (ofvalue && oft == BSON_ARRAY &&
                        (!fvalue || ft != oft || fvaluesz != ofvaluesz || 
                         memcmp(fvalue, ofvalue, fvaluesz))) {
                             
                    tcmapput(rimap, ikey, mkeysz, ofvalue, ofvaluesz);
                    rm = true;
                }
                if (fvalue && fvaluesz > 0 && ft == BSON_ARRAY && (!ofvalue || rm)) {
                    tcmapput(imap, ikey, mkeysz, fvalue, fvaluesz);
                }
                continue;
            } else {
                continue;
            }
            if (ofvalue && oft != BSON_ARRAY &&
                    (!fvalue || ft != oft || fvaluesz != ofvaluesz || 
                     memcmp(fvalue, ofvalue, fvaluesz))) {
                         
                tcmapput(rimap, ikey, mkeysz, ofvalue, ofvaluesz);
                rm = true;
            }
            if (fvalue && fvaluesz > 0 && ft != BSON_ARRAY && (!ofvalue || rm)) {
                tcmapput(imap, ikey, mkeysz, fvalue, fvaluesz);
            }
        }
        if (fvalue) TCFREE(fvalue);
        if (ofvalue) TCFREE(ofvalue);
    }
    tcmapdel(cmeta);

    if (dlist) { // Storage for deffered index ops provided, save changes into
        _DEFFEREDIDXCTX dctx;
        dctx.oid = *oid;
        dctx.rmap = (rimap && TCMAPRNUM(rimap) > 0) ? tcmapdup(rimap) : NULL;
        dctx.imap = (imap && TCMAPRNUM(imap) > 0) ? tcmapdup(imap) : NULL;
        if (dctx.imap || dctx.rmap) {
            TCLISTPUSH(dlist, &dctx, sizeof (dctx));
        }
        // Flush deffered indexes if number pending objects greater JBMAXDEFFEREDIDXNUM
        if (TCLISTNUM(dlist) >= JBMAXDEFFEREDIDXNUM) {
            for (int i = 0; i < TCLISTNUM(dlist); ++i) {
                _DEFFEREDIDXCTX *di = TCLISTVALPTR(dlist, i);
                assert(di);
                if (di->rmap) {
                    tctdbidxout2(coll->tdb, &(di->oid), sizeof (di->oid), di->rmap);
                    tcmapdel(di->rmap);
                }
                if (di->imap) {
                    tctdbidxput2(coll->tdb, &(di->oid), sizeof (di->oid), di->imap);
                    tcmapdel(di->imap);
                }
            }
            TCLISTTRUNC(dlist, 0);
        }
    } else { //apply index changes immediately
        if (rimap && !tctdbidxout2(coll->tdb, oid, sizeof (*oid), rimap)) rv = false;
        if (imap && !tctdbidxput2(coll->tdb, oid, sizeof (*oid), imap)) rv = false;
    }
    if (imap) tcmapdel(imap);
    if (rimap) tcmapdel(rimap);
    return rv;
}

static void _delcoldb(EJCOLL *coll) {
    assert(coll);
    tctdbdel(coll->tdb);
    coll->tdb = NULL;
    coll->jb = NULL;
    coll->cnamesz = 0;
    TCFREE(coll->cname);
    if (coll->mmtx) {
        pthread_rwlock_destroy(coll->mmtx);
        TCFREE(coll->mmtx);
    }
}

static bool _addcoldb0(const char *cname, EJDB *jb, EJCOLLOPTS *opts, EJCOLL **res) {
    int i;
    TCTDB *cdb;
    
    for (i = 0; i < EJDB_MAX_COLLECTIONS && jb->cdbs[i]; ++i);
    if (i == EJDB_MAX_COLLECTIONS) {
        _ejdbsetecode(jb, JBEMAXNUMCOLS, __FILE__, __LINE__, __func__);
        return false;
    }
    if (!_createcoldb(cname, jb, opts, &cdb)) {
        return false;
    }
    EJCOLL *coll;
    TCCALLOC(coll, 1, sizeof (*coll));
    jb->cdbs[i] = coll;
    ++jb->cdbsnum;
    coll->cname = tcstrdup(cname);
    coll->cnamesz = strlen(cname);
    coll->tdb = cdb;
    coll->jb = jb;
    coll->mmtx = NULL;
    if (!_ejdbcolsetmutex(coll)) {
        return false;
    }
    *res = coll;
    return true;
}

static bool _createcoldb(const char *colname, EJDB *jb, EJCOLLOPTS *opts, TCTDB **res) {
    assert(jb && jb->metadb);
    if (!JBISVALCOLNAME(colname)) {
        _ejdbsetecode(jb, JBEINVALIDCOLNAME, __FILE__, __LINE__, __func__);
        *res = NULL;
        return false;
    }
    bool rv = true;
    TCTDB *cdb = tctdbnew();
    tctdbsetmutex(cdb);
    if (opts) {
        if (opts->cachedrecords > 0) {
            tctdbsetcache(cdb, opts->cachedrecords, 0, 0);
        }
        int bnum = 0;
        uint8_t tflags = 0;
        if (opts->records > 0) {
            bnum = tclmax(opts->records * 2 + 1, TDBDEFBNUM);
        }
        if (opts->large) {
            tflags |= TDBTLARGE;
        }
        if (opts->compressed) {
            tflags |= TDBTDEFLATE;
        }
        tctdbtune(cdb, bnum, 0, 0, tflags);
    }
    const char *mdbpath = jb->metadb->hdb->path;
    assert(mdbpath);
    TCXSTR *cxpath = tcxstrnew2(mdbpath);
    tcxstrcat2(cxpath, "_");
    tcxstrcat2(cxpath, colname);
    uint32_t mode = jb->metadb->hdb->omode;
    if (mode & (JBOWRITER | JBOCREAT)) {
        mode |= JBOCREAT;
    }
    rv = tctdbopen(cdb, tcxstrptr(cxpath), mode);
    if (!rv) {
        tctdbdel(cdb);
        *res = NULL;
    } else {
        *res = cdb;
    }
    tcxstrdel(cxpath);
    return rv;
}

/* Check whether a string includes all tokens in another string.*/
static bool _qrycondcheckstrand(const char *vbuf, const TCLIST *tokens) {
    assert(vbuf && tokens);
    for (int i = 0; i < TCLISTNUM(tokens); ++i) {
        const char *token = TCLISTVALPTR(tokens, i);
        int tokensz = TCLISTVALSIZ(tokens, i);
        bool found = false;
        const char *str = vbuf;
        while (true) {
            const char *sp = str;
            while (*str != '\0' && !strchr(", ", *str)) {
                str++;
            }
            if (tokensz == (str - sp) && !strncmp(token, sp, tokensz)) { //Token matched
                found = true;
                break;
            }
            if (*str == '\0') break;
            str++;
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

/* Check whether a string includes at least one tokens in another string.*/
static bool _qrycondcheckstror(const char *vbuf, const TCLIST *tokens) {
    for (int i = 0; i < TCLISTNUM(tokens); ++i) {
        const char *token = TCLISTVALPTR(tokens, i);
        int tokensz = TCLISTVALSIZ(tokens, i);
        bool found = false;
        const char *str = vbuf;
        while (true) {
            const char *sp = str;
            while (*str != '\0' && !strchr(", ", *str)) {
                str++;
            }
            if (tokensz == (str - sp) && !strncmp(token, sp, tokensz)) { //Token matched
                found = true;
                break;
            }
            if (*str == '\0') break;
            str++;
        }
        if (found) {
            return true;
        }
    }
    return false;
}
