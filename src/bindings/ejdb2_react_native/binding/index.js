import { NativeEventEmitter, NativeModules } from 'react-native';

const { EJDB2DB: ejdb2, EJDB2JQL: ejdb2jql } = NativeModules;

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
    const msg = (err.message || '').toString();
    return msg.indexOf('@ejdb IWRC:75001') != -1;
  }

  static isInvalidQuery(err) {
    const msg = (err.message || '').toString();
    return msg.indexOf('@ejdb IWRC:87001') != -1;
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
 * EJDB Query.
 */
class JQL {
  /**
   * @param db {EJDB2}
   * @param jql
   */
  constructor(db, jql) {
    this._db = db;
    this._jql = jql;
    this._nee = new NativeEventEmitter(ejdb2jql);
    this._eid = 0;
    this._explain = null;
    this._withExplain = false;
  }

  /**
   * Get database instance associated with this query.
   * @returns {EJDB2}
   */
  get db() {
    return this._db;
  }

  /**
   * Get query text
   * @return {string}
   */
  get query() {
    return ejdb2jql.getQuery(this._jql);
  }

  /**
   * Get documents collection name used in query.
   * @return {string}
   */
  get collection() {
    return ejdb2jql.getCollection(this._jql);
  }

  /**
   * Set collection to use with query.
   * Value overrides collection encoded in query.
   * @param coll
   * @return {JQL}
   */
  withCollection(coll) {
    ejdb2jql.setCollection(this._jql, coll);
    return this;
  }

  /**
   * Retrieve query execution plan.
   * @return {string|null}
   */
  get explainLog() {
    return this._explainLog || ejdb2jql.getExplainLog(this._jql);
  }

  /**
   * Get number of documents to skip.
   * @return {number}
   */
  get skip() {
    return parseInt(ejdb2jql.getSkip(this._jql));
  }

  /**
   * Set number of documents to skip.
   * Specified value overrides `skip` option encoded in query.
   * @param {number} skip
   * @return {JQL}
   */
  withSkip(skip) {
    if (!this._isInteger(skip)) {
      throw new Error('`skip` argument must an Integer');
    }
    ejdb2jql.setSkip(this._jql, skip.toString());
    return this;
  }

  /**
   * Get maximum number of documents to fetch.
   * @return {number}
   */
  get limit() {
    return parseInt(ejdb2jql.getLimit(this._jql));
  }

  /**
   * Set maximum number of documents to fetch.
   * Specified value overrides `limit` option encoded in query.
   * @param {number} limit Limit
   * @return {JQL}
   */
  withLimit(limit) {
    if (!this._isInteger(limit)) {
      new Error('`limit` argument must an Integer');
    }
    ejdb2jql.setLimit(this._jql, limit.toString());
    return this;
  }

  /**
   * Turn on explain query mode
   * @return {JQL}
   */
  withExplain() {
    ejdb2jql.withExplain(this._jql);
    this._withExplain = true;
    this._explainLog = null;
    return this;
  }

  /**
   * Set string [val] at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {string} val
   * @return {JQL}
   */
  setString(placeholder, val) {
    if (val != null && typeof val !== 'string') {
      val = val.toString();
    }
    this._p('setString', placeholder)(this._jql, placeholder, val);
    return this;
  }

  /**
   * Set [json] at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {string|object} val
   * @return {JQL}
   */
  setJSON(placeholder, val) {
    if (typeof val !== 'string') {
      val = JSON.stringify(val);
    }
    this._p('setJSON', placeholder)(this._jql, placeholder, val);
    return this;
  }

  /**
   * Set [regexp] string at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {string|RegExp} val
   * @return {JQL}
   */
  setRegexp(placeholder, val) {
    if (val instanceof RegExp) {
      const sval = val.toString();
      val = sval.substring(1, sval.lastIndexOf('/'));
    } else if (typeof val !== 'string') {
      throw new Error('Regexp argument must be a string or RegExp object');
    }
    this._p('setRegexp', placeholder)(this._jql, placeholder, val);
    return this;
  }

  /**
   * Set number [val] at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {number} val
   * @returns {JQL}
   */
  setNumber(placeholder, val) {
    if (typeof val !== 'number') {
      throw new Error('Value must be a number');
    }
    if (this._isInteger(val)) {
      this._p('setLong', placeholder)(this._jql, placeholder, val.toString());
    } else {
      this._p('setDouble', placeholder)(this._jql, placeholder, val);
    }
    return this;
  }

  /**
   * Set boolean [val] at the specified [placeholder].
   * @param {string|number} placeholder
   * @param {boolean} val
   * @return {JQL}
   */
  setBoolean(placeholder, val) {
    this._p('setBoolean', placeholder)(this._jql, placeholder, !!val);
    return this;
  }

  /**
   * Set `null` at the specified [placeholder].
   * @param {string|number} placeholder
   * @return {JQL}
   */
  setNull(placeholder) {
    this._p('setNull', placeholder)(this._jql, placeholder);
    return this;
  }

  /**
   * @callback JQLExecuteCallback
   * @param {JBDOC} doc
   * @param {JQL} jql
   */
  /**
   * Execute this query.
   *
   * @param {boolean} dispose Dispose this query after execution
   * @param {JQLExecuteCallback} callback
   * @return {Promise<void>}
   */
  execute(dispose, callback) {
    const eid = this._nextEventId();
    const reg = this._nee.addListener(eid, data => {
      data.forEach(row => {
        if (row != null) {
          callback && callback(new JBDOC(row.id, row.raw), this);
        }
      });
      if (data.length && data[data.length - 1] == null) {
        // EOF last element is null
        reg.remove();
      }
    });
    this._explainLog = null;
    return ejdb2jql
      .execute(this._jql, eid)
      .catch(err => {
        reg.remove();
        return err;
      })
      .then(err => {
        if (this._withExplain) {
          // Save explain log before close
          this._explainLog = ejdb2jql.getExplainLog(this._jql);
        }
        if (dispose) {
          this.close();
        }
        if (err) {
          return Promise.reject(err);
        }
      });
  }

  /**
   * Execute this query then dispose it.
   *
   * @param {JQLExecuteCallback?} callback
   * @return {Promise<void>}
   */
  useExecute(callback) {
    return this.execute(true, callback);
  }

  /**
   * Uses query in `callback` then closes it.
   * @param {UseQueryCallback} callback Function accepts query object
   * @return {Promise<*>}
   */
  async use(callback) {
    let ret;
    try {
      this._explainLog = null;
      ret = await callback(this);
    } finally {
      if (this._withExplain) {
        // Save explain log before close
        this._explainLog = ejdb2jql.getExplainLog(this._jql);
      }
      this.close();
    }
    return ret;
  }

  /**
   *
   * @param {object?} opts Optional options object.
   *        - `limit` Override maximum number of documents in result set
   *        - `skip` Override skip number of documents to skip
   * @return {Promise<JBDOC[]>}
   */
  list(opts_ = {}) {
    const opts = { ...opts_ };
    if (opts.limit != null && typeof opts.limit !== 'string') {
      opts.limit = `${opts.limit}`;
    }
    if (opts.skip != null && typeof opts.skip !== 'string') {
      opts.skip = `${opts.skip}`;
    }
    return ejdb2jql.list(this._jql, opts).then(l => l.map(data => new JBDOC(data.id, data.raw)));
  }

  /**
   * Collects up to [n] documents from result set into array.
   * @param {number|string} n Upper limit of documents in result set
   * @return {Promise<JBDOC[]>}
   */
  firstN(n = 1) {
    return this.list({ limit: n });
  }

  /**
   * Returns a first record in result set.
   * If no reconds found `null` resolved promise will be returned.
   * @returns {Promise<JBDOC|null>}
   */
  first() {
    return ejdb2jql.first(this._jql).then(data => (data ? new JBDOC(data.id, data.raw) : null));
  }

  /**
   * Returns a scalar integer value as result of query execution.
   * Eg.: A count query: `/... | count`
   * @return {Promise<number>}
   */
  scalarInt() {
    return ejdb2jql.executeScalarInt(this._jql).then(r => parseInt(r));
  }

  /**
   * Reset query parameters.
   * @returns {Promise<JQL>}
   */
  reset() {
    return ejdb2jql.reset(this._jql).then(_ => this);
  }

  /**
   * Disposes JQL instance and releases all underlying resources.
   */
  close() {
    return ejdb2jql.close(this._jql);
  }

  /**
   * @private
   */
  _isInteger(n) {
    return n === +n && n === (n | 0);
  }

  /**
   * @private
   */
  _p(name, placeholder) {
    const t = typeof placeholder;
    if (!(t === 'number' || t === 'string')) {
      throw new Error('Invalid placeholder specified, must be either string or number');
    }
    return ejdb2jql[(this._isInteger(placeholder) ? 'p' : '') + name];
  }

  /**
   * Generate next event id.
   * @private
   * @return {string}
   */
  _nextEventId() {
    return `jql${this._eid++}`;
  }
}

/**
 * EJDB2 React Native wrapper.
 */
class EJDB2 {
  /**
   * Open database instance.
   *
   * @param {string} path Path to database
   * @param {Object} [opts]
   * @return {Promise<EJDB2>} EJDB2 instance promise
   */
  static open(path, opts = {}) {
    return ejdb2.open(path, opts).then(db => new EJDB2(db));
  }

  constructor(db) {
    this._db = db;
  }

  /**
   * Closes database instance.
   * @return {Promise<void>}
   */
  close() {
    return ejdb2.close(this._db);
  }

  /**
   * Get json body with database metadata.
   *
   * @return {Promise<object>}
   */
  info() {
    return ejdb2.info(this._db).then(JSON.parse);
  }

  /**
   * @callback UseQueryCallback
   * @param {JQL} query
   * @return {Promise<*>}
   */

  /**
   * Create instance of [query] specified for [collection].
   * If [collection] is not specified a [query] spec must contain collection name,
   * eg: `@mycollection/[foo=bar]`
   *
   * @note WARNING In order to avoid memory leaks, dispose created query object by {@link JQL#close}
   *       or use {@link JQL#use}
   *
   * @param {string} query Query text
   * @param {string} [collection]
   * @returns {JQL}
   */
  createQuery(query, collection) {
    const qh = ejdb2.createQuery(this._db, query, collection);
    return new JQL(this, qh);
  }

  /**
   * Saves [json] document under specified [id] or create a document
   * with new generated `id`. Returns promise holding actual document `id`.
   *
   * @param {string} collection
   * @param {Object|string} json
   * @param {number} [id]
   * @returns {Promise<number>}
   */
  put(collection, json, id = 0) {
    json = typeof json === 'string' ? json : JSON.stringify(json);
    return ejdb2.put(this._db, collection, json, id).then(v => parseInt(v));
  }

  /**
   * Apply rfc6902/rfc7386 JSON [patch] to the document identified by [id].
   *
   * @param {string} collection
   * @param {Object|string} json
   * @param {number} id
   * @return {Promise<void>}
   */
  patch(collection, json, id) {
    json = typeof json === 'string' ? json : JSON.stringify(json);
    return ejdb2.patch(this._db, collection, json, id);
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
    json = typeof json === 'string' ? json : JSON.stringify(json);
    return ejdb2.patchOrPut(this._db, collection, json, id);
  }

  /**
   * Get json body of document identified by [id] and stored in [collection].
   *
   * @param {string} collection
   * @param {number} id
   * @return {Promise<object>} JSON object
   */
  get(collection, id) {
    return ejdb2.get(this._db, collection, id).then(JSON.parse);
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
   * Removes document idenfied by [id] from [collection].
   *
   * @param {string} collection
   * @param {number} id
   * @return {Promise<void>}
   */
  del(collection, id) {
    return ejdb2.del(this._db, collection, id);
  }

  /**
   * Renames collection.
   *
   * @param {string} oldCollectionName Collection to be renamed
   * @param {string} newCollectionName New name of collection
   * @return {Promise<void>}
   */
  renameCollection(oldCollectionName, newCollectionName) {
    return ejdb2.renameCollection(this._db, oldCollectionName, newCollectionName);
  }

  /**
   * Removes database [collection].
   *
   * @param {string} collection
   * @return {Promise<void>}
   */
  removeCollection(collection) {
    return ejdb2.removeCollection(this._db, collection).then(_ => this);
  }

  /**
   * Ensures json document database index specified by [path] json pointer to string data type.
   *
   * @param {string} collection
   * @param {string} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  ensureStringIndex(collection, path, unique) {
    return ejdb2.ensureStringIndex(this._db, collection, path, unique).then(_ => this);
  }

  /**
   * Removes specified database index.
   *
   * @param {string} collection
   * @param {string} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  removeStringIndex(collection, path, unique) {
    return ejdb2.removeStringIndex(this._db, collection, path, unique).then(_ => this);
  }

  /**
   * Ensures json document database index specified by [path] json pointer to integer data type.
   *
   * @param {string} collection
   * @param {string} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  ensureIntIndex(collection, path, unique) {
    return ejdb2.ensureIntIndex(this._db, collection, path, unique).then(_ => this);
  }

  /**
   * Removes specified database index.
   *
   * @param {string} collection
   * @param {string} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  removeIntIndex(collection, path, unique) {
    return ejdb2.removeIntIndex(this._db, collection, path, unique).then(_ => this);
  }

  /**
   * Ensures json document database index specified by [path] json pointer to floating point data type.
   *
   * @param {string} collection
   * @param {string} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  ensureFloatIndex(collection, path, unique) {
    return ejdb2.ensureFloatIndex(this._db, collection, path, unique).then(_ => this);
  }

  /**
   * Removes specified database index.
   *
   * @param {string} collection
   * @param {string} path
   * @param {boolean} [unique=false]
   * @return {Promise<void>}
   */
  removeFloatIndex(collection, path, unique) {
    return ejdb2.removeFloatIndex(this._db, collection, path, unique).then(_ => this);
  }

  /**
   * Creates an online database backup image and copies it into the specified [fileName].
   * During online backup phase read/write database operations are allowed and not
   * blocked for significant amount of time. Returns promise with backup
   * finish time as number of milliseconds since epoch.
   *
   * @param {string} fileName Backup image file path.
   * @returns {Promise<number>}
   */
  onlineBackup(fileName) {
    return ejdb2.onlineBackup(this._db, fileName);
  }
}

module.exports = {
  EJDB2,
  JBE
};
