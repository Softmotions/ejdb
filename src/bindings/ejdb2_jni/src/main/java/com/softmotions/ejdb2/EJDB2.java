package com.softmotions.ejdb2;

import java.io.OutputStream;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class EJDB2 implements AutoCloseable {

  static {
    loadLibrary();
  }

  private static void loadLibrary() {
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

  public void getPretty(String collection, long id, OutputStream out) {
    _get(collection, id, out, true);
  }

  public void info(OutputStream out) throws EJDB2Exception {
    _info(out);
  }

  private native void _open(EJDB2Builder opts) throws EJDB2Exception;

  private native void _dispose() throws EJDB2Exception;

  private native long _put(String collection, String json, long id) throws EJDB2Exception;

  private native void _del(String collection, long id) throws EJDB2Exception;

  private native void _patch(String collection, String patch, long id) throws EJDB2Exception;

  private native void _get(String collection, long id, OutputStream out, boolean pretty) throws EJDB2Exception;

  private native void _info(OutputStream out) throws EJDB2Exception;
}
