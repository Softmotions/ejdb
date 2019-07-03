/**************************************************************************************************
 * EJDB2
 *
 * MIT License
 *
 * Copyright (c) 2012-2019 Softmotions Ltd <info@softmotions.com>
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

/// <reference types="node"/>

declare namespace ejdb2_node {

  export type Placeholder = string | number;

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
   * EJDB document.
   */
  interface JBDOC {

    /**
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

  interface JBDOCStream extends NodeJS.ReadableStream {
  }

  /**
   * Query execution options.
   */
  interface QueryOptions {

    /**
     * Overrides `limit` encoded in query text
     */
    limit?: number,

    /**
     * Calback used to get query execution log.
     */
    explainCallback?: (log: string) => void;
  }

  /**
   * EJDB Query.
   */
  class JQL {

    /**
     * Database to which query attached.
     */
    db: EJDB2;

    /**
     * Get `limit` value used by query.
     */
    limit: number

    /**
     * Executes a query and returns a
     * readable stream of matched documents.
     *
     */
    stream(opts?: QueryOptions): JBDOCStream;

    /**
     * Executes this query and waits its completion.
     */
    completionPromise(opts?: QueryOptions): Promise<void>

    /**
     * Returns a scalar integer value as result of query execution.
     * Eg.: A count query: `/... | count`
     */
    scalarInt(opts?: QueryOptions): Promise<number>

    /**
     * Returns result set as a list.
     * Use it with caution on large data sets.
     */
    list(opts?: QueryOptions): Promise<Array<JBDOC>>

    /**
     * Collects up to [n] documents from result set into array.
     */
    firstN(n: number, opts?: QueryOptions): Promise<Array<JBDOC>>

    /**
     * Returns a first record in result set.
     * If record is not found promise with `undefined` will be returned.
     */
    first(opts?: QueryOptions): Promise<JBDOC | undefined>

    /**
     * Set [json] at the specified [placeholder].
     */
    setJSON(placeholder: Placeholder, val: object | string): JQL

    /**
     * Set [regexp] string at the specified [placeholder].
     */
    setRegexp(placeholder: Placeholder, val: string): JQL

    /**
     * Set number [val] at the specified [placeholder].
     */
    setNumber(placeholder: Placeholder, val: number): JQL

    /**
     * Set boolean [val] at the specified [placeholder].
     */
    setBoolean(placeholder: Placeholder, val: boolean): JQL

    /**
     * Set string [val] at the specified [placeholder].
     */
    setString(placeholder: Placeholder, val: string): JQL

    /**
     * Set `null` at the specified [placeholder].
     */
    setNull(placeholder: Placeholder): JQL
  }

  interface OpenOptions {
    readonly?: boolean;
    truncate?: boolean;
    wal_enabled?: boolean;
    wal_check_crc_on_checkpoint?: boolean;
    wal_checkpoint_buffer_sz?: number;
    wal_checkpoint_timeout_sec?: number;
    wal_savepoint_timeout_sec?: number;
    wal_wal_buffer_sz?: number;
    document_buffer_sz?: number;
    sort_buffer_sz?: number;
    http_enabled?: boolean;
    http_access_token?: string;
    http_bind?: string;
    http_max_body_size?: number;
    http_port?: number;
    http_read_anon?: boolean;
  }

  /**
   * EJDB2 Node.js wrapper.
   */
  export class EJDB2 {

    /**
     * Open databse instance.
     * @param path Database file path
     * @param opts Options
     */
    static open(path: String, opts?: OpenOptions): EJDB2;

    /**
     * Closes this database instance.
     */
    close(): Promise<void>

    /**
     * Saves [json] document under specified [id] or create a document
     * with new generated `id`. Returns promise holding actual document `id`.
     */
    put(collection: String, json: object | string, id?: number): Promise<number>

    /**
     * Apply rfc6902/rfc6901 JSON [patch] to the document identified by [id].
     */
    patch(collection: string, json: object | string, id: number): Promise<void>

    /**
     * Get json body of document identified by [id] and stored in [collection].
     *
     * If document with given `id` is not found then `Error` will be thrown.
     * Not found error can be detected by {@link JBE.isNotFound}
     */
    get(collection: string, id: number): Promise<object>

    /**
     * Get json body with database metadata.
     */
    info(): Promise<object>

    /**
     * Removes document idenfied by [id] from [collection].
     *
     * If document with given `id` is not found then `Error` will be thrown.
     * Not found error can be detected by {@link JBE.isNotFound}
     */
    del(collection: string, id: number): Promise<void>;

    /**
     * Ensures json document database index specified by [path] json pointer to string data type.
     */
    ensureStringIndex(collection: string, path: string, unique?: boolean): Promise<void>

    /**
     * Removes specified database index.
     */
    removeStringIndex(collection: string, path: string, unique?: boolean): Promise<void>;

    /**
     * Ensures json document database index specified by [path] json pointer to integer data type.
     */
    ensureIntIndex(collection: string, path: string, unique?: boolean): Promise<void>

    /**
     * Removes specified database index.
     */
    removeIntIndex(collection: string, path: string, unique?: boolean): Promise<void>

    /**
     * Ensures json document database index specified by [path] json pointer to floating point data type.
     */
    ensureFloatIndex(collection: string, path: string, unique?: boolean): Promise<void>

    /**
     * Removes specified database index.
     */
    removeFloatIndex(collection: string, path: string, unique?: boolean): Promise<void>

    /**
     * Removes database [collection].
     */
    removeCollection(collection: string): Promise<void>

    /**
     * Create instance of [query] specified for [collection].
     * If [collection] is not specified a [query] spec must contain collection name,
     * eg: `@mycollection/[foo=bar]`
     */
    createQuery(query: string, collection?: string): JQL
  }
}

export = ejdb2_node;