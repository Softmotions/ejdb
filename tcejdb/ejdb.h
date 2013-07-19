/**************************************************************************************************
 *  C/C++ API for EJDB database library http://ejdb.org
 *  Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
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

typedef struct { /**< EJDB collection tuning options. */
    bool large; /**< Large collection. It can be larger than 2GB. Default false */
    bool compressed; /**< Collection records will be compressed with DEFLATE compression. Default: false */
    int64_t records; /**< Expected records number in the collection. Default: 128K */
    int cachedrecords; /**< Maximum number of cached records. Default: 0 */
} EJCOLLOPTS;


typedef TCLIST* EJQRESULT; /**< EJDB query result */

#define JBMAXCOLNAMELEN 128

enum { /** Error codes */
    JBEINVALIDCOLNAME = 9000, /**< Invalid collection name. */
    JBEINVALIDBSON = 9001, /**< Invalid bson object. */
    JBEINVALIDBSONPK = 9002, /**< Invalid bson object id. */
    JBEQINVALIDQCONTROL = 9003, /**< Invalid query control field starting with '$'. */
    JBEQINOPNOTARRAY = 9004, /**< $strand, $stror, $in, $nin, $bt keys requires not empty array value. */
    JBEMETANVALID = 9005, /**< Inconsistent database metadata. */
    JBEFPATHINVALID = 9006, /**< Invalid field path value. */
    JBEQINVALIDQRX = 9007, /**< Invalid query regexp value. */
    JBEQRSSORTING = 9008, /**< Result set sorting error. */
    JBEQERROR = 9009, /**< Query generic error. */
    JBEQUPDFAILED = 9010, /**< Updating failed. */
    JBEQONEEMATCH = 9011, /**< Only one $elemMatch allowed in the fieldpath. */
    JBEQINCEXCL = 9012, /**< $fields hint cannot mix include and exclude fields */
    JBEQACTKEY = 9013, /**< action key in $do block can only be one of: $join */
    JBEMAXNUMCOLS = 9014, /**< Exceeded the maximum number of collections per database */
    JBEEI = 9015, /**< EJDB export/import error */
    JBEEJSONPARSE = 9016, /**< JSON parsing failed */
    JBETOOBIGBSON = 9017, /**< BSON size is too big */
    JBEINVALIDCMD = 9018 /**< Invalid ejdb command specified */
};

enum { /** Database open modes */
    JBOREADER = 1 << 0, /**< Open as a reader. */
    JBOWRITER = 1 << 1, /**< Open as a writer. */
    JBOCREAT = 1 << 2, /**< Create if db file not exists. */
    JBOTRUNC = 1 << 3, /**< Truncate db on open. */
    JBONOLCK = 1 << 4, /**< Open without locking. */
    JBOLCKNB = 1 << 5, /**< Lock without blocking. */
    JBOTSYNC = 1 << 6 /**< Synchronize every transaction. */
};

enum { /** Index modes, index types. */
    JBIDXDROP = 1 << 0, /**< Drop index. */
    JBIDXDROPALL = 1 << 1, /**< Drop index for all types. */
    JBIDXOP = 1 << 2, /**< Optimize indexes. */
    JBIDXREBLD = 1 << 3, /**< Rebuild index. */
    JBIDXNUM = 1 << 4, /**< Number index. */
    JBIDXSTR = 1 << 5, /**< String index.*/
    JBIDXARR = 1 << 6, /**< Array token index. */
    JBIDXISTR = 1 << 7 /**< Case insensitive string index */
};

enum { /*< Query search mode flags in ejdbqryexecute() */
    JBQRYCOUNT = 1, /*< Query only count(*) */
    JBQRYFINDONE = 1 << 1 /*< Fetch first record only */
};

/**
 * Returns EJDB library version string. Eg: "1.1.13"
 */
EJDB_EXPORT const char *ejdbversion();

/**
 * Return true if passed `oid` string cat be converted to valid
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
 * Create query object.
 * Sucessfully created queries must be destroyed with ejdbquerydel().
 *
 * EJDB queries inspired by MongoDB (mongodb.org) and follows same philosophy.
 *
 *  - Supported queries:
 *      - Simple matching of String OR Number OR Array value:
 *          -   {'fpath' : 'val', ...}
 *      - $not Negate operation.
 *          -   {'fpath' : {'$not' : val}} //Field not equal to val
 *          -   {'fpath' : {'$not' : {'$begin' : prefix}}} //Field not begins with val
 *      - $begin String starts with prefix
 *          -   {'fpath' : {'$begin' : prefix}}
 *      - $gt, $gte (>, >=) and $lt, $lte for number types:
 *          -   {'fpath' : {'$gt' : number}, ...}
 *      - $bt Between for number types:
 *          -   {'fpath' : {'$bt' : [num1, num2]}}
 *      - $in String OR Number OR Array val matches to value in specified array:
 *          -   {'fpath' : {'$in' : [val1, val2, val3]}}
 *      - $nin - Not IN
 *      - $strand String tokens OR String array val matches all tokens in specified array:
 *          -   {'fpath' : {'$strand' : [val1, val2, val3]}}
 *      - $stror String tokens OR String array val matches any token in specified array:
 *          -   {'fpath' : {'$stror' : [val1, val2, val3]}}
 *      - $exists Field existence matching:
 *          -   {'fpath' : {'$exists' : true|false}}
 *      - $icase Case insensitive string matching:
 *          -    {'fpath' : {'$icase' : 'val1'}} //icase matching
 *          Ignore case matching with '$in' operation:
 *          -    {'name' : {'$icase' : {'$in' : ['théâtre - театр', 'hello world']}}}
 *          For case insensitive matching you can create special index of type: `JBIDXISTR`
 *      - $elemMatch The $elemMatch operator matches more than one component within an array element.
 *          -    { array: { $elemMatch: { value1 : 1, value2 : { $gt: 1 } } } }
 *          Restriction: only one $elemMatch allowed in context of one array field.
 *
 *  - Queries can be used to update records:
 *
 *      $set Field set operation.
 *          - {.., '$set' : {'field1' : val1, 'fieldN' : valN}}
 *      $upsert Atomic upsert. If matching records are found it will be '$set' operation,
 *              otherwise new record will be inserted
 *              with fields specified by argment object.
 *          - {.., '$upsert' : {'field1' : val1, 'fieldN' : valN}}
 *      $inc Increment operation. Only number types are supported.
 *          - {.., '$inc' : {'field1' : number, ...,  'field1' : number}
 *      $dropall In-place record removal operation.
 *          - {.., '$dropall' : true}
 *      $addToSet Atomically adds value to the array only if its not in the array already.
 *                If containing array is missing it will be created.
 *          - {.., '$addToSet' : {'fpath' : val1, 'fpathN' : valN, ...}}
 *      $addToSetAll Batch version if $addToSet
 *          - {.., '$addToSetAll' : {'fpath' : [array of values to add], ...}}
 *      $pull  Atomically removes all occurrences of value from field, if field is an array.
 *          - {.., '$pull' : {'fpath' : val1, 'fpathN' : valN, ...}}
 *      $pullAll Batch version of $pull
 *          - {.., '$pullAll' : {'fpath' : [array of values to remove], ...}}
 *
 * - Collection joins supported in the following form:
 *
 *      {..., $do : {fpath : {$join : 'collectionname'}} }
 *      Where 'fpath' value points to object's OIDs from 'collectionname'. Its value
 *      can be OID, string representation of OID or array of this pointers.
 *
 *  NOTE: It is better to execute update queries with `JBQRYCOUNT`
 *        control flag to avoid unnecessarily data fetching.
 *
 *  NOTE: Negate operations: $not and $nin not using indexes
 *  so they can be slow in comparison to other matching operations.
 *
 *  NOTE: Only one index can be used in search query operation.
 *
 *  QUERY HINTS (specified by `hints` argument):
 *      - $max Maximum number in the result set
 *      - $skip Number of skipped results in the result set
 *      - $orderby Sorting order of query fields.
 *      - $fields Set subset of fetched fields
 *          If a field presented in $orderby clause it will be forced to include in resulting records.
 *          Example:
 *          hints:    {
 *                      "$orderby" : { //ORDER BY field1 ASC, field2 DESC
 *                          "field1" : 1,
 *                          "field2" : -1
 *                      },
 *                      "$fields" : { //SELECT ONLY {_id, field1, field2}
 *                          "field1" : 1,
 *                          "field2" : 1
 *                      }
 *                    }
 *
 * Many query examples can be found in `testejdb/t2.c` test case.
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
 * `$set` and `$inc` operations are supported:
 *
 * `$set` Field set operation:
 *      - {some fields for selection, '$set' : {'field1' : {obj}, ...,  'field1' : {obj}}}
 * `$inc` Increment operation. Only number types are supported.
 *      - {some fields for selection, '$inc' : {'field1' : number, ...,  'field1' : {number}}
 *
 * @return Number of updated records
 */
EJDB_EXPORT uint32_t ejdbupdate(EJCOLL *jcoll, bson *qobj, bson *orqobjs, int orqobjsnum, bson *hints, TCXSTR *log);

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
    JBJSONEXPORT = 1, //If set json collection data will be exported as JSON files instead of BSON.
    JBIMPORTUPDATE = 1 << 1, //Update existing collection entries with imported ones. Collections will not be recreated and its options are ignored.
    JBIMPORTREPLACE = 1 << 2 //Recreate existing collections and replace all collection data with imported entries.
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
 *          "path" : string,                    //Exports database collections data
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
 *          "path" : string                     //The directory path in which data resides
 *          "cnames" : [string array]|null,     //List of collection names to import
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
 * @return Allocated BSON command response object. Caller should call `bson_del()` on it.
 */
EJDB_EXPORT bson* ejdbcommand(EJDB *jb, bson *cmdbson);
EJDB_EXPORT bson* ejdbcommand2(EJDB *jb, void *cmdbsondata);

EJDB_EXTERN_C_END

#endif        /* EJDB_H */

