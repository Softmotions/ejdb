package com.softmotions.ejdb2;

import java.io.ByteArrayOutputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * EJDB2 Query specification.
 * <p>
 * Query can be reused multiple times with various placeholder parameters. See
 * JQL specification:
 * https://github.com/Softmotions/ejdb/blob/master/README.md#jql
 * <p>
 * Memory resources used by JQL instance can be released explicitly by
 * {@link JQL#close()}.
 * <p>
 * <strong>Note:</strong> If user did not close instance explicitly it will be
 * freed anyway once jql object will be garbage collected.
 * <p>
 * Typical usage:
 *
 * <pre>
 * {@code
 *    try {
 *       q.execute((docId, doc) -> {
 *         JQL q = db.createQuery("/[foo=:val]", "mycoll").setString("val", "bar")
 *         System.out.println(String.format("Found %d %s", docId, doc));
 *         return 1;
 *       });
 *    } catch (Exception ex) {
 *      ...
 *    }
 * }
 * </pre>
 */
public final class JQL {

  private static final ReferenceQueue<JQL> refQueue = new ReferenceQueue<JQL>();

  @SuppressWarnings("StaticCollection")
  private static final Map<Long, Reference> refs = new ConcurrentHashMap<Long, Reference>();

  private static final Thread cleanupThread = new Thread(new Runnable() {
    @Override
    public void run() {
      while (true) {
        try {
          ((Reference) refQueue.remove()).cleanup();
        } catch (InterruptedException ignored) {
        }
      }
    }
  });

  static {
    cleanupThread.setDaemon(true);
    cleanupThread.start();
  }

  private final EJDB2 db;

  private final String query;

  private String collection;

  private long skip;

  private long limit;

  private long _handle;

  private ByteArrayOutputStream explain;

  /**
   * Owner database instance
   */
  public EJDB2 getDb() {
    return db;
  }

  /**
   * Query specification used to construct this query object.
   */
  public String getQuery() {
    return query;
  }

  /**
   * Collection name used for this query
   */
  public String getCollection() {
    return collection;
  }

  public JQL setCollection(String collection) {
    this.collection = collection;
    return this;
  }

  /**
   * Turn on collecting of query execution log
   *
   * @see #getExplainLog()
   */
  public JQL withExplain() {
    explain = new ByteArrayOutputStream();
    return this;
  }

  /**
   * Turn off collecting of query execution log
   *
   * @see #getExplainLog()
   */
  public JQL withNoExplain() {
    explain = null;
    return this;
  }

  public String getExplainLog() throws UnsupportedEncodingException {
    return explain != null ? explain.toString("UTF-8") : null;
  }

  /**
   * Number of records to skip. This parameter takes precedence over {@code skip}
   * encoded in query spec.
   *
   * @return
   */
  public JQL setSkip(long skip) {
    this.skip = skip;
    return this;
  }

  public long getSkip() {
    return skip > 0 ? skip : _get_skip();
  }

  /**
   * Maximum number of records to retrive. This parameter takes precedence over
   * {@code limit} encoded in query spec.
   */
  public JQL setLimit(long limit) {
    this.limit = limit;
    return this;
  }

  public long getLimit() {
    return limit > 0 ? limit : _get_limit();
  }

  /**
   * Set positional string parameter starting for {@code 0} index.
   * <p>
   * Example:
   *
   * <pre>
   * {@code
   *  db.createQuery("/[foo=:?]", "mycoll").setString(0, "zaz")
   * }
   * </pre>
   *
   * @param pos Zero based positional index
   * @param val Value to set
   * @return
   * @throws EJDB2Exception
   */
  public JQL setString(int pos, String val) throws EJDB2Exception {
    _set_string(pos, null, val, 0);
    return this;
  }

  /**
   * Set string parameter placeholder in query spec.
   * <p>
   * Example:
   *
   * <pre>
   * {@code
   *  db.createQuery("/[foo=:val]", "mycoll").setString("val", "zaz");
   * }
   * </pre>
   *
   * @param placeholder Placeholder name
   * @param val         Value to set
   * @return
   * @throws EJDB2Exception
   */
  public JQL setString(String placeholder, String val) throws EJDB2Exception {
    _set_string(0, placeholder, val, 0);
    return this;
  }

  public JQL setLong(int pos, long val) throws EJDB2Exception {
    _set_long(pos, null, val);
    return this;
  }

  public JQL setLong(String placeholder, long val) throws EJDB2Exception {
    _set_long(0, placeholder, val);
    return this;
  }

  public JQL setJSON(int pos, String json) throws EJDB2Exception {
    _set_string(pos, null, json, 1);
    return this;
  }

  public JQL setJSON(String placeholder, String json) throws EJDB2Exception {
    _set_string(0, placeholder, json, 1);
    return this;
  }

  public JQL setRegexp(int pos, String regexp) throws EJDB2Exception {
    _set_string(pos, null, regexp, 2);
    return this;
  }

  public JQL setRegexp(String placeholder, String regexp) throws EJDB2Exception {
    _set_string(0, placeholder, regexp, 2);
    return this;
  }

  public JQL setDouble(int pos, double val) throws EJDB2Exception {
    _set_double(pos, null, val);
    return this;
  }

  public JQL setDouble(String placeholder, double val) throws EJDB2Exception {
    _set_double(0, placeholder, val);
    return this;
  }

  public JQL setBoolean(int pos, boolean val) throws EJDB2Exception {
    _set_boolean(pos, null, val);
    return this;
  }

  public JQL setBoolean(String placeholder, boolean val) throws EJDB2Exception {
    _set_boolean(0, placeholder, val);
    return this;
  }

  public JQL setNull(int pos) throws EJDB2Exception {
    _set_null(pos, null);
    return this;
  }

  public JQL setNull(String placeholder) throws EJDB2Exception {
    _set_null(0, placeholder);
    return this;
  }

  /**
   * Execute query without result set callback.
   *
   * @throws EJDB2Exception
   */
  public void execute() throws EJDB2Exception {
    if (explain != null) {
      explain.reset();
    }
    _execute(db, null, explain);
  }

  /**
   * Execute query and handle records by provided {@code cb}
   *
   * @param cb Optional callback SAM
   * @throws EJDB2Exception
   */
  public void execute(JQLCallback cb) throws EJDB2Exception {
    if (explain != null) {
      explain.reset();
    }
    _execute(db, cb, explain);
  }

  /**
   * Get first record entry: {@code [documentId, json]} in results set. Entry will
   * contain nulls if no records found.
   */
  public Map.Entry<Long, String> first() {
    final Long[] idh = {null};
    final String[] jsonh = {null};
    if (explain != null) {
      explain.reset();
    }
    _execute(db, new JQLCallback() {
      @Override
      public long onRecord(long id, String json) {
        idh[0] = id;
        jsonh[0] = json;
        return 0;
      }
    }, explain);
    return new Map.Entry<Long, String>() {
      private Long _key = idh[0];
      private String _value = jsonh[0];

      @Override
      public Long getKey() {
        return _key;
      }

      @Override
      public String getValue() {
        return _value;
      }

      @Override
      public String setValue(String value) {
        _value = value;
        return _value;
      }

      @Override
      public boolean equals(Object o) {
        if (o == null) {
          return false;
        }
        if (o.getClass() != this.getClass()) {
          return false;
        }
        Map.Entry<Long, String> entry = ((Map.Entry<Long, String>) o);
        return entry.getKey().equals(this._key) && entry.getValue().equals(this._value);
      }

      @Override
      public int hashCode() {
        return 0;
      }
    };
  }

  /**
   * Get first document body as JSON string or null.
   */
  public String firstJson() {
    return first().getValue();
  }

  /**
   * Get first document id ot null
   */
  public Long firstId() {
    return first().getKey();
  }

  /**
   * Execute scalar query.
   * <p>
   * Example:
   *
   * <pre>
   * long count = db.createQuery("@mycoll/* | count").executeScalarInt();
   * </pre>
   */
  public long executeScalarInt() {
    if (explain != null) {
      explain.reset();
    }
    return _execute_scalar_long(db, explain);
  }

  /**
   * Reset data stored in positional placeholderss
   */
  public void reset() {
    if (explain != null) {
      explain.reset();
    }
    _reset();
  }

  /**
   * Close query instance releasing memory resources
   */
  public void close() throws Exception {
    Reference ref = refs.get(_handle);
    if (ref != null) {
      ref.enqueue();
    } else {
      long h = _handle;
      if (h != 0) {
        _destroy(h);
      }
    }
  }

  JQL(EJDB2 db, String query, String collection) throws EJDB2Exception {
    this.db = db;
    this.query = query;
    this.collection = collection;
    _init(db, query, collection);
    // noinspection InstanceVariableUsedBeforeInitialized
    refs.put(_handle, new Reference(this, refQueue));
  }

  @Override
  public String toString() {
    return new StringBuilder().append(JQL.class.getSimpleName()).append("[")
      .append("query=").append(query).append(", ")
      .append("collection=").append(collection)
      .append("]").toString();
  }

  private static class Reference extends WeakReference<JQL> {
    private long handle;

    Reference(JQL jql, ReferenceQueue<JQL> rq) {
      super(jql, rq);
      handle = jql._handle;
    }

    void cleanup() {
      long h = handle;
      handle = 0L;
      if (h != 0) {
        refs.remove(h);
        _destroy(h);
      }
    }
  }

  private static native void _destroy(long handle);

  private native void _init(EJDB2 db, String query, String collection);

  private native void _execute(EJDB2 db, JQLCallback cb, OutputStream explainLog);

  private native long _execute_scalar_long(EJDB2 db, OutputStream explainLog);

  private native void _reset();

  private native long _get_limit();

  private native long _get_skip();

  private native void _set_string(int pos, String placeholder, String val, int type);

  private native void _set_long(int pos, String placeholder, long val);

  private native void _set_double(int pos, String placeholder, double val);

  private native void _set_boolean(int pos, String placeholder, boolean val);

  private native void _set_null(int pos, String placeholder);
}