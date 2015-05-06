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

#ifndef EJDB_PRIVATE_H
#define EJDB_PRIVATE_H

#include "ejdb.h"
#include "tcutil.h"
#include "tctdb.h"
#include "tchdb.h"

#include <assert.h>

EJDB_EXTERN_C_START

#define BSON_IS_IDXSUPPORTED_TYPE(atype) (atype == BSON_STRING || \
                                          atype == BSON_INT || atype == BSON_LONG || atype == BSON_DOUBLE || \
                                          atype == BSON_ARRAY || atype == BSON_DATE)

#define EJDB_MAX_COLLECTIONS 1024
#define EJDB_MAGIC 0xEBB1
#define EJDB_MAGIC_SZ 2;    //number of bytes to encode magic in TCTDB opaque data
#define EJDB_VERSION_SZ 4;  //number of bytes to encode version in TCTDB opaque data 


struct EJCOLL { /**> EJDB Collection. */
    char *cname; /**> Collection name. */
    int cnamesz; /**> Collection name length. */
    TCTDB *tdb; /**> Collection TCTDB. */
    EJDB *jb; /**> Database handle. */
    void *mmtx; /*> Mutex for method */
};

struct EJDB {
    EJCOLL * cdbs[EJDB_MAX_COLLECTIONS]; /*> Collection DBs for JSON collections. */
    int cdbsnum; /*> Count of collection DB. */
    uint32_t fversion; /*> Database format version */
    TCTDB *metadb; /*> Metadata DB. */
    void *mmtx; /*> Mutex for method */
};

enum { /**> Query field flags */
    // Comparison flags
    EJCOMPGT = 1u, /**> Comparison GT */
    EJCOMPGTE = 1u << 1, /**> Comparison GTE */
    EJCOMPLT = 1u << 2, /**> Comparison LT */
    EJCOMPLTE = 1u << 3, /**> Comparison LTE */
    EJCONDSTARTWITH = 1u << 4, /**> Starts with */

    EJFEXCLUDED = 1u << 5, /**> If query field excluded from matching */
    EJFNOINDEX = 1u << 6, /**> Do not use index for field */
    EJFORDERUSED = 1u << 7, /**> This ordering field was used */
    EJFPKMATCHING = 1u << 8, /**> _id PK field matching */

    EJCONDICASE = 1u << 9, /**> Ignore case in matching */

    EJCONDSET = 1u << 10, /**> $set Set field update operation */
    EJCONDINC = 1u << 11, /**> $inc Inc field update operation */
    EJCONDADDSET = 1u << 12, /**> $addToSet, $addToSetAll. Adds a value to the array only if its not in the array already.  */
    EJCONDPULL = 1u << 13, /**> $pull Removes all occurrences of value from field, if field is an array */
    EJCONDUPSERT = 1u << 14, /**> $upsert Upsert $set operation */
    EJCONDALL = 1u << 15, /**> 'All' modificator for $pull(All), $addToSet(All), $push(All) */
    EJCONDOIT = 1u << 16, /**> $do query field operation */
    EJCONDUNSET = 1u << 17, /**> $unset Field value */
    EJCONDRENAME = 1u << 18, /**> $rename Field value */
    EJCONDPUSH  = 1u << 19 /**> $push, $pushAll. Adds a value to the array */
};

enum { /**> Query flags */
    EJQINTERNAL = 1, /**> Internal query object used in _ejdbqryexecute */
    EJQUPDATING = 1u << 1, /**> Query in updating mode */
    EJQDROPALL = 1u << 2, /**> Drop bson object if matched */
    EJQONLYCOUNT = 1u << 3, /**> Only count mode */
    EJQHASUQUERY = 1u << 4 /**> It means the query contains update $(query) fields #91 */
};

typedef struct { /**> $(query) matchin slot used in update $ placeholder processing. #91 */
    int32_t mpos; /**> array position of matched element */
    int32_t dpos; /**> $ position in the fieldpath */
    const void *op; /**> Opaque pointer associated with slot */
} USLOT;

struct EJQF { /**> Matching field and status */
    bool negate; /**> Negate expression */
    int fpathsz; /**>JSON field path size */
    int exprsz; /**> Size of query operand expression */
    int tcop; /**> Matching operation eg. TDBQCSTREQ */
    bson_type ftype; /**> BSON field type */
    uint32_t flags; /**> Various field matching|status flags */
    uint32_t mflags; /**> Temporary matching flags used during single record matching */
    int order; /**> 0 no order, 1 ASC, -1 DESC */
    int orderseq; /**> Seq number for order fields */
    int elmatchgrp; /**> $elemMatch group id */
    int elmatchpos; /**> $elemMatch fieldpath position */
    char *fpath; /**>JSON field path */
    char *expr; /**> Query operand expression, string or TCLIST data */
    const TDBIDX *idx; /**> Column index for this field if exists */
    bson *idxmeta; /**> Index metainfo */
    bson *updateobj; /**> Update bson object for $set and $inc operations */
    TCLIST *exprlist; /**> List representation of expression */
    TCMAP *exprmap; /**> Hash map for expression tokens used in $in matching operation. */
    void *regex; /**> Regular expression object */
    EJDB *jb; /**> Reference to the EJDB during query processing */
    EJQ *q; /**> Query object in which this field embedded */
    double exprdblval; /**> Double value representation */
    int64_t exprlongval; /**> Integer value represeintation */
    TCLIST *ufields; /**> Update $(query) prositional fields #91 */
    TCLIST *uslots; /**> $(query) matching slots USLOT #91 */
};
typedef struct EJQF EJQF;

struct EJQ { /**> Query object. */
    TCLIST *qflist; /**> List of query field objects *EJQF */
    TCLIST *orqlist; /**> List of $or joined query objects *EJQ */
    TCLIST *andqlist; /**> List of $and joined query objects *EJQ */
    bson *hints; /**> Hints bson object */
    /**> Include $(projection) fields char* names.
     *  Mapping EJQF fpath => $(projection) field name
     *  http://docs.mongodb.org/manual/reference/projection/positional/#proj._S_
     */
    TCMAP *ifields;
    uint32_t skip; /**> Number of records to skip. */
    uint32_t max; /**> Max number of results */
    uint32_t flags; /**> Control flags */
    EJQ *lastmatchedorq; /**> Reference to the last matched $or query */
    EJQF **allqfields; /**> NULL terminated list of all *EJQF fields including all $and $or QF*/

    //Temporal buffers used during query processing
    TCXSTR *colbuf; /**> TCTDB current column buffer */
    TCXSTR *bsbuf; /**> current bson object buff */
    TCXSTR *tmpbuf; /**> Tmp buffer */
};

#define JDBCOLBSON "$"  /**> TCDB colname with BSON byte data */
#define JDBCOLBSONL 1  /**> TCDB colname with BSON byte data columen len */


#define JBINOPTMAPTHRESHOLD 16 /**> If number of tokens in `$in` array exeeds it then TCMAP will be used in fullscan matching of tokens */


EJDB_EXPORT bool ejcollockmethod(EJCOLL *coll, bool wr);
EJDB_EXPORT bool ejcollunlockmethod(EJCOLL *coll);

EJDB_EXTERN_C_END

#endif        /* EJDB_PRIVATE_H */

