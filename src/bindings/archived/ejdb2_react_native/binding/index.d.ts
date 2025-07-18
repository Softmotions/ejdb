/**************************************************************************************************
 * EJDB2
 *
 * MIT License
 *
 * Copyright (c) 2012-2022 Softmotions Ltd <info@softmotions.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *************************************************************************************************/

declare namespace ejdb2_react_native {
  export type Placeholder = string | number;

  export type JQLExecuteCallback = (doc: JBDOC, jql?: JQL) => any;

  interface JQLUseQueryCallback<T> {
    (jql: JQL): Promise<T>;
  }

  /**
   * EJDB2 Error helpers.
   */
  export class JBE {
    /**
     * Returns `true` if given error [err] is `IWKV_ERROR_NOTFOUND`
     * @param err
     */
    static isNotFound(err: any): boolean;

    /**
     * Returns `true` if given errror [err] is `JQL_ERROR_QUERY_PARSE`
     * @param {Error} err
     * @return {boolean}
     */
    static isInvalidQuery(err: any): boolean;
  }

  /**
   * Database open options.
   */
  interface OpenOptions {
    /**
     * Open databas in read-only mode.
     */
    readonly?: boolean;

    /**
     * Truncate database file on open.
     */
    truncate?: boolean;

    /**
     * Enable WAL. Default: true.
     */
    wal_enabled?: boolean;

    /**
     * Check CRC32 sum for every WAL checkpoint.
     * Default: false.
     */
    wal_check_crc_on_checkpoint?: boolean;

    /**
     * Size of checkpoint buffer in bytes.
     * Default: 1Gb. Android: 64Mb
     */
    wal_checkpoint_buffer_sz?: number;

    /**
     * WAL buffer size in bytes.
     * Default: 8Mb. Android: 2Mb.
     */
    wal_wal_buffer_sz?: number;

    /**
     * Checkpoint timeout in seconds.
     * Default: 300. Android: 60.
     */
    wal_checkpoint_timeout_sec?: number;

    /**
     * Savepoint timeout in secods.
     * Default: 10.
     */
    wal_savepoint_timeout_sec?: number;

    /**
     * Initial size of buffer in bytes used to process/store document on queries.
     * Preferable average size of document.
     * Default: 65536. Minimal: 16384.
     */
    document_buffer_sz?: number;

    /**
     * Max sorting buffer size in bytes.
     * If exceeded, an overflow temp file for data will be created.
     * Default: 16777216. Minimal: 1048576
     */
    sort_buffer_sz?: number;
  }

  /**
   * Query execution options.
   */
  interface QueryOptions {
    /**
     * Overrides `limit` encoded in query
     */
    limit?: number | string;

    /**
     *  Overrides `skip` encoded in query
     */
    skip?: number | string;
  }

  /**
   * Collection index descriptor.
   */
  interface IndexDescriptor {
    /**
     * rfc6901 JSON pointer to indexed field.
     */
    ptr: string;

    /**
     * Index mode as bit mask:
     *
     * - 0x01 EJDB_IDX_UNIQUE 	Index is unique
     * - 0x04 EJDB_IDX_STR 	Index for JSON string field value type
     * - 0x08 EJDB_IDX_I64 	Index for 8 bytes width signed integer field values
     * - 0x10 EJDB_IDX_F64 	Index for 8 bytes width signed floating point field values.
     */
    mode: number;

    /**
     * Index flags. See iwkv.h#iwdb_flags_t
     */
    idbf: number;

    /**
     * Internal index database identifier.
     */
    dbid: number;

    /**
     * Number of indexed records.
     */
    rnum: number;
  }

  /**
   * Collection descriptor.
   */
  interface CollectionDescriptor {
    /**
     * Name of collection.
     */
    name: string;

    /**
     * Internal database identifier.
     */
    dbid: number;

    /**
     * Number of documents stored in collection.
     */
    rnum: number;

    /**
     * List of collection indexes.
     */
    indexes: Array<IndexDescriptor>;
  }

  interface EJDB2Info {
    /**
     * Database engine version string.
     * Eg. "2.0.29"
     */
    version: string;

    /**
     * Database file path.
     */
    file: string;

    /**
     * Database file size in bytes.
     */
    size: number;

    /**
     * List of database collections.
     */
    collections: Array<CollectionDescriptor>;
  }

  /**
   * EJDB2 document.
   */
  interface JBDOC {
    /*
     * Document identifier
     */
    id: number;

    /**
     * Document JSON object
     */
    json: any;

    /**
     * String represen
     */
    toString(): string;
  }

  /**
   * Database wrapper.
   */
  export class EJDB2 {
    /**
     * Open database identified by relative `filename` in context of
     * React Native database directory used for sqlite db.
     *
     * @param filename `filename` in context of RN sqlite database directory
     * @param opts Database mode options
     */
    static open(name: string, opts?: OpenOptions): EJDB2;

    /**
     * Closes database instance.
     */
    close(): Promise<void>;

    /**
     * Get json body with database metadata object.
     */
    info(): Promise<EJDB2Info>;

    /**
     * Create instance of [query] specified for [collection].
     * If [collection] is not specified a [query] spec must contain collection name,
     * eg: `@mycollection/[foo=bar]`
     *
     * @note WARNING In order to avoid memory leaks, dispose created query object by {@link JQL#close}
     *       or use {@link JQL#use}
     *
     * @param query Query text
     * @param collection Optional collection used in query
     * @returns {JQL}
     */
    createQuery(query: string, collection?: string): JQL;

    /**
     * Saves [json] document under specified [id] or create a document
     * with new generated `id`. Returns promise holding actual document `id`.
     */
    put(collection: String, json: object | string, id?: number): Promise<number>;

    /**
     * Apply rfc6902/rfc7386 JSON [patch] to the document identified by [id].
     */
    patch(collection: string, json: object | string, id: number): Promise<void>;

    /**
     * Apply JSON merge patch (rfc7396) to the document identified by `id` or
     * insert new document under specified `id`.
     */
    patchOrPut(collection: string, json: object | string, id: number): Promise<void>;

    /**
     * Get json body of document identified by [id] and stored in [collection].
     *
     * If document with given `id` is not found then `Error` will be thrown.
     * Not found error can be detected by {@link JBE.isNotFound}
     */
    get(collection: string, id: number): Promise<object>;

    /**
     * Get json body of document identified by [id] and stored in [collection].
     *
     * If document with given `id` is not found then `null` will be resoved.
     */
    getOrNull(collection: string, id: number): Promise<object|null>;

    /**
     * Removes document idenfied by [id] from [collection].
     *
     * If document with given `id` is not found then `Error` will be thrown.
     * Not found error can be detected by {@link JBE.isNotFound}
     */
    del(collection: string, id: number): Promise<void>;

    /**
     * Renames collection
     */
    renameCollection(oldCollectionName: string, newCollectionName: string): Promise<void>;

    /**
     * Ensures json document database index specified by [path] json pointer to string data type.
     */
    ensureStringIndex(collection: string, path: string, unique?: boolean): Promise<void>;

    /**
     * Removes specified database index.
     */
    removeStringIndex(collection: string, path: string, unique?: boolean): Promise<void>;

    /**
     * Ensures json document database index specified by [path] json pointer to integer data type.
     */
    ensureIntIndex(collection: string, path: string, unique?: boolean): Promise<void>;

    /**
     * Removes specified database index.
     */
    removeIntIndex(collection: string, path: string, unique?: boolean): Promise<void>;

    /**
     * Ensures json document database index specified by [path] json pointer to floating point data type.
     */
    ensureFloatIndex(collection: string, path: string, unique?: boolean): Promise<void>;

    /**
     * Removes specified database index.
     */
    removeFloatIndex(collection: string, path: string, unique?: boolean): Promise<void>;

    /**
     * Removes database [collection].
     */
    removeCollection(collection: string): Promise<void>;

    /**
     * Creates an online database backup image and copies it into the specified [fileName].
     * During online backup phase read/write database operations are allowed and not
     * blocked for significant amount of time. Returns promise with backup
     * finish time as number of milliseconds since epoch.
     */
    onlineBackup(fileName: string): Promise<number>;
  }

  /**
   * EJDB Query.
   */
  interface JQL {
    /**
     * Database instance associated with this query.
     */
    readonly db: EJDB2;

    /**
     * Query text.
     */
    readonly query: string;

    /**
     * Retrieve query execution plan.
     */
    readonly explainLog: string | null;

    /**
     * Number of documents to skip.
     */
    readonly skip: number;

    /**
     * Maximum number of documents to fetch.
     */
    readonly limit: number;

    /**
     * Collection name used in query
     */
    readonly collection: string;

    /**
     * Set number of documents to skip.
     * Specified value overrides `skip` option encoded in query.
     * @param {number} skip
     */
    withSkip(skip: number): JQL;

    /**
     * Set maximum number of documents to fetch.
     * Specified value overrides `limit` option encoded in query.
     * @param {number} limit Limit
     */
    withLimit(limit: number): JQL;

    /**
     * Set collection to use with query.
     * Value overrides collection encoded in query.
     */
    withCollection(coll: string): JQL;

    /**
     * Turn on explain query mode
     */
    withExplain(): JQL;

    /**
     * Set string [val] at the specified [placeholder].
     */
    setString(placeholder: Placeholder, val: string): JQL;

    /**
     * Set [json] at the specified [placeholder].
     */
    setJSON(placeholder: Placeholder, val: string | object): JQL;

    /**
     * Set [regexp] string at the specified [placeholder].
     * @note `RegExp` options are ignored.
     */
    setRegexp(placeholder: Placeholder, val: string | RegExp): JQL;

    /**
     * Set number [val] at the specified [placeholder].
     */
    setNumber(placeholder: Placeholder, val: number): JQL;

    /**
     * Set boolean [val] at the specified [placeholder].
     */
    setBoolean(placeholder: Placeholder, val: boolean): JQL;

    /**
     * Set `null` at the specified [placeholder].
     */
    setNull(placeholder: Placeholder): JQL;

    /**
     * Execute this query.
     */
    execute(dispose: boolean, callback: JQLExecuteCallback): Promise<void>;

    /**
     * Execute this query then dispose it.
     */
    useExecute(callback: JQLExecuteCallback): Promise<void>;

    /**
     * Uses query in `callback` then closes it.
     * @param callback Function accepts `JQL` query object.
     * @return `usefn` promise value.
     */
    use<T>(callback: JQLUseQueryCallback<T>): Promise<T>;

    /**
     * List matched document.
     * @note Use it with care to avoid wasting of memory.
     *       See {@link JQL#use} for result-set callback API.
     * @param opts
     */
    list(opts: QueryOptions): Promise<JBDOC[]>;

    /**
     * Collects up to [n] documents from result set into array.
     * @param n Default: 1
     */
    firstN(n?: number): Promise<JBDOC[]>;

    /**
     * Returns a first record in result set.
     * If no reconds found `null` resolved promise will be returned.
     */
    first(): Promise<JBDOC | null>;

    /**
     * Returns a scalar integer value as result of query execution.
     * Eg.: A count query: `/... | count`
     */
    scalarInt(): Promise<number>;

    /**
     * Reset query parameters.
     */
    reset(): Promise<void>;

    /**
     * Disposes JQL instance and releases all underlying resources.
     */
    close(): void;
  }
}

export = ejdb2_react_native;
