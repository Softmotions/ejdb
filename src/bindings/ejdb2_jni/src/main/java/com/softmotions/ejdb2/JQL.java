package com.softmotions.ejdb2;

import java.util.StringJoiner;

public final class JQL implements AutoCloseable {

  private final EJDB2 db;

  private final String query;

  private String collection;

  private Long skip;

  private Long limit;

  private long _handle;

  public EJDB2 getDb() {
    return db;
  }

  public String getQuery() {
    return query;
  }

  public String getCollection() {
    return collection;
  }

  public Long getSkip() {
    return skip;
  }

  public void setSkip(Long skip) {
    this.skip = skip;
  }

  public Long getLimit() {
    return limit;
  }

  public void setLimit(Long limit) {
    this.limit = limit;
  }

  JQL(EJDB2 db, String query, String collection) throws EJDB2Exception {
    this.db = db;
    this.query = query;
    this.collection = collection;
    _init();
  }

  void execute(JQLCallback cb) throws EJDB2Exception {
    _execute(cb);
  }

  long executeScalarLong() {
    return _execute_scalar_long();
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

  private native void _init();

  private native void _destroy();

  private native void _execute(JQLCallback cb);

  private native long _execute_scalar_long();
}