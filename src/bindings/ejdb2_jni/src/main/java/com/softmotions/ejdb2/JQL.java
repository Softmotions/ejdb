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
    this.explain = null;
    //todo:
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
}