package com.softmotions.ejdb2;

import java.io.ByteArrayOutputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.StringJoiner;

public final class JQL implements AutoCloseable {

  private final EJDB2 db;

  private final String query;

  private String collection;

  private long skip;

  private long limit;

  private long _handle;

  private ByteArrayOutputStream explain;

  public EJDB2 getDb() {
    return db;
  }

  public String getQuery() {
    return query;
  }

  public String getCollection() {
    return collection;
  }

  public JQL setCollection(String collection) {
    this.collection = collection;
    return this;
  }

  public JQL withExplain() {
    explain = new ByteArrayOutputStream();
    return this;
  }

  public String getExplainLog() {
    return explain != null ? explain.toString(StandardCharsets.UTF_8) : null;
  }

  public long getSkip() {
    return skip;
  }

  public JQL setSkip(long skip) {
    this.skip = skip;
    return this;
  }

  public long getLimit() {
    return limit;
  }

  public JQL setLimit(long limit) {
    this.limit = limit;
    return this;
  }

  public JQL setString(int pos, String val) throws EJDB2Exception {
    _set_string(pos, null, val, 0);
    return this;
  }

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
   * @param db         Database instance.
   * @param query      Query specification.
   * @param collection Optional collection name.
   * @throws EJDB2Exception
   */
  JQL(EJDB2 db, String query, String collection) throws EJDB2Exception {
    this.db = db;
    this.query = query;
    this.collection = collection;
    _init(db, query, collection);
  }

  void execute(JQLCallback cb) throws EJDB2Exception {
    if (explain != null) {
      explain.reset();
    }
    _execute(db, cb, explain);
  }

  long executeScalarLong() {
    if (explain != null) {
      explain.reset();
    }
    return _execute_scalar_long(db, explain);
  }

  public void reset() {
    if (explain != null) {
      explain.reset();
    }
    _reset();
  }

  @Override
  public void close() throws Exception {
    this._destroy();
  }

  @Override
  public String toString() {
    return new StringJoiner(", ", JQL.class.getSimpleName() + "[", "]").add("query=" + query)
        .add("collection=" + collection).toString();
  }

  private native void _init(EJDB2 db, String query, String collection);

  private native void _destroy();

  private native void _execute(EJDB2 db, JQLCallback cb, OutputStream explainLog);

  private native long _execute_scalar_long(EJDB2 db, OutputStream explainLog);

  private native void _reset();

  private native void _set_string(int pos, String placeholder, String val, int type);

  private native void _set_long(int pos, String placeholder, long val);

  private native void _set_double(int pos, String placeholder, double val);

  private native void _set_boolean(int pos, String placeholder, boolean val);

  private native void _set_null(int pos, String placeholder);
}