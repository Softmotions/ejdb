/*************************************************************************************************
 * The hash database API of Tokyo Cabinet
 *                                                               Copyright (C) 2006-2012 FAL Labs
 *                                                               Copyright (C) 2012-2015 Softmotions Ltd <info@softmotions.com>
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
#include "myconf.h"

#define HDBFILEMODE    00644             // permission of created files
#define HDBIOBUFSIZ    16384             // size of an I/O buffer

#define HDBMAGICDATA   "ToKyO CaBiNeT"   // magic data for identification
#define HDBHEADSIZ     256               // size of the region of the header
#define HDBTYPEOFF     32                // offset of the region for the database type
#define HDBFLAGSOFF    33                // offset of the region for the additional flags
#define HDBAPOWOFF     34                // offset of the region for the alignment power
#define HDBFPOWOFF     35                // offset of the region for the free block pool power
#define HDBOPTSOFF     36                // offset of the region for the options
#define HDBBNUMOFF     40                // offset of the region for the bucket number
#define HDBRNUMOFF     48                // offset of the region for the record number
#define HDBFSIZOFF     56                // offset of the region for the file size
#define HDBFRECOFF     64                // offset of the region for the first record offset
#define HDBOPAQUEOFF   128               // offset of the region for the opaque field
#define HDBOPAQUESZ    HDBHEADSIZ - HDBOPAQUEOFF //opaque data size
#define HDBB64(TC_hdb)  ((uint64_t *) ((TC_hdb)->map + HDBHEADSIZ))
#define HDBB32(TC_hdb)  ((uint32_t *) ((TC_hdb)->map + HDBHEADSIZ))

#define HDBDEFBNUM     131071            // default number of buckets
#define HDBDEFAPOW     4                 // default alignment power
#define HDBMAXAPOW     16                // maximum alignment power
#define HDBDEFFPOW     10                // default free block pool power
#define HDBMAXFPOW     20                // maximum free block pool power

//#ifdef _WIN32
//#define HDBDEFXMSIZ    _maxof(off_t)     // default size of the extra mapped memory
//#else
#define HDBDEFXMSIZ    (64LL<<20)        // default size of the extra mapped memory
//#endif
#define HDBXFSIZINC    1048576           // 1MB increment of extra file size
#define HDBMINRUNIT    48                // minimum record reading unit
#define HDBMAXHSIZ     32                // maximum record header size
#define HDBFBPALWRAT   2                 // allowance ratio of the free block pool
#define HDBFBPBSIZ     64                // base region size of the free block pool
#define HDBFBPESIZ     4                 // size of each region of the free block pool
#define HDBFBPMGFREQ   4096              // frequency to merge the free block pool
#define HDBDRPUNIT     65536             // unit size of the delayed record pool
#define HDBDRPLAT      2048              // latitude size of the delayed record pool
#define HDBDFRSRAT     2                 // step ratio of auto defragmentation
#define HDBFBMAXSIZ    (INT32_MAX/4)     // maximum size of a free block pool
#define HDBCACHEOUT    128               // number of records in a process of cacheout
#define HDBWALSUFFIX   "wal"             // suffix of write ahead logging file

typedef struct { // type of structure for a record
    uint64_t off; // offset of the record
    uint32_t rsiz; // size of the whole record
    uint8_t magic; // magic number
    uint8_t hash; // second hash value
    uint64_t left; // offset of the left child record
    uint64_t right; // offset of the right child record
    uint32_t ksiz; // size of the key
    uint32_t vsiz; // size of the value
    uint16_t psiz; // size of the padding
    const char *kbuf; // pointer to the key
    const char *vbuf; // pointer to the value
    uint64_t boff; // offset of the body
    char *bbuf; // buffer of the body
} TCHREC;

typedef struct { // type of structure for a free block
    uint64_t off; // offset of the block
    uint32_t rsiz; // size of the block
} HDBFB;

enum { // enumeration for magic data
    HDBMAGICREC = 0xc8, // for data block
    HDBMAGICFB = 0xb0 // for free block
};

enum { // enumeration for duplication behavior
    HDBPDOVER, // overwrite an existing value
    HDBPDKEEP, // keep the existing value
    HDBPDCAT, // concatenate values
    HDBPDADDINT, // add an integer
    HDBPDADDDBL, // add a real number
    HDBPDPROC // process by a callback function
};

typedef struct { // type of structure for a duplication callback
    TCPDPROC proc; // function pointer
    void *op; // opaque pointer
} HDBPDPROCOP;

enum {
    HDBWRITENOWALL = 1, //do not write into wall in tchdbseekwrite
    HDBOPTNOSMLOCK = 1 << 1, //do not use shared smem rw lock
    HDBTRALLOWSHRINK = 1 << 2, //resize file on truncation
    HDBSEEKTRY = 1 << 3, //perform strict seek try
    HDBOPTLOCKDB = 1 << 4
};

/* private macros */
#define HDBLOCKMETHOD(TC_hdb, TC_wr)                            \
  ((TC_hdb)->mmtx ? tchdblockmethod((TC_hdb), (TC_wr)) : true)
#define HDBUNLOCKMETHOD(TC_hdb)                         \
  ((TC_hdb)->mmtx ? tchdbunlockmethod(TC_hdb) : true)
#define HDBLOCKRECORD(TC_hdb, TC_bidx, TC_wr)                           \
  ((TC_hdb)->mmtx ? tchdblockrecord((TC_hdb), (uint8_t)(TC_bidx), (TC_wr)) : true)
#define HDBUNLOCKRECORD(TC_hdb, TC_bidx)                                \
  ((TC_hdb)->mmtx ? tchdbunlockrecord((TC_hdb), (uint8_t)(TC_bidx)) : true)
#define HDBLOCKALLRECORDS(TC_hdb, TC_wr)                                \
  ((TC_hdb)->mmtx ? tchdblockallrecords((TC_hdb), (TC_wr)) : true)
#define HDBUNLOCKALLRECORDS(TC_hdb)                             \
  ((TC_hdb)->mmtx ? tchdbunlockallrecords(TC_hdb) : true)
#define HDBLOCKDB(TC_hdb)                       \
  ((TC_hdb)->mmtx ? tchdblockdb(TC_hdb) : true)
#define HDBUNLOCKDB(TC_hdb)                             \
  ((TC_hdb)->mmtx ? tchdbunlockdb(TC_hdb) : true)

#ifdef _WIN32
#define HDBLOCKSMEMPTR(TC_hdb, TC_wr)                       \
  ((TC_hdb)->smtx ? tchdblocksmem((TC_hdb), (TC_wr)) : ((TC_hdb)->map != NULL))
#define HDBLOCKSMEMPTR2(TC_hdb, TC_wr)                       \
  ((TC_hdb)->smtx ? tchdblocksmem2((TC_hdb), (TC_wr)) : true)
#define HDBUNLOCKSMEMPTR(TC_hdb)                             \
  ((TC_hdb)->smtx ? tchdbunlocksmem(TC_hdb) : true)
#else
#define HDBLOCKSMEMPTR(TC_hdb, TC_wr) (true)
#define HDBLOCKSMEMPTR2(TC_hdb, TC_wr) (true)
#define HDBUNLOCKSMEMPTR(TC_hdb) while(false)
#endif

#define HDBLOCKWAL(TC_hdb)                              \
  ((TC_hdb)->mmtx ? tchdblockwal(TC_hdb) : true)
#define HDBUNLOCKWAL(TC_hdb)                            \
  ((TC_hdb)->mmtx ? tchdbunlockwal(TC_hdb) : true)
#define HDBTHREADYIELD(TC_hdb)                          \
  do { if((TC_hdb)->mmtx) sched_yield(); } while(false)

/* private function prototypes */
static uint64_t tcgetprime(uint64_t num);
static bool tchdbseekwrite(TCHDB *hdb, off_t off, const void *buf, size_t size);
static bool tchdbseekwrite2(TCHDB *hdb, off_t off, const void *buf, size_t size, int opts);
static bool tchdbseekread(TCHDB *hdb, off_t off, void *buf, size_t size);
static bool tchdbseekread2(TCHDB *hdb, off_t off, void *buf, size_t size, int opts);
static bool tchdbdumpmeta(TCHDB *hdb, char *hbuf);
static bool tchdbloadmeta(TCHDB *hdb, const char *hbuf);
static void tchdbclear(TCHDB *hdb);
static int32_t tchdbpadsize(TCHDB *hdb, uint64_t off);
static bool tchdbsetflag(TCHDB *hdb, int flag, bool sign);
static uint64_t tchdbbidx(TCHDB *hdb, const char *kbuf, int ksiz, uint8_t *hp);
static off_t tchdbgetbucket(TCHDB *hdb, uint64_t bidx);
static void tchdbsetbucket(TCHDB *hdb, uint64_t bidx, uint64_t off);
static bool tchdbsavefbp(TCHDB *hdb);
static bool tchdbloadfbp(TCHDB *hdb);
static void tcfbpsortbyoff(HDBFB *fbpool, int fbpnum);
static void tcfbpsortbyrsiz(HDBFB *fbpool, int fbpnum);
static void tchdbfbpmerge(TCHDB *hdb);
static void tchdbfbpinsert(TCHDB *hdb, uint64_t off, uint32_t rsiz);
static bool tchdbfbpsearch(TCHDB *hdb, TCHREC *rec);
static bool tchdbfbpsplice(TCHDB *hdb, TCHREC *rec, uint32_t nsiz);
static void tchdbfbptrim(TCHDB *hdb, uint64_t base, uint64_t next, uint64_t off, uint32_t rsiz);
static bool tchdbwritefb(TCHDB *hdb, uint64_t off, uint32_t rsiz);
static bool tchdbwriterec(TCHDB *hdb, TCHREC *rec, uint64_t bidx, off_t entoff, bool newrec);
static bool tchdbreadrec(TCHDB *hdb, TCHREC *rec, char *rbuf);
static bool tchdbreadrecbody(TCHDB *hdb, TCHREC *rec);
static bool tchdbremoverec(TCHDB *hdb, TCHREC *rec, char *rbuf, uint64_t bidx, off_t entoff);
static bool tchdbshiftrec(TCHDB *hdb, TCHREC *rec, char *rbuf, off_t destoff);
static int tcreckeycmp(const char *abuf, int asiz, const char *bbuf, int bsiz);
static bool tchdbflushdrp(TCHDB *hdb);
static void tchdbcacheadjust(TCHDB *hdb);
static bool tchdbwalinit(TCHDB *hdb);
static bool tchdbwalwrite(TCHDB *hdb, uint64_t off, int64_t size);
static bool tchdbwalrestore(TCHDB *hdb, const char *path);
static bool tchdbwalremove(TCHDB *hdb, const char *path);
static bool tchdbopenimpl(TCHDB *hdb, const char *path, int omode);
static bool tchdbcloseimpl(TCHDB *hdb);
static bool tchdbputimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash,
        const char *vbuf, int vsiz, int dmode);
static void tchdbdrpappend(TCHDB *hdb, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
        uint8_t hash);
static bool tchdbputasyncimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx,
        uint8_t hash, const char *vbuf, int vsiz);
static bool tchdboutimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash);
static char *tchdbgetimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash,
        int *sp);
static int tchdbgetintobuf(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash,
        char *vbuf, int max);
static int tchdbgetintoxstrimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash,
        TCXSTR *xstr);
static char *tchdbgetnextimpl(TCHDB *hdb, const char *kbuf, int ksiz, int *sp,
        const char **vbp, int *vsp);
static int tchdbvsizimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash);
static char *tchdbiternextimpl(TCHDB *hdb, int *sp);
static bool tchdbiternextintoxstr(TCHDB *hdb, TCXSTR *kxstr, TCXSTR *vxstr);
static bool tchdbiternextintoxstr2(TCHDB *hdb, uint64_t *iter, TCXSTR *kxstr, TCXSTR *vxstr);
static bool tchdboptimizeimpl(TCHDB *hdb, int64_t bnum, int8_t apow, int8_t fpow, uint8_t opts);
static bool tchdbvanishimpl(TCHDB *hdb);
static bool tchdbcopyimpl(TCHDB *hdb, const char *path);
static bool tchdbdefragimpl(TCHDB *hdb, int64_t step);
static bool tchdbiterjumpimpl(TCHDB *hdb, const char *kbuf, int ksiz);
static bool tchdbforeachimpl(TCHDB *hdb, TCITER iter, void *op);
static bool tchdbftruncate(TCHDB *hdb, off_t length);
static bool tchdbftruncate2(TCHDB *hdb, off_t length, int opts);
EJDB_INLINE bool tchdbwritesmem(TCHDB *hdb, size_t off, const void *ptr, size_t sz, int opts);
EJDB_INLINE bool tchdbreadsmem(TCHDB *hdb, void *dst, size_t off, size_t sz, int opts);
EJDB_INLINE bool tchdblockmethod(TCHDB *hdb, bool wr);
EJDB_INLINE bool tchdbunlockmethod(TCHDB *hdb);
EJDB_INLINE bool tchdblocksmem(TCHDB *hdb, bool wr) __attribute__((unused));
EJDB_INLINE bool tchdblocksmem2(TCHDB *hdb, bool wr) __attribute__((unused));
EJDB_INLINE bool tchdbunlocksmem(TCHDB *hdb);
EJDB_INLINE bool tchdblockrecord(TCHDB *hdb, uint8_t bidx, bool wr);
EJDB_INLINE bool tchdbunlockrecord(TCHDB *hdb, uint8_t bidx);
EJDB_INLINE bool tchdblockallrecords(TCHDB *hdb, bool wr);
EJDB_INLINE bool tchdbunlockallrecords(TCHDB *hdb);
EJDB_INLINE bool tchdblockdb(TCHDB *hdb);
EJDB_INLINE bool tchdbunlockdb(TCHDB *hdb);
EJDB_INLINE bool tchdblockwal(TCHDB *hdb);
EJDB_INLINE bool tchdbunlockwal(TCHDB *hdb);

/* debugging function prototypes */
void tchdbprintmeta(TCHDB *hdb);
void tchdbprintrec(TCHDB *hdb, TCHREC *rec);



/*************************************************************************************************
 * API
 *************************************************************************************************/

/* Get the message string corresponding to an error code. */
const char *tchdberrmsg(int ecode) {
    return tcerrmsg(ecode);
}

/* Create a hash database object. */
TCHDB *tchdbnew(void) {
    TCHDB *hdb;
    TCMALLOC(hdb, sizeof (*hdb));
    tchdbclear(hdb);
    return hdb;
}

/* Delete a hash database object. */
void tchdbdel(TCHDB *hdb) {
    assert(hdb);
    if (!INVALIDHANDLE(hdb->fd)) tchdbclose(hdb);
    if (hdb->mmtx) {
        pthread_mutex_destroy(hdb->wmtx);
        pthread_mutex_destroy(hdb->dmtx);
        for (int i = UINT8_MAX; i >= 0; i--) {
            pthread_rwlock_destroy((pthread_rwlock_t *) hdb->rmtxs + i);
        }
        pthread_rwlock_destroy(hdb->mmtx);
        pthread_rwlock_destroy(hdb->smtx);
        TCFREE(hdb->wmtx);
        TCFREE(hdb->dmtx);
        TCFREE(hdb->smtx);
        TCFREE(hdb->rmtxs);
        TCFREE(hdb->mmtx);
    }
    if (hdb->eckey) {
        pthread_key_delete(*(pthread_key_t *) hdb->eckey);
        TCFREE(hdb->eckey);
    }
    if (hdb->iter2list) {
        for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
            TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
            assert(pit && *pit);
            TCFREE(*pit);
        }
        tclistdel(hdb->iter2list);
    }
    TCFREE(hdb);
}

/* Get the last happened error code of a hash database object. */
int tchdbecode(TCHDB *hdb) {
    assert(hdb);
    return (hdb->eckey) ? (int) (intptr_t) pthread_getspecific(*(pthread_key_t *) hdb->eckey) : hdb->ecode;
}

/* Set mutual exclusion control of a hash database object for threading. */
bool tchdbsetmutex(TCHDB *hdb) {
    assert(hdb);
    if (hdb->mmtx || !INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    TCMALLOC(hdb->mmtx, sizeof (pthread_rwlock_t));
    TCMALLOC(hdb->smtx, sizeof (pthread_rwlock_t));
    TCMALLOC(hdb->rmtxs, (UINT8_MAX + 1) * sizeof (pthread_rwlock_t));
    TCMALLOC(hdb->dmtx, sizeof (pthread_mutex_t));
    TCMALLOC(hdb->wmtx, sizeof (pthread_mutex_t));
    bool err = false;
    if (pthread_rwlock_init(hdb->smtx, NULL) != 0) err = true;
    if (pthread_rwlock_init(hdb->mmtx, NULL) != 0) err = true;
    for (int i = 0; i <= UINT8_MAX; i++) {
        if (pthread_rwlock_init((pthread_rwlock_t *) hdb->rmtxs + i, NULL) != 0) err = true;
    }
    if (pthread_mutex_init(hdb->dmtx, NULL) != 0) err = true;
    if (pthread_mutex_init(hdb->wmtx, NULL) != 0) err = true;
    if (err) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        TCFREE(hdb->wmtx);
        TCFREE(hdb->dmtx);
        TCFREE(hdb->rmtxs);
        TCFREE(hdb->smtx);
        TCFREE(hdb->mmtx);
        hdb->wmtx = NULL;
        hdb->dmtx = NULL;
        hdb->rmtxs = NULL;
        hdb->smtx = NULL;
        hdb->mmtx = NULL;
        return false;
    }
    return true;
}

/* Set the tuning parameters of a hash database object. */
bool tchdbtune(TCHDB *hdb, int64_t bnum, int8_t apow, int8_t fpow, uint8_t opts) {
    assert(hdb);
    if (!INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    hdb->bnum = (bnum > 0) ? tcgetprime(bnum) : HDBDEFBNUM;
    hdb->apow = (apow >= 0) ? tclmin(apow, HDBMAXAPOW) : HDBDEFAPOW;
    hdb->fpow = (fpow >= 0) ? tclmin(fpow, HDBMAXFPOW) : HDBDEFFPOW;
    hdb->opts = opts;
    if (!_tc_deflate) hdb->opts &= ~HDBTDEFLATE;
    if (!_tc_bzcompress) hdb->opts &= ~HDBTBZIP;
    return true;
}

/* Set the caching parameters of a hash database object. */
bool tchdbsetcache(TCHDB *hdb, int32_t rcnum) {
    assert(hdb);
    if (!INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    hdb->rcnum = (rcnum > 0) ? tclmin(tclmax(rcnum, HDBCACHEOUT * 2), INT_MAX / 4) : 0;
    return true;
}

/* Set the size of the extra mapped memory of a hash database object. */
bool tchdbsetxmsiz(TCHDB *hdb, int64_t xmsiz) {
    assert(hdb);
    if (!INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    hdb->xmsiz = (xmsiz > 0) ? tcpagealign(xmsiz) : 0;
    return true;
}

/* Set the unit step number of auto defragmentation of a hash database object. */
bool tchdbsetdfunit(TCHDB *hdb, int32_t dfunit) {
    assert(hdb);
    if (!INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    hdb->dfunit = (dfunit > 0) ? dfunit : 0;
    return true;
}

/* Open a database file and connect a hash database object.
   #METHOD WLOCK */
bool tchdbopen(TCHDB *hdb, const char *path, int omode) {
    assert(hdb && path);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (!INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!hdb->eckey) {
        TCMALLOC(hdb->eckey, sizeof (pthread_key_t));
        if (pthread_key_create(hdb->eckey, NULL)) {
            TCFREE(hdb->eckey);
            hdb->eckey = NULL;
            HDBUNLOCKMETHOD(hdb);
            tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
            return false;
        }
    }
    char *rpath = tcrealpath(path);
    if (!rpath) {
        tchdbsetecode(hdb, tcfilerrno2tcerr(TCEOPEN), __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!tcpathlock(rpath)) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        TCFREE(rpath);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool rv = tchdbopenimpl(hdb, path, omode);
    if (rv) {
        hdb->rpath = rpath;
    } else {
        tcpathunlock(rpath);
        TCFREE(rpath);
    }
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Close a database object. */
bool tchdbclose(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool rv = tchdbcloseimpl(hdb);
    tcpathunlock(hdb->rpath);
    TCFREE(hdb->rpath);
    hdb->rpath = NULL;
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Store a record into a hash database object. */
bool tchdbput(TCHDB *hdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(hdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (!HDBLOCKMETHOD(hdb, false)) return false;
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }

    uint8_t hash;
    char *zbuf = NULL;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
   
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!HDBLOCKRECORD(hdb, bidx, true)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->zmode) {
        if (hdb->opts & HDBTDEFLATE) {
            zbuf = _tc_deflate(vbuf, vsiz, &vsiz, _TCZMRAW);
        } else if (hdb->opts & HDBTBZIP) {
            zbuf = _tc_bzcompress(vbuf, vsiz, &vsiz);
        } else if (hdb->opts & HDBTTCBS) {
            zbuf = tcbsencode(vbuf, vsiz, &vsiz);
        } else {
            zbuf = hdb->enc(vbuf, vsiz, &vsiz, hdb->encop);
        }
        if (!zbuf) {
            tchdbsetecode(hdb, TCEDATACOMPRESS, __FILE__, __LINE__, __func__);
            HDBUNLOCKRECORD(hdb, bidx);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
    }
    bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, (zbuf ? zbuf : vbuf), vsiz, HDBPDOVER);
    if (zbuf) TCFREE(zbuf);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    if (hdb->dfunit > 0) {
        uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
        if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
            rv = false;
        }
    }
    return rv;
}

/* Store a string record into a hash database object. */
bool tchdbput2(TCHDB *hdb, const char *kstr, const char *vstr) {
    assert(hdb && kstr && vstr);
    return tchdbput(hdb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Store a new record into a hash database object. */
bool tchdbputkeep(TCHDB *hdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(hdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (!HDBLOCKMETHOD(hdb, false)) return false;
    uint8_t hash;
    char *zbuf = NULL;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!HDBLOCKRECORD(hdb, bidx, true)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->zmode) {
        if (hdb->opts & HDBTDEFLATE) {
            zbuf = _tc_deflate(vbuf, vsiz, &vsiz, _TCZMRAW);
        } else if (hdb->opts & HDBTBZIP) {
            zbuf = _tc_bzcompress(vbuf, vsiz, &vsiz);
        } else if (hdb->opts & HDBTTCBS) {
            zbuf = tcbsencode(vbuf, vsiz, &vsiz);
        } else {
            zbuf = hdb->enc(vbuf, vsiz, &vsiz, hdb->encop);
        }
        if (!zbuf) {
            tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
            HDBUNLOCKRECORD(hdb, bidx);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
    }
    bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, (zbuf ? zbuf : vbuf), vsiz, HDBPDKEEP);
    if (zbuf) TCFREE(zbuf);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    if (hdb->dfunit > 0) {
        uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
        if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
            rv = false;
        }
    }
    return rv;
}

/* Store a new string record into a hash database object. */
bool tchdbputkeep2(TCHDB *hdb, const char *kstr, const char *vstr) {
    return tchdbputkeep(hdb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Concatenate a value at the end of the existing record in a hash database object. */
bool tchdbputcat(TCHDB *hdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(hdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (!HDBLOCKMETHOD(hdb, false)) return false;
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!HDBLOCKRECORD(hdb, bidx, true)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->zmode) {
        char *zbuf;
        int osiz;
        char *obuf = tchdbgetimpl(hdb, kbuf, ksiz, bidx, hash, &osiz);
        if (obuf) {
            TCREALLOC(obuf, obuf, osiz + vsiz + 1);
            memcpy(obuf + osiz, vbuf, vsiz);
            if (hdb->opts & HDBTDEFLATE) {
                zbuf = _tc_deflate(obuf, osiz + vsiz, &vsiz, _TCZMRAW);
            } else if (hdb->opts & HDBTBZIP) {
                zbuf = _tc_bzcompress(obuf, osiz + vsiz, &vsiz);
            } else if (hdb->opts & HDBTTCBS) {
                zbuf = tcbsencode(obuf, osiz + vsiz, &vsiz);
            } else {
                zbuf = hdb->enc(obuf, osiz + vsiz, &vsiz, hdb->encop);
            }
            TCFREE(obuf);
        } else {
            if (hdb->opts & HDBTDEFLATE) {
                zbuf = _tc_deflate(vbuf, vsiz, &vsiz, _TCZMRAW);
            } else if (hdb->opts & HDBTBZIP) {
                zbuf = _tc_bzcompress(vbuf, vsiz, &vsiz);
            } else if (hdb->opts & HDBTTCBS) {
                zbuf = tcbsencode(vbuf, vsiz, &vsiz);
            } else {
                zbuf = hdb->enc(vbuf, vsiz, &vsiz, hdb->encop);
            }
        }
        if (!zbuf) {
            tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
            HDBUNLOCKRECORD(hdb, bidx);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
        bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, zbuf, vsiz, HDBPDOVER);
        TCFREE(zbuf);
        HDBUNLOCKRECORD(hdb, bidx);
        HDBUNLOCKMETHOD(hdb);
        if (hdb->dfunit > 0) {
            uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
            if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
                rv = false;
            }
        }
        return rv;
    }
    bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, vbuf, vsiz, HDBPDCAT);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    if (hdb->dfunit > 0) {
        uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
        if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
            rv = false;
        }
    }
    return rv;
}

/* Concatenate a string value at the end of the existing record in a hash database object. */
bool tchdbputcat2(TCHDB *hdb, const char *kstr, const char *vstr) {
    assert(hdb && kstr && vstr);
    return tchdbputcat(hdb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Store a record into a hash database object in asynchronous fashion. */
bool tchdbputasync(TCHDB *hdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
    assert(hdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    uint8_t hash;
    char *zbuf = NULL;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    hdb->async = true;
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->zmode) {
        if (hdb->opts & HDBTDEFLATE) {
            zbuf = _tc_deflate(vbuf, vsiz, &vsiz, _TCZMRAW);
        } else if (hdb->opts & HDBTBZIP) {
            zbuf = _tc_bzcompress(vbuf, vsiz, &vsiz);
        } else if (hdb->opts & HDBTTCBS) {
            zbuf = tcbsencode(vbuf, vsiz, &vsiz);
        } else {
            zbuf = hdb->enc(vbuf, vsiz, &vsiz, hdb->encop);
        }
        if (!zbuf) {
            tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
    }
    bool rv = tchdbputasyncimpl(hdb, kbuf, ksiz, bidx, hash, (zbuf ? zbuf : vbuf), vsiz);
    if (zbuf) TCFREE(zbuf);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Store a string record into a hash database object in asynchronous fashion. */
bool tchdbputasync2(TCHDB *hdb, const char *kstr, const char *vstr) {
    assert(hdb && kstr && vstr);
    return tchdbputasync(hdb, kstr, strlen(kstr), vstr, strlen(vstr));
}

/* Remove a record of a hash database object. */
bool tchdbout(TCHDB *hdb, const void *kbuf, int ksiz) {
    assert(hdb && kbuf && ksiz >= 0);
    if (!HDBLOCKMETHOD(hdb, false)) return false;
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!HDBLOCKRECORD(hdb, bidx, true)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool rv = tchdboutimpl(hdb, kbuf, ksiz, bidx, hash);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    if (hdb->dfunit > 0) {
        uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
        if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
            rv = false;
        }
    }
    return rv;
}

/* Remove a string record of a hash database object. */
bool tchdbout2(TCHDB *hdb, const char *kstr) {
    assert(hdb && kstr);
    return tchdbout(hdb, kstr, strlen(kstr));
}

/* Retrieve a record in a hash database object. */
void *tchdbget(TCHDB *hdb, const void *kbuf, int ksiz, int *sp) {
    assert(hdb && kbuf && ksiz >= 0 && sp);
    if (!HDBLOCKMETHOD(hdb, false)) return NULL;
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    if (!HDBLOCKRECORD(hdb, bidx, false)) {
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    char *rv = tchdbgetimpl(hdb, kbuf, ksiz, bidx, hash, sp);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

int tchdbgetintoxstr(TCHDB *hdb, const void *kbuf, int ksiz, TCXSTR *xstr) {
    assert(hdb && kbuf && ksiz >= 0 && xstr);
    if (!HDBLOCKMETHOD(hdb, false)) return -1;
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    if (!HDBLOCKRECORD(hdb, bidx, false)) {
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    int rv = tchdbgetintoxstrimpl(hdb, kbuf, ksiz, bidx, hash, xstr);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    return rv;

}

/* Retrieve a string record in a hash database object. */
char *tchdbget2(TCHDB *hdb, const char *kstr) {
    assert(hdb && kstr);
    int vsiz;
    return tchdbget(hdb, kstr, strlen(kstr), &vsiz);
}

/* Retrieve a record in a hash database object and write the value into a buffer. */
int tchdbget3(TCHDB *hdb, const void *kbuf, int ksiz, void *vbuf, int max) {
    assert(hdb && kbuf && ksiz >= 0 && vbuf && max >= 0);
    if (!HDBLOCKMETHOD(hdb, false)) return -1;
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    if (!HDBLOCKRECORD(hdb, bidx, false)) {
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    int rv = tchdbgetintobuf(hdb, kbuf, ksiz, bidx, hash, vbuf, max);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Get the size of the value of a record in a hash database object. */
int tchdbvsiz(TCHDB *hdb, const void *kbuf, int ksiz) {
    assert(hdb && kbuf && ksiz >= 0);
    if (!HDBLOCKMETHOD(hdb, false)) return -1;
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    if (!HDBLOCKRECORD(hdb, bidx, false)) {
        HDBUNLOCKMETHOD(hdb);
        return -1;
    }
    int rv = tchdbvsizimpl(hdb, kbuf, ksiz, bidx, hash);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Get the size of the value of a string record in a hash database object. */
int tchdbvsiz2(TCHDB *hdb, const char *kstr) {
    assert(hdb && kstr);
    return tchdbvsiz(hdb, kstr, strlen(kstr));
}

/* Initialize the iterator of a hash database object. */
bool tchdbiterinit(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    hdb->iter = hdb->frec;
    HDBUNLOCKMETHOD(hdb);
    return true;
}

/* Initialize the iterator of a hash database object. */
TCHDBITER* tchdbiter2init(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return NULL;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    if (hdb->iter2list == NULL) {
        hdb->iter2list = tclistnew2(16);
    }
    TCHDBITER *it;
    TCMALLOC(it, sizeof (*it));
    it->pos = hdb->frec;
    TCLISTPUSH(hdb->iter2list, &it, sizeof (it));
    HDBUNLOCKMETHOD(hdb);
    return it;
}

bool tchdbiter2dispose(TCHDB *hdb, TCHDBITER* iter) {
    assert(hdb);
    if (!iter) return false;
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool found = false;
    for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
        TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
        assert(pit && *pit);
        if (*pit == iter) {
            TCFREE(tclistremove2(hdb->iter2list, i));
            found = true;
            break;
        }
    }
    if (found) {
        TCFREE(iter);
    }
    HDBUNLOCKMETHOD(hdb);
    return found;
}

EJDB_EXPORT bool tchdbiter2next(TCHDB *hdb, TCHDBITER* iter, TCXSTR *kxstr, TCXSTR *vxstr) {
    assert(hdb && kxstr && vxstr && iter);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (INVALIDHANDLE(hdb->fd) || iter->pos < 1) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool rv = tchdbiternextintoxstr2(hdb, &iter->pos, kxstr, vxstr);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Get the next key of the iterator of a hash database object. */
void *tchdbiternext(TCHDB *hdb, int *sp) {
    assert(hdb && sp);
    if (!HDBLOCKMETHOD(hdb, true)) return NULL;
    if (INVALIDHANDLE(hdb->fd) || hdb->iter < 1) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    char *rv = tchdbiternextimpl(hdb, sp);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Get the next key string of the iterator of a hash database object. */
char *tchdbiternext2(TCHDB *hdb) {
    assert(hdb);
    int vsiz;
    return tchdbiternext(hdb, &vsiz);
}

/* Get the next extensible objects of the iterator of a hash database object. */
bool tchdbiternext3(TCHDB *hdb, TCXSTR *kxstr, TCXSTR *vxstr) {
    assert(hdb && kxstr && vxstr);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (INVALIDHANDLE(hdb->fd) || hdb->iter < 1) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool rv = tchdbiternextintoxstr(hdb, kxstr, vxstr);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Get forward matching keys in a hash database object. */
TCLIST *tchdbfwmkeys(TCHDB *hdb, const void *pbuf, int psiz, int max) {
    assert(hdb && pbuf && psiz >= 0);
    TCLIST* keys = tclistnew();
    if (!HDBLOCKMETHOD(hdb, true)) return keys;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return keys;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return keys;
    }
    if (max < 0) max = INT_MAX;
    uint64_t iter = hdb->iter;
    hdb->iter = hdb->frec;
    char *kbuf;
    int ksiz;
    while (TCLISTNUM(keys) < max && (kbuf = tchdbiternextimpl(hdb, &ksiz)) != NULL) {
        if (ksiz >= psiz && !memcmp(kbuf, pbuf, psiz)) {
            tclistpushmalloc(keys, kbuf, ksiz);
        } else {
            TCFREE(kbuf);
        }
    }
    hdb->iter = iter;
    HDBUNLOCKMETHOD(hdb);
    return keys;
}

/* Get forward matching string keys in a hash database object. */
TCLIST *tchdbfwmkeys2(TCHDB *hdb, const char *pstr, int max) {
    assert(hdb && pstr);
    return tchdbfwmkeys(hdb, pstr, strlen(pstr), max);
}

/* Add an integer to a record in a hash database object. */
int tchdbaddint(TCHDB *hdb, const void *kbuf, int ksiz, int num) {
    assert(hdb && kbuf && ksiz >= 0);
    if (!HDBLOCKMETHOD(hdb, false)) return INT_MIN;
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return INT_MIN;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return INT_MIN;
    }
    if (!HDBLOCKRECORD(hdb, bidx, true)) {
        HDBUNLOCKMETHOD(hdb);
        return INT_MIN;
    }
    if (hdb->zmode) {
        char *zbuf;
        int osiz;
        char *obuf = tchdbgetimpl(hdb, kbuf, ksiz, bidx, hash, &osiz);
        if (obuf) {
            if (osiz != sizeof (num)) {
                tchdbsetecode(hdb, TCEKEEP, __FILE__, __LINE__, __func__);
                TCFREE(obuf);
                HDBUNLOCKRECORD(hdb, bidx);
                HDBUNLOCKMETHOD(hdb);
                return INT_MIN;
            }
            num += *(int *) obuf;
            TCFREE(obuf);
        }
        int zsiz;
        if (hdb->opts & HDBTDEFLATE) {
            zbuf = _tc_deflate((char *) &num, sizeof (num), &zsiz, _TCZMRAW);
        } else if (hdb->opts & HDBTBZIP) {
            zbuf = _tc_bzcompress((char *) &num, sizeof (num), &zsiz);
        } else if (hdb->opts & HDBTTCBS) {
            zbuf = tcbsencode((char *) &num, sizeof (num), &zsiz);
        } else {
            zbuf = hdb->enc((char *) &num, sizeof (num), &zsiz, hdb->encop);
        }
        if (!zbuf) {
            tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
            HDBUNLOCKRECORD(hdb, bidx);
            HDBUNLOCKMETHOD(hdb);
            return INT_MIN;
        }
        bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, zbuf, zsiz, HDBPDOVER);
        TCFREE(zbuf);
        HDBUNLOCKRECORD(hdb, bidx);
        HDBUNLOCKMETHOD(hdb);
        if (hdb->dfunit > 0) {
            uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
            if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
                rv = false;
            }
        }
        return rv ? num : INT_MIN;
    }
    bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, (char *) &num, sizeof (num), HDBPDADDINT);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    if (hdb->dfunit > 0) {
        uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
        if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
            rv = false;
        }
    }
    return rv ? num : INT_MIN;
}

/* Add a real number to a record in a hash database object. */
double tchdbadddouble(TCHDB *hdb, const void *kbuf, int ksiz, double num) {
    assert(hdb && kbuf && ksiz >= 0);
    if (!HDBLOCKMETHOD(hdb, false)) return nan("");
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return nan("");
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return nan("");
    }
    if (!HDBLOCKRECORD(hdb, bidx, true)) {
        HDBUNLOCKMETHOD(hdb);
        return nan("");
    }
    if (hdb->zmode) {
        char *zbuf;
        int osiz;
        char *obuf = tchdbgetimpl(hdb, kbuf, ksiz, bidx, hash, &osiz);
        if (obuf) {
            if (osiz != sizeof (num)) {
                tchdbsetecode(hdb, TCEKEEP, __FILE__, __LINE__, __func__);
                TCFREE(obuf);
                HDBUNLOCKRECORD(hdb, bidx);
                HDBUNLOCKMETHOD(hdb);
                return nan("");
            }
            num += *(double *) obuf;
            TCFREE(obuf);
        }
        int zsiz;
        if (hdb->opts & HDBTDEFLATE) {
            zbuf = _tc_deflate((char *) &num, sizeof (num), &zsiz, _TCZMRAW);
        } else if (hdb->opts & HDBTBZIP) {
            zbuf = _tc_bzcompress((char *) &num, sizeof (num), &zsiz);
        } else if (hdb->opts & HDBTTCBS) {
            zbuf = tcbsencode((char *) &num, sizeof (num), &zsiz);
        } else {
            zbuf = hdb->enc((char *) &num, sizeof (num), &zsiz, hdb->encop);
        }
        if (!zbuf) {
            tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
            HDBUNLOCKRECORD(hdb, bidx);
            HDBUNLOCKMETHOD(hdb);
            return nan("");
        }
        bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, zbuf, zsiz, HDBPDOVER);
        TCFREE(zbuf);
        HDBUNLOCKRECORD(hdb, bidx);
        HDBUNLOCKMETHOD(hdb);
        if (hdb->dfunit > 0) {
            uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
            if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
                rv = false;
            }
        }
        return rv ? num : nan("");
    }
    bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, (char *) &num, sizeof (num), HDBPDADDDBL);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    if (hdb->dfunit > 0) {
        uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
        if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
            rv = false;
        }
    }
    return rv ? num : nan("");
}

/* Synchronize updated contents of a hash database object with the file and the device. */
bool tchdbsync(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (hdb->tran) {
        tchdbsetecode(hdb, TCETR, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool rv = tchdbmemsync(hdb, true);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Optimize the file of a hash database object. */
bool tchdboptimize(TCHDB *hdb, int64_t bnum, int8_t apow, int8_t fpow, uint8_t opts) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (hdb->tran) {
        tchdbsetecode(hdb, TCETR, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    HDBTHREADYIELD(hdb);
    bool rv = tchdboptimizeimpl(hdb, bnum, apow, fpow, opts);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Remove all records of a hash database object. */
bool tchdbvanish(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (hdb->tran) {
        tchdbsetecode(hdb, TCETR, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    HDBTHREADYIELD(hdb);
    bool rv = tchdbvanishimpl(hdb);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Copy the database file of a hash database object. */
bool tchdbcopy(TCHDB *hdb, const char *path) {
    assert(hdb && path);
    if (!HDBLOCKMETHOD(hdb, false)) return false;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!HDBLOCKALLRECORDS(hdb, false)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    HDBTHREADYIELD(hdb);
    bool rv = tchdbcopyimpl(hdb, path);
    HDBUNLOCKALLRECORDS(hdb);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Begin the transaction of a hash database object. */
bool tchdbtranbegin(TCHDB *hdb) {
    assert(hdb);
    for (double wsec = 1.0 / sysconf_SC_CLK_TCK; true; wsec *= 2) {
        if (!HDBLOCKMETHOD(hdb, true)) return false;
        if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER) || hdb->fatal) {
            tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
        if (!hdb->tran) break;
        HDBUNLOCKMETHOD(hdb);
        if (wsec > 1.0) wsec = 1.0;
        tcsleep(wsec);
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!tchdbmemsync(hdb, (hdb->omode & HDBOTSYNC))) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (INVALIDHANDLE(hdb->walfd)) {
        char *tpath = tcsprintf("%s%c%s", hdb->path, MYEXTCHR, HDBWALSUFFIX);
#ifndef _WIN32
        HANDLE walfd = open(tpath, O_RDWR | O_CREAT | O_TRUNC, HDBFILEMODE);
#else
        HANDLE walfd = CreateFile(tpath, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
        TCFREE(tpath);
        if (INVALIDHANDLE(walfd)) {
            tchdbsetecode(hdb, tcfilerrno2tcerr(TCEOPEN), __FILE__, __LINE__, __func__);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
        hdb->walfd = walfd;
    }
    tchdbsetflag(hdb, HDBFOPEN, false);
    if (!tchdbwalinit(hdb)) {
        tchdbsetflag(hdb, HDBFOPEN, true);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    tchdbsetflag(hdb, HDBFOPEN, true);
    hdb->tran = true;
    HDBUNLOCKMETHOD(hdb);
    return true;
}

/* Commit the transaction of a hash database object. */
bool tchdbtrancommit(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (!hdb->tran) {
        tchdbsetecode(hdb, TCETR, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER) || hdb->fatal) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool err = false;
    if (hdb->async && !tchdbflushdrp(hdb)) err = true;
    if (!tchdbmemsync(hdb, (hdb->omode & HDBOTSYNC))) err = true;
    if (HDBLOCKWAL(hdb)) {
        if (!err && !tcftruncate(hdb->walfd, 0)) {
            tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
            err = true;
        }
        HDBUNLOCKWAL(hdb);
    } else {
        err = true;
    }
    hdb->tran = false;
    HDBUNLOCKMETHOD(hdb);
    return !err;
}

/* Abort the transaction of a hash database object. */
bool tchdbtranabort(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (!hdb->tran) {
        tchdbsetecode(hdb, TCETR, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool err = false;
    if (hdb->async && !tchdbflushdrp(hdb)) err = true;
    if (!tchdbmemsync(hdb, false)) err = true;
    if (!tchdbwalrestore(hdb, hdb->path)) err = true;
    hdb->dfcur = hdb->frec;
    hdb->iter = 0;
    hdb->fbpnum = 0;
    if (hdb->recc) tcmdbvanish(hdb->recc);
    hdb->tran = false;
    HDBUNLOCKMETHOD(hdb);
    return !err;
}

/* Get the file path of a hash database object. */
const char *tchdbpath(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return NULL;
    }
    return (hdb->path);
}

/* Get the number of records of a hash database object. */
uint64_t tchdbrnum(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    if (!HDBLOCKMETHOD(hdb, false)) {
        return 0;
    }
    if (!HDBLOCKDB(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return 0;
    }
    uint64_t rv = hdb->rnum;
    HDBUNLOCKDB(hdb);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Get the size of the database file of a hash database object. */
uint64_t tchdbfsiz(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    if (!HDBLOCKMETHOD(hdb, false)) {
        return 0;
    }
    if (!HDBLOCKDB(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return 0;
    }
    uint64_t rv = hdb->fsiz;
    HDBUNLOCKDB(hdb);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/*************************************************************************************************
 * features for experts
 *************************************************************************************************/

/* Set the error code of a hash database object. */
void tchdbsetecode(TCHDB *hdb, int ecode, const char *filename, int line, const char *func) {
    tchdbsetecode2(hdb, ecode, filename, line, func, false);
}

/* Set the error code of a hash database object. */
void tchdbsetecode2(TCHDB *hdb, int ecode, const char *filename, int line, const char *func, bool notfatal) {
    assert(hdb && filename && line >= 1 && func);
    if (!hdb->fatal) {
        if (hdb->eckey) {
            pthread_setspecific(*(pthread_key_t *) hdb->eckey, (void *) (intptr_t) ecode);
        }
        hdb->ecode = ecode;
    }
    switch (ecode) { //Fatal errors
        case TCETHREAD:
        case TCENOFILE:
        case TCENOPERM:
        case TCEMETA:
        case TCERHEAD:
        case TCEOPEN:
        case TCECLOSE:
        case TCETRUNC:
        case TCESYNC:
        case TCESTAT:
        case TCESEEK:
        case TCEREAD:
        case TCEWRITE:
        case TCEMMAP:
        case TCELOCK:
        case TCEUNLINK:
        case TCERENAME:
        case TCEMKDIR:
        case TCERMDIR:
        case TCEICOMPRESS:
        {
            if (!notfatal) {
                hdb->fatal = true;
                if (!INVALIDHANDLE(hdb->fd) && (hdb->omode & HDBOWRITER)) tchdbsetflag(hdb, HDBFFATAL, true);
            }
            break;
        }
        case TCESUCCESS:
        case TCENOREC:
        case TCEKEEP:
            return;
            break;
        default:
            break;
    }
#ifdef _WIN32
    DWORD winerrno = GetLastError();
#endif
    int stderrno = errno;
    if (!INVALIDHANDLE(hdb->dbgfd) || hdb->fatal) {
        HANDLE dbgfd = INVALIDHANDLE(hdb->dbgfd) ? GET_STDERR_HANDLE() : (hdb->dbgfd);
        char obuf[HDBIOBUFSIZ];
#ifdef _WIN32
        LPTSTR errorText = NULL;
        if (winerrno > 0) {
            DWORD ret = FormatMessage(
                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, winerrno, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) & errorText, 0, NULL);
            if (!ret) {
                if (errorText) LocalFree(errorText);
                errorText = NULL;
            }
        }
        int osiz = sprintf(obuf, "ERROR:%s:%d:%s:%s:%d:%s:%d:%s:%d:%s\n",
                filename, line,
                func, (hdb->path ? hdb->path : "-"),
                ecode, tchdberrmsg(ecode),
                (int) winerrno, (errorText ? errorText : "-"),
                stderrno, (stderrno > 0 ? strerror(stderrno) : "-"));
        if (errorText) LocalFree(errorText);
#else
        int osiz = sprintf(obuf, "ERROR:%s:%d:%s:%s:%d:%s:%d:%s\n",
                filename, line,
                func, (hdb->path ? hdb->path : "-"),
                ecode, tchdberrmsg(ecode),
                stderrno, strerror(stderrno));
#endif
        tcwrite(dbgfd, obuf, osiz);
    }
}

/* Set the type of a hash database object. */
void tchdbsettype(TCHDB *hdb, uint8_t type) {
    assert(hdb);
    if (!INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return;
    }
    hdb->type = type;
}

/* Set the file descriptor for debugging output. */
void tchdbsetdbgfd(TCHDB *hdb, HANDLE fd) {
    assert(hdb);
    hdb->dbgfd = fd;
}

/* Get the file descriptor for debugging output. */
HANDLE tchdbdbgfd(TCHDB *hdb) {
    assert(hdb);
    return hdb->dbgfd;
}

/* Check whether mutual exclusion control is set to a hash database object. */
bool tchdbhasmutex(TCHDB *hdb) {
    assert(hdb);
    return (hdb->mmtx != NULL);
}

/* Synchronize updating contents on memory of a hash database object. */
bool tchdbmemsync(TCHDB *hdb, bool phys) {
    assert(hdb);
    bool err = false;
    char hbuf[HDBHEADSIZ];
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    if (!tchdbdumpmeta(hdb, hbuf)) {
        return false;
    }
    tchdbwritesmem(hdb, 0, hbuf, HDBOPAQUEOFF, 0);
    if (phys) {
        if (!HDBLOCKSMEMPTR(hdb, false)) return false;
#ifdef _WIN32
        if (!hdb->map || !FlushViewOfFile((LPCVOID) hdb->map, 0)) {
            tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
            err = true;
        }
#elif !defined __GNU__
        size_t xmsiz = (hdb->xmsiz > hdb->msiz) ? hdb->xmsiz : hdb->msiz;
        if (!hdb->map || msync(hdb->map, xmsiz, MS_SYNC)) {
            tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
            err = true;
        }
#endif
        HDBUNLOCKSMEMPTR(hdb);
        if (fsync(hdb->fd)) {
            tchdbsetecode(hdb, TCESYNC, __FILE__, __LINE__, __func__);
            err = true;
        }
    }
    return !err;
}

/* Get the number of elements of the bucket array of a hash database object. */
uint64_t tchdbbnum(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->bnum;
}

/* Get the record alignment a hash database object. */
uint32_t tchdbalign(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->align;
}

/* Get the maximum number of the free block pool of a a hash database object. */
uint32_t tchdbfbpmax(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->fbpmax;
}

/* Get the size of the extra mapped memory of a hash database object. */
uint64_t tchdbxmsiz(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->xmsiz;
}

/* Get the inode number of the database file of a hash database object. */
uint64_t tchdbinode(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->inode;
}

/* Get the modification time of the database file of a hash database object. */
time_t tchdbmtime(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->mtime;
}

/* Get the connection mode of a hash database object. */
int tchdbomode(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->omode;
}

/* Get the database type of a hash database object. */
uint8_t tchdbtype(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->type;
}

/* Get the additional flags of a hash database object. */
uint8_t tchdbflags(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->flags;
}

/* Get the options of a hash database object. */
uint8_t tchdbopts(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    return hdb->opts;
}

size_t tchdbmaxopaquesz() {
    return HDBOPAQUESZ;
}

int tchdbcopyopaque(TCHDB *dst, TCHDB *src, int off, int rsz) {
    assert(dst && src);
    if (rsz == -1) {
        rsz = HDBOPAQUESZ;
    }
    char odata[HDBOPAQUESZ];
    rsz = tchdbreadopaque(src, odata, off, rsz);
    if (rsz == -1) {
        tchdbsetecode(dst, tchdbecode(src), __FILE__, __LINE__, __func__);
        return -1;
    }
    return (tchdbwriteopaque(dst, odata, off, rsz) == rsz);
}

int tchdbreadopaque(TCHDB *hdb, void *dst, int off, int bsiz) {
    assert(hdb && dst);
    if (bsiz == -1) {
        bsiz = HDBOPAQUESZ;
    }
    if (off > HDBOPAQUESZ) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return -1;
    }
    bsiz = MIN(bsiz, HDBOPAQUESZ - off);
    if (bsiz <= 0) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return -1;
    }
    if (!tchdbreadsmem(hdb, dst, HDBOPAQUEOFF + off, bsiz, 0)) {
        return -1;
    }
    return bsiz;
}

int tchdbwriteopaque(TCHDB *hdb, const void *src, int off, int nb) {
    assert(hdb && src);
    if (nb == -1) {
        nb = HDBOPAQUESZ;
    }
    if (off > HDBOPAQUESZ) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return -1;
    }
    nb = MIN(nb, HDBOPAQUESZ - off);
    if (nb <= 0) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return -1;
    }
    if (!tchdbwritesmem(hdb, HDBOPAQUEOFF + off, src, nb, 0)) {
        return -1;
    }
    return nb;
}

/* Get the number of used elements of the bucket array of a hash database object. */
uint64_t tchdbbnumused(TCHDB *hdb) {
    assert(hdb);
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return 0;
    }
    if (!HDBLOCKMETHOD(hdb, false)) return 0;
    if (!HDBLOCKSMEMPTR(hdb, false)) {
        HDBUNLOCKMETHOD(hdb);
        return 0;
    }
    uint64_t unum = 0;
    if (hdb->ba64) {
        uint64_t *buckets = HDBB64(hdb);
        for (int i = 0; i < hdb->bnum; i++) {
            if (buckets[i]) unum++;
        }
    } else {
        uint32_t *buckets = HDBB32(hdb);
        for (int i = 0; i < hdb->bnum; i++) {
            if (buckets[i]) unum++;
        }
    }
    HDBUNLOCKSMEMPTR(hdb);
    HDBUNLOCKMETHOD(hdb);
    return unum;
}

/* Set the custom codec functions of a hash database object. */
bool tchdbsetcodecfunc(TCHDB *hdb, TCCODEC enc, void *encop, TCCODEC dec, void *decop) {
    assert(hdb && enc && dec);
    if (!INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        return false;
    }
    hdb->enc = enc;
    hdb->encop = encop;
    hdb->dec = dec;
    hdb->decop = decop;
    return true;
}

/* Get the unit step number of auto defragmentation of a hash database object. */
uint32_t tchdbdfunit(TCHDB *hdb) {
    assert(hdb);
    return hdb->dfunit;
}

/* Perform dynamic defragmentation of a hash database object. */
bool tchdbdefrag(TCHDB *hdb, int64_t step) {
    assert(hdb);
    if (step > 0) {
        if (!HDBLOCKMETHOD(hdb, true)) return false;
        if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
            tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
        if (hdb->async && !tchdbflushdrp(hdb)) {
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
        bool rv = tchdbdefragimpl(hdb, step);
        HDBUNLOCKMETHOD(hdb);
        return rv;
    }
    if (!HDBLOCKMETHOD(hdb, false)) return false;
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool err = false;
    bool init = false;
    bool stop = false;
    while (!err && !stop) {
        if (HDBLOCKALLRECORDS(hdb, true)) {
            if (!init) {
                hdb->dfcur = hdb->frec;
                init = true;
            }
            uint64_t cur = hdb->dfcur;
            if (!tchdbdefragimpl(hdb, UINT8_MAX)) err = true;
            if (hdb->dfcur <= cur) stop = true;
            HDBUNLOCKALLRECORDS(hdb);
            HDBTHREADYIELD(hdb);
        } else {
            err = true;
        }
    }
    HDBUNLOCKMETHOD(hdb);
    return !err;
}

/* Clear the cache of a hash tree database object. */
bool tchdbcacheclear(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    HDBTHREADYIELD(hdb);
    if (hdb->recc) tcmdbvanish(hdb->recc);
    HDBUNLOCKMETHOD(hdb);
    return true;
}

/* Store a record into a hash database object with a duplication handler. */
bool tchdbputproc(TCHDB *hdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz,
        TCPDPROC proc, void *op) {
    assert(hdb && kbuf && ksiz >= 0 && proc);
    if (!HDBLOCKMETHOD(hdb, false)) return false;
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!HDBLOCKRECORD(hdb, bidx, true)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->zmode) {
        char *zbuf;
        int osiz;
        char *obuf = tchdbgetimpl(hdb, kbuf, ksiz, bidx, hash, &osiz);
        if (obuf) {
            int nsiz;
            char *nbuf = proc(obuf, osiz, &nsiz, op);
            if (nbuf == (void *) - 1) {
                bool rv = tchdboutimpl(hdb, kbuf, ksiz, bidx, hash);
                TCFREE(obuf);
                HDBUNLOCKRECORD(hdb, bidx);
                HDBUNLOCKMETHOD(hdb);
                return rv;
            } else if (nbuf) {
                if (hdb->opts & HDBTDEFLATE) {
                    zbuf = _tc_deflate(nbuf, nsiz, &vsiz, _TCZMRAW);
                } else if (hdb->opts & HDBTBZIP) {
                    zbuf = _tc_bzcompress(nbuf, nsiz, &vsiz);
                } else if (hdb->opts & HDBTTCBS) {
                    zbuf = tcbsencode(nbuf, nsiz, &vsiz);
                } else {
                    zbuf = hdb->enc(nbuf, nsiz, &vsiz, hdb->encop);
                }
                TCFREE(nbuf);
            } else {
                zbuf = NULL;
            }
            TCFREE(obuf);
        } else if (vbuf) {
            if (hdb->opts & HDBTDEFLATE) {
                zbuf = _tc_deflate(vbuf, vsiz, &vsiz, _TCZMRAW);
            } else if (hdb->opts & HDBTBZIP) {
                zbuf = _tc_bzcompress(vbuf, vsiz, &vsiz);
            } else if (hdb->opts & HDBTTCBS) {
                zbuf = tcbsencode(vbuf, vsiz, &vsiz);
            } else {
                zbuf = hdb->enc(vbuf, vsiz, &vsiz, hdb->encop);
            }
        } else {
            tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
            HDBUNLOCKRECORD(hdb, bidx);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
        if (!zbuf) {
            tchdbsetecode(hdb, TCEKEEP, __FILE__, __LINE__, __func__);
            HDBUNLOCKRECORD(hdb, bidx);
            HDBUNLOCKMETHOD(hdb);
            return false;
        }
        bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, zbuf, vsiz, HDBPDOVER);
        TCFREE(zbuf);
        HDBUNLOCKRECORD(hdb, bidx);
        HDBUNLOCKMETHOD(hdb);
        if (hdb->dfunit > 0) {
            uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
            if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
                rv = false;
            }
        }
        return rv;
    }
    HDBPDPROCOP procop;
    procop.proc = proc;
    procop.op = op;
    HDBPDPROCOP *procptr = &procop;
    tcgeneric_t stack[(TCNUMBUFSIZ * 2) / sizeof (tcgeneric_t) + 1];
    char *rbuf;
    if (ksiz <= sizeof (stack) - sizeof (procptr)) {
        rbuf = (char *) stack;
    } else {
        TCMALLOC(rbuf, ksiz + sizeof (procptr));
    }
    char *wp = rbuf;
    memcpy(wp, &procptr, sizeof (procptr));
    wp += sizeof (procptr);
    memcpy(wp, kbuf, ksiz);
    kbuf = rbuf + sizeof (procptr);
    bool rv = tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, vbuf, vsiz, HDBPDPROC);
    if (rbuf != (char *) stack) TCFREE(rbuf);
    HDBUNLOCKRECORD(hdb, bidx);
    HDBUNLOCKMETHOD(hdb);
    if (hdb->dfunit > 0) {
        uint32_t dfcnt = __atomic_load_n(&hdb->dfcnt, __ATOMIC_ACQUIRE);
        if (dfcnt > hdb->dfunit && !tchdbdefrag(hdb, hdb->dfunit * HDBDFRSRAT + 1)) {
            rv = false;
        }
    }
    return rv;
}

/* Get the custom codec functions of a hash database object. */
void tchdbcodecfunc(TCHDB *hdb, TCCODEC *ep, void **eop, TCCODEC *dp, void **dop) {
    assert(hdb && ep && eop && dp && dop);
    *ep = hdb->enc;
    *eop = hdb->encop;
    *dp = hdb->dec;
    *dop = hdb->decop;
}

/* Retrieve the next record of a record in a hash database object. */
void *tchdbgetnext(TCHDB *hdb, const void *kbuf, int ksiz, int *sp) {
    assert(hdb && sp);
    if (!HDBLOCKMETHOD(hdb, true)) return NULL;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    char *rv = tchdbgetnextimpl(hdb, kbuf, ksiz, sp, NULL, NULL);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Retrieve the next record of a string record in a hash database object. */
char *tchdbgetnext2(TCHDB *hdb, const char *kstr) {
    assert(hdb);
    int vsiz;
    return tchdbgetnext(hdb, kstr, strlen(kstr), &vsiz);
}

/* Retrieve the key and the value of the next record of a record in a hash database object. */
char *tchdbgetnext3(TCHDB *hdb, const char *kbuf, int ksiz, int *sp, const char **vbp, int *vsp) {
    assert(hdb && sp && vbp && vsp);
    if (!HDBLOCKMETHOD(hdb, true)) return NULL;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return NULL;
    }
    char *rv = tchdbgetnextimpl(hdb, kbuf, ksiz, sp, vbp, vsp);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Move the iterator to the record corresponding a key of a hash database object. */
bool tchdbiterinit2(TCHDB *hdb, const void *kbuf, int ksiz) {
    assert(hdb && kbuf && ksiz >= 0);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    bool rv = tchdbiterjumpimpl(hdb, kbuf, ksiz);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Move the iterator to the record corresponding a key string of a hash database object. */
bool tchdbiterinit3(TCHDB *hdb, const char *kstr) {
    assert(hdb && kstr);
    return tchdbiterinit2(hdb, kstr, strlen(kstr));
}

/* Process each record atomically of a hash database object. */
bool tchdbforeach(TCHDB *hdb, TCITER iter, void *op) {
    assert(hdb && iter);
    if (!HDBLOCKMETHOD(hdb, false)) return false;
    if (INVALIDHANDLE(hdb->fd)) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (hdb->async && !tchdbflushdrp(hdb)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (!HDBLOCKALLRECORDS(hdb, false)) {
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    HDBTHREADYIELD(hdb);
    bool rv = tchdbforeachimpl(hdb, iter, op);
    HDBUNLOCKALLRECORDS(hdb);
    HDBUNLOCKMETHOD(hdb);
    return rv;
}

/* Void the transaction of a hash database object. */
bool tchdbtranvoid(TCHDB *hdb) {
    assert(hdb);
    if (!HDBLOCKMETHOD(hdb, true)) return false;
    if (!hdb->tran) {
        tchdbsetecode(hdb, TCETR, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    if (INVALIDHANDLE(hdb->fd) || !(hdb->omode & HDBOWRITER) || hdb->fatal) {
        tchdbsetecode(hdb, TCEINVALID, __FILE__, __LINE__, __func__);
        HDBUNLOCKMETHOD(hdb);
        return false;
    }
    hdb->tran = false;
    HDBUNLOCKMETHOD(hdb);
    return true;
}



/*************************************************************************************************
 * private features
 *************************************************************************************************/

/* Get a natural prime number not less than a floor number.
   `num' specified the floor number.
   The return value is a prime number not less than the floor number. */
static uint64_t tcgetprime(uint64_t num) {
    uint64_t primes[] = {
        1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 43, 47, 53, 59, 61, 71, 79, 83,
        89, 103, 109, 113, 127, 139, 157, 173, 191, 199, 223, 239, 251, 283, 317, 349,
        383, 409, 443, 479, 509, 571, 631, 701, 761, 829, 887, 953, 1021, 1151, 1279,
        1399, 1531, 1663, 1789, 1913, 2039, 2297, 2557, 2803, 3067, 3323, 3583, 3833,
        4093, 4603, 5119, 5623, 6143, 6653, 7159, 7673, 8191, 9209, 10223, 11261,
        12281, 13309, 14327, 15359, 16381, 18427, 20479, 22511, 24571, 26597, 28669,
        30713, 32749, 36857, 40949, 45053, 49139, 53239, 57331, 61417, 65521, 73727,
        81919, 90107, 98299, 106487, 114679, 122869, 131071, 147451, 163819, 180221,
        196597, 212987, 229373, 245759, 262139, 294911, 327673, 360439, 393209, 425977,
        458747, 491503, 524287, 589811, 655357, 720887, 786431, 851957, 917503, 982981,
        1048573, 1179641, 1310719, 1441771, 1572853, 1703903, 1835003, 1966079,
        2097143, 2359267, 2621431, 2883577, 3145721, 3407857, 3670013, 3932153,
        4194301, 4718579, 5242877, 5767129, 6291449, 6815741, 7340009, 7864301,
        8388593, 9437179, 10485751, 11534329, 12582893, 13631477, 14680063, 15728611,
        16777213, 18874367, 20971507, 23068667, 25165813, 27262931, 29360087, 31457269,
        33554393, 37748717, 41943023, 46137319, 50331599, 54525917, 58720253, 62914549,
        67108859, 75497467, 83886053, 92274671, 100663291, 109051903, 117440509,
        125829103, 134217689, 150994939, 167772107, 184549373, 201326557, 218103799,
        234881011, 251658227, 268435399, 301989881, 335544301, 369098707, 402653171,
        436207613, 469762043, 503316469, 536870909, 603979769, 671088637, 738197503,
        805306357, 872415211, 939524087, 1006632947, 1073741789, 1207959503,
        1342177237, 1476394991, 1610612711, 1744830457, 1879048183, 2013265907,
        2576980349, 3092376431, 3710851741, 4718021527, 6133428047, 7973456459,
        10365493393, 13475141413, 17517683831, 22772988923, 29604885677, 38486351381,
        50032256819, 65041933867, 84554514043, 109920868241, 153889215497, 0
    };
    int i;
    for (i = 0; primes[i] > 0; i++) {
        if (num <= primes[i]) return primes[i];
    }
    return primes[i - 1];
}

static bool tchdbseekwrite(TCHDB *hdb, off_t off, const void *buf, size_t size) {
    return tchdbseekwrite2(hdb, off, buf, size, 0);
}

/* Seek and write data into a file.
   `hdb' specifies the hash database object.
   `off' specifies the offset of the region to seek.
   `buf' specifies the buffer to store into.
   `size' specifies the size of the buffer.
   The return value is true if successful, else, it is false. */
static bool tchdbseekwrite2(TCHDB *hdb, off_t off, const void *buf, size_t size, int opts) {
    assert(hdb && off >= 0 && buf && size >= 0);
    if (hdb->tran && !(opts & HDBWRITENOWALL) && !tchdbwalwrite(hdb, off, size)) return false;
    off_t end = off + size;
    uint64_t xfsiz = __atomic_load_n64(&hdb->xfsiz, __ATOMIC_ACQUIRE);
    if (end >= xfsiz) {
        if (!tchdbftruncate2(hdb, end, opts)) {
            tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
            return false;
        }
        xfsiz = __atomic_load_n64(&hdb->xfsiz, __ATOMIC_ACQUIRE);
    }
    uint64_t xmsiz = hdb->xmsiz;
    if (end <= xmsiz && end <= xfsiz) {
        if (opts & HDBOPTNOSMLOCK) {
            if (hdb->map == NULL) {
                tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
                return false;
            }
            memcpy((void *) (hdb->map + off), buf, size);
            return true;
        } else {
            if (!HDBLOCKSMEMPTR(hdb, false)) return false;
            if (end <= xmsiz && end <= xfsiz) {
                memcpy((void *) (hdb->map + off), buf, size);
                HDBUNLOCKSMEMPTR(hdb);
                return true;
            }
            HDBUNLOCKSMEMPTR(hdb);
        }
    }
    if (!TCUBCACHE && off < xmsiz && off < xfsiz) {
        int head = xmsiz - off;
        if (opts & HDBOPTNOSMLOCK) {
            if (hdb->map == NULL) {
                tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
                return false;
            }
            memcpy((void *) (hdb->map + off), buf, head);
            off += head;
            buf = (char *) buf + head;
            size -= head;
        } else {
            if (!HDBLOCKSMEMPTR(hdb, false)) return false;
            if (off < xmsiz && off < xfsiz) {
                memcpy((void *) (hdb->map + off), buf, head);
                off += head;
                buf = (char *) buf + head;
                size -= head;
            }
            HDBUNLOCKSMEMPTR(hdb);
        }
    }
    while (true) {
        int wb = pwrite(hdb->fd, buf, size, off);
        if (wb >= size) {
            return true;
        } else if (wb > 0) {
            buf = (char *) buf + wb;
            size -= wb;
            off += wb;
        } else if (wb == -1) {
            if (errno != EINTR) {
                tchdbsetecode(hdb, tcfilerrno2tcerr(TCEWRITE), __FILE__, __LINE__, __func__);
                return false;
            }
        } else {
            if (size > 0) {
                tchdbsetecode(hdb, tcfilerrno2tcerr(TCEWRITE), __FILE__, __LINE__, __func__);
                return false;
            }
        }
    }
    return true;
}

/* Seek and read data from a file.
   `hdb' specifies the hash database object.
   `off' specifies the offset of the region to seek.
   `buf' specifies the buffer to store into.
   `size' specifies the size of the buffer.
   The return value is true if successful, else, it is false. */
static bool tchdbseekread(TCHDB *hdb, off_t off, void *buf, size_t size) {
    return tchdbseekread2(hdb, off, buf, size, 0);
}

static bool tchdbseekread2(TCHDB *hdb, off_t off, void *buf, size_t size, int opts) {
    assert(hdb && off >= 0 && buf && size >= 0);
    bool err = false;
    off_t end = off + size;
    if (!(opts & HDBOPTNOSMLOCK)) {
        if (!HDBLOCKSMEMPTR(hdb, false)) {
            return false;
        }
    }
    if (opts & HDBSEEKTRY) {
        uint64_t fsiz = hdb->fsiz;
        if (end > fsiz) {
            err = true;
            goto finish;
        }
    }
    uint64_t xfsiz = __atomic_load_n64(&hdb->xfsiz, __ATOMIC_ACQUIRE);
    uint64_t xmsiz = hdb->xmsiz;
    if (end <= xmsiz && end <= xfsiz) {
        memcpy(buf, (void *) (hdb->map + off), size);
        goto finish;
    }
    if (!TCUBCACHE && off < xmsiz && off < xfsiz) {
        int head = xmsiz - off;
        if (off < xmsiz && off < xfsiz) {
            memcpy(buf, (void *) (hdb->map + off), head);
            off += head;
            buf = (char *) buf + head;
            size -= head;
        }
    }
    while (true) {
        int rb = pread(hdb->fd, buf, size, off);
        if (rb >= size) {
            break;
        } else if (rb > 0) {
            buf = (char *) buf + rb;
            size -= rb;
            off += rb;
        } else if (rb == -1) {
            //if (errno != EINTR) {
            tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
            err = true;
            break;
            //}
        } else {
            if (size > 0) {
                tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
                err = true;
                break;
            }
        }
    }
finish:
    if (!(opts & HDBOPTNOSMLOCK)) HDBUNLOCKSMEMPTR(hdb);
    return !err;
}

/* Serialize meta data into a buffer.
   `hdb' specifies the hash database object.
   `hbuf' specifies the buffer. */
static bool tchdbdumpmeta(TCHDB *hdb, char *hbuf) {
    if (!HDBLOCKDB(hdb)) return false;
    memset(hbuf, 0, HDBHEADSIZ);
    sprintf(hbuf, "%s\n%s:%d\n", HDBMAGICDATA, _TC_FORMATVER, _TC_LIBVER);
    memcpy(hbuf + HDBTYPEOFF, &(hdb->type), sizeof (hdb->type));
    memcpy(hbuf + HDBFLAGSOFF, &(hdb->flags), sizeof (hdb->flags));
    memcpy(hbuf + HDBAPOWOFF, &(hdb->apow), sizeof (hdb->apow));
    memcpy(hbuf + HDBFPOWOFF, &(hdb->fpow), sizeof (hdb->fpow));
    memcpy(hbuf + HDBOPTSOFF, &(hdb->opts), sizeof (hdb->opts));
    uint64_t llnum;
    llnum = hdb->bnum;
    llnum = TCHTOILL(llnum);
    memcpy(hbuf + HDBBNUMOFF, &llnum, sizeof (llnum));
    llnum = hdb->rnum;
    llnum = TCHTOILL(llnum);
    memcpy(hbuf + HDBRNUMOFF, &llnum, sizeof (llnum));
    llnum = hdb->fsiz;
    llnum = TCHTOILL(llnum);
    memcpy(hbuf + HDBFSIZOFF, &llnum, sizeof (llnum));
    llnum = hdb->frec;
    llnum = TCHTOILL(llnum);
    memcpy(hbuf + HDBFRECOFF, &llnum, sizeof (llnum));
    HDBUNLOCKDB(hdb);
    return true;
}

/* Deserialize meta data from a buffer.
   `hdb' specifies the hash database object.
   `hbuf' specifies the buffer. */
static bool tchdbloadmeta(TCHDB *hdb, const char *hbuf) {
    if (!HDBLOCKDB(hdb)) return false;
    memcpy(&(hdb->type), hbuf + HDBTYPEOFF, sizeof (hdb->type));
    memcpy(&(hdb->flags), hbuf + HDBFLAGSOFF, sizeof (hdb->flags));
    memcpy(&(hdb->apow), hbuf + HDBAPOWOFF, sizeof (hdb->apow));
    memcpy(&(hdb->fpow), hbuf + HDBFPOWOFF, sizeof (hdb->fpow));
    memcpy(&(hdb->opts), hbuf + HDBOPTSOFF, sizeof (hdb->opts));
    uint64_t llnum;
    memcpy(&llnum, hbuf + HDBBNUMOFF, sizeof (llnum));
    hdb->bnum = TCITOHLL(llnum);
    memcpy(&llnum, hbuf + HDBRNUMOFF, sizeof (llnum));
    hdb->rnum = TCITOHLL(llnum);
    memcpy(&llnum, hbuf + HDBFSIZOFF, sizeof (llnum));
    hdb->fsiz = TCITOHLL(llnum);
    memcpy(&llnum, hbuf + HDBFRECOFF, sizeof (llnum));
    hdb->frec = TCITOHLL(llnum);
    HDBUNLOCKDB(hdb);
    return true;
}

/* Clear all members.
   `hdb' specifies the hash database object. */
static void tchdbclear(TCHDB *hdb) {
    assert(hdb);
    hdb->mmtx = NULL;
    hdb->smtx = NULL;
    hdb->rmtxs = NULL;
    hdb->dmtx = NULL;
    hdb->wmtx = NULL;
    hdb->eckey = NULL;
    hdb->rpath = NULL;
    hdb->type = TCDBTHASH;
    hdb->flags = 0;
    hdb->bnum = HDBDEFBNUM;
    hdb->apow = HDBDEFAPOW;
    hdb->fpow = HDBDEFFPOW;
    hdb->opts = 0;
    hdb->path = NULL;
    hdb->fd = INVALID_HANDLE_VALUE;
    hdb->omode = 0;
    hdb->rnum = 0;
    hdb->fsiz = 0;
    hdb->frec = 0;
    hdb->dfcur = 0;
    hdb->iter = 0;
    hdb->iter2list = NULL;
    hdb->map = NULL;
    hdb->msiz = 0;
    hdb->xmsiz = HDBDEFXMSIZ;
    hdb->xfsiz = 0;
    hdb->ba64 = false;
    hdb->align = 0;
    hdb->runit = 0;
    hdb->zmode = false;
    hdb->fbpmax = 0;
    hdb->fbpool = NULL;
    hdb->fbpnum = 0;
    hdb->fbpmis = 0;
    hdb->async = false;
    hdb->drpool = NULL;
    hdb->drpdef = NULL;
    hdb->drpoff = 0;
    hdb->recc = NULL;
    hdb->rcnum = 0;
    hdb->enc = NULL;
    hdb->encop = NULL;
    hdb->dec = NULL;
    hdb->decop = NULL;
    hdb->ecode = TCESUCCESS;
    hdb->fatal = false;
    hdb->inode = 0;
    hdb->mtime = 0;
    hdb->dfunit = 0;
    hdb->dfcnt = 0;
    hdb->tran = false;
    hdb->walfd = INVALID_HANDLE_VALUE;
    hdb->walend = 0;
    hdb->dbgfd = INVALID_HANDLE_VALUE;

#ifndef NDEBUG
    hdb->cnt_writerec = -1;
    hdb->cnt_reuserec = -1;
    hdb->cnt_moverec = -1;
    hdb->cnt_readrec = -1;
    hdb->cnt_searchfbp = -1;
    hdb->cnt_insertfbp = -1;
    hdb->cnt_splicefbp = -1;
    hdb->cnt_dividefbp = -1;
    hdb->cnt_mergefbp = -1;
    hdb->cnt_reducefbp = -1;
    hdb->cnt_appenddrp = -1;
    hdb->cnt_deferdrp = -1;
    hdb->cnt_flushdrp = -1;
    hdb->cnt_adjrecc = -1;
    hdb->cnt_defrag = -1;
    hdb->cnt_shiftrec = -1;
    hdb->cnt_trunc = -1;
#endif

#ifdef _WIN32
    hdb->w32hmap = INVALID_HANDLE_VALUE;
#endif
    TCDODEBUG(hdb->cnt_writerec = 0);
    TCDODEBUG(hdb->cnt_reuserec = 0);
    TCDODEBUG(hdb->cnt_moverec = 0);
    TCDODEBUG(hdb->cnt_readrec = 0);
    TCDODEBUG(hdb->cnt_searchfbp = 0);
    TCDODEBUG(hdb->cnt_insertfbp = 0);
    TCDODEBUG(hdb->cnt_splicefbp = 0);
    TCDODEBUG(hdb->cnt_dividefbp = 0);
    TCDODEBUG(hdb->cnt_mergefbp = 0);
    TCDODEBUG(hdb->cnt_reducefbp = 0);
    TCDODEBUG(hdb->cnt_appenddrp = 0);
    TCDODEBUG(hdb->cnt_deferdrp = 0);
    TCDODEBUG(hdb->cnt_flushdrp = 0);
    TCDODEBUG(hdb->cnt_adjrecc = 0);
    TCDODEBUG(hdb->cnt_defrag = 0);
    TCDODEBUG(hdb->cnt_shiftrec = 0);
    TCDODEBUG(hdb->cnt_trunc = 0);
}

/* Get the padding size to record alignment.
   `hdb' specifies the hash database object.
   `off' specifies the current offset.
   The return value is the padding size. */
static int32_t tchdbpadsize(TCHDB *hdb, uint64_t off) {
    assert(hdb);
    int32_t diff = off & (hdb->align - 1);
    return (diff > 0) ? hdb->align - diff : 0;
}

/* Set the open flag.
   `hdb' specifies the hash database object.
   `flag' specifies the flag value.
   `sign' specifies the sign. */
static bool tchdbsetflag(TCHDB *hdb, int flag, bool sign) {
    bool rv = false;
    assert(hdb);
    if (!HDBLOCKSMEMPTR(hdb, false)) {
        return rv;
    }
    if (!HDBLOCKDB(hdb)) {
        HDBUNLOCKSMEMPTR(hdb);
        return rv;
    }
    if (!hdb->map)
        goto out;

    char *fp = (char*) hdb->map + HDBFLAGSOFF;
    if (sign) {
        *fp |= (uint8_t) flag;
    } else {
        *fp &= ~(uint8_t) flag;
    }
    hdb->flags = *fp;
    rv = true;
out:
    HDBUNLOCKDB(hdb);
    HDBUNLOCKSMEMPTR(hdb);
    return rv;
}

/* Get the bucket index of a record.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `hp' specifies the pointer to the variable into which the second hash value is assigned.
   The return value is the bucket index. */
static uint64_t tchdbbidx(TCHDB *hdb, const char *kbuf, int ksiz, uint8_t *hp) {
    assert(hdb && kbuf && ksiz >= 0 && hp);
    uint64_t idx = 19780211;
    uint32_t hash = 751;
    const char *rp = kbuf + ksiz;
    while (ksiz--) {
        idx = idx * 37 + *(uint8_t *) kbuf++;
        hash = (hash * 31) ^ *(uint8_t *)--rp;
    }
    *hp = hash;
    return idx % hdb->bnum;
}

/* Get the offset of the record of a bucket element.
   `hdb' specifies the hash database object.
   `bidx' specifies the index of the bucket.
   The return value is the offset of the record. */
static off_t tchdbgetbucket(TCHDB *hdb, uint64_t bidx) {
    assert(hdb && bidx >= 0);
    if (!HDBLOCKSMEMPTR(hdb, false)) return -1;
    off_t ret;
    if (hdb->ba64) {
        uint64_t llnum = HDBB64(hdb)[bidx];
        ret = TCITOHLL(llnum) << hdb->apow;
    } else {
        uint32_t lnum = HDBB32(hdb)[bidx];
        ret = (off_t) TCITOHL(lnum) << hdb->apow;
    }
    HDBUNLOCKSMEMPTR(hdb);
    return ret;
}

/* Get the offset of the record of a bucket element.
   `hdb' specifies the hash database object.
   `bidx' specifies the index of the record.
   `off' specifies the offset of the record.
    #METHOD ANY LOCK, ROWS LOCKED */
static void tchdbsetbucket(TCHDB *hdb, uint64_t bidx, uint64_t off) {
    assert(hdb && bidx >= 0);
    if (hdb->ba64) {
        uint64_t llnum = off >> hdb->apow;
        if (hdb->tran) tchdbwalwrite(hdb, HDBHEADSIZ + bidx * sizeof (llnum), sizeof (llnum));
        bool l = HDBLOCKSMEMPTR(hdb, false);
        uint64_t *ba = HDBB64(hdb);
        ba[bidx] = TCHTOILL(llnum);
        if (l) HDBUNLOCKSMEMPTR(hdb);
    } else {
        uint32_t lnum = off >> hdb->apow;
        if (hdb->tran) tchdbwalwrite(hdb, HDBHEADSIZ + bidx * sizeof (lnum), sizeof (lnum));
        bool l = HDBLOCKSMEMPTR(hdb, false);
        uint32_t *ba = HDBB32(hdb);
        ba[bidx] = TCHTOIL(lnum);
        if (l) HDBUNLOCKSMEMPTR(hdb);
    }
}

/* Save the free block pool into the file.
   The return value is true if successful, else, it is false. */
static bool tchdbsavefbp(TCHDB *hdb) {
    assert(hdb);
    bool err = false;
    if (hdb->fbpnum > hdb->fbpmax) {
        tchdbfbpmerge(hdb);
    } else if (hdb->fbpnum > 1) {
        tcfbpsortbyoff(hdb->fbpool, hdb->fbpnum);
    }
    int bsiz = hdb->frec - hdb->msiz;
    char *buf;
    TCMALLOC(buf, bsiz);
    char *wp = buf;
    HDBFB *cur = hdb->fbpool;
    HDBFB *end = cur + hdb->fbpnum;
    uint64_t base = 0;
    bsiz -= sizeof (HDBFB) + sizeof (uint8_t) + sizeof (uint8_t);
    while (cur < end && bsiz > 0) {
        uint64_t noff = cur->off >> hdb->apow;
        int step;
        uint64_t llnum = noff - base;
        TCSETVNUMBUF64(step, wp, llnum);
        wp += step;
        bsiz -= step;
        uint32_t lnum = cur->rsiz >> hdb->apow;
        TCSETVNUMBUF(step, wp, lnum);
        wp += step;
        bsiz -= step;
        base = noff;
        cur++;
    }
    *(wp++) = '\0';
    *(wp++) = '\0';
    if (!tchdbseekwrite(hdb, hdb->msiz, buf, wp - buf)) {
        err = true;
    }
    TCFREE(buf);
    return !err;
}

/* Load the free block pool from the file.
   The return value is true if successful, else, it is false. */
static bool tchdbloadfbp(TCHDB *hdb) {
    int bsiz = hdb->frec - hdb->msiz;
    char *buf;
    TCMALLOC(buf, bsiz);
    if (!tchdbseekread(hdb, hdb->msiz, buf, bsiz)) {
        TCFREE(buf);
        return false;
    }
    const char *rp = buf;
    HDBFB *cur = hdb->fbpool;
    HDBFB *end = cur + hdb->fbpmax * HDBFBPALWRAT;
    uint64_t base = 0;
    while (cur < end && *rp != '\0') {
        int step;
        uint64_t llnum;
        TCREADVNUMBUF64(rp, llnum, step);
        base += llnum << hdb->apow;
        cur->off = base;
        rp += step;
        uint32_t lnum;
        TCREADVNUMBUF(rp, lnum, step);
        cur->rsiz = lnum << hdb->apow;
        rp += step;
        cur++;
    }
    hdb->fbpnum = cur - (HDBFB *) hdb->fbpool;
    TCFREE(buf);
    tcfbpsortbyrsiz(hdb->fbpool, hdb->fbpnum);
    return true;
}

/* Sort the free block pool by offset.
   `fbpool' specifies the free block pool.
   `fbpnum' specifies the number of blocks. */
static void tcfbpsortbyoff(HDBFB *fbpool, int fbpnum) {
    assert(fbpool && fbpnum >= 0);
    fbpnum--;
    int bottom = fbpnum / 2 + 1;
    int top = fbpnum;
    while (bottom > 0) {
        bottom--;
        int mybot = bottom;
        int i = mybot * 2;
        while (i <= top) {
            if (i < top && fbpool[i + 1].off > fbpool[i].off) i++;
            if (fbpool[mybot].off >= fbpool[i].off) break;
            HDBFB swap = fbpool[mybot];
            fbpool[mybot] = fbpool[i];
            fbpool[i] = swap;
            mybot = i;
            i = mybot * 2;
        }
    }
    while (top > 0) {
        HDBFB swap = fbpool[0];
        fbpool[0] = fbpool[top];
        fbpool[top] = swap;
        top--;
        int mybot = bottom;
        int i = mybot * 2;
        while (i <= top) {
            if (i < top && fbpool[i + 1].off > fbpool[i].off) i++;
            if (fbpool[mybot].off >= fbpool[i].off) break;
            swap = fbpool[mybot];
            fbpool[mybot] = fbpool[i];
            fbpool[i] = swap;
            mybot = i;
            i = mybot * 2;
        }
    }
}

/* Sort the free block pool by record size.
   `fbpool' specifies the free block pool.
   `fbpnum' specifies the number of blocks. */
static void tcfbpsortbyrsiz(HDBFB *fbpool, int fbpnum) {
    assert(fbpool && fbpnum >= 0);
    fbpnum--;
    int bottom = fbpnum / 2 + 1;
    int top = fbpnum;
    while (bottom > 0) {
        bottom--;
        int mybot = bottom;
        int i = mybot * 2;
        while (i <= top) {
            if (i < top && fbpool[i + 1].rsiz > fbpool[i].rsiz) i++;
            if (fbpool[mybot].rsiz >= fbpool[i].rsiz) break;
            HDBFB swap = fbpool[mybot];
            fbpool[mybot] = fbpool[i];
            fbpool[i] = swap;
            mybot = i;
            i = mybot * 2;
        }
    }
    while (top > 0) {
        HDBFB swap = fbpool[0];
        fbpool[0] = fbpool[top];
        fbpool[top] = swap;
        top--;
        int mybot = bottom;
        int i = mybot * 2;
        while (i <= top) {
            if (i < top && fbpool[i + 1].rsiz > fbpool[i].rsiz) i++;
            if (fbpool[mybot].rsiz >= fbpool[i].rsiz) break;
            swap = fbpool[mybot];
            fbpool[mybot] = fbpool[i];
            fbpool[i] = swap;
            mybot = i;
            i = mybot * 2;
        }
    }
}

/* Merge successive records in the free block pool.
   `hdb' specifies the hash database object. */
static void tchdbfbpmerge(TCHDB *hdb) {
    assert(hdb);
    TCDODEBUG(hdb->cnt_mergefbp++);
    tcfbpsortbyoff(hdb->fbpool, hdb->fbpnum);
    HDBFB *wp = hdb->fbpool;
    HDBFB *cur = wp;
    HDBFB *end = wp + hdb->fbpnum - 1;
    while (cur < end) {
        if (cur->off > 0) {
            HDBFB *next = cur + 1;
            if (cur->off + cur->rsiz == next->off && cur->rsiz + next->rsiz <= HDBFBMAXSIZ) {
                if (hdb->dfcur == next->off) hdb->dfcur += next->rsiz;
                if (hdb->iter == next->off) hdb->iter += next->rsiz;
                if (hdb->iter2list) {
                    for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
                        TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
                        if ((*pit)->pos == next->off) (*pit)->pos += next->rsiz;
                    }
                }
                cur->rsiz += next->rsiz;
                next->off = 0;
            }
            *(wp++) = *cur;
        }
        cur++;
    }
    if (end->off > 0) *(wp++) = *end;
    hdb->fbpnum = wp - (HDBFB *) hdb->fbpool;
    hdb->fbpmis = hdb->fbpnum * -1;
}

/* Insert a block into the free block pool.
   `hdb' specifies the hash database object.
   `off' specifies the offset of the block.
   `rsiz' specifies the size of the block. */
static void tchdbfbpinsert(TCHDB *hdb, uint64_t off, uint32_t rsiz) {
    assert(hdb && off > 0 && rsiz > 0);
    TCDODEBUG(hdb->cnt_insertfbp++);
    if (hdb->fpow < 1) {
        return;
    }
    __atomic_add_fetch(&hdb->dfcnt, 1, __ATOMIC_RELEASE);
    HDBFB *pv = hdb->fbpool;
    if (hdb->fbpnum >= hdb->fbpmax * HDBFBPALWRAT) {
        tchdbfbpmerge(hdb);
        tcfbpsortbyrsiz(hdb->fbpool, hdb->fbpnum);
        int diff = hdb->fbpnum - hdb->fbpmax;
        if (diff > 0) {
            TCDODEBUG(hdb->cnt_reducefbp++);
            memmove(pv, pv + diff, (hdb->fbpnum - diff) * sizeof (*pv));
            hdb->fbpnum -= diff;
        }
        hdb->fbpmis = 0;
    }
    int num = hdb->fbpnum;
    int left = 0;
    int right = num;
    int i = (left + right) / 2;
    int cand = -1;
    while (right >= left && i < num) {
        int rv = (int) rsiz - (int) pv[i].rsiz;
        if (rv == 0) {
            cand = i;
            break;
        } else if (rv <= 0) {
            cand = i;
            right = i - 1;
        } else {
            left = i + 1;
        }
        i = (left + right) / 2;
    }
    if (cand >= 0) {
        pv += cand;
        memmove(pv + 1, pv, sizeof (*pv) * (num - cand));
    } else {
        pv += num;
    }
    pv->off = off;
    pv->rsiz = rsiz;
    hdb->fbpnum++;
}

/* Search the free block pool for the minimum region.
   `hdb' specifies the hash database object.
   `rec' specifies the record object to be stored.
   The return value is true if successful, else, it is false. 
   #DB LOCK */
static bool tchdbfbpsearch(TCHDB *hdb, TCHREC *rec) {
    assert(hdb && rec);
    TCDODEBUG(hdb->cnt_searchfbp++);
    if (hdb->fbpnum < 1) {
        rec->off = hdb->fsiz;
        rec->rsiz = 0;
        return true;
    }
    uint32_t rsiz = rec->rsiz;
    HDBFB *pv = hdb->fbpool;
    int num = hdb->fbpnum;
    int left = 0;
    int right = num;
    int i = (left + right) / 2;
    int cand = -1;
    while (right >= left && i < num) {
        int rv = (int) rsiz - (int) pv[i].rsiz;
        if (rv == 0) {
            cand = i;
            break;
        } else if (rv <= 0) {
            cand = i;
            right = i - 1;
        } else {
            left = i + 1;
        }
        i = (left + right) / 2;
    }
    if (cand >= 0) {
        pv += cand;
        if (pv->rsiz > rsiz * 2) {
            uint32_t psiz = tchdbpadsize(hdb, pv->off + rsiz);
            uint64_t noff = pv->off + rsiz + psiz;
            if (pv->rsiz >= (noff - pv->off) * 2) {
                TCDODEBUG(hdb->cnt_dividefbp++);
                rec->off = pv->off;
                rec->rsiz = noff - pv->off;
                pv->off = noff;
                pv->rsiz -= rec->rsiz;
                return tchdbwritefb(hdb, pv->off, pv->rsiz);
            }
        }
        rec->off = pv->off;
        rec->rsiz = pv->rsiz;
        memmove(pv, pv + 1, sizeof (*pv) * (num - cand - 1));
        hdb->fbpnum--;
        return true;
    }
    rec->off = hdb->fsiz;
    rec->rsiz = 0;
    hdb->fbpmis++;
    if (hdb->fbpmis >= HDBFBPMGFREQ) {
        tchdbfbpmerge(hdb);
        tcfbpsortbyrsiz(hdb->fbpool, hdb->fbpnum);
    }
    return true;
}

/* Splice the trailing free block
   `hdb' specifies the hash database object.
   `rec' specifies the record object to be stored.
   `nsiz' specifies the needed size.
   The return value is whether splicing succeeded or not. */
static bool tchdbfbpsplice(TCHDB *hdb, TCHREC *rec, uint32_t nsiz) {
    assert(hdb && rec && nsiz > 0);
    if (nsiz > (0x80000000 - hdb->align)) {
        return false;
    }
    if (hdb->mmtx) {
        if (hdb->fbpnum < 1) {
            return false;
        }
        uint64_t off = rec->off + rec->rsiz;
        uint32_t rsiz = rec->rsiz;
        uint8_t magic;
        if (tchdbseekread2(hdb, off, &magic, sizeof (magic), HDBSEEKTRY) && magic != HDBMAGICFB) {
            return false;
        }
        HDBFB *pv = hdb->fbpool;
        HDBFB *ep = pv + hdb->fbpnum;
        while (pv < ep) {
            if (pv->off == off && rsiz + pv->rsiz >= nsiz) {
                if (hdb->dfcur == pv->off) hdb->dfcur += pv->rsiz;
                if (hdb->iter == pv->off) hdb->iter += pv->rsiz;
                if (hdb->iter2list) {
                    for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
                        TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
                        if ((*pit)->pos == pv->off) (*pit)->pos += pv->rsiz;
                    }
                }
                rec->rsiz += pv->rsiz;
                memmove(pv, pv + 1, sizeof (*pv) * ((ep - pv) - 1));
                hdb->fbpnum--;
                return true;
            }
            pv++;
        }
        return false;
    }
    uint64_t off = rec->off + rec->rsiz;
    TCHREC nrec;
    char nbuf[HDBIOBUFSIZ];
    while (off < hdb->fsiz) {
        nrec.off = off;
        if (!tchdbreadrec(hdb, &nrec, nbuf)) return false;
        if (nrec.magic != HDBMAGICFB) break;
        if (hdb->dfcur == off) hdb->dfcur += nrec.rsiz;
        if (hdb->iter == off) hdb->iter += nrec.rsiz;
        if (hdb->iter2list) {
            for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
                TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
                if ((*pit)->pos == nrec.off) (*pit)->pos += nrec.rsiz;
            }
        }
        off += nrec.rsiz;
    }
    uint32_t jsiz = off - rec->off;
    if (jsiz < nsiz) return false;
    rec->rsiz = jsiz;
    uint64_t base = rec->off;
    HDBFB *wp = hdb->fbpool;
    HDBFB *cur = wp;
    HDBFB *end = wp + hdb->fbpnum;
    while (cur < end) {
        if (cur->off < base || cur->off > off) *(wp++) = *cur;
        cur++;
    }
    hdb->fbpnum = wp - (HDBFB *) hdb->fbpool;
    if (jsiz > nsiz * 2) {
        uint32_t psiz = tchdbpadsize(hdb, rec->off + nsiz);
        uint64_t noff = rec->off + nsiz + psiz;
        if (jsiz >= (noff - rec->off) * 2) {
            TCDODEBUG(hdb->cnt_dividefbp++);
            nsiz = off - noff;
            if (!tchdbwritefb(hdb, noff, nsiz)) return false;
            rec->rsiz = noff - rec->off;
            tchdbfbpinsert(hdb, noff, nsiz);
        }
    }
    return true;
}

/* Remove blocks of a region from the free block pool.
   `hdb' specifies the hash database object.
   `base' specifies the base offset of the region.
   `next' specifies the offset of the next region.
   `off' specifies the offset of the block.
   `rsiz' specifies the size of the block. */
static void tchdbfbptrim(TCHDB *hdb, uint64_t base, uint64_t next, uint64_t off, uint32_t rsiz) {
    assert(hdb && base > 0 && next > 0);
    if (hdb->fpow < 1) return;
    if (hdb->fbpnum < 1) {
        if (off > 0) {
            HDBFB *fbpool = hdb->fbpool;
            fbpool->off = off;
            fbpool->rsiz = rsiz;
            hdb->fbpnum = 1;
        }
        return;
    }
    HDBFB *wp = hdb->fbpool;
    HDBFB *cur = wp;
    HDBFB *end = wp + hdb->fbpnum;
    if (hdb->fbpnum >= hdb->fbpmax * HDBFBPALWRAT) cur++;
    while (cur < end) {
        if (cur->rsiz >= rsiz && off > 0) {
            TCDODEBUG(hdb->cnt_insertfbp++);
            wp->off = off;
            wp->rsiz = rsiz;
            wp++;
            off = 0;
        } else if (cur->off < base || cur->off >= next) {
            *(wp++) = *cur;
        }
        cur++;
    }
    if (off > 0) {
        TCDODEBUG(hdb->cnt_insertfbp++);
        wp->off = off;
        wp->rsiz = rsiz;
        wp++;
        off = 0;
    }
    hdb->fbpnum = wp - (HDBFB *) hdb->fbpool;
}

/* Write a free block into the file.
   `hdb' specifies the hash database object.
   `off' specifies the offset of the block.
   `rsiz' specifies the size of the block.
   The return value is true if successful, else, it is false. */
static bool tchdbwritefb(TCHDB *hdb, uint64_t off, uint32_t rsiz) {
    assert(hdb && off > 0 && rsiz > 0);
    char rbuf[HDBMAXHSIZ];
    char *wp = rbuf;
    *(uint8_t *) (wp++) = HDBMAGICFB;
    uint32_t lnum = TCHTOIL(rsiz);
    memcpy(wp, &lnum, sizeof (lnum));
    wp += sizeof (lnum);
    return tchdbseekwrite(hdb, off, rbuf, wp - rbuf);
}

/* Write a record into the file.
   `hdb' specifies the hash database object.
   `rec' specifies the record object.
   `bidx' specifies the index of the bucket.
   `entoff' specifies the offset of the tree entry.
   The return value is true if successful, else, it is false. */
static bool tchdbwriterec(TCHDB *hdb, TCHREC *rec, uint64_t bidx, off_t entoff, bool newrec) {
    assert(hdb && rec);
    char stack[HDBIOBUFSIZ];
    TCDODEBUG(hdb->cnt_writerec++);
    int bsiz = 0;
    char *rbuf = NULL;
    bool dblocked = false;
    char *wp;

begining:
    bsiz = (rec->rsiz > 0) ? rec->rsiz : HDBMAXHSIZ + rec->ksiz + rec->vsiz + hdb->align;
    if (bsiz <= HDBIOBUFSIZ) {
        rbuf = stack;
    } else {
        TCMALLOC(rbuf, bsiz);
    }
    wp = rbuf;
    *(uint8_t *) (wp++) = HDBMAGICREC;
    *(uint8_t *) (wp++) = rec->hash;
    if (hdb->ba64) {
        uint64_t llnum;
        llnum = rec->left >> hdb->apow;
        llnum = TCHTOILL(llnum);
        memcpy(wp, &llnum, sizeof (llnum));
        wp += sizeof (llnum);
        llnum = rec->right >> hdb->apow;
        llnum = TCHTOILL(llnum);
        memcpy(wp, &llnum, sizeof (llnum));
        wp += sizeof (llnum);
    } else {
        uint32_t lnum;
        lnum = rec->left >> hdb->apow;
        lnum = TCHTOIL(lnum);
        memcpy(wp, &lnum, sizeof (lnum));
        wp += sizeof (lnum);
        lnum = rec->right >> hdb->apow;
        lnum = TCHTOIL(lnum);
        memcpy(wp, &lnum, sizeof (lnum));
        wp += sizeof (lnum);
    }
    uint16_t snum;
    char *pwp = wp;
    wp += sizeof (snum);
    int step;
    TCSETVNUMBUF(step, wp, rec->ksiz);
    wp += step;
    TCSETVNUMBUF(step, wp, rec->vsiz);
    wp += step;
    int32_t hsiz = wp - rbuf;
    int32_t rsiz = hsiz + rec->ksiz + rec->vsiz;
    int32_t finc = 0;
    if (rec->rsiz < 1) {
        if (!dblocked) {
            if (!HDBLOCKDB(hdb)) return false;
            dblocked = true;
        }
        uint16_t psiz = tchdbpadsize(hdb, hdb->fsiz + rsiz);
        rec->rsiz = rsiz + psiz;
        rec->psiz = psiz;
        rec->off = hdb->fsiz;
        finc = rec->rsiz;
    } else if (rsiz > rec->rsiz) {
        if (rbuf != stack) TCFREE(rbuf);
        if (!dblocked) {
            if (!HDBLOCKDB(hdb)) return false;
            dblocked = true;
        }
        if (tchdbfbpsplice(hdb, rec, rsiz)) {
            TCDODEBUG(hdb->cnt_splicefbp++);
            goto begining;
        }
        TCDODEBUG(hdb->cnt_moverec++);
        if (!tchdbwritefb(hdb, rec->off, rec->rsiz)) {
            if (dblocked) HDBUNLOCKDB(hdb);
            return false;
        }
        tchdbfbpinsert(hdb, rec->off, rec->rsiz);
        rec->rsiz = rsiz;
        if (!tchdbfbpsearch(hdb, rec)) {
            if (dblocked) HDBUNLOCKDB(hdb);
            return false;
        }
        goto begining;
    } else {
        TCDODEBUG(hdb->cnt_reuserec++);
        uint32_t psiz = rec->rsiz - rsiz;
        if (psiz > UINT16_MAX) {
            TCDODEBUG(hdb->cnt_dividefbp++);
            psiz = tchdbpadsize(hdb, rec->off + rsiz);
            uint64_t noff = rec->off + rsiz + psiz;
            uint32_t nsiz = rec->rsiz - rsiz - psiz;
            rec->rsiz = noff - rec->off;
            rec->psiz = psiz;
            if (!dblocked) {
                if (!HDBLOCKDB(hdb)) return false;
                dblocked = true;
            }
            if (!tchdbwritefb(hdb, noff, nsiz)) {
                if (rbuf != stack) TCFREE(rbuf);
                if (dblocked) HDBUNLOCKDB(hdb);
                return false;
            }
            tchdbfbpinsert(hdb, noff, nsiz);
        }
        rec->psiz = psiz;
    }
    snum = rec->psiz;
    snum = TCHTOIS(snum);
    memcpy(pwp, &snum, sizeof (snum));
    rsiz = rec->rsiz;
    rsiz -= hsiz;
    memcpy(wp, rec->kbuf, rec->ksiz);
    wp += rec->ksiz;
    rsiz -= rec->ksiz;
    memcpy(wp, rec->vbuf, rec->vsiz);
    wp += rec->vsiz;
    rsiz -= rec->vsiz;
    memset(wp, 0, rsiz);
    if (!tchdbseekwrite(hdb, rec->off, rbuf, rec->rsiz)) {
        if (rbuf != stack) TCFREE(rbuf);
        if (dblocked) HDBUNLOCKDB(hdb);
        return false;
    }
    if (rbuf != stack) TCFREE(rbuf);
    if (finc != 0) { // HDBLOCKDB applied if true
        hdb->fsiz += finc;
        uint64_t llnum = hdb->fsiz;
        llnum = TCHTOILL(llnum);
        memcpy((void *) (hdb->map + HDBFSIZOFF), &llnum, sizeof (llnum));
    }
    if (newrec) {
        if (!dblocked && HDBLOCKDB(hdb)) {
            dblocked = true;
        }
        uint64_t llnum = ++(hdb->rnum);
        llnum = TCHTOILL(llnum);
        if (HDBLOCKSMEMPTR(hdb, false)) {
            memcpy((void *) (hdb->map + HDBRNUMOFF), &llnum, sizeof (llnum));
            HDBUNLOCKSMEMPTR(hdb);
        }
    }
    if (dblocked) HDBUNLOCKDB(hdb);
    if (entoff > 0) {
        if (hdb->ba64) {
            uint64_t llnum = rec->off >> hdb->apow;
            llnum = TCHTOILL(llnum);
            if (!tchdbseekwrite(hdb, entoff, &llnum, sizeof (uint64_t))) return false;
        } else {
            uint32_t lnum = rec->off >> hdb->apow;
            lnum = TCHTOIL(lnum);
            if (!tchdbseekwrite(hdb, entoff, &lnum, sizeof (uint32_t))) return false;
        }
    } else {
        tchdbsetbucket(hdb, bidx, rec->off);
    }
    return true;
}

/* Read a record from the file.
   `hdb' specifies the hash database object.
   `rec' specifies the record object.
   `rbuf' specifies the buffer for reading.
   The return value is true if successful, else, it is false.
   #METHOD RLOCK + BNUM WLOCK */
static bool tchdbreadrec(TCHDB *hdb, TCHREC *rec, char *rbuf) {
    assert(hdb && rec && rbuf);
    TCDODEBUG(hdb->cnt_readrec++);
    int rsiz = hdb->runit;
    if (!HDBLOCKSMEMPTR(hdb, false)) return false;
    if (!tchdbseekread2(hdb, rec->off, rbuf, rsiz, HDBOPTNOSMLOCK | HDBSEEKTRY)) {
        if (!HDBLOCKDB(hdb)) {
            HDBUNLOCKSMEMPTR(hdb);
            return false;
        }
        uint64_t fsiz = hdb->fsiz;
        rsiz = fsiz - rec->off;
        if (rsiz > hdb->runit) {
            rsiz = hdb->runit;
        } else if (rsiz < (int) (sizeof (uint8_t) + sizeof (uint32_t))) {
            tchdbsetecode(hdb, TCERHEAD, __FILE__, __LINE__, __func__);
            HDBUNLOCKDB(hdb);
            HDBUNLOCKSMEMPTR(hdb);
            return false;
        }
        if (!tchdbseekread2(hdb, rec->off, rbuf, rsiz, HDBOPTNOSMLOCK)) {
            HDBUNLOCKDB(hdb);
            HDBUNLOCKSMEMPTR(hdb);
            return false;
        }
        HDBUNLOCKDB(hdb);
    }
    HDBUNLOCKSMEMPTR(hdb);

    const char *rp = rbuf;
    rec->magic = *(uint8_t *) (rp++);
    if (rec->magic == HDBMAGICFB) {
        uint32_t lnum;
        memcpy(&lnum, rp, sizeof (lnum));
        rec->rsiz = TCITOHL(lnum);
		rec->ksiz = rec->vsiz = 0;
		rec->left = rec->right = 0;
		rec->vbuf = rec->kbuf = NULL;
		rec->boff = 0;
		rec->hash = 0;
        return true;
    } else if (rec->magic != HDBMAGICREC) {
        tchdbsetecode(hdb, TCERHEAD, __FILE__, __LINE__, __func__);
        return false;
    }
    rec->hash = *(uint8_t *) (rp++);
    if (hdb->ba64) {
        uint64_t llnum;
        memcpy(&llnum, rp, sizeof (llnum));
        rec->left = TCITOHLL(llnum) << hdb->apow;
        rp += sizeof (llnum);
        memcpy(&llnum, rp, sizeof (llnum));
        rec->right = TCITOHLL(llnum) << hdb->apow;
        rp += sizeof (llnum);
    } else {
        uint32_t lnum;
        memcpy(&lnum, rp, sizeof (lnum));
        rec->left = (uint64_t) TCITOHL(lnum) << hdb->apow;
        rp += sizeof (lnum);
        memcpy(&lnum, rp, sizeof (lnum));
        rec->right = (uint64_t) TCITOHL(lnum) << hdb->apow;
        rp += sizeof (lnum);
    }
    uint16_t snum;
    memcpy(&snum, rp, sizeof (snum));
    rec->psiz = TCITOHS(snum);
    rp += sizeof (snum);
    uint32_t lnum;
    int step;
    TCREADVNUMBUF(rp, lnum, step);
    rec->ksiz = lnum;
    rp += step;
    TCREADVNUMBUF(rp, lnum, step);
    rec->vsiz = lnum;
    rp += step;
    int32_t hsiz = rp - rbuf;
    rec->rsiz = hsiz + rec->ksiz + rec->vsiz + rec->psiz;
    rec->kbuf = NULL;
    rec->vbuf = NULL;
    rec->boff = rec->off + hsiz;
    rec->bbuf = NULL;
    rsiz -= hsiz;
    if (rsiz >= rec->ksiz) {
        rec->kbuf = rp;
        rsiz -= rec->ksiz;
        rp += rec->ksiz;
        if (rsiz >= rec->vsiz) rec->vbuf = rp;
    }
    return true;
}

/* Read the body of a record from the file.
   `hdb' specifies the hash database object.
   `rec' specifies the record object.
   The return value is true if successful, else, it is false.
   #METHOD RLOCK + BNUM RWLOCK */
static bool tchdbreadrecbody(TCHDB *hdb, TCHREC *rec) {
    assert(hdb && rec);
    int32_t bsiz = rec->ksiz + rec->vsiz;
    TCMALLOC(rec->bbuf, bsiz + 1);
    if (!tchdbseekread(hdb, rec->boff, rec->bbuf, bsiz)) {
        return false;
    }
    rec->kbuf = rec->bbuf;
    rec->vbuf = rec->bbuf + rec->ksiz;
    return true;
}

/* Remove a record from the file.
   `hdb' specifies the hash database object.
   `rec' specifies the record object.
   `rbuf' specifies the buffer for reading.
   `bidx' specifies the index of the bucket.
   `entoff' specifies the offset of the tree entry.
   The return value is true if successful, else, it is false. */
static bool tchdbremoverec(TCHDB *hdb, TCHREC *rec, char *rbuf, uint64_t bidx, off_t entoff) {
    assert(hdb && rec);
    bool err = false;
    if (!tchdbwritefb(hdb, rec->off, rec->rsiz)) {
        return false;
    }
    if (!HDBLOCKDB(hdb)) return false;
    tchdbfbpinsert(hdb, rec->off, rec->rsiz);
    HDBUNLOCKDB(hdb);
    uint64_t child;
    if (rec->left > 0 && rec->right < 1) {
        child = rec->left;
    } else if (rec->left < 1 && rec->right > 0) {
        child = rec->right;
    } else if (rec->left < 1) {
        child = 0;
    } else {
        child = rec->left;
        uint64_t right = rec->right;
        rec->right = child;
        while (rec->right > 0) {
            rec->off = rec->right;
            if (!tchdbreadrec(hdb, rec, rbuf)) return false;
        }
        if (hdb->ba64) {
            off_t toff = rec->off + (sizeof (uint8_t) + sizeof (uint8_t) + sizeof (uint64_t));
            uint64_t llnum = right >> hdb->apow;
            llnum = TCHTOILL(llnum);
            if (!tchdbseekwrite(hdb, toff, &llnum, sizeof (uint64_t))) return false;
        } else {
            off_t toff = rec->off + (sizeof (uint8_t) + sizeof (uint8_t) + sizeof (uint32_t));
            uint32_t lnum = right >> hdb->apow;
            lnum = TCHTOIL(lnum);
            if (!tchdbseekwrite(hdb, toff, &lnum, sizeof (uint32_t))) return false;
        }
    }
    if (entoff > 0) {
        if (hdb->ba64) {
            uint64_t llnum = child >> hdb->apow;
            llnum = TCHTOILL(llnum);
            if (!tchdbseekwrite(hdb, entoff, &llnum, sizeof (uint64_t))) return false;
        } else {
            uint32_t lnum = child >> hdb->apow;
            lnum = TCHTOIL(lnum);
            if (!tchdbseekwrite(hdb, entoff, &lnum, sizeof (uint32_t))) return false;
        }
    } else {
        tchdbsetbucket(hdb, bidx, child);
    }
    if (!HDBLOCKDB(hdb)) return false;
    hdb->rnum--;
    uint64_t llnum = hdb->rnum;
    llnum = TCHTOILL(llnum);
    if (HDBLOCKSMEMPTR(hdb, false)) {
        memcpy((void *) (hdb->map + HDBRNUMOFF), &llnum, sizeof (llnum));
        HDBUNLOCKSMEMPTR(hdb);
    } else {
        err = true;
    }
    HDBUNLOCKDB(hdb);
    return !err;
}

/* Remove a record from the file.
   `hdb' specifies the hash database object.
   `rec' specifies the record object.
   `rbuf' specifies the buffer for reading.
   `destoff' specifies the offset of the destination.
   The return value is true if successful, else, it is false. */
static bool tchdbshiftrec(TCHDB *hdb, TCHREC *rec, char *rbuf, off_t destoff) {
    assert(hdb && rec && rbuf && destoff >= 0);
    TCDODEBUG(hdb->cnt_shiftrec++);
    if (!rec->vbuf && !tchdbreadrecbody(hdb, rec)) {
        return false;
    }
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, rec->kbuf, rec->ksiz, &hash);
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return false;
    if (rec->off == off) {
        bool err = false;
        rec->off = destoff;
        if (!tchdbwriterec(hdb, rec, bidx, 0, false)) err = true;
        TCFREE(rec->bbuf);
        rec->kbuf = NULL;
        rec->vbuf = NULL;
        rec->bbuf = NULL;
        return !err;
    }
    TCHREC trec;
    char tbuf[HDBIOBUFSIZ];
    char *bbuf = rec->bbuf;
    const char *kbuf = rec->kbuf;
    int ksiz = rec->ksiz;
    const char *vbuf = rec->vbuf;
    int vsiz = rec->vsiz;
    rec->kbuf = NULL;
    rec->vbuf = NULL;
    rec->bbuf = NULL;
    off_t entoff = 0;
    while (off > 0) {
        trec.off = off;
        if (!tchdbreadrec(hdb, &trec, tbuf)) {
            TCFREE(bbuf);
            return false;
        }
        if (hash > trec.hash) {
            off = trec.left;
            entoff = trec.off + (sizeof (uint8_t) + sizeof (uint8_t));
        } else if (hash < trec.hash) {
            off = trec.right;
            entoff = trec.off + (sizeof (uint8_t) + sizeof (uint8_t)) +
                    (hdb->ba64 ? sizeof (uint64_t) : sizeof (uint32_t));
        } else {
            if (!trec.kbuf && !tchdbreadrecbody(hdb, &trec)) {
                TCFREE(bbuf);
                return false;
            }
            int kcmp = tcreckeycmp(kbuf, ksiz, trec.kbuf, trec.ksiz);
            if (kcmp > 0) {
                off = trec.left;
                TCFREE(trec.bbuf);
                trec.kbuf = NULL;
                trec.bbuf = NULL;
                entoff = trec.off + (sizeof (uint8_t) + sizeof (uint8_t));
            } else if (kcmp < 0) {
                off = trec.right;
                TCFREE(trec.bbuf);
                trec.kbuf = NULL;
                trec.bbuf = NULL;
                entoff = trec.off + (sizeof (uint8_t) + sizeof (uint8_t)) +
                        (hdb->ba64 ? sizeof (uint64_t) : sizeof (uint32_t));
            } else {
                TCFREE(trec.bbuf);
                trec.bbuf = NULL;
                bool err = false;
                rec->off = destoff;
                rec->kbuf = kbuf;
                rec->ksiz = ksiz;
                rec->vbuf = vbuf;
                rec->vsiz = vsiz;
                if (!tchdbwriterec(hdb, rec, bidx, entoff, false)) err = true;
                TCFREE(bbuf);
                return !err;
            }
        }
    }
    TCFREE(bbuf);
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return false;
}

/* Compare keys of two records.
   `abuf' specifies the pointer to the region of the former.
   `asiz' specifies the size of the region.
   `bbuf' specifies the pointer to the region of the latter.
   `bsiz' specifies the size of the region.
   The return value is 0 if two equals, positive if the formar is big, else, negative. */
static int tcreckeycmp(const char *abuf, int asiz, const char *bbuf, int bsiz) {
    assert(abuf && asiz >= 0 && bbuf && bsiz >= 0);
    if (asiz > bsiz) return 1;
    if (asiz < bsiz) return -1;
    return memcmp(abuf, bbuf, asiz);
}

/* Flush the delayed record pool.
   `hdb' specifies the hash database object.
   The return value is true if successful, else, it is false.
    #METHOD ANY LOCK
 */
static bool tchdbflushdrp(TCHDB *hdb) {
    assert(hdb);
    bool err = false;
    if (!HDBLOCKDB(hdb)) return false;
    if (!hdb->drpool) {
        HDBUNLOCKDB(hdb);
        return true;
    }
    HDBUNLOCKDB(hdb);
    if (!HDBLOCKALLRECORDS(hdb, true)) {
        return false;
    }
    if (!HDBLOCKDB(hdb)) {
        HDBUNLOCKALLRECORDS(hdb);
        return false;
    }
    if (!hdb->drpool) {
        HDBUNLOCKDB(hdb);
        HDBUNLOCKALLRECORDS(hdb);
        return true;
    }
    HDBUNLOCKDB(hdb);
    TCDODEBUG(hdb->cnt_flushdrp++);
    if (!tchdbseekwrite(hdb, hdb->drpoff, TCXSTRPTR(hdb->drpool), TCXSTRSIZE(hdb->drpool))) {
        HDBUNLOCKALLRECORDS(hdb);
        return false;
    }
    const char *rp = TCXSTRPTR(hdb->drpdef);
    int size = TCXSTRSIZE(hdb->drpdef);
    while (size > 0) {
        int ksiz, vsiz;
        memcpy(&ksiz, rp, sizeof (int));
        rp += sizeof (int);
        memcpy(&vsiz, rp, sizeof (int));
        rp += sizeof (int);
        const char *kbuf = rp;
        rp += ksiz;
        const char *vbuf = rp;
        rp += vsiz;
        uint8_t hash;
        uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
        if (!tchdbputimpl(hdb, kbuf, ksiz, bidx, hash, vbuf, vsiz, HDBPDOVER)) {
            tcxstrdel(hdb->drpdef);
            tcxstrdel(hdb->drpool);
            hdb->drpool = NULL;
            hdb->drpdef = NULL;
            hdb->drpoff = 0;
            HDBUNLOCKALLRECORDS(hdb);
            return false;
        }
        size -= sizeof (int) * 2 + ksiz + vsiz;
    }
    tcxstrdel(hdb->drpdef);
    tcxstrdel(hdb->drpool);
    hdb->drpool = NULL;
    hdb->drpdef = NULL;
    hdb->drpoff = 0;
    if (HDBLOCKSMEMPTR(hdb, false)) {
        uint64_t llnum;
        llnum = hdb->rnum;
        llnum = TCHTOILL(llnum);
        memcpy((void *) (hdb->map + HDBRNUMOFF), &llnum, sizeof (llnum));
        llnum = hdb->fsiz;
        llnum = TCHTOILL(llnum);
        memcpy((void *) (hdb->map + HDBFSIZOFF), &llnum, sizeof (llnum));
        HDBUNLOCKSMEMPTR(hdb);
    } else {
        err = true;
    }
    HDBUNLOCKALLRECORDS(hdb);
    return !err;
}

/* Adjust the caches for leaves and nodes.
   `hdb' specifies the hash tree database object. */
static void tchdbcacheadjust(TCHDB *hdb) {
    assert(hdb);
    TCDODEBUG(hdb->cnt_adjrecc++);
    tcmdbcutfront(hdb->recc, HDBCACHEOUT);
}

/* Initialize the write ahead logging file.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false.
   #METHOD WLOCK */
static bool tchdbwalinit(TCHDB *hdb) {
    assert(hdb);
    bool err = false;
    if (!HDBLOCKWAL(hdb)) return false;
    if (!tcfseek(hdb->walfd, 0, TCFSTART)) {
        tchdbsetecode(hdb, TCESEEK, __FILE__, __LINE__, __func__);
        HDBUNLOCKWAL(hdb);
        return false;
    }
    if (!tcftruncate(hdb->walfd, 0)) {
        tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
        HDBUNLOCKWAL(hdb);
        return false;
    }

    uint64_t llnum = hdb->fsiz;
    llnum = TCHTOILL(llnum);
    if (!tcwrite(hdb->walfd, &llnum, sizeof (llnum))) {
        tchdbsetecode(hdb, TCEWRITE, __FILE__, __LINE__, __func__);
        err = true;
    }
    if (!err) hdb->walend = llnum;
    HDBUNLOCKWAL(hdb);
    if (!tchdbwalwrite(hdb, 0, HDBHEADSIZ)) return false;
    return !err;
}

/* Write an event into the write ahead logging file.
   `hdb' specifies the hash database object.
   `off' specifies the offset of the region to be updated.
   `size' specifies the size of the region.
   If successful, the return value is true, else, it is false. */
static bool tchdbwalwrite(TCHDB *hdb, uint64_t off, int64_t size) {
    assert(hdb && off >= 0 && size >= 0);
    if (off + size > hdb->walend) size = hdb->walend - off;
    if (size < 1) return true;
    char stack[HDBIOBUFSIZ];
    char *buf;
    if (size + sizeof (off) + sizeof (size) <= HDBIOBUFSIZ) {
        buf = stack;
    } else {
        TCMALLOC(buf, size + sizeof (off) + sizeof (size));
    }
    char *wp = buf;
    uint64_t llnum = TCHTOILL(off);
    memcpy(wp, &llnum, sizeof (llnum));
    wp += sizeof (llnum);
    uint32_t lnum = TCHTOIL(size);
    memcpy(wp, &lnum, sizeof (lnum));
    wp += sizeof (lnum);
    if (!tchdbseekread(hdb, off, wp, size)) {
        if (buf != stack) TCFREE(buf);
        return false;
    }
    wp += size;
    if (!HDBLOCKWAL(hdb)) {
        if (buf != stack) TCFREE(buf);
        return false;
    }
    if (!tcwrite(hdb->walfd, buf, wp - buf)) {
        tchdbsetecode(hdb, TCEWRITE, __FILE__, __LINE__, __func__);
        if (buf != stack) TCFREE(buf);
        HDBUNLOCKWAL(hdb);
        return false;
    }
    if (buf != stack) TCFREE(buf);
    if ((hdb->omode & HDBOTSYNC) && fsync(hdb->walfd)) {
        tchdbsetecode(hdb, TCESYNC, __FILE__, __LINE__, __func__);
        HDBUNLOCKWAL(hdb);
        return false;
    }
    HDBUNLOCKWAL(hdb);
    return true;
}

/* Restore the database from the write ahead logging file.
   `hdb' specifies the hash database object.
   `path' specifies the path of the database file.
   If successful, the return value is true, else, it is false.
   #METHOD WLOCK  */
static bool tchdbwalrestore(TCHDB *hdb, const char *path) {
    assert(hdb && path);
    bool err = false;
    uint64_t walsiz = 0;
    HANDLE walfd;
    if (!HDBLOCKWAL(hdb)) return false;
    char *tpath = tcsprintf("%s%c%s", path, MYEXTCHR, HDBWALSUFFIX);
#ifndef _WIN32
    walfd = open(tpath, O_RDONLY, HDBFILEMODE);
#else
    walfd = CreateFile(tpath, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
    TCFREE(tpath);
    if (INVALIDHANDLE(walfd)) {
        HDBUNLOCKWAL(hdb);
        err = true;
        goto finish;
    }
    struct stat sbuf;
    if (!fstat(walfd, &sbuf)) {
        walsiz = sbuf.st_size;
    }
    if (!(hdb->omode & HDBOWRITER) || walsiz < sizeof (walsiz) + HDBHEADSIZ) {
        HDBUNLOCKWAL(hdb);
        err = true;
        goto finish;
    }
    uint64_t fsiz = 0;
    if (tcread(walfd, &fsiz, sizeof (fsiz))) {
        fsiz = TCITOHLL(fsiz);
    } else {
        HDBUNLOCKWAL(hdb);
        tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
        err = true;
        goto finish;
    }
    TCLIST *list = tclistnew();
    uint64_t waloff = sizeof (fsiz);
    char stack[HDBIOBUFSIZ];
    while (waloff < walsiz) {
        uint64_t off;
        uint32_t size;
        if (!tcread(walfd, stack, sizeof (off) + sizeof (size))) {
            tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
            err = true;
            break;
        }
        memcpy(&off, stack, sizeof (off));
        off = TCITOHLL(off);
        memcpy(&size, stack + sizeof (off), sizeof (size));
        size = TCITOHL(size);
        char *buf;
        if (sizeof (off) + size <= HDBIOBUFSIZ) {
            buf = stack;
        } else {
            TCMALLOC(buf, sizeof (off) + size);
        }
        *(uint64_t *) buf = off;
        if (!tcread(walfd, buf + sizeof (off), size)) {
            tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
            err = true;
            if (buf != stack) TCFREE(buf);
            break;
        }
        TCLISTPUSH(list, buf, sizeof (off) + size);
        if (buf != stack) TCFREE(buf);
        waloff += sizeof (off) + sizeof (size) + size;
    }
    if (!CLOSEFH(walfd)) {
        tchdbsetecode(hdb, TCECLOSE, __FILE__, __LINE__, __func__);
        err = true;
    } else {
        walfd = INVALID_HANDLE_VALUE;
    }
    HDBUNLOCKWAL(hdb);

    char hbuf[HDBHEADSIZ];
    for (int i = TCLISTNUM(list) - 1; i >= 0; i--) {
        const char *rec;
        int size;
        TCLISTVAL(rec, list, i, size);
        uint64_t off = *(uint64_t *) rec;
        rec += sizeof (off);
        size -= sizeof (off);
        if (!tchdbseekwrite2(hdb, off, rec, size, HDBWRITENOWALL)) {
            err = true;
            break;
        }
    }
    tclistdel(list);
    if (!err && tchdbseekread2(hdb, 0, hbuf, HDBHEADSIZ, 0)) {
        tchdbloadmeta(hdb, hbuf);
    } else {
        err = true;
    }
    if (!tchdbmemsync(hdb, (hdb->omode & HDBOTSYNC))) {
        tchdbsetecode(hdb, TCESYNC, __FILE__, __LINE__, __func__);
        err = true;
    }
finish:
    if (!INVALIDHANDLE(walfd) && !CLOSEFH(walfd)) {
        err = true;
    }
    return !err;
}

/* Remove the write ahead logging file.
   `hdb' specifies the hash database object.
   `path' specifies the path of the database file.
   If successful, the return value is true, else, it is false.
   #METHOD WLOCK */
static bool tchdbwalremove(TCHDB *hdb, const char *path) {
    assert(hdb && path);
    if (!HDBLOCKWAL(hdb)) return false;
    char *tpath = tcsprintf("%s%c%s", path, MYEXTCHR, HDBWALSUFFIX);
    bool err = false;
    if (!tcunlinkfile(tpath) && errno != ENOENT) {
        tchdbsetecode(hdb, TCEUNLINK, __FILE__, __LINE__, __func__);
        err = true;
    }
    HDBUNLOCKWAL(hdb);
    TCFREE(tpath);
    return !err;
}

/* Open a database file and connect a hash database object.
   `hdb' specifies the hash database object.
   `path' specifies the path of the database file.
   `omode' specifies the connection mode.
   If successful, the return value is true, else, it is false.
   #METHOD WLOCK */
static bool tchdbopenimpl(TCHDB *hdb, const char *path, int omode) {
    assert(hdb && path);
    HANDLE fd;
#ifndef _WIN32
    int mode = O_RDONLY;
    if (omode & HDBOWRITER) {
        mode = O_RDWR;
        if (omode & HDBOCREAT) mode |= O_CREAT;
    }
    fd = open(path, mode, HDBFILEMODE);
#else
    DWORD mode, cmode;
    mode = GENERIC_READ;
    cmode = OPEN_EXISTING;
    if (omode & HDBOWRITER) {
        mode |= GENERIC_WRITE;
        if (omode & HDBOTRUNC) {
            cmode = CREATE_ALWAYS;
        } else if (omode & HDBOCREAT) {
            cmode = OPEN_ALWAYS;
        }
    }
    fd = CreateFile(path, mode,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, cmode, FILE_ATTRIBUTE_NORMAL, NULL);
#endif
    if (INVALIDHANDLE(fd)) {
        tchdbsetecode(hdb, tcfilerrno2tcerr(TCEOPEN), __FILE__, __LINE__, __func__);
        return false;
    }
    hdb->omode = omode;
    hdb->fd = fd;

    if (!(omode & HDBONOLCK)) {
        if (!tclock(fd, omode & HDBOWRITER, omode & HDBOLCKNB)) {
            tchdbsetecode(hdb, TCELOCK, __FILE__, __LINE__, __func__);
            CLOSEFH2(hdb->fd);
            return false;
        }
    }
    if ((omode & HDBOWRITER) && (omode & HDBOTRUNC)) {
        if (!tchdbftruncate2(hdb, 0, HDBTRALLOWSHRINK)) {
            tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
            CLOSEFH2(hdb->fd);
            return false;
        }
        if (!tchdbwalremove(hdb, path)) {
            CLOSEFH2(hdb->fd);
            return false;
        }
    }
    struct stat sbuf;
    if (fstat(fd, &sbuf) || !S_ISREG(sbuf.st_mode)) {
        tchdbsetecode(hdb, TCESTAT, __FILE__, __LINE__, __func__);
        CLOSEFH2(hdb->fd);
        return false;
    }
    char hbuf[HDBHEADSIZ];
    if ((omode & HDBOWRITER) && (sbuf.st_size < 1 || (omode & HDBOTRUNC))) {
        hdb->flags = 0;
        hdb->rnum = 0;
        uint32_t fbpmax = 1 << hdb->fpow;
        uint32_t fbpsiz = HDBFBPBSIZ + fbpmax * HDBFBPESIZ;
        int besiz = (hdb->opts & HDBTLARGE) ? sizeof (int64_t) : sizeof (int32_t);
        hdb->align = 1 << hdb->apow;
        hdb->fsiz = HDBHEADSIZ + besiz * hdb->bnum + fbpsiz;
        hdb->fsiz += tchdbpadsize(hdb, hdb->fsiz);
        hdb->frec = hdb->fsiz;
        tchdbdumpmeta(hdb, hbuf);
        bool err = false;
        if (!tcwrite(fd, hbuf, HDBHEADSIZ)) err = true;
        char pbuf[HDBIOBUFSIZ];
        memset(pbuf, 0, HDBIOBUFSIZ);
        uint64_t psiz = hdb->fsiz - HDBHEADSIZ;
        while (psiz > 0) {
            if (psiz > HDBIOBUFSIZ) {
                if (!tcwrite(fd, pbuf, HDBIOBUFSIZ)) err = true;
                psiz -= HDBIOBUFSIZ;
            } else {
                if (!tcwrite(fd, pbuf, psiz)) err = true;
                psiz = 0;
            }
        }
        if (err) {
            tchdbsetecode(hdb, TCEWRITE, __FILE__, __LINE__, __func__);
            CLOSEFH2(hdb->fd);
            return false;
        }
        sbuf.st_size = hdb->fsiz;
    }
    if (!tcfseek(fd, 0, TCFSTART)) {
        tchdbsetecode(hdb, TCESEEK, __FILE__, __LINE__, __func__);
        CLOSEFH2(hdb->fd);
        return false;
    }
    if (!tcread(fd, hbuf, HDBHEADSIZ)) {
        tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
        CLOSEFH2(hdb->fd);
        return false;
    }
    int type = hdb->type;
    tchdbloadmeta(hdb, hbuf);
	int oflags = hdb->flags;
    if (oflags & HDBFOPEN) { /* DB was not closed properly */ 
		if (tchdbwalrestore(hdb, path)) {
			if (!tcfseek(fd, 0, TCFSTART)) {
				tchdbsetecode(hdb, TCESEEK, __FILE__, __LINE__, __func__);
				CLOSEFH2(hdb->fd);
				return false;
			}
			if (!tcread(fd, hbuf, HDBHEADSIZ)) {
				tchdbsetecode(hdb, TCEREAD, __FILE__, __LINE__, __func__);
				CLOSEFH2(hdb->fd);
				return false;
			}
			tchdbloadmeta(hdb, hbuf);
			if (!tchdbwalremove(hdb, path)) {
				CLOSEFH2(hdb->fd);
				return false;
			}
		} 
	}
    int besiz = (hdb->opts & HDBTLARGE) ? sizeof (int64_t) : sizeof (int32_t);
    size_t msiz = HDBHEADSIZ + hdb->bnum * besiz;

    if (memcmp(hbuf, HDBMAGICDATA, strlen(HDBMAGICDATA)) || hdb->type != type ||
            hdb->frec < msiz + HDBFBPBSIZ || hdb->frec > hdb->fsiz || sbuf.st_size < hdb->fsiz) {
        tchdbsetecode(hdb, TCEMETA, __FILE__, __LINE__, __func__);
        CLOSEFH2(hdb->fd);
        return false;
    }
    
    if (((hdb->opts & HDBTDEFLATE) && !_tc_deflate) ||
            ((hdb->opts & HDBTBZIP) && !_tc_bzcompress) ||
            ((hdb->opts & HDBTEXCODEC) && !hdb->enc)) {
        tchdbsetecode(hdb, TCEICOMPRESS, __FILE__, __LINE__, __func__);
        CLOSEFH2(hdb->fd);
        return false;
    }
#ifndef _WIN32
    size_t xmsiz = (hdb->xmsiz > msiz) ? hdb->xmsiz : msiz;
    if (!(omode & HDBOWRITER) && xmsiz > hdb->fsiz) xmsiz = hdb->fsiz;
    void *map = mmap(0, xmsiz, PROT_READ | ((omode & HDBOWRITER) ? PROT_WRITE : 0),
            MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
        CLOSEFH2(hdb->fd);
        return false;
    }
    hdb->map = map;
    hdb->xfsiz = sbuf.st_size;
#else
	if (!tchdbftruncate2(hdb,
						(oflags & HDBFOPEN) /* db was not closed properly */ ? hdb->fsiz : sbuf.st_size,
						(oflags & HDBFOPEN) ? HDBTRALLOWSHRINK : 0)) {
		tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
		CLOSEFH2(hdb->fd);
		return false;
	}
#endif
    hdb->fbpmax = 1 << hdb->fpow;
    if (omode & HDBOWRITER) {
        TCMALLOC(hdb->fbpool, hdb->fbpmax * HDBFBPALWRAT * sizeof (HDBFB));
    } else {
        hdb->fbpool = NULL;
    }
    hdb->fbpnum = 0;
    hdb->fbpmis = 0;
    hdb->async = false;
    hdb->drpool = NULL;
    hdb->drpdef = NULL;
    hdb->drpoff = 0;
    hdb->recc = (hdb->rcnum > 0) ? tcmdbnew2(hdb->rcnum * 2 + 1) : NULL;
    hdb->path = tcstrdup(path);
    hdb->dfcur = hdb->frec;
    hdb->iter = 0;
    if (hdb->iter2list) {
        for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
            TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
            assert(pit && *pit);
            TCFREE(*pit);
        }
        tclistdel(hdb->iter2list);
        hdb->iter2list = NULL;
    }
    hdb->ba64 = (hdb->opts & HDBTLARGE);
    hdb->msiz = msiz;
    hdb->align = 1 << hdb->apow;
    hdb->runit = tclmin(tclmax(hdb->align, HDBMINRUNIT), HDBIOBUFSIZ);
    hdb->zmode = (hdb->opts & HDBTDEFLATE) || (hdb->opts & HDBTBZIP) ||
            (hdb->opts & HDBTTCBS) || (hdb->opts & HDBTEXCODEC);
    hdb->ecode = TCESUCCESS;
    hdb->fatal = false;
    hdb->inode = (uint64_t) sbuf.st_ino;
    hdb->mtime = sbuf.st_mtime;
    hdb->dfcnt = 0;
    hdb->tran = false;
    hdb->walfd = INVALID_HANDLE_VALUE;
    hdb->walend = 0;
    if (hdb->omode & HDBOWRITER) {
        bool err = false;
        if (!(hdb->flags & HDBFOPEN) && !tchdbloadfbp(hdb)) err = true;
        memset(hbuf, 0, 2);
        if (!tchdbseekwrite(hdb, hdb->msiz, hbuf, 2)) err = true;
        if (err) {
            TCFREE(hdb->path);
            TCFREE(hdb->fbpool);
            if (hdb->map) {
#ifdef _WIN32
                FlushViewOfFile((LPCVOID) hdb->map, 0);
                UnmapViewOfFile((LPCVOID) hdb->map);
                CLOSEFH2(hdb->w32hmap);
#else
                munmap(hdb->map, xmsiz);
#endif
                hdb->map = NULL;
            }
            CLOSEFH2(hdb->fd);
            return false;
        }
        tchdbsetflag(hdb, HDBFOPEN, true);
    }
    return true;
}

/* Close a hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false.
    #METHOD WLOCK */
static bool tchdbcloseimpl(TCHDB *hdb) {
    assert(hdb);
    bool err = false;
    if ((hdb->omode & HDBOWRITER)) {
        if (!tchdbflushdrp(hdb)) err = true;
    }
    if (hdb->tran) {
        if (!tchdbwalrestore(hdb, hdb->path)) err = true;
        hdb->tran = false;
        hdb->fbpnum = 0;
    }
    if (hdb->recc) {
        tcmdbdel(hdb->recc);
        hdb->recc = NULL;
    }
    if ((hdb->omode & HDBOWRITER)) {
        if (!tchdbsavefbp(hdb)) err = true;
        TCFREE(hdb->fbpool);
        tchdbsetflag(hdb, HDBFOPEN, false);
        if (!err && !tchdbftruncate2(hdb, hdb->fsiz, HDBTRALLOWSHRINK)) {
            tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
            err = true;
        }
        if (!tchdbmemsync(hdb, false)) err = true;
    }

#ifndef _WIN32
    size_t xmsiz = (hdb->xmsiz > hdb->msiz) ? hdb->xmsiz : hdb->msiz;
    if (!(hdb->omode & HDBOWRITER) && xmsiz > hdb->fsiz) xmsiz = hdb->fsiz;
    if (!hdb->map || munmap(hdb->map, xmsiz)) {
#else
    if (hdb->map) {
        FlushViewOfFile((LPCVOID) hdb->map, 0);
        UnmapViewOfFile((LPCVOID) hdb->map);
        CloseHandle(hdb->w32hmap);
        hdb->w32hmap = INVALID_HANDLE_VALUE;
    } else {
#endif
        tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
        err = true;
    }
    hdb->map = NULL;
    if (!INVALIDHANDLE(hdb->walfd)) {
        if (!CLOSEFH(hdb->walfd)) {
            tchdbsetecode(hdb, TCECLOSE, __FILE__, __LINE__, __func__);
            err = true;
        }
        if (!hdb->fatal && !tchdbwalremove(hdb, hdb->path)) {
            err = true;
        }
    }
    if (!INVALIDHANDLE(hdb->fd) && !CLOSEFH(hdb->fd)) {
        tchdbsetecode(hdb, TCECLOSE, __FILE__, __LINE__, __func__);
        err = true;
    }
    TCFREE(hdb->path);
    hdb->path = NULL;
    hdb->fd = INVALID_HANDLE_VALUE;
    hdb->walfd = INVALID_HANDLE_VALUE;
    return !err;
}

/* Store a record.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `bidx' specifies the index of the bucket array.
   `hash' specifies the hash value for the collision tree.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   `dmode' specifies behavior when the key overlaps.
   If successful, the return value is true, else, it is false.
   #METHOD RLOCK + BNUM WLOCK */
static bool tchdbputimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash,
        const char *vbuf, int vsiz, int dmode) {
    assert(hdb && kbuf && ksiz >= 0);
    if (hdb->recc) tcmdbout(hdb->recc, kbuf, ksiz);
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return false;
    off_t entoff = 0;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return false;
        if (hash > rec.hash) {
            off = rec.left;
            entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t)); //magic + hash
        } else if (hash < rec.hash) {
            off = rec.right;
            entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t)) + //magic + hash + sizeof(left)
                    (hdb->ba64 ? sizeof (uint64_t) : sizeof (uint32_t));
        } else {
            if (!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return false;
            int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
            if (kcmp > 0) {
                off = rec.left;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
                entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t)); //magic + hash
            } else if (kcmp < 0) {
                off = rec.right;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
                entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t)) + //magic + hash + sizeof(left)
                        (hdb->ba64 ? sizeof (uint64_t) : sizeof (uint32_t));
            } else {
                bool rv;
                int nvsiz;
                char *nvbuf;
                HDBPDPROCOP *procptr;
                switch (dmode) {
                    case HDBPDKEEP:
                        tchdbsetecode(hdb, TCEKEEP, __FILE__, __LINE__, __func__);
                        TCFREE(rec.bbuf);
                        return false;
                    case HDBPDCAT:
                        if (vsiz < 1) {
                            TCFREE(rec.bbuf);
                            return true;
                        }
                        if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) {
                            TCFREE(rec.bbuf);
                            return false;
                        }
                        nvsiz = rec.vsiz + vsiz;
                        if (rec.bbuf) {
                            TCREALLOC(rec.bbuf, rec.bbuf, rec.ksiz + nvsiz);
                            memcpy(rec.bbuf + rec.ksiz + rec.vsiz, vbuf, vsiz);
                            rec.kbuf = rec.bbuf;
                            rec.vbuf = rec.kbuf + rec.ksiz;
                            rec.vsiz = nvsiz;
                        } else {
                            TCMALLOC(rec.bbuf, nvsiz + 1);
                            memcpy(rec.bbuf, rec.vbuf, rec.vsiz);
                            memcpy(rec.bbuf + rec.vsiz, vbuf, vsiz);
                            rec.vbuf = rec.bbuf;
                            rec.vsiz = nvsiz;
                        }
                        rv = tchdbwriterec(hdb, &rec, bidx, entoff, false);
                        TCFREE(rec.bbuf);
                        return rv;
                    case HDBPDADDINT:
                        if (rec.vsiz != sizeof (int)) {
                            tchdbsetecode(hdb, TCEKEEP, __FILE__, __LINE__, __func__);
                            TCFREE(rec.bbuf);
                            return false;
                        }
                        if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) {
                            TCFREE(rec.bbuf);
                            return false;
                        }
                        int lnum;
                        memcpy(&lnum, rec.vbuf, sizeof (lnum));
                        if (*(int *) vbuf == 0) {
                            TCFREE(rec.bbuf);
                            *(int *) vbuf = lnum;
                            return true;
                        }
                        lnum += *(int *) vbuf;
                        rec.vbuf = (char *) &lnum;
                        *(int *) vbuf = lnum;
                        rv = tchdbwriterec(hdb, &rec, bidx, entoff, false);
                        TCFREE(rec.bbuf);
                        return rv;
                    case HDBPDADDDBL:
                        if (rec.vsiz != sizeof (double)) {
                            tchdbsetecode(hdb, TCEKEEP, __FILE__, __LINE__, __func__);
                            TCFREE(rec.bbuf);
                            return false;
                        }
                        if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) {
                            TCFREE(rec.bbuf);
                            return false;
                        }
                        double dnum;
                        memcpy(&dnum, rec.vbuf, sizeof (dnum));
                        if (*(double *) vbuf == 0.0) {
                            TCFREE(rec.bbuf);
                            *(double *) vbuf = dnum;
                            return true;
                        }
                        dnum += *(double *) vbuf;
                        rec.vbuf = (char *) &dnum;
                        *(double *) vbuf = dnum;
                        rv = tchdbwriterec(hdb, &rec, bidx, entoff, false);
                        TCFREE(rec.bbuf);
                        return rv;
                    case HDBPDPROC:
                        if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) {
                            TCFREE(rec.bbuf);
                            return false;
                        }
                        procptr = *(HDBPDPROCOP **) ((char *) kbuf - sizeof (procptr));
                        nvbuf = procptr->proc(rec.vbuf, rec.vsiz, &nvsiz, procptr->op);
                        TCFREE(rec.bbuf);
                        if (nvbuf == (void *) - 1) {
                            return tchdbremoverec(hdb, &rec, rbuf, bidx, entoff);
                        } else if (nvbuf) {
                            rec.kbuf = kbuf;
                            rec.ksiz = ksiz;
                            rec.vbuf = nvbuf;
                            rec.vsiz = nvsiz;
                            rv = tchdbwriterec(hdb, &rec, bidx, entoff, false);
                            TCFREE(nvbuf);
                            return rv;
                        }
                        tchdbsetecode(hdb, TCEKEEP, __FILE__, __LINE__, __func__);
                        return false;
                    default:
                        break;
                }
                TCFREE(rec.bbuf);
                rec.ksiz = ksiz;
                rec.vsiz = vsiz;
                rec.kbuf = kbuf;
                rec.vbuf = vbuf;
                return tchdbwriterec(hdb, &rec, bidx, entoff, false);
            }
        }
    }
    if (!vbuf) {
        tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
        return false;
    }
    rec.rsiz = hdb->ba64 ? 
            sizeof (uint8_t) * 2 + sizeof (uint64_t) * 2 + sizeof (uint16_t) :
            sizeof (uint8_t) * 2 + sizeof (uint32_t) * 2 + sizeof (uint16_t);
    if (ksiz < (1U << 7)) {
        rec.rsiz += 1;
    } else if (ksiz < (1U << 14)) {
        rec.rsiz += 2;
    } else if (ksiz < (1U << 21)) {
        rec.rsiz += 3;
    } else if (ksiz < (1U << 28)) {
        rec.rsiz += 4;
    } else {
        rec.rsiz += 5;
    }
    if (vsiz < (1U << 7)) {
        rec.rsiz += 1;
    } else if (vsiz < (1U << 14)) {
        rec.rsiz += 2;
    } else if (vsiz < (1U << 21)) {
        rec.rsiz += 3;
    } else if (vsiz < (1U << 28)) {
        rec.rsiz += 4;
    } else {
        rec.rsiz += 5;
    }
    
    if (!HDBLOCKDB(hdb)) {
        return false;
    }
    if (!tchdbfbpsearch(hdb, &rec)) {
        HDBUNLOCKDB(hdb);
        return false;
    }
    HDBUNLOCKDB(hdb);

    rec.hash = hash;
    rec.left = 0;
    rec.right = 0;
    rec.ksiz = ksiz;
    rec.vsiz = vsiz;
    rec.psiz = 0;
    rec.kbuf = kbuf;
    rec.vbuf = vbuf;
    return tchdbwriterec(hdb, &rec, bidx, entoff, true);
}

/* Append a record to the delayed record pool.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   `hash' specifies the second hash value.
    #METHOD WLOCK */
static void tchdbdrpappend(TCHDB *hdb, const char *kbuf, int ksiz, const char *vbuf, int vsiz,
        uint8_t hash) {
    assert(hdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    TCDODEBUG(hdb->cnt_appenddrp++);
    char rbuf[HDBIOBUFSIZ];
    char *wp = rbuf;
    *(uint8_t *) (wp++) = HDBMAGICREC;
    *(uint8_t *) (wp++) = hash;
    if (hdb->ba64) {
        memset(wp, 0, sizeof (uint64_t) * 2);
        wp += sizeof (uint64_t) * 2;
    } else {
        memset(wp, 0, sizeof (uint32_t) * 2);
        wp += sizeof (uint32_t) * 2;
    }
    uint16_t snum;
    char *pwp = wp;
    wp += sizeof (snum);
    int step;
    TCSETVNUMBUF(step, wp, ksiz);
    wp += step;
    TCSETVNUMBUF(step, wp, vsiz);
    wp += step;
    int32_t hsiz = wp - rbuf;
    int32_t rsiz = hsiz + ksiz + vsiz;

    uint16_t psiz = tchdbpadsize(hdb, hdb->fsiz + rsiz);
    hdb->fsiz += rsiz + psiz;
    snum = TCHTOIS(psiz);
    memcpy(pwp, &snum, sizeof (snum));
    TCXSTR *drpool = hdb->drpool;
    TCXSTRCAT(drpool, rbuf, hsiz);
    TCXSTRCAT(drpool, kbuf, ksiz);
    TCXSTRCAT(drpool, vbuf, vsiz);
    if (psiz > 0) {
        char pbuf[psiz];
        memset(pbuf, 0, psiz);
        TCXSTRCAT(drpool, pbuf, psiz);
    }
    hdb->rnum++;
}

/* Store a record in asynchronus fashion.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `bidx' specifies the index of the bucket array.
   `hash' specifies the hash value for the collision tree.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false.
   #METHOD WLOCK */
static bool tchdbputasyncimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx,
        uint8_t hash, const char *vbuf, int vsiz) {
    assert(hdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
    if (hdb->recc) tcmdbout(hdb->recc, kbuf, ksiz);
    if (!hdb->drpool) {
        hdb->drpool = tcxstrnew3(HDBDRPUNIT + HDBDRPLAT);
        hdb->drpdef = tcxstrnew3(HDBDRPUNIT);
        hdb->drpoff = hdb->fsiz;
    }
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return false;
    off_t entoff = 0;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        if (off >= hdb->drpoff - hdb->runit) {
            TCDODEBUG(hdb->cnt_deferdrp++);
            TCXSTR *drpdef = hdb->drpdef;
            TCXSTRCAT(drpdef, &ksiz, sizeof (ksiz));
            TCXSTRCAT(drpdef, &vsiz, sizeof (vsiz));
            TCXSTRCAT(drpdef, kbuf, ksiz);
            TCXSTRCAT(drpdef, vbuf, vsiz);
            if (TCXSTRSIZE(hdb->drpdef) > HDBDRPUNIT && !tchdbflushdrp(hdb)) return false;
            return true;
        }
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return false;
        if (hash > rec.hash) {
            off = rec.left;
            entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t));
        } else if (hash < rec.hash) {
            off = rec.right;
            entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t)) +
                    (hdb->ba64 ? sizeof (uint64_t) : sizeof (uint32_t));
        } else {
            TCDODEBUG(hdb->cnt_deferdrp++);
            TCXSTR *drpdef = hdb->drpdef;
            TCXSTRCAT(drpdef, &ksiz, sizeof (ksiz));
            TCXSTRCAT(drpdef, &vsiz, sizeof (vsiz));
            TCXSTRCAT(drpdef, kbuf, ksiz);
            TCXSTRCAT(drpdef, vbuf, vsiz);
            if (TCXSTRSIZE(hdb->drpdef) > HDBDRPUNIT && !tchdbflushdrp(hdb)) return false;
            return true;
        }
    }
    if (entoff > 0) {
        if (hdb->ba64) {
            uint64_t llnum = hdb->fsiz >> hdb->apow;
            llnum = TCHTOILL(llnum);
            if (!tchdbseekwrite(hdb, entoff, &llnum, sizeof (uint64_t))) return false;
        } else {
            uint32_t lnum = hdb->fsiz >> hdb->apow;
            lnum = TCHTOIL(lnum);
            if (!tchdbseekwrite(hdb, entoff, &lnum, sizeof (uint32_t))) return false;
        }
    } else {
        tchdbsetbucket(hdb, bidx, hdb->fsiz);
    }
    tchdbdrpappend(hdb, kbuf, ksiz, vbuf, vsiz, hash);
    if (TCXSTRSIZE(hdb->drpool) > HDBDRPUNIT && !tchdbflushdrp(hdb)) return false;
    return true;
}

/* Remove a record of a hash database object.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `bidx' specifies the index of the bucket array.
   `hash' specifies the hash value for the collision tree.
   If successful, the return value is true, else, it is false.
   #METHOD RLOCK + BNUM WLOCK */
static bool tchdboutimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash) {
    assert(hdb && kbuf && ksiz >= 0);
    if (hdb->recc) tcmdbout(hdb->recc, kbuf, ksiz);
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return false;
    off_t entoff = 0;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return false;
        if (hash > rec.hash) {
            off = rec.left;
            entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t));
        } else if (hash < rec.hash) {
            off = rec.right;
            entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t)) +
                    (hdb->ba64 ? sizeof (uint64_t) : sizeof (uint32_t));
        } else {
            if (!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return false;
            int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
            if (kcmp > 0) {
                off = rec.left;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
                entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t));
            } else if (kcmp < 0) {
                off = rec.right;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
                entoff = rec.off + (sizeof (uint8_t) + sizeof (uint8_t)) +
                        (hdb->ba64 ? sizeof (uint64_t) : sizeof (uint32_t));
            } else {
                TCFREE(rec.bbuf);
                rec.bbuf = NULL;
                return tchdbremoverec(hdb, &rec, rbuf, bidx, entoff);
            }
        }
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return false;
}

/* Retrieve a record in a hash database object.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `bidx' specifies the index of the bucket array.
   `hash' specifies the hash value for the collision tree.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the corresponding
   record.
   #METHOD RLOCK + BNUM RLOCK */
static char *tchdbgetimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash,
        int *sp) {
    assert(hdb && kbuf && ksiz >= 0 && sp);
    if (hdb->recc) {
        int tvsiz;
        char *tvbuf = tcmdbget(hdb->recc, kbuf, ksiz, &tvsiz);
        if (tvbuf) {
            if (*tvbuf == '*') {
                tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
                TCFREE(tvbuf);
                return NULL;
            }
            *sp = tvsiz - 1;
            memmove(tvbuf, tvbuf + 1, tvsiz);
            return tvbuf;
        }
    }
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return NULL;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return NULL;
        if (hash > rec.hash) {
            off = rec.left;
        } else if (hash < rec.hash) {
            off = rec.right;
        } else {
            if (!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return NULL;
            int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
            if (kcmp > 0) {
                off = rec.left;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else if (kcmp < 0) {
                off = rec.right;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else {
                if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) return NULL;
                if (hdb->zmode) {
                    int zsiz;
                    char *zbuf;
                    if (hdb->opts & HDBTDEFLATE) {
                        zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
                    } else if (hdb->opts & HDBTBZIP) {
                        zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
                    } else if (hdb->opts & HDBTTCBS) {
                        zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
                    } else {
                        zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
                    }
                    TCFREE(rec.bbuf);
                    if (!zbuf) {
                        tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                        return NULL;
                    }
                    if (hdb->recc) {
                        if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
                        tcmdbput4(hdb->recc, kbuf, ksiz, "=", 1, zbuf, zsiz);
                    }
                    *sp = zsiz;
                    return zbuf;
                }
                if (hdb->recc) {
                    if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
                    tcmdbput4(hdb->recc, kbuf, ksiz, "=", 1, rec.vbuf, rec.vsiz);
                }
                if (rec.bbuf) {
                    memmove(rec.bbuf, rec.vbuf, rec.vsiz);
                    rec.bbuf[rec.vsiz] = '\0';
                    *sp = rec.vsiz;
                    return rec.bbuf;
                }
                *sp = rec.vsiz;
                char *rv;
                TCMEMDUP(rv, rec.vbuf, rec.vsiz);
                return rv;
            }
        }
    }
    if (hdb->recc) {
        if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
        tcmdbput(hdb->recc, kbuf, ksiz, "*", 1);
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return NULL;
}

/* #METHOD RLOCK + BNUM RLOCK */
static int tchdbgetintoxstrimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash, TCXSTR *xstr) {
    assert(hdb && kbuf && ksiz >= 0 && xstr);
    if (hdb->recc) {
        int tvsiz;
        char *tvbuf = tcmdbget(hdb->recc, kbuf, ksiz, &tvsiz);
        if (tvbuf) {
            if (*tvbuf == '*') {
                tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
                TCFREE(tvbuf);
                return -1;
            }
            tvsiz = tvsiz - 1;
            TCXSTRCAT(xstr, tvbuf + 1, tvsiz);
            TCFREE(tvbuf); //todo
            return tvsiz;
        }
    }
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return -1;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return -1;
        if (hash > rec.hash) {
            off = rec.left;
        } else if (hash < rec.hash) {
            off = rec.right;
        } else {
            if (!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return -1;
            int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
            if (kcmp > 0) {
                off = rec.left;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else if (kcmp < 0) {
                off = rec.right;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else {
                if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) return -1;
                if (hdb->zmode) {
                    int zsiz;
                    char *zbuf;
                    if (hdb->opts & HDBTDEFLATE) {
                        zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
                    } else if (hdb->opts & HDBTBZIP) {
                        zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
                    } else if (hdb->opts & HDBTTCBS) {
                        zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
                    } else {
                        zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
                    }
                    TCFREE(rec.bbuf);
                    if (!zbuf) {
                        tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                        return -1;
                    }
                    if (hdb->recc) {
                        if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
                        tcmdbput4(hdb->recc, kbuf, ksiz, "=", 1, zbuf, zsiz);
                    }
                    TCXSTRCAT(xstr, zbuf, zsiz);
                    TCFREE(zbuf);
                    return zsiz;
                }
                if (hdb->recc) {
                    if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
                    tcmdbput4(hdb->recc, kbuf, ksiz, "=", 1, rec.vbuf, rec.vsiz);
                }
                TCXSTRCAT(xstr, rec.vbuf, rec.vsiz);
                TCFREE(rec.bbuf);
                return rec.vsiz;
            }
        }
    }
    if (hdb->recc) {
        if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
        tcmdbput(hdb->recc, kbuf, ksiz, "*", 1);
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return -1;
}

/* Retrieve a record in a hash database object and write the value into a buffer.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `bidx' specifies the index of the bucket array.
   `hash' specifies the hash value for the collision tree.
   `vbuf' specifies the pointer to the buffer into which the value of the corresponding record is
   written.
   `max' specifies the size of the buffer.
   If successful, the return value is the size of the written data, else, it is -1.
   #METHOD RLOCK + BNUM RLOCK */
static int tchdbgetintobuf(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash,
        char *vbuf, int max) {
    assert(hdb && kbuf && ksiz >= 0 && vbuf && max >= 0);
    if (hdb->recc) {
        int tvsiz;
        char *tvbuf = tcmdbget(hdb->recc, kbuf, ksiz, &tvsiz);
        if (tvbuf) {
            if (*tvbuf == '*') {
                tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
                TCFREE(tvbuf);
                return -1;
            }
            tvsiz = tclmin(tvsiz - 1, max);
            memcpy(vbuf, tvbuf + 1, tvsiz);
            TCFREE(tvbuf);
            return tvsiz;
        }
    }
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return -1;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return -1;
        if (hash > rec.hash) {
            off = rec.left;
        } else if (hash < rec.hash) {
            off = rec.right;
        } else {
            if (!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return -1;
            int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
            if (kcmp > 0) {
                off = rec.left;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else if (kcmp < 0) {
                off = rec.right;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else {
                if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) return -1;
                if (hdb->zmode) {
                    int zsiz;
                    char *zbuf;
                    if (hdb->opts & HDBTDEFLATE) {
                        zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
                    } else if (hdb->opts & HDBTBZIP) {
                        zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
                    } else if (hdb->opts & HDBTTCBS) {
                        zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
                    } else {
                        zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
                    }
                    TCFREE(rec.bbuf);
                    if (!zbuf) {
                        tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                        return -1;
                    }
                    if (hdb->recc) {
                        if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
                        tcmdbput4(hdb->recc, kbuf, ksiz, "=", 1, zbuf, zsiz);
                    }
                    zsiz = tclmin(zsiz, max);
                    memcpy(vbuf, zbuf, zsiz);
                    TCFREE(zbuf);
                    return zsiz;
                }
                if (hdb->recc) {
                    if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
                    tcmdbput4(hdb->recc, kbuf, ksiz, "=", 1, rec.vbuf, rec.vsiz);
                }
                int vsiz = tclmin(rec.vsiz, max);
                memcpy(vbuf, rec.vbuf, vsiz);
                TCFREE(rec.bbuf);
                return vsiz;
            }
        }
    }
    if (hdb->recc) {
        if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
        tcmdbput(hdb->recc, kbuf, ksiz, "*", 1);
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return -1;
}

/* Retrieve the next record of a record in a hash database object.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   `vbp' specifies the pointer to the variable into which the pointer to the value is assigned.
   `vsp' specifies the pointer to the variable into which the size of the value is assigned.
   If successful, the return value is the pointer to the region of the value of the next
   record.
   #METHOD WLOCK */
static char *tchdbgetnextimpl(TCHDB *hdb, const char *kbuf, int ksiz, int *sp,
        const char **vbp, int *vsp) {
    assert(hdb && sp);
    if (!kbuf) {
        uint64_t iter = hdb->frec;
        TCHREC rec;
        char rbuf[HDBIOBUFSIZ];
        while (iter < hdb->fsiz) {
            rec.off = iter;
            if (!tchdbreadrec(hdb, &rec, rbuf)) return NULL;
            iter += rec.rsiz;
            if (rec.magic == HDBMAGICREC) {
                if (vbp) {
                    if (hdb->zmode) {
                        if (!tchdbreadrecbody(hdb, &rec)) return NULL;
                        int zsiz;
                        char *zbuf;
                        if (hdb->opts & HDBTDEFLATE) {
                            zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
                        } else if (hdb->opts & HDBTBZIP) {
                            zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
                        } else if (hdb->opts & HDBTTCBS) {
                            zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
                        } else {
                            zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
                        }
                        if (!zbuf) {
                            tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                            TCFREE(rec.bbuf);
                            return NULL;
                        }
                        char *rv;
                        TCMALLOC(rv, rec.ksiz + zsiz + 1);
                        memcpy(rv, rec.kbuf, rec.ksiz);
                        memcpy(rv + rec.ksiz, zbuf, zsiz);
                        *sp = rec.ksiz;
                        *vbp = rv + rec.ksiz;
                        *vsp = zsiz;
                        TCFREE(zbuf);
                        TCFREE(rec.bbuf);
                        return rv;
                    }
                    if (rec.vbuf) {
                        char *rv;
                        TCMALLOC(rv, rec.ksiz + rec.vsiz + 1);
                        memcpy(rv, rec.kbuf, rec.ksiz);
                        memcpy(rv + rec.ksiz, rec.vbuf, rec.vsiz);
                        *sp = rec.ksiz;
                        *vbp = rv + rec.ksiz;
                        *vsp = rec.vsiz;
                        return rv;
                    }
                    if (!tchdbreadrecbody(hdb, &rec)) return NULL;
                    *sp = rec.ksiz;
                    *vbp = rec.vbuf;
                    *vsp = rec.vsiz;
                    return rec.bbuf;
                }
                if (rec.kbuf) {
                    *sp = rec.ksiz;
                    char *rv;
                    TCMEMDUP(rv, rec.kbuf, rec.ksiz);
                    return rv;
                }
                if (!tchdbreadrecbody(hdb, &rec)) return NULL;
                rec.bbuf[rec.ksiz] = '\0';
                *sp = rec.ksiz;
                return rec.bbuf;
            }
        }
        tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
        return NULL;
    }
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return NULL;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return NULL;
        if (hash > rec.hash) {
            off = rec.left;
        } else if (hash < rec.hash) {
            off = rec.right;
        } else {
            if (!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return NULL;
            int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
            if (kcmp > 0) {
                off = rec.left;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else if (kcmp < 0) {
                off = rec.right;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else {
                uint64_t iter = rec.off + rec.rsiz;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
                while (iter < hdb->fsiz) {
                    rec.off = iter;
                    if (!tchdbreadrec(hdb, &rec, rbuf)) return NULL;
                    iter += rec.rsiz;
                    if (rec.magic == HDBMAGICREC) {
                        if (vbp) {
                            if (hdb->zmode) {
                                if (!tchdbreadrecbody(hdb, &rec)) return NULL;
                                int zsiz;
                                char *zbuf;
                                if (hdb->opts & HDBTDEFLATE) {
                                    zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
                                } else if (hdb->opts & HDBTBZIP) {
                                    zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
                                } else if (hdb->opts & HDBTTCBS) {
                                    zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
                                } else {
                                    zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
                                }
                                if (!zbuf) {
                                    tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                                    TCFREE(rec.bbuf);
                                    return NULL;
                                }
                                char *rv;
                                TCMALLOC(rv, rec.ksiz + zsiz + 1);
                                memcpy(rv, rec.kbuf, rec.ksiz);
                                memcpy(rv + rec.ksiz, zbuf, zsiz);
                                *sp = rec.ksiz;
                                *vbp = rv + rec.ksiz;
                                *vsp = zsiz;
                                TCFREE(zbuf);
                                TCFREE(rec.bbuf);
                                return rv;
                            }
                            if (rec.vbuf) {
                                char *rv;
                                TCMALLOC(rv, rec.ksiz + rec.vsiz + 1);
                                memcpy(rv, rec.kbuf, rec.ksiz);
                                memcpy(rv + rec.ksiz, rec.vbuf, rec.vsiz);
                                *sp = rec.ksiz;
                                *vbp = rv + rec.ksiz;
                                *vsp = rec.vsiz;
                                return rv;
                            }
                            if (!tchdbreadrecbody(hdb, &rec)) return NULL;
                            *sp = rec.ksiz;
                            *vbp = rec.vbuf;
                            *vsp = rec.vsiz;
                            return rec.bbuf;
                        }
                        if (rec.kbuf) {
                            *sp = rec.ksiz;
                            char *rv;
                            TCMEMDUP(rv, rec.kbuf, rec.ksiz);
                            return rv;
                        }
                        if (!tchdbreadrecbody(hdb, &rec)) return NULL;
                        rec.bbuf[rec.ksiz] = '\0';
                        *sp = rec.ksiz;
                        return rec.bbuf;
                    }
                }
                tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
                return NULL;
            }
        }
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return NULL;
}

/* Get the size of the value of a record in a hash database object.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `bidx' specifies the index of the bucket array.
   `hash' specifies the hash value for the collision tree.
   If successful, the return value is the size of the value of the corresponding record, else,
   it is -1.
   #METHOD RLOCK + BNUM RLOCK */
static int tchdbvsizimpl(TCHDB *hdb, const char *kbuf, int ksiz, uint64_t bidx, uint8_t hash) {
    assert(hdb && kbuf && ksiz >= 0);
    if (hdb->recc) {
        int tvsiz;
        char *tvbuf = tcmdbget(hdb->recc, kbuf, ksiz, &tvsiz);
        if (tvbuf) {
            if (*tvbuf == '*') {
                tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
                TCFREE(tvbuf);
                return -1;
            }
            TCFREE(tvbuf);
            return tvsiz - 1;
        }
    }
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return -1;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return -1;
        if (hash > rec.hash) {
            off = rec.left;
        } else if (hash < rec.hash) {
            off = rec.right;
        } else {
            if (!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return -1;
            int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
            if (kcmp > 0) {
                off = rec.left;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else if (kcmp < 0) {
                off = rec.right;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else {
                if (hdb->zmode) {
                    if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) return -1;
                    int zsiz;
                    char *zbuf;
                    if (hdb->opts & HDBTDEFLATE) {
                        zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
                    } else if (hdb->opts & HDBTBZIP) {
                        zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
                    } else if (hdb->opts & HDBTTCBS) {
                        zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
                    } else {
                        zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
                    }
                    TCFREE(rec.bbuf);
                    if (!zbuf) {
                        tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                        return -1;
                    }
                    if (hdb->recc) {
                        if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
                        tcmdbput4(hdb->recc, kbuf, ksiz, "=", 1, zbuf, zsiz);
                    }
                    TCFREE(zbuf);
                    return zsiz;
                }
                if (hdb->recc && rec.vbuf) {
                    if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
                    tcmdbput4(hdb->recc, kbuf, ksiz, "=", 1, rec.vbuf, rec.vsiz);
                }
                TCFREE(rec.bbuf);
                return rec.vsiz;
            }
        }
    }
    if (hdb->recc) {
        if (tcmdbrnum(hdb->recc) >= hdb->rcnum) tchdbcacheadjust(hdb);
        tcmdbput(hdb->recc, kbuf, ksiz, "*", 1);
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return -1;
}

/* Get the next key of the iterator of a hash database object.
   `hdb' specifies the hash database object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the next key, else, it is
   `NULL'.
   #METHOD WLOCK */
static char *tchdbiternextimpl(TCHDB *hdb, int *sp) {
    assert(hdb && sp);
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (hdb->iter < hdb->fsiz) {
        rec.off = hdb->iter;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return NULL;
        hdb->iter += rec.rsiz;
        if (rec.magic == HDBMAGICREC) {
            if (rec.kbuf) {
                *sp = rec.ksiz;
                char *rv;
                TCMEMDUP(rv, rec.kbuf, rec.ksiz);
                return rv;
            }
            if (!tchdbreadrecbody(hdb, &rec)) return NULL;
            rec.bbuf[rec.ksiz] = '\0';
            *sp = rec.ksiz;
            return rec.bbuf;
        }
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return NULL;
}

/* Get the next extensible objects of the iterator of a hash database object.
   #METHOD WLOCK */
static bool tchdbiternextintoxstr(TCHDB *hdb, TCXSTR *kxstr, TCXSTR *vxstr) {
    return tchdbiternextintoxstr2(hdb, &hdb->iter, kxstr, vxstr);
}

/* #METHOD WLOCK  */
static bool tchdbiternextintoxstr2(TCHDB *hdb, uint64_t *iter, TCXSTR *kxstr, TCXSTR *vxstr) {
    assert(hdb && kxstr && vxstr);
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (*iter < hdb->fsiz) {
        rec.off = *iter;
        if (!tchdbreadrec(hdb, &rec, rbuf)) {
            return false;
        }
        *iter = *iter + rec.rsiz;
        if (rec.magic == HDBMAGICREC) {
            if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) {
                return false;
            }
            tcxstrclear(kxstr);
            TCXSTRCAT(kxstr, rec.kbuf, rec.ksiz);
            tcxstrclear(vxstr);
            if (hdb->zmode) {
                int zsiz;
                char *zbuf;
                if (hdb->opts & HDBTDEFLATE) {
                    zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
                } else if (hdb->opts & HDBTBZIP) {
                    zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
                } else if (hdb->opts & HDBTTCBS) {
                    zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
                } else {
                    zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
                }
                if (!zbuf) {
                    tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                    TCFREE(rec.bbuf);
                    return false;
                }
                TCXSTRCAT(vxstr, zbuf, zsiz);
                TCFREE(zbuf);
            } else {
                TCXSTRCAT(vxstr, rec.vbuf, rec.vsiz);
            }
            TCFREE(rec.bbuf);
            return true;
        }
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return false;
}

/* Optimize the file of a hash database object.
   `hdb' specifies the hash database object.
   `bnum' specifies the number of elements of the bucket array.
   `apow' specifies the size of record alignment by power of 2.
   `fpow' specifies the maximum number of elements of the free block pool by power of 2.
   `opts' specifies options by bitwise-or.
   If successful, the return value is true, else, it is false.
   #METHOD WLOCK */
static bool tchdboptimizeimpl(TCHDB *hdb, int64_t bnum, int8_t apow, int8_t fpow, uint8_t opts) {
    assert(hdb);
    bool err = false;
    char *tpath = tcsprintf("%s%ctmp%c%" PRIu64 "", hdb->path, MYEXTCHR, MYEXTCHR, hdb->inode);
    int omode = (hdb->omode & ~HDBOCREAT) & ~HDBOTRUNC;
    TCHDB *thdb = tchdbnew();
    thdb->dbgfd = hdb->dbgfd;
    thdb->enc = hdb->enc;
    thdb->encop = hdb->encop;
    thdb->dec = hdb->dec;
    thdb->decop = hdb->decop;
    if (bnum < 1) {
        bnum = hdb->rnum * 2 + 1;
        if (bnum < HDBDEFBNUM) bnum = HDBDEFBNUM;
    }
    if (apow < 0) apow = hdb->apow;
    if (fpow < 0) fpow = hdb->fpow;
    if (opts == UINT8_MAX) opts = hdb->opts;
    tchdbtune(thdb, bnum, apow, fpow, opts);
    if (!tchdbopen(thdb, tpath, HDBOWRITER | HDBOCREAT | HDBOTRUNC) ||
         tchdbcopyopaque(thdb, hdb, 0, HDBOPAQUESZ) < 0) {
        tchdbdel(thdb);
        TCFREE(tpath);
        return false;
    }

    uint64_t off = hdb->frec;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off < hdb->fsiz) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) {
            err = true;
            break;
        }
        off += rec.rsiz;
        if (rec.magic != HDBMAGICREC) {
            continue;
        }
        if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) {
            if (rec.bbuf) TCFREE(rec.bbuf);
            err = true;
            break;
        }
        if (hdb->zmode) {
            int zsiz;
            char *zbuf;
            if (hdb->opts & HDBTDEFLATE) {
                zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
            } else if (hdb->opts & HDBTBZIP) {
                zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
            } else if (hdb->opts & HDBTTCBS) {
                zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
            } else {
                zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
            }
            if (zbuf) {
                if (!tchdbput(thdb, rec.kbuf, rec.ksiz, zbuf, zsiz)) {
                    tchdbsetecode(hdb, thdb->ecode, __FILE__, __LINE__, __func__);
                    err = true;
                }
                TCFREE(zbuf);
            } else {
                tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                err = true;
            }
        } else {
            if (!tchdbput(thdb, rec.kbuf, rec.ksiz, rec.vbuf, rec.vsiz)) {
                tchdbsetecode(hdb, thdb->ecode, __FILE__, __LINE__, __func__);
                err = true;
            }
        }
        TCFREE(rec.bbuf);
    }
    if (!tchdbclose(thdb)) {
        tchdbsetecode(hdb, thdb->ecode, __FILE__, __LINE__, __func__);
        err = true;
    }
    bool esc = false;
    if (err && (hdb->omode & HDBONOLCK) && !thdb->fatal) {
        err = false;
        esc = true;
    }
    tchdbdel(thdb);
    if (err) {
        TCFREE(tpath);
        return false;
    }
    char *opath = tcstrdup(hdb->path);
    if (!tchdbcloseimpl(hdb)) {
        TCFREE(opath);
        TCFREE(tpath);
        return false;
    }

    if (esc) {
        char *bpath = tcsprintf("%s%cbroken", tpath, MYEXTCHR);
        if (!tcrenamefile(opath, bpath)) {
            tchdbsetecode(hdb, TCEUNLINK, __FILE__, __LINE__, __func__);
            err = true;
        }
        TCFREE(bpath);
    } else {
        if (!tcunlinkfile(opath)) {
            tchdbsetecode(hdb, TCEUNLINK, __FILE__, __LINE__, __func__);
            err = true;
        }
    }

    if (!tcrenamefile(tpath, opath)) {
        tchdbsetecode(hdb, TCERENAME, __FILE__, __LINE__, __func__);
        err = true;
    }
    TCFREE(tpath);
    if (err) {
        TCFREE(opath);
        return false;
    }
    bool rv = tchdbopenimpl(hdb, opath, omode);
    TCFREE(opath);
    return rv;
}

/* Remove all records of a hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false.
   #METHOD WLOCK */
static bool tchdbvanishimpl(TCHDB *hdb) {
    assert(hdb);
    char *path = tcstrdup(hdb->path);
    int omode = hdb->omode;
    bool err = false;
    if (!tchdbcloseimpl(hdb)) err = true;
    if (!tchdbopenimpl(hdb, path, HDBOTRUNC | omode)) {
        tcpathunlock(hdb->rpath);
        TCFREE(hdb->rpath);
        err = true;
    }
    TCFREE(path);
    return !err;
}

/* Copy the database file of a hash database object.
   `hdb' specifies the hash database object.
   `path' specifies the path of the destination file.
   If successful, the return value is true, else, it is false.
   #METHOD RLOCK + BNUM LOCK */
static bool tchdbcopyimpl(TCHDB *hdb, const char *path) {
    assert(hdb && path);
    bool err = false;
    if (hdb->omode & HDBOWRITER) {
        if (!tchdbsavefbp(hdb)) err = true;
        if (!tchdbmemsync(hdb, false)) err = true;
        tchdbsetflag(hdb, HDBFOPEN, false);
    }
    if (*path == '@') {
        char tsbuf[TCNUMBUFSIZ];
        sprintf(tsbuf, "%" PRIu64 "", (uint64_t) (tctime() * 1000000));
        const char *args[3];
        args[0] = path + 1;
        args[1] = hdb->path;
        args[2] = tsbuf;
        if (tcsystem(args, sizeof (args) / sizeof (*args)) != 0) err = true;
    } else {
        if (!tccopyfile(hdb->path, path)) {
            tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
            err = true;
        }
    }
    if (hdb->omode & HDBOWRITER) tchdbsetflag(hdb, HDBFOPEN, true);
    return !err;
}

/* Perform dynamic defragmentation of a hash database object.
   `hdb' specifies the hash database object connected.
   `step' specifie the number of steps.
   If successful, the return value is true, else, it is false.
   #METHOD RLOCK + ALL REC RLOCK OR METHOD WLOCK
 */
static bool tchdbdefragimpl(TCHDB *hdb, int64_t step) {
    assert(hdb && step >= 0);
    TCDODEBUG(hdb->cnt_defrag++);
    hdb->dfcnt = 0;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    bool err = false;
    while (true) {
        if (hdb->dfcur >= hdb->fsiz) {
            hdb->dfcur = hdb->frec;
            return true;
        }
        if (step-- < 1) return true;
        rec.off = hdb->dfcur;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return false;
        if (rec.magic == HDBMAGICFB) break;
        hdb->dfcur += rec.rsiz;
    }
    uint32_t align = hdb->align;
    uint64_t base = hdb->dfcur;
    uint64_t dest = base;
    uint64_t cur = base;
    if (hdb->iter == cur) hdb->iter += rec.rsiz;
    if (hdb->iter2list) {
        for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
            TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
            if ((*pit)->pos == cur) (*pit)->pos += rec.rsiz;
        }
    }
    cur += rec.rsiz;
    uint64_t fbsiz = cur - dest;
    step++;
    while (step > 0 && cur < hdb->fsiz) {
        rec.off = cur;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return false;
        uint32_t rsiz = rec.rsiz;
        if (rec.magic == HDBMAGICREC) {
            if (rec.psiz >= align) {
                int diff = rec.psiz - rec.psiz % align;
                rec.psiz -= diff;
                rec.rsiz -= diff;
                fbsiz += diff;
            }
            if (!tchdbshiftrec(hdb, &rec, rbuf, dest)) {
                fprintf(stderr, "\n %d %d %d %d @000010\n", (int) cur, (int) hdb->fsiz, (int) (hdb->fsiz - cur), (int) rsiz);
                return false;
            }
            if (hdb->iter == cur) hdb->iter = dest;
            if (hdb->iter2list) {
                for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
                    TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
                    if ((*pit)->pos == cur) (*pit)->pos = dest;
                }
            }
            dest += rec.rsiz;
            step--;
        } else {
            if (hdb->iter == cur) hdb->iter += rec.rsiz;
            if (hdb->iter2list) {
                for (int i = TCLISTNUM(hdb->iter2list) - 1; i >= 0; --i) {
                    TCHDBITER **pit = TCLISTVALPTR(hdb->iter2list, i);
                    if ((*pit)->pos == cur) (*pit)->pos += rec.rsiz;
                }
            }
            fbsiz += rec.rsiz;
        }
        cur += rsiz;
    }
    if (cur < hdb->fsiz) {
        if (fbsiz > HDBFBMAXSIZ) {
            tchdbfbptrim(hdb, base, cur, 0, 0);
            uint64_t off = dest;
            uint64_t size = fbsiz;
            while (size > 0) {
                uint32_t rsiz = (size > HDBFBMAXSIZ) ? HDBFBMAXSIZ : size;
                if (size - rsiz < HDBMINRUNIT) rsiz = size;
                tchdbfbpinsert(hdb, off, rsiz);
                if (!tchdbwritefb(hdb, off, rsiz)) return false;
                off += rsiz;
                size -= rsiz;
            }
        } else {
            tchdbfbptrim(hdb, base, cur, dest, fbsiz);
            if (!tchdbwritefb(hdb, dest, fbsiz)) return false;
        }
        hdb->dfcur = cur - fbsiz;
    } else {
        TCDODEBUG(hdb->cnt_trunc++);
        if (hdb->tran && !tchdbwalwrite(hdb, dest, fbsiz)) {
            return false;
        }
        tchdbfbptrim(hdb, base, cur, 0, 0);
        hdb->dfcur = hdb->frec;
        hdb->fsiz = dest;
        uint64_t llnum = hdb->fsiz;
        llnum = TCHTOILL(llnum);
        if (HDBLOCKSMEMPTR(hdb, false)) {
            memcpy((void *) (hdb->map + HDBFSIZOFF), &llnum, sizeof (llnum));
            HDBUNLOCKSMEMPTR(hdb);
        } else {
            err = true;
        }
        if (hdb->iter >= hdb->fsiz) {
            hdb->iter = UINT64_MAX;
        }
        if (!hdb->tran && !tchdbftruncate(hdb, hdb->fsiz)) {
            tchdbsetecode(hdb, TCETRUNC, __FILE__, __LINE__, __func__);
            return false;
        }
    }
    return !err;
}

/* Move the iterator to the record corresponding a key of a hash database object.
   `hdb' specifies the hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   If successful, the return value is true, else, it is false. */
static bool tchdbiterjumpimpl(TCHDB *hdb, const char *kbuf, int ksiz) {
    assert(hdb && kbuf && ksiz);
    uint8_t hash;
    uint64_t bidx = tchdbbidx(hdb, kbuf, ksiz, &hash);
    off_t off = tchdbgetbucket(hdb, bidx);
    if (off == -1) return false;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    while (off > 0) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) return false;
        if (hash > rec.hash) {
            off = rec.left;
        } else if (hash < rec.hash) {
            off = rec.right;
        } else {
            if (!rec.kbuf && !tchdbreadrecbody(hdb, &rec)) return false;
            int kcmp = tcreckeycmp(kbuf, ksiz, rec.kbuf, rec.ksiz);
            if (kcmp > 0) {
                off = rec.left;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else if (kcmp < 0) {
                off = rec.right;
                TCFREE(rec.bbuf);
                rec.kbuf = NULL;
                rec.bbuf = NULL;
            } else {
                hdb->iter = off;
                return true;
            }
        }
    }
    tchdbsetecode(hdb, TCENOREC, __FILE__, __LINE__, __func__);
    return false;
}

/* Process each record atomically of a hash database object.
   `hdb' specifies the hash database object.
   `func' specifies the pointer to the iterator function called for each record.
   `op' specifies an arbitrary pointer to be given as a parameter of the iterator function.
   If successful, the return value is true, else, it is false. */
static bool tchdbforeachimpl(TCHDB *hdb, TCITER iter, void *op) {
    assert(hdb && iter);
    bool err = false;
    uint64_t off = hdb->frec;
    TCHREC rec;
    char rbuf[HDBIOBUFSIZ];
    bool cont = true;
    while (cont && off < hdb->fsiz) {
        rec.off = off;
        if (!tchdbreadrec(hdb, &rec, rbuf)) {
            err = true;
            break;
        }
        off += rec.rsiz;
        if (rec.magic == HDBMAGICREC) {
            if (!rec.vbuf && !tchdbreadrecbody(hdb, &rec)) {
                TCFREE(rec.bbuf);
                err = true;
            } else {
                if (hdb->zmode) {
                    int zsiz;
                    char *zbuf;
                    if (hdb->opts & HDBTDEFLATE) {
                        zbuf = _tc_inflate(rec.vbuf, rec.vsiz, &zsiz, _TCZMRAW);
                    } else if (hdb->opts & HDBTBZIP) {
                        zbuf = _tc_bzdecompress(rec.vbuf, rec.vsiz, &zsiz);
                    } else if (hdb->opts & HDBTTCBS) {
                        zbuf = tcbsdecode(rec.vbuf, rec.vsiz, &zsiz);
                    } else {
                        zbuf = hdb->dec(rec.vbuf, rec.vsiz, &zsiz, hdb->decop);
                    }
                    if (zbuf) {
                        cont = iter(rec.kbuf, rec.ksiz, zbuf, zsiz, op);
                        TCFREE(zbuf);
                    } else {
                        tchdbsetecode(hdb, TCEMISC, __FILE__, __LINE__, __func__);
                        err = true;
                    }
                } else {
                    cont = iter(rec.kbuf, rec.ksiz, rec.vbuf, rec.vsiz, op);
                }
            }
            TCFREE(rec.bbuf);
        }
    }
    return !err;
}

/* Lock a method of the hash database object.
   `hdb' specifies the hash database object.
   `wr' specifies whether the lock is writer or not.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdblockmethod(TCHDB *hdb, bool wr) {
    assert(hdb);
    if (wr ? pthread_rwlock_wrlock(hdb->mmtx) != 0 : pthread_rwlock_rdlock(hdb->mmtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Unlock a method of the hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdbunlockmethod(TCHDB *hdb) {
    assert(hdb);
    if (pthread_rwlock_unlock(hdb->mmtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Lock shared memory  */
EJDB_INLINE bool tchdblocksmem(TCHDB *hdb, bool wr) {
    assert(hdb);
    if (wr ? pthread_rwlock_wrlock(hdb->smtx) != 0 : pthread_rwlock_rdlock(hdb->smtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    if (hdb->map == NULL) {
        tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
        tchdbunlocksmem(hdb);
        return false;
    }
    TCTESTYIELD();
    return true;
}

EJDB_INLINE bool tchdblocksmem2(TCHDB *hdb, bool wr) {
    assert(hdb);
    if (wr ? pthread_rwlock_wrlock(hdb->smtx) != 0 : pthread_rwlock_rdlock(hdb->smtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Unlock shared memory */
EJDB_INLINE bool tchdbunlocksmem(TCHDB *hdb) {
    assert(hdb);
    if (pthread_rwlock_unlock(hdb->smtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Lock a record of the hash database object.
   `hdb' specifies the hash database object.
   `bidx' specifies the bucket index of the record.
   `wr' specifies whether the lock is writer or not.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdblockrecord(TCHDB *hdb, uint8_t bidx, bool wr) {
    assert(hdb);
    if (wr ? pthread_rwlock_wrlock((pthread_rwlock_t *) hdb->rmtxs + bidx) != 0 :
            pthread_rwlock_rdlock((pthread_rwlock_t *) hdb->rmtxs + bidx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Unlock a record of the hash database object.
   `hdb' specifies the hash database object.
   `bidx' specifies the bucket index of the record.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdbunlockrecord(TCHDB *hdb, uint8_t bidx) {
    assert(hdb);
    if (pthread_rwlock_unlock((pthread_rwlock_t *) hdb->rmtxs + bidx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Lock all records of the hash database object.
   `hdb' specifies the hash database object.
   `wr' specifies whether the lock is writer or not.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdblockallrecords(TCHDB *hdb, bool wr) {
    assert(hdb);
    for (int i = 0; i <= UINT8_MAX; i++) {
        if (wr ? pthread_rwlock_wrlock((pthread_rwlock_t *) hdb->rmtxs + i) != 0 :
                pthread_rwlock_rdlock((pthread_rwlock_t *) hdb->rmtxs + i) != 0) {
            tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
            while (--i >= 0) {
                pthread_rwlock_unlock((pthread_rwlock_t *) hdb->rmtxs + i);
            }
            return false;
        }
    }
    TCTESTYIELD();
    return true;
}

/* Unlock all records of the hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdbunlockallrecords(TCHDB *hdb) {
    assert(hdb);
    bool err = false;
    for (int i = UINT8_MAX; i >= 0; i--) {
        if (pthread_rwlock_unlock((pthread_rwlock_t *) hdb->rmtxs + i)) err = true;
    }
    TCTESTYIELD();
    if (err) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    return true;
}

/* Lock the whole database of the hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdblockdb(TCHDB *hdb) {
    assert(hdb);
    if (pthread_mutex_lock(hdb->dmtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Unlock the whole database of the hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdbunlockdb(TCHDB *hdb) {
    assert(hdb);
    if (pthread_mutex_unlock(hdb->dmtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Lock the write ahead logging file of the hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdblockwal(TCHDB *hdb) {
    assert(hdb);
    if (pthread_mutex_lock(hdb->wmtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

/* Unlock the write ahead logging file of the hash database object.
   `hdb' specifies the hash database object.
   If successful, the return value is true, else, it is false. */
EJDB_INLINE bool tchdbunlockwal(TCHDB *hdb) {
    assert(hdb);
    if (pthread_mutex_unlock(hdb->wmtx) != 0) {
        tchdbsetecode(hdb, TCETHREAD, __FILE__, __LINE__, __func__);
        return false;
    }
    TCTESTYIELD();
    return true;
}

static bool tchdbftruncate(TCHDB *hdb, off_t length) {
    return tchdbftruncate2(hdb, length, 0);
}

static bool tchdbftruncate2(TCHDB *hdb, off_t length, int opts) {
    uint64_t xfsiz = __atomic_load_n64(&hdb->xfsiz, __ATOMIC_ACQUIRE);
#ifndef _WIN32
    length = length ? tcpagealign(length) : 0;
    if (!(hdb->omode & HDBOWRITER) || (length <= xfsiz && !(opts & HDBTRALLOWSHRINK))) {
        return true;
    }
    if (length > xfsiz && !(opts & HDBTRALLOWSHRINK)) {
        off_t o1 = tcpagealign((_maxof(off_t) - length < HDBXFSIZINC) ? length : length + HDBXFSIZINC);
        off_t o2 = tcpagealign((((uint64_t) length) * 3) >> 1);
        length = MAX(o1, o2);
    }
    if (!ftruncate(hdb->fd, length)) {
        __atomic_store_n64(&hdb->xfsiz, length, __ATOMIC_RELEASE);
        return true;
    } else {
        return false;
    }
#else
    bool err = false;
    LARGE_INTEGER size, msize;
    //MSDN: Applications should test for files with a length of 0 (zero) and reject those files.
    size.QuadPart = (hdb->omode & HDBOWRITER) ? tcpagealign((length == 0) ? 1 : length) : length;
    if (hdb->map &&
            length > 0 &&
            (!(hdb->omode & HDBOWRITER) || (size.QuadPart <= xfsiz && !(opts & HDBTRALLOWSHRINK)))) {
        return true;
    }
    if (!(opts & HDBOPTNOSMLOCK) && !HDBLOCKSMEMPTR2(hdb, true)) {
        return false;
    }
        
    if ((hdb->omode & HDBOWRITER) && size.QuadPart > xfsiz && !(opts & HDBTRALLOWSHRINK)) {
        off_t o1 = tcpagealign((_maxof(off_t) - (off_t) size.QuadPart < HDBXFSIZINC) ? size.QuadPart : size.QuadPart + HDBXFSIZINC);
        off_t o2 = tcpagealign((((uint64_t) size.QuadPart) * 3) >> 1);
        size.QuadPart = MAX(o1, o2);
    }
    msize = size;
    if (hdb->map) {
        FlushViewOfFile((LPCVOID) hdb->map, 0);
        if (!UnmapViewOfFile((LPCVOID) hdb->map) || !CloseHandle(hdb->w32hmap)) {
            tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
            err = true;
            goto finish;
        }
        hdb->map = NULL;
        hdb->w32hmap = INVALID_HANDLE_VALUE;
    }
    if (hdb->omode & HDBOWRITER) {
        if (!SetFilePointerEx(hdb->fd, size, NULL, FILE_BEGIN) || !SetEndOfFile(hdb->fd)) {
            tchdbsetecode(hdb, TCESEEK, __FILE__, __LINE__, __func__);
            err = true;
            goto finish;
        }
    }
    if (msize.QuadPart > hdb->xmsiz) {
        msize.QuadPart = hdb->xmsiz;
    }
    hdb->w32hmap = CreateFileMapping(hdb->fd, NULL,
            ((hdb->omode & HDBOWRITER) ? PAGE_READWRITE : PAGE_READONLY),
            msize.HighPart, msize.LowPart, NULL);
    if (INVALIDHANDLE(hdb->w32hmap)) {
        tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
        err = true;
        goto finish;
    }
    hdb->map = MapViewOfFile(hdb->w32hmap,
            ((hdb->omode & HDBOWRITER) ? FILE_MAP_WRITE : FILE_MAP_READ),
            0, 0, 0);
    if (hdb->map == NULL) {
        tchdbsetecode(hdb, TCEMMAP, __FILE__, __LINE__, __func__);
        err = true;
        goto finish;
    }
    __atomic_store_n64(&hdb->xfsiz, size.QuadPart, __ATOMIC_RELEASE);
    if (length == 0) {
        size.QuadPart = 0;
        if (!SetFilePointerEx(hdb->fd, size, NULL, FILE_BEGIN)) {
            tchdbsetecode(hdb, TCESEEK, __FILE__, __LINE__, __func__);
            err = true;
            goto finish;
        }
    }
finish:
    if (!(opts & HDBOPTNOSMLOCK)) HDBUNLOCKSMEMPTR(hdb);
    return !err;
#endif
}

EJDB_INLINE bool tchdbwritesmem(TCHDB *hdb, size_t off, const void *ptr, size_t sz, int opts) {
    if (off < HDBOPAQUEOFF) { //writing in header
        opts |= HDBOPTLOCKDB;
    }
    if ((opts & HDBOPTLOCKDB) && !HDBLOCKDB(hdb)) {
        return false;
    }
    if (!(opts & HDBOPTNOSMLOCK) && !HDBLOCKSMEMPTR(hdb, false)) {
        if (opts & HDBOPTLOCKDB) HDBUNLOCKDB(hdb);
        return false;
    }
    memcpy(hdb->map + off, ptr, sz);
    if (!(opts & HDBOPTNOSMLOCK)) HDBUNLOCKSMEMPTR(hdb);
    if (opts & HDBOPTLOCKDB) HDBUNLOCKDB(hdb);
    return true;
}

EJDB_INLINE bool tchdbreadsmem(TCHDB *hdb, void *dst, size_t off, size_t sz, int opts) {
    if (off < HDBOPAQUEOFF) { //reading header
        opts |= HDBOPTLOCKDB;
    }
    if ((opts & HDBOPTLOCKDB) && !HDBLOCKDB(hdb)) {
        return false;
    }
    if (!(opts & HDBOPTNOSMLOCK) && !HDBLOCKSMEMPTR(hdb, false)) {
        if (opts & HDBOPTLOCKDB) HDBUNLOCKDB(hdb);
        return false;
    }
    memcpy(dst, hdb->map + off, sz);
    if (!(opts & HDBOPTNOSMLOCK)) HDBUNLOCKSMEMPTR(hdb);
    if (opts & HDBOPTLOCKDB) HDBUNLOCKDB(hdb);
    return true;
}

/*************************************************************************************************
 * debugging functions
 *************************************************************************************************/

/* Print meta data of the header into the debugging output.
   `hdb' specifies the hash database object. */
void tchdbprintmeta(TCHDB *hdb) {
    assert(hdb);
    HANDLE dbgfd = hdb->dbgfd;
    if (INVALIDHANDLE(dbgfd)) {
        dbgfd = GET_STDOUT_HANDLE();
    }
    char buf[HDBIOBUFSIZ];
    char *wp = buf;
    wp += sprintf(wp, "META:");
    wp += sprintf(wp, " mmtx=%p", (void *) hdb->mmtx);
    wp += sprintf(wp, " rmtxs=%p", (void *) hdb->rmtxs);
    wp += sprintf(wp, " dmtx=%p", (void *) hdb->dmtx);
    wp += sprintf(wp, " smtx=%p", (void *) hdb->smtx);
    wp += sprintf(wp, " wmtx=%p", (void *) hdb->wmtx);
    wp += sprintf(wp, " eckey=%p", (void *) hdb->eckey);
    wp += sprintf(wp, " rpath=%s", hdb->rpath ? hdb->rpath : "-");
    wp += sprintf(wp, " type=%02X", hdb->type);
    wp += sprintf(wp, " flags=%02X", hdb->flags);
    wp += sprintf(wp, " bnum=%" PRIu64 "", (uint64_t) hdb->bnum);
    wp += sprintf(wp, " apow=%u", hdb->apow);
    wp += sprintf(wp, " fpow=%u", hdb->fpow);
    wp += sprintf(wp, " opts=%u", hdb->opts);
    wp += sprintf(wp, " path=%s", hdb->path ? hdb->path : "-");
    wp += sprintf(wp, " omode=%u", hdb->omode);
    wp += sprintf(wp, " rnum=%" PRIu64 "", (uint64_t) hdb->rnum);
    wp += sprintf(wp, " fsiz=%" PRIu64 "", (uint64_t) hdb->fsiz);
    wp += sprintf(wp, " frec=%" PRIu64 "", (uint64_t) hdb->frec);
    wp += sprintf(wp, " dfcur=%" PRIu64 "", (uint64_t) hdb->dfcur);
    wp += sprintf(wp, " iter=%" PRIu64 "", (uint64_t) hdb->iter);
    wp += sprintf(wp, " map=%p", (void *) hdb->map);
    wp += sprintf(wp, " msiz=%" PRIu64 "", (uint64_t) hdb->msiz);
    wp += sprintf(wp, " ba64=%d", hdb->ba64);
    wp += sprintf(wp, " align=%u", hdb->align);
    wp += sprintf(wp, " runit=%u", hdb->runit);
    wp += sprintf(wp, " zmode=%u", hdb->zmode);
    wp += sprintf(wp, " fbpmax=%d", hdb->fbpmax);
    wp += sprintf(wp, " fbpool=%p", (void *) hdb->fbpool);
    wp += sprintf(wp, " fbpnum=%d", hdb->fbpnum);
    wp += sprintf(wp, " fbpmis=%d", hdb->fbpmis);
    wp += sprintf(wp, " drpool=%p", (void *) hdb->drpool);
    wp += sprintf(wp, " drpdef=%p", (void *) hdb->drpdef);
    wp += sprintf(wp, " drpoff=%" PRIu64 "", (uint64_t) hdb->drpoff);
    wp += sprintf(wp, " recc=%p", (void *) hdb->recc);
    wp += sprintf(wp, " rcnum=%u", hdb->rcnum);
    wp += sprintf(wp, " ecode=%d", hdb->ecode);
    wp += sprintf(wp, " fatal=%u", hdb->fatal);
    wp += sprintf(wp, " inode=%" PRIu64 "", (uint64_t) (uint64_t) hdb->inode);
    wp += sprintf(wp, " mtime=%" PRIu64 "", (uint64_t) (uint64_t) hdb->mtime);
    wp += sprintf(wp, " dfunit=%u", hdb->dfunit);
    wp += sprintf(wp, " dfcnt=%u", hdb->dfcnt);
    wp += sprintf(wp, " tran=%d", hdb->tran);
    wp += sprintf(wp, " walend=%" PRIu64 "", (uint64_t) hdb->walend);
#ifndef _WIN32
    wp += sprintf(wp, " fd=%d", hdb->fd);
    wp += sprintf(wp, " walfd=%d", hdb->walfd);
    wp += sprintf(wp, " dbgfd=%d", hdb->dbgfd);
#endif

#ifndef NDEBUG
    wp += sprintf(wp, " cnt_writerec=%" PRId64 "", (int64_t) hdb->cnt_writerec);
    wp += sprintf(wp, " cnt_reuserec=%" PRId64 "", (int64_t) hdb->cnt_reuserec);
    wp += sprintf(wp, " cnt_moverec=%" PRId64 "", (int64_t) hdb->cnt_moverec);
    wp += sprintf(wp, " cnt_readrec=%" PRId64 "", (int64_t) hdb->cnt_readrec);
    wp += sprintf(wp, " cnt_searchfbp=%" PRId64 "", (int64_t) hdb->cnt_searchfbp);
    wp += sprintf(wp, " cnt_insertfbp=%" PRId64 "", (int64_t) hdb->cnt_insertfbp);
    wp += sprintf(wp, " cnt_splicefbp=%" PRId64 "", (int64_t) hdb->cnt_splicefbp);
    wp += sprintf(wp, " cnt_dividefbp=%" PRId64 "", (int64_t) hdb->cnt_dividefbp);
    wp += sprintf(wp, " cnt_mergefbp=%" PRId64 "", (int64_t) hdb->cnt_mergefbp);
    wp += sprintf(wp, " cnt_reducefbp=%" PRId64 "", (int64_t) hdb->cnt_reducefbp);
    wp += sprintf(wp, " cnt_appenddrp=%" PRId64 "", (int64_t) hdb->cnt_appenddrp);
    wp += sprintf(wp, " cnt_deferdrp=%" PRId64 "", (int64_t) hdb->cnt_deferdrp);
    wp += sprintf(wp, " cnt_flushdrp=%" PRId64 "", (int64_t) hdb->cnt_flushdrp);
    wp += sprintf(wp, " cnt_adjrecc=%" PRId64 "", (int64_t) hdb->cnt_adjrecc);
    wp += sprintf(wp, " cnt_defrag=%" PRId64 "", (int64_t) hdb->cnt_defrag);
    wp += sprintf(wp, " cnt_shiftrec=%" PRId64 "", (int64_t) hdb->cnt_shiftrec);
    wp += sprintf(wp, " cnt_trunc=%" PRId64 "", (int64_t) hdb->cnt_trunc);
#endif

    *(wp++) = '\n';
    tcwrite(dbgfd, buf, wp - buf);
}

/* Print a record information into the debugging output.
   `hdb' specifies the hash database object.
   `rec' specifies the record. */
void tchdbprintrec(TCHDB *hdb, TCHREC *rec) {
    assert(hdb && rec);
    HANDLE dbgfd = hdb->dbgfd;
    if (INVALIDHANDLE(dbgfd)) {
        dbgfd = GET_STDOUT_HANDLE();
    }
    char buf[HDBIOBUFSIZ];
    char *wp = buf;
    wp += sprintf(wp, "REC:");
    wp += sprintf(wp, " off=%" PRIu64 "", (uint64_t) rec->off);
    wp += sprintf(wp, " rsiz=%u", rec->rsiz);
    wp += sprintf(wp, " magic=%02X", rec->magic);
    wp += sprintf(wp, " hash=%02X", rec->hash);
    wp += sprintf(wp, " left=%" PRIu64 "", (uint64_t) rec->left);
    wp += sprintf(wp, " right=%" PRIu64 "", (uint64_t) rec->right);
    wp += sprintf(wp, " ksiz=%u", rec->ksiz);
    wp += sprintf(wp, " vsiz=%u", rec->vsiz);
    wp += sprintf(wp, " psiz=%u", rec->psiz);
    wp += sprintf(wp, " kbuf=%p", (void *) rec->kbuf);
    wp += sprintf(wp, " vbuf=%p", (void *) rec->vbuf);
    wp += sprintf(wp, " boff=%" PRIu64 "", (uint64_t) rec->boff);
    wp += sprintf(wp, " bbuf=%p", (void *) rec->bbuf);
    *(wp++) = '\n';
    tcwrite(dbgfd, buf, wp - buf);
}



// END OF FILE
