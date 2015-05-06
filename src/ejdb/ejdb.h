/**************************************************************************************************
 *  C/C++ API for EJDB database library http://ejdb.org
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

#ifndef EJDB_H
#define        EJDB_H

#include "bson.h"
#include "tcutil.h"

EJDB_EXTERN_C_START

#ifndef EJDB_MAX_IMPORTED_BSON_SIZE
//64 MB is the default maximum size of bson object imported with `ejdbimport()`
#define EJDB_MAX_IMPORTED_BSON_SIZE 67108864
#endif

struct EJDB; /**< EJDB database object. */
typedef struct EJDB EJDB;

struct EJCOLL; /*< EJDB collection handle. */
typedef struct EJCOLL EJCOLL;

struct EJQ; /**< EJDB query. */
typedef struct EJQ EJQ;

typedef struct {        /**< EJDB collection tuning options. */
    bool large;         /**< Large collection. It can be larger than 2GB. Default false */
    bool compressed;    /**< Collection records will be compressed with DEFLATE compression. Default: false */
    int64_t records;    /**< Expected records number in the collection. Default: 128K */
    int cachedrecords;  /**< Maximum number of records cached in memory. Default: 0 */
} EJCOLLOPTS;


typedef TCLIST* EJQRESULT; /**< EJDB query result */

#define JBMAXCOLNAMELEN 128

enum { /** Error codes */
    JBEINVALIDCOLNAME = 9000,   /**< Invalid collection name. */
    JBEINVALIDBSON = 9001,      /**< Invalid bson object. */
    JBEINVALIDBSONPK = 9002,    /**< Invalid bson object id. */
    JBEQINVALIDQCONTROL = 9003, /**< Invalid query control field starting with '$'. */
    JBEQINOPNOTARRAY = 9004,    /**< $strand, $stror, $in, $nin, $bt keys requires not empty array value. */
    JBEMETANVALID = 9005,       /**< Inconsistent database metadata. */
    JBEFPATHINVALID = 9006,     /**< Invalid field path value. */
    JBEQINVALIDQRX = 9007,      /**< Invalid query regexp value. */
    JBEQRSSORTING = 9008,       /**< Result set sorting error. */
    JBEQERROR = 9009,           /**< Query generic error. */
    JBEQUPDFAILED = 9010,       /**< Updating failed. */
    JBEQONEEMATCH = 9011,       /**< Only one $elemMatch allowed in the fieldpath. */
    JBEQINCEXCL = 9012,         /**< $fields hint cannot mix include and exclude fields */
    JBEQACTKEY = 9013,          /**< action key in $do block can only be one of: $join, $slice */
    JBEMAXNUMCOLS = 9014,       /**< Exceeded the maximum number of collections per database */
    JBEEI = 9015,               /**< EJDB export/import error */
    JBEEJSONPARSE = 9016,       /**< JSON parsing failed */
    JBETOOBIGBSON = 9017,       /**< BSON size is too big */
    JBEINVALIDCMD = 9018        /**< Invalid ejdb command specified */
};

enum { /** Database open modes */
    JBOREADER = 1u << 0,    /**< Open as a reader. */
    JBOWRITER = 1u << 1,    /**< Open as a writer. */
    JBOCREAT = 1u << 2,     /**< Create if db file not exists. */
    JBOTRUNC = 1u << 3,     /**< Truncate db on open. */
    JBONOLCK = 1u << 4,     /**< Open without locking. */
    JBOLCKNB = 1u << 5,     /**< Lock without blocking. */
    JBOTSYNC = 1u << 6      /**< Synchronize every transaction. */
};

enum { /** Index modes, index types. */
    JBIDXDROP = 1u << 0,    /**< Drop index. */
    JBIDXDROPALL = 1u << 1, /**< Drop index for all types. */
    JBIDXOP = 1u << 2,      /**< Optimize indexes. */
    JBIDXREBLD = 1u << 3,   /**< Rebuild index. */
    JBIDXNUM = 1u << 4,     /**< Number index. */
    JBIDXSTR = 1u << 5,     /**< String index.*/
    JBIDXARR = 1u << 6,     /**< Array token index. */
    JBIDXISTR = 1u << 7     /**< Case insensitive string index */
};

enum { /*< Query search mode flags in ejdbqryexecute() */
    JBQRYCOUNT = 1u,        /*< Query only count(*) */
    JBQRYFINDONE = 1u << 1  /*< Fetch first record only */
};

/**
 * Return EJDB library version string. Eg: "1.1.13"
 */
EJDB_EXPORT const char *ejdbversion();

/**
 * Return EJDB database format version.
 * 
 * Format version number uses the following convention:
 *  100000 * major + 1000 * minor + patch.
 * 
 * Return `0` 
 *  - Database is not opened
 *  - Database was created by libejdb < v1.2.8 and opened in read-only mode.
 */
EJDB_EXPORT uint32_t ejdbformatversion(EJDB *jb);

/**
 * Return EJDB database `major` format version.
 * 
 * Return `0` 
 *  - Database is not opened
 *  - Database was created by libejdb < v1.2.8 and opened in read-only mode.
 */
EJDB_EXPORT uint8_t ejdbformatversionmajor(EJDB *jb);

/**
 * Return EJDB database `minor` format version.
 * 
 * Return `0` 
 *  - Database is not opened
 *  - Database was created by libejdb < v1.2.8 and opened in read-only mode.
 */
EJDB_EXPORT uint16_t ejdbformatversionminor(EJDB *jb);

/**
 * Return EJDB database `patch` format version.
 * 
 * Return `0` 
 *  - Database is not opened
 *  - Database was created by libejdb < v1.2.8 and opened in read-only mode.
 */
EJDB_EXPORT uint16_t ejdbformatversionpatch(EJDB *jb);


/**
 * Return true if a passed `oid` string cat be converted to valid
 * 12 bit BSON object identifier (OID).
 * @param oid String
 */
EJDB_EXPORT bool ejdbisvalidoidstr(const char *oid);

/**
 * Get the message string corresponding to an error code.
 * @param ecode `ecode' specifies the error code.
 * @return The return value is the message string of the error code.
 */
EJDB_EXPORT const char *ejdberrmsg(int ecode);

/**
 * Get the last happened error code of a EJDB database object.
 * @param jb
 * @return The return value is the last happened error code.
 */
EJDB_EXPORT int ejdbecode(EJDB *jb);

/**
 * Create a EJDB database object. On error returns NULL.
 * Created pointer must be freed by ejdbdel()
 * @return The return value is the new EJDB database object or NULL if error.
 */
EJDB_EXPORT EJDB* ejdbnew(void);

/**
 * Delete database object. If the database is not closed, it is closed implicitly.
 * Note that the deleted object and its derivatives can not be used anymore
 * @param jb
 */
EJDB_EXPORT void ejdbdel(EJDB *jb);

/**
 * Close a table database object. If a writer opens a database but does not close it appropriately,
 * the database will be broken.
 * @param jb EJDB handle.
 * @return If successful return true, otherwise return false.
 */
EJDB_EXPORT bool ejdbclose(EJDB *jb);

/**
 * Opens EJDB database.
 * @param jb   Database object created with `ejdbnew'
 * @param path Path to the database file.
 * @param mode Open mode bitmask flags:
 * `JBOREADER` Open as a reader.
 * `JBOWRITER` Open as a writer.
 * `JBOCREAT` Create db if it not exists
 * `JBOTRUNC` Truncate db.
 * `JBONOLCK` Open without locking.
 * `JBOLCKNB` Lock without blocking.
 * `JBOTSYNC` Synchronize every transaction.
 * @return
 */
EJDB_EXPORT bool ejdbopen(EJDB *jb, const char *path, int mode);


/**
 * Return true if database is in open state
 * @param jb EJDB database hadle
 * @return True if database is in open state otherwise false
 */
EJDB_EXPORT bool ejdbisopen(EJDB *jb);

/**
 * Retrieve collection handle for collection specified `collname`.
 * If collection with specified name does't exists it will return NULL.
 * @param jb EJDB handle.
 * @param colname Name of collection.
 * @return If error NULL will be returned.
 */
EJDB_EXPORT EJCOLL* ejdbgetcoll(EJDB *jb, const char *colname);

/**
 * Allocate a new TCLIST populated with shallow copy of all
 * collection handles (EJCOLL) currently opened.
 * @param jb EJDB handle.
 * @return If error NULL will be returned.
 */
EJDB_EXPORT TCLIST* ejdbgetcolls(EJDB *jb);

/**
 * Same as ejdbgetcoll() but automatically creates new collection
 * if it does't exists.
 *
 * @param jb EJDB handle.
 * @param colname Name of collection.
 * @param opts Options applied only for newly created collection.
 *              For existing collections it takes no effect.
 *
 * @return Collection handle or NULL if error.
 */
EJDB_EXPORT EJCOLL* ejdbcreatecoll(EJDB *jb, const char *colname, EJCOLLOPTS *opts);

/**
 * Removes collections specified by `colname`
 * @param jb EJDB handle.
 * @param colname Name of collection.
 * @param unlink It true the collection db file and all of its index files will be removed.
 * @return If successful return true, otherwise return false.
 */
EJDB_EXPORT bool ejdbrmcoll(EJDB *jb, const char *colname, bool unlinkfile);

/**
 * Persist BSON object in the collection.
 * If saved bson does't have _id primary key then `oid` will be set to generated bson _id,
 * otherwise `oid` will be set to the current bson's _id field.
 *
 * NOTE: Field names of passed `bs` object may not contain `$` and `.` characters,
 *       error condition will be fired in this case.
 *
 * @param coll JSON collection handle.
 * @param bson BSON object id pointer.
 * @param oid OID pointer will be set to object's _id
 * @return If successful return true, otherwise return false.
 */
EJDB_EXPORT bool ejdbsavebson(EJCOLL *coll, bson *bs, bson_oid_t *oid);

/**
 * Persist BSON object in the collection.
 * If saved bson does't have _id primary key then `oid` will be set to generated bson _id,
 * otherwise `oid` will be set to the current bson's _id field.
 *
 * NOTE: Field names of passed `bs` object may not contain `$` and `.` characters,
 *       error condition will be fired in this case.
 *
 * @param coll JSON collection handle.
 * @param bs BSON object id pointer.
 * @param oid OID pointer will be set to object's _id
 * @param merge If true the merge will be performend with old and new objects. Otherwise old object will be replaced.
 * @return If successful return true, otherwise return false.
 */
EJDB_EXPORT bool ejdbsavebson2(EJCOLL *jcoll, bson *bs, bson_oid_t *oid, bool merge);

EJDB_EXPORT bool ejdbsavebson3(EJCOLL *jcoll, const void *bsdata, bson_oid_t *oid, bool merge);

/**
 * Remove BSON object from collection.
 * The `oid` argument should points the primary key (_id)
 * of the bson record to be removed.
 * @param coll JSON collection ref.
 * @param oid BSON object id pointer.
 * @return
 */
EJDB_EXPORT bool ejdbrmbson(EJCOLL *coll, bson_oid_t *oid);

/**
 * Load BSON object with specified 'oid'.
 * If loaded bson is not NULL it must be freed by bson_del().
 * @param coll
 * @param oid
 * @return BSON object if exists otherwise return NULL.
 */
EJDB_EXPORT bson* ejdbloadbson(EJCOLL *coll, const bson_oid_t *oid);

/**
 * Create the query object.
 * 
 * Sucessfully created queries must be destroyed with ejdbquerydel().
 * 
 * See the complete query language specification: 
 * 
 *      http://ejdb.org/doc/ql/ql.html
 * 
 * Many query examples can be found in `src/ejdb/test/ejdbtest2.c` test case.
 *
 * @param jb EJDB database handle.
 * @param qobj Main BSON query object.
 * @param orqobjs Array of additional OR query objects (joined with OR predicate).
 * @param orqobjsnum Number of OR query objects.
 * @param hints BSON object with query hints.
 * @return On success return query handle. On error returns NULL.
 */
EJDB_EXPORT EJQ* ejdbcreatequery(EJDB *jb, bson *qobj, bson *orqobjs, int orqobjsnum, bson *hints);


/**
 * Alternative query creation method convenient to use
 * with `ejdbqueryaddor` and `ejdbqueryhints` methods.
 * @param jb EJDB database handle.
 * @param qobj Main query object BSON data.
 * @return On success return query handle. On error returns NULL.
 */
EJDB_EXPORT EJQ* ejdbcreatequery2(EJDB *jb, const void *qbsdata);

/**
 * Add OR restriction to query object.
 * @param jb EJDB database handle.
 * @param q Query handle.
 * @param orbsdata OR restriction BSON data.
 * @return NULL on error.
 */
EJDB_EXPORT EJQ* ejdbqueryaddor(EJDB *jb, EJQ *q, const void *orbsdata);

/**
 * Set hints for the query.
 * @param jb EJDB database handle.
 * @param hintsbsdata Query hints BSON data.
 * @return NULL on error.
 */
EJDB_EXPORT EJQ* ejdbqueryhints(EJDB *jb, EJQ *q, const void *hintsbsdata);

/**
 * Destroy query object created with ejdbcreatequery().
 * @param q
 */
EJDB_EXPORT void ejdbquerydel(EJQ *q);

/**
 * Set index for JSON field in EJDB collection.
 *
 *  - Available index types:
 *      - `JBIDXSTR` String index for JSON string values.
 *      - `JBIDXISTR` Case insensitive string index for JSON string values.
 *      - `JBIDXNUM` Index for JSON number values.
 *      - `JBIDXARR` Token index for JSON arrays and string values.
 *
 *  - One JSON field can have several indexes for different types.
 *
 *  - Available index operations:
 *      - `JBIDXDROP` Drop index of specified type.
 *              - Eg: flag = JBIDXDROP | JBIDXNUM (Drop number index)
 *      - `JBIDXDROPALL` Drop index for all types.
 *      - `JBIDXREBLD` Rebuild index of specified type.
 *      - `JBIDXOP` Optimize index of specified type. (Optimize the B+ tree index file)
 *
 *  Examples:
 *      - Set index for JSON path `addressbook.number` for strings and numbers:
 *          `ejdbsetindex(ccoll, "album.number", JBIDXSTR | JBIDXNUM)`
 *      - Set index for array:
 *          `ejdbsetindex(ccoll, "album.tags", JBIDXARR)`
 *      - Rebuild previous index:
 *          `ejdbsetindex(ccoll, "album.tags", JBIDXARR | JBIDXREBLD)`
 *
 *   Many index examples can be found in `testejdb/t2.c` test case.
 *
 * @param coll Collection handle.
 * @param ipath BSON field path.
 * @param flags Index flags.
 * @return
 */
EJDB_EXPORT bool ejdbsetindex(EJCOLL *coll, const char *ipath, int flags);

/**
 * Execute the query against EJDB collection.
 * It is better to execute update queries with specified `JBQRYCOUNT` control
 * flag avoid unnecessarily rows fetching.
 *
 * @param jcoll EJDB database
 * @param q Query handle created with ejdbcreatequery()
 * @param count Output count pointer. Result set size will be stored into it.
 * @param qflags Execution flag.
 *          * `JBQRYCOUNT` The only count of matching records will be computed
 *                         without resultset, this operation is analog of count(*)
 *                         in SQL and can be faster than operations with resultsets.
 * @param log Optional extended string to collect debug information during query execution, can be NULL.
 * @return TCLIST with matched bson records data.
 * If (qflags & JBQRYCOUNT) then NULL will be returned
 * and only count reported.
 */
EJDB_EXPORT EJQRESULT ejdbqryexecute(EJCOLL *jcoll, const EJQ *q, uint32_t *count, int qflags, TCXSTR *log);

/**
 * Returns the number of elements in the query result set.
 * @param qr Query result set. Can be `NULL` in this case 0 is returned.
 */
EJDB_EXPORT int ejdbqresultnum(EJQRESULT qr);

/**
 * Gets the pointer of query result BSON data buffer at the specified position `pos`.
 * If `qr` is `NULL` or `idx` is put of index range then the `NULL` pointer will be returned.
 * @param qr Query result set object.
 * @param pos Zero based position of the record.
 */
EJDB_EXPORT const void* ejdbqresultbsondata(EJQRESULT qr, int pos, int *size);

/**
 * Disposes the query result set and frees a records buffers.
 */
EJDB_EXPORT void ejdbqresultdispose(EJQRESULT qr);

/**
 * Convenient method to execute update queries.
 *
 * See the complete query language specification: 
 * 
 *      http://ejdb.org/doc/ql/ql.html
 *
 *
 * @return Number of updated records
 */
EJDB_EXPORT uint32_t ejdbupdate(EJCOLL *jcoll, bson *qobj, bson *orqobjs, int orqobjsnum, bson *hints, TCXSTR *log);

/**
 * Provides 'distinct' operation over query 
 * 
 * (http://docs.mongodb.org/manual/reference/method/db.collection.distinct/).
 * 
 * @param jcoll EJDB database collection handle.
 * @param fpath Field path to collect distinct values from.
 * @param qobj Main BSON query object.
 * @param orqobjs Array of additional OR query objects (joined with OR predicate).
 * @param orqobjsnum Number of OR query objects.
 * 
 * NOTE: Queries with update instruction not supported.
 * 
 * @return Unique values by specified path and query (as BSON array)
 */
EJDB_EXPORT bson* ejdbqrydistinct(EJCOLL *jcoll, const char *fpath, bson *qobj, bson *orqobjs, int orqobjsnum, uint32_t *count, TCXSTR *log);

/**
 * Synchronize content of a EJDB collection database with the file on device.
 * @param jcoll EJDB collection.
 * @return On success return true.
 */
EJDB_EXPORT bool ejdbsyncoll(EJCOLL *jcoll);

/**
 * Synchronize entire EJDB database and
 * all of its collections with storage.
 * @param jb Database hand
 */
EJDB_EXPORT bool ejdbsyncdb(EJDB *jb);

/** Begin transaction for EJDB collection. */
EJDB_EXPORT bool ejdbtranbegin(EJCOLL *coll);

/** Commit transaction for EJDB collection. */
EJDB_EXPORT bool ejdbtrancommit(EJCOLL *coll);

/** Abort transaction for EJDB collection. */
EJDB_EXPORT bool ejdbtranabort(EJCOLL *coll);

/** Get current transaction status, it will be placed into txActive*/
EJDB_EXPORT bool ejdbtranstatus(EJCOLL *jcoll, bool *txactive);


/** Gets description of EJDB database and its collections. */
EJDB_EXPORT bson* ejdbmeta(EJDB *jb);

/** Export/Import settings used in `ejdbexport()` and `ejdbimport()` functions. */
enum {
    JBJSONEXPORT = 1, //Database collections will be exported as JSON files.
    JBIMPORTUPDATE = 1 << 1, //Update existing collection entries with imported ones. Missing collections will be created.
    JBIMPORTREPLACE = 1 << 2 //Recreate all collections and replace all collection data with imported entries.
};

/**
 * Exports database collections data to the specified directory.
 * Database read lock will be taken on each collection.
 *
 * NOTE: Only data exported as BSONs can be imported with `ejdbimport()`
 *
 * @param jb EJDB database handle.
 * @param path The directory path in which data will be exported.
 * @param cnames List of collection names to export. `NULL` implies that all existing collections will be exported.
 * @param flags. Can be set to `JBJSONEXPORT` in order to export data as JSON files instead exporting into BSONs.
 * @param log Optional operation string log buffer.
 * @return on sucess `true`
 */
EJDB_EXPORT bool ejdbexport(EJDB *jb, const char *path, TCLIST *cnames, int flags, TCXSTR *log);

/**
 * Imports previously exported collections data into ejdb.
 * Global database write lock will be applied during import operation.
 *
 * NOTE: Only data exported as BSONs can be imported with `ejdbimport()`
 *
 * @param jb EJDB database handle.
 * @param path The directory path in which data resides.
 * @param cnames List of collection names to import. `NULL` implies that all collections found in `path` will be imported.
 * @param flags Can be one of:
 *             `JBIMPORTUPDATE`  Update existing collection entries with imported ones.
 *                               Existing collections will not be recreated.
 *                               For existing collections options will not be imported.
 *
 *             `JBIMPORTREPLACE` Recreate existing collections and replace
 *                               all their data with imported entries.
 *                               Collections options will be imported.
 *
 *             `0`              Implies `JBIMPORTUPDATE`
 * @param log Optional operation log buffer.
 * @return
 */
EJDB_EXPORT bool ejdbimport(EJDB *jb, const char *path, TCLIST *cnames, int flags, TCXSTR *log);

/**
 * Execute the ejdb database command.
 *
 * Supported commands:
 *
 *
 *  1) Exports database collections data. See ejdbexport() method.
 *
 *    "export" : {
 *          "path" : string,                    //Export files target directory
 *          "cnames" : [string array]|null,     //List of collection names to export
 *          "mode" : int|null                   //Values: null|`JBJSONEXPORT` See ejdbexport() method
 *    }
 *
 *    Command response:
 *       {
 *          "log" : string,        //Diagnostic log about executing this command
 *          "error" : string|null, //ejdb error message
 *          "errorCode" : int|0,   //ejdb error code
 *       }
 *
 *  2) Imports previously exported collections data into ejdb.
 *
 *    "import" : {
 *          "path" : string                  //Import files source directory
 *          "cnames" : [string array]|null,  //List of collection names to import
 *          "mode" : int|null                //Values: null|`JBIMPORTUPDATE`|`JBIMPORTREPLACE` See ejdbimport() method
 *     }
 *
 *     Command response:
 *       {
 *          "log" : string,        //Diagnostic log about executing this command
 *          "error" : string|null, //ejdb error message
 *          "errorCode" : int|0,   //ejdb error code
 *       }
 *
 * @param jb    EJDB database handle.
 * @param cmd   BSON command spec.
 * @return Allocated command response BSON object. Caller should call `bson_del()` on it.
 */
EJDB_EXPORT bson* ejdbcommand(EJDB *jb, bson *cmdbson);
EJDB_EXPORT bson* ejdbcommand2(EJDB *jb, void *cmdbsondata);

EJDB_EXTERN_C_END

#endif        /* EJDB_H */

