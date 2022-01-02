/**************************************************************************************************
 * EJDB2 Node.js native API binding.
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

const semver = require('semver');
const {engines} = require('./package');

if (!semver.satisfies(process.version, engines.node)) {
  console.log(`Required node version ${engines.node} not satisfied with current version ${process.version}.`);
  process.exit(1);
}

global.__ejdb_add_stream_result__ = addStreamResult; // Passing it to ejdb2_node init
const {EJDB2Impl} = require('./binary')('ejdb2_node');
const {Readable} = require('stream');
delete global.__ejdb_add_stream_result__;

/**
 * EJDB2 Error helpers.
 */
class JBE {

  /**
   * Returns `true` if given error [err] is `IWKV_ERROR_NOTFOUND`
   * @param {Error} err
   * @returns {boolean}
   */
  static isNotFound(err) {
    const code = (err.code || '').toString();
    return code.indexOf('@ejdb IWRC:75001') == 0;
  }

  /**
   * Returns `true` if given errror [err] is `JQL_ERROR_QUERY_PARSE`
   * @param {Error} err
   * @return {boolean}
   */
  static isInvalidQuery(err) {
    const code = (err.code || '').toString();
    return code.indexOf('@ejdb IWRC:87001') == 0;
  }
}

/**
 * EJDB document.
 */
class JBDOC {

  /**
   * Get document JSON object
   */
  get json() {
    if (this._json != null) {
      return this._json;
    }
    this._json = JSON.parse(this._raw);
    this._raw = null;
    return this._json;
  }

  /**
   * @param {number} id Document ID
   * @param {string} raw Document JSON as string
   */
  constructor(id, raw) {
    this.id = id;
    this._raw = raw;
    this._json = null;
  }

  toString() {
    return `JBDOC: ${this.id} ${this._raw != null ? this._raw : JSON.stringify(this.json)}`;
  }
}

/**
 * EJDB Query resultset stream.
 */
class JBDOCStream extends Readable {

  get _impl() {
    return this.jql._impl;
  }

  get writable() {
    return !this._paused && !this._destroyed;
  }

  /**
   * Query result stream.
   * @param {JQL} jql
   */
  constructor(jql, opts) {
    super({
      objectMode: true,
      highWaterMark: 64
    });
    this._pending = [];
    this._paused = true;
    this._destroyed = false;
    this._aborted = false;
    this.jql = jql;
    this.opts = opts;
    this.promise = this._impl.jql_stream_attach(jql, this, [opts.limit, opts.explainCallback])
                       .catch((err) => this.destroy(err));
  }

  abort() {
    // unpause if paused
    // needed to release frozen on pause db query thread
    this._doResume();
    // set internal abort state
    this._impl.jql_stream_abort(this);
    this._aborted = true;
  }

  /**
   * Destroy stream
   */
  _destroy(err, callback) {
    if (!this._destroyed) {
      this.abort();
      this._destroyed = true;
      setImmediate(() => this._impl.jql_stream_destroy(this));
    }
    callback(err);
  }

  _read() {
    if (this._destroyed) {
      return;
    }
    this._doResume();
    if (this._pending.length > 0) {
      // Flush pending records to consumer
      // Maintaining pending list since `napi_threadsafe_function` queue
      // may be not empty at the time of `jql_stream_pause` call
      let pv = this._pending.shift();
      while (this.writable && pv) {
        if (pv.length == 0) { // Got pending EOF
          this._pending.length = 0;
          this.destroy();
          break;
        }
        addStreamResult(this, pv[0], pv[1]);
        pv = this._pending.shift();
      }
    }
  }

  _doResume() {
    if (this._paused) {
      this._impl.jql_stream_resume(this);
      this._paused = false;
    }
  }

  _doPause() {
    if (!this._paused) {
      this._impl.jql_stream_pause(this);
      this._paused = true;
    }
  }
}

// Global module function for add results to query stream
function addStreamResult(stream, id, jsondoc, log) {
  if (stream._destroyed) {
    return;
  }
  if (log != null && stream.opts.explainCallback != null) {
    stream.opts.explainCallback(log);
    delete stream.opts.explainCallback;
  }

  const count = (typeof jsondoc === 'number');
  if (id >= 0 || count) {
    if (!stream._aborted) {
      if (stream._paused) {
        // Maintaining pending list since `napi_threadsafe_function` queue
        // may be not empty at the time of `jql_stream_pause` call
        stream.pending.push([id, jsondoc]);
        return;
      }
      let doc;
      if (count) { // count int response
        doc = new JBDOC(jsondoc, jsondoc);
      } else if (jsondoc != null) {
        doc = new JBDOC(id, jsondoc);
      }
      if (doc != null && stream.push(doc) == false) {
        stream._doPause();
      }
    }
  }

  if (id < 0) { // last record
    if (!stream._aborted && stream._paused) {
      stream.pending.push([]);
    } else {
      stream.destroy();
    }
  }
}

/**
 * EJDB Query.
 */
class JQL {

  get _impl() {
    return this.db._impl;
  }

  /**
   * Get `limit` value used by query.
   */
  get limit() {
    return this._impl.jql_limit(this);
  }

  /**
   * @param {EJDB2} db
   * @param {string} query
   * @param {string} collection
   */
  constructor(db, query, collection) {
    this.db = db;
    this.query = query;
    this.collection = collection;
    this._impl.jql_init(this, query, collection);
  }

  /**
   * Executes a query and returns a
   * readable stream of matched documents.
   *
   * @param {Object} [opts]
   * @return {ReadableStream<JBDOC>}
   */
  stream(opts) {
    return new JBDOCStream(this, opts || {});
  }

  /**
   * Executes this query and waits its completion.
   *
   * @param {Promise} opts
   */
  completionPromise(opts) {
    const stream = this.stream(opts || {});
    return new Promise((resolve, reject) => {
      stream.on('data', () => stream.destroy());
      stream.on('close', () => resolve());
      stream.on('error', (err) => reject(err));
    });
  }

  /**
   * Returns a scalar integer value as result of query execution.
   * Eg.: A count query: `/... | count`
   * @param {Object} [opts]
   * @return {Promise<number>}
   */
  scalarInt(opts) {
    const stream = this.stream(opts);
    return new Promise((resolve, reject) => {
      stream.on('data', (doc) => {
        resolve(doc.id);
        stream.destroy();
      });
      stream.on('error', (err) => reject(err));
    });
  }

  /**
   * Returns result set as a list.
   * Use it with caution on large data sets.
   *
   * @param {Object} [opts]
   * @return {Promise<Array<JBDOC>>}
   */
  list(opts) {
    const ret = [];
    const stream = this.stream(opts);
    return new Promise((resolve, reject) => {
      stream.on('data', (doc) => ret.push(doc));
      stream.on('close', () => resolve(ret));
      stream.on('error', (err) => reject(err));
    });
  }

  /**
   * Collects up to [n] documents from result set into array.
   * @param {number} n
   * @param {Object} [opts]
   * @return {Promise<Array<JBDOC>>}
   */
  firstN(n, opts) {
    opts = opts || {};
    opts.limit = n;
    const ret = [];
    const stream = this.stream(opts);
    return new Promise((resolve, reject) => {
      stream.on('data', (doc) => {
        ret.push(doc);
        if (ret.length >= n) {
          stream.destroy();
        }
      });
      stream.on('close', () => resolve(ret));
      stream.on('error', (err) => reject(err));
    });
  }

  /**
   * Returns a first record in result set.
   * If record is not found promise with `undefined` will be returned.
   *
   * @param {Object} [opts]
   * @return {Promise<JBDOC|undefined>}
   */
  async first(opts) {
    const fv = await this.firstN(1, opts);
    return fv[0];
  }

  /**
   * Set [json] at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {string|object} val
   * @return {JQL}
   */
  setJSON(placeholder, val) {
    this._checkPlaceholder(placeholder);
    if (typeof val !== 'string') {
      val = JSON.stringify(val);
    }
    this._impl.jql_set(this, placeholder, val, 1);
    return this;
  }

  /**
   * Set [regexp] string at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {string|RegExp} val
   * @return {JQL}
   */
  setRegexp(placeholder, val) {
    this._checkPlaceholder(placeholder);
    if (val instanceof RegExp) {
      const sval = val.toString();
      val = sval.substring(1, sval.lastIndexOf('/'));
    } else if (typeof val !== 'string') {
      throw new Error('Regexp argument must be a string or RegExp object');
    }
    this._impl.jql_set(this, placeholder, val, 2);
    return this;
  }

  /**
   * Set number [val] at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {number} val
   * @return {JQL}
   */
  setNumber(placeholder, val) {
    this._checkPlaceholder(placeholder);
    if (typeof val !== 'number') {
      throw new Error('Value must be a number');
    }
    this._impl.jql_set(this, placeholder, val, this._isInteger(val) ? 3 : 4);
    return this;
  }

  /**
   * Set boolean [val] at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {boolean} val
   * @return {JQL}
   */
  setBoolean(placeholder, val) {
    this._checkPlaceholder(placeholder);
    this._impl.jql_set(this, placeholder, !!val, 5);
    return this;
  }

  /**
   * Set string [val] at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {string} val
   * @return {JQL}
   */
  setString(placeholder, val) {
    this._checkPlaceholder(placeholder);
    if (val != null && typeof val !== 'string') {
      val = val.toString();
    }
    this._impl.jql_set(this, placeholder, val, 6);
    return this;
  }

  /**
   * Set `null` at the specified [placeholder].
   * @param {string|number} placeholder
   * @return {JQL}
   */
  setNull(placeholder) {
    this._checkPlaceholder(placeholder);
    this._impl.jql_set(this, placeholder, null, 7);
    return this;
  }

  _isInteger(n) {
    return n === +n && n === (n | 0);
  }

  _checkPlaceholder(placeholder) {
    const t = typeof placeholder;
    if (t !== 'number' && t !== 'string') {
      throw new Error('Invalid placeholder specified, must be either string or number');
    }
  }
}

/**
 * EJDB2 Nodejs wrapper.
 */
class EJDB2 {

  /**
   * Open database instance.
   *
   * @param {String} path Path to database
   * @param {Object} [opts]
   * @returns {Promise<EJDB2>} EJDB2 instance promise
   */
  static open(path, opts) {
    opts = opts || {};

    function toArgs() {
      let oflags = 0;
      const ret = [path];
      if (opts['readonly']) {
        oflags |= 0x02;
      }
      if (opts['truncate']) {
        oflags |= 0x04;
      }
      ret.push(oflags);
      ret.push(opts['wal_enabled'] != null ? !!opts['wal_enabled'] : true);
      ret.push(opts['wal_check_crc_on_checkpoint']);
      ret.push(opts['wal_checkpoint_buffer_sz']);
      ret.push(opts['wal_checkpoint_timeout_sec']);
      ret.push(opts['wal_savepoint_timeout_sec']);
      ret.push(opts['wal_wal_buffer_sz']);
      ret.push(opts['document_buffer_sz']);
      ret.push(opts['sort_buffer_sz']);
      ret.push(opts['http_enabled']);
      ret.push(opts['http_access_token']);
      ret.push(opts['http_bind']);
      ret.push(opts['http_max_body_size']);
      ret.push(opts['http_port']);
      ret.push(opts['http_read_anon']);
      return ret;
    }

    const inst = new EJDB2(toArgs());
    return inst._impl.open().then(() => inst);
  }

  constructor(args) {
    this._impl = new EJDB2Impl(args);
  }

  /**
   * Closes database instance.
   * @return {Promise<void>}
   */
  close() {
    return this._impl.close();
  }

  /**
   * Saves [json] document under specified [id] or create a document
   * with new generated `id`. Returns promise holding actual document `id`.
   *
   * @param {String} collection
   * @param {Object|string} json
   * @param {number} [id]
   * @returns {Promise<number>}
   */
  put(collection, json, id) {
    if (typeof json !== 'string') {
      json = JSON.stringify(json);
    }
    return this._impl.put(collection, json, id);
  }

  /**
   * Apply rfc6902/rfc7386 JSON [patch] to the document identified by [id].
   *
   * @param {String} collection
   * @param {Object|string} json
   * @param {number} id
   * @return {Promise<void>}
   */
  patch(collection, json, id) {
    return this._impl.patch(collection, json, id);
  }

  /**
   * Apply JSON merge patch (rfc7396) to the document identified by `id` or
   * insert new document under specified `id`.
   *
   * @param {String} collection
   * @param {Object|string} json
   * @param {number} id
   * @return {Promise<void>}
   */
  patchOrPut(collection, json, id) {
    return this._impl.patch_or_put(collection, json, id);
  }

  /**
   * Get json body of document identified by [id] and stored in [collection].
   *
   * @param {String} collection
   * @param {number} id
   * @return {Promise<object>} JSON object
   */
  get(collection, id) {
    return this._impl.get(collection, id).then((raw) => JSON.parse(raw));
  }

  /**
   * Get json body of document identified by [id] and stored in [collection].
   * If document with given `id` is not found then `null` will be resoved.
   *
   * @param {string} collection
   * @param {number} id
   * @return {Promise<object|null>} JSON object
   */
  getOrNull(collection, id) {
    return this.get(collection, id).catch((err) => {
      if (JBE.isNotFound(err)) {
        return null;
      } else {
        return Promise.reject(err);
      }
    });
  }

  /**
   * Get json body with database metadata.
   *
   * @return {Promise<object>}
   */
  info() {
    return this._impl.info().then((raw) => JSON.parse(raw));
  }

  /**
   * Removes document idenfied by [id] from [collection].
   *
   * @param {String} collection
   * @param {number} id
   * @return {Promise<void>}
   */
  del(collection, id) {
    return this._impl.del(collection, id);
  }

  /**
   * Renames collection.
   *
   * @param {String} oldCollectionName Collection to be renamed
   * @param {String} newCollectionName New name of collection
   * @return {Promise<void>}
   */
  renameCollection(oldCollectionName, newCollectionName) {
    return this._impl.rename_collection(oldCollectionName, newCollectionName);
  }

  /**
   * Ensures json document database index specified by [path] json pointer to string data type.
   *
   * @param {String} collection
   * @param {String} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  ensureStringIndex(collection, path, unique) {
    return this._impl.index(collection, path, 0x04 | (unique ? 0x01 : 0), false);
  }

  /**
   * Removes specified database index.
   *
   * @param {String} collection
   * @param {String} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  removeStringIndex(collection, path, unique) {
    return this._impl.index(collection, path, 0x04 | (unique ? 0x01 : 0), true);
  }

  /**
   * Ensures json document database index specified by [path] json pointer to integer data type.
   *
   * @param {String} collection
   * @param {String} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  ensureIntIndex(collection, path, unique) {
    return this._impl.index(collection, path, 0x08 | (unique ? 0x01 : 0), false);
  }

  /**
   * Removes specified database index.
   *
   * @param {String} collection
   * @param {String} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  removeIntIndex(collection, path, unique) {
    return this._impl.index(collection, path, 0x08 | (unique ? 0x01 : 0), true);
  }

  /**
   * Ensures json document database index specified by [path] json pointer to floating point data type.
   *
   * @param {String} collection
   * @param {String} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  ensureFloatIndex(collection, path, unique) {
    return this._impl.index(collection, path, 0x10 | (unique ? 0x01 : 0), false);
  }

  /**
   * Removes specified database index.
   *
   * @param {String} collection
   * @param {String} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  removeFloatIndex(collection, path, unique) {
    return this._impl.index(collection, path, 0x10 | (unique ? 0x01 : 0), true);
  }

  /**
   * Removes database [collection].
   *
   * @param {String} collection
   * @return {Promise<void>}
   */
  removeCollection(collection) {
    return this._impl.rmcoll(collection);
  }

  /**
   * Create instance of [query] specified for [collection].
   * If [collection] is not specified a [query] spec must contain collection name,
   * eg: `@mycollection/[foo=bar]`
   *
   * @param {String} query
   * @param {String} [collection]
   * @returns {JQL}
   */
  createQuery(query, collection) {
    return new JQL(this, query, collection);
  }

  /**
   * Creates an online database backup image and copies it into the specified [fileName].
   * During online backup phase read/write database operations are allowed and not
   * blocked for significant amount of time. Returns promise with backup
   * finish time as number of milliseconds since epoch.
   *
   * @param {String} fileName Backup image file path.
   * @returns {Promise<number>}
   */
  onlineBackup(fileName) {
    return this._impl.online_backup(fileName);
  }
}

module.exports = {
  EJDB2,
  JBE
};


