package com.softmotions.ejdb2;

import java.io.Serializable;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class EJDB2Builder implements Serializable {

  private static final long serialVersionUID = 6475291112728462635L;

  private boolean no_wal;

  private int sort_buffer_sz;

  private int document_buffer_sz;

  private final IWKVOptions iwkv;

  private final EJDB2HttpOptions http;

  public IWKVOptions getIWKVOptions() {
    return iwkv;
  }

  public EJDB2HttpOptions getHttpOptions() {
    return http;
  }

  public boolean isNoWal() {
    return no_wal;
  }

  public int getSortBufferSize() {
    return sort_buffer_sz;
  }

  public int getDocumentBufferSize() {
    return document_buffer_sz;
  }

  public EJDB2Builder(String path) {
    iwkv = new IWKVOptions(path);
    http = new EJDB2HttpOptions();
  }

  public EJDB2Builder noWAL(boolean v) {
    this.no_wal = v;
    return this;
  }

  public EJDB2Builder withWAL() {
    this.no_wal = false;
    return this;
  }

  public EJDB2Builder sortBufferSize(int v) {
    this.sort_buffer_sz = v;
    return this;
  }

  public EJDB2Builder documentBufferSize(int v) {
    this.document_buffer_sz = v;
    return this;
  }

  public EJDB2Builder truncate() {
    iwkv.truncate();
    return this;
  }

  public EJDB2Builder readonly() {
    iwkv.readonly();
    return this;
  }

  EJDB2Builder fileLockFailFast(boolean v) {
    iwkv.fileLockFailFast(v);
    return this;
  }

  EJDB2Builder randomSeed(long seed) {
    iwkv.randomSeed(seed);
    return this;
  }

  EJDB2Builder walCRCOnCheckpoint(boolean v) {
    iwkv.walCRCOnCheckpoint(v);
    return this;
  }

  EJDB2Builder walSavepointTimeoutSec(int v) {
    iwkv.walSavepointTimeoutSec(v);
    return this;
  }

  EJDB2Builder walCheckpointTimeoutSec(int v) {
    iwkv.walCheckpointTimeoutSec(v);
    return this;
  }

  EJDB2Builder walBufferSize(long v) {
    iwkv.walBufferSize(v);
    return this;
  }

  EJDB2Builder walCheckpointBufferSize(long v) {
    iwkv.walCheckpointBufferSize(v);
    return this;
  }

  public EJDB2Builder httpEnabled(boolean v) {
    http.enabled = v;
    return this;
  }

  public EJDB2Builder httpPort(int v) {
    http.port = v;
    return this;
  }

  public EJDB2Builder httpBind(String v) {
    http.bind = v;
    return this;
  }

  public EJDB2Builder httpAccessToken(String v) {
    http.access_token = v;
    return this;
  }

  public EJDB2Builder httpReadAnon(boolean v) {
    http.read_anon = v;
    return this;
  }

  public EJDB2Builder httpMaxBodySize(int v) {
    http.max_body_size = v;
    return this;
  }

  public EJDB2 open() {
    return new EJDB2(this);
  }

  @Override
  public String toString() {
    return new StringBuilder().append(EJDB2Builder.class.getSimpleName()).append("[")
      .append("no_wal=").append(no_wal).append(", ")
      .append("sort_buffer_sz=").append(sort_buffer_sz).append(", ")
      .append("document_buffer_sz=").append(document_buffer_sz).append(", ")
      .append("iwkv=").append(iwkv).append(", ")
      .append("http=").append(http)
      .append("]").toString();
  }

  public static class EJDB2HttpOptions implements Serializable {

    private static final long serialVersionUID = 5957416367411081107L;

    boolean enabled;
    int port;
    String bind;
    String access_token;
    boolean read_anon;
    int max_body_size;

    public boolean isEnabled() {
      return enabled;
    }

    public int getPort() {
      return port;
    }

    public String getBind() {
      return bind;
    }

    public String getAccessToken() {
      return access_token;
    }

    boolean getReadAnon() {
      return read_anon;
    }

    int getMaxBodySize() {
      return max_body_size;
    }

    @Override
    public String toString() {
      return new StringBuilder().append(EJDB2HttpOptions.class.getSimpleName()).append("[")
        .append("enabled=").append(enabled).append(", ")
        .append("port=").append(port).append(", ")
        .append("bind=").append(bind).append(", ")
        .append("access_token=").append(access_token).append(", ")
        .append("read_anon=").append(read_anon).append(", ")
        .append("max_body_size=").append(max_body_size)
        .append("]").toString();
    }
  }
}
