package com.softmotions.ejdb2;

import java.io.OutputStream;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public final class EJDB2 implements AutoCloseable {

  static {
    System.loadLibrary("ejdb2jni");
  }

  private long _handle;

  private final EJDB2Builder opts;

  public EJDB2(EJDB2Builder opts) throws EJDB2Exception {
    this.opts = opts;
    _open(opts);
  }

  @Override
  public void close() {
    _dispose();
  }

  public JQL createQuery(String query, String collection) throws EJDB2Exception {
    return new JQL(this, query, collection);
  }

  public long putNew(String collection, String json) throws EJDB2Exception {
    return _put(collection, json, 0);
  }

  public long put(String collection, String json, long id) throws EJDB2Exception {
    return _put(collection, json, id);
  }

  public void del(String collection, long id) throws EJDB2Exception {
    _del(collection, id);
  }

  public void patch(String collection, String patch, long id) throws EJDB2Exception {
    _patch(collection, patch, id);
  }

  public void get(String collection, long id, OutputStream out) {
    _get(collection, id, out, false);
  }

  public void getPrettified(String collection, long id, OutputStream out) {
    _get(collection, id, out, true);
  }

  public void info(OutputStream out) throws EJDB2Exception {
    _info(out);
  }

  public EJDB2 removeCollection(String collection) throws EJDB2Exception {
    _remove_collection(collection);
    return this;
  }

  public EJDB2 ensureStringIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _ensure_index(collection, path, 0x04 | (unique ? 0x01 : 0));
    return this;
  }

  public EJDB2 removeStringIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _remove_index(collection, path, 0x04 | (unique ? 0x01 : 0));
    return this;
  }

  public EJDB2 ensureIntIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _ensure_index(collection, path, 0x08 | (unique ? 0x01 : 0));
    return this;
  }

  public EJDB2 removeIntIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _remove_index(collection, path, 0x08 | (unique ? 0x01 : 0));
    return this;
  }

  public EJDB2 ensureFloatIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _ensure_index(collection, path, 0x10 | (unique ? 0x01 : 0));
    return this;
  }

  public EJDB2 removeFloatIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _remove_index(collection, path, 0x10 | (unique ? 0x01 : 0));
    return this;
  }

  private native void _open(EJDB2Builder opts) throws EJDB2Exception;

  private native void _dispose() throws EJDB2Exception;

  private native long _put(String collection, String json, long id) throws EJDB2Exception;

  private native void _del(String collection, long id) throws EJDB2Exception;

  private native void _patch(String collection, String patch, long id) throws EJDB2Exception;

  private native void _get(String collection, long id, OutputStream out, boolean pretty) throws EJDB2Exception;

  private native void _info(OutputStream out) throws EJDB2Exception;

  private native void _remove_collection(String collection) throws EJDB2Exception;

  private native void _ensure_index(String collection, String path, int mode) throws EJDB2Exception;

  private native void _remove_index(String collection, String path, int mode) throws EJDB2Exception;
}
