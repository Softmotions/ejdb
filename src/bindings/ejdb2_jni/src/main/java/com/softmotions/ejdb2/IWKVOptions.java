package com.softmotions.ejdb2;

import java.io.Serializable;
import java.util.StringJoiner;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public final class IWKVOptions implements Serializable {

  private static final long serialVersionUID = -6138184402489045096L;

  private final String path;

  private long oflags;

  private long random_seed;

  private boolean file_lock_fail_fast = true;

  private final WALOptions wal = new WALOptions();

  public String getPath() {
    return path;
  }

  public boolean isTruncate() {
    return (oflags & 0x4) != 0;
  }

  public boolean isReadOnly() {
    return (oflags & 0x4) != 0;
  }

  public long getRandomSeed() {
    return random_seed;
  }

  public boolean getFileLockFailFast() {
    return file_lock_fail_fast;
  }

  public WALOptions getWALOptions() {
    return this.wal;
  }

  public IWKVOptions(String path) {
    this.path = path;
  }

  IWKVOptions truncate() {
    oflags |= 0x4;
    return this;
  }

  IWKVOptions readonly() {
    oflags |= 0x2;
    return this;
  }

  IWKVOptions fileLockFailFast(boolean v) {
    this.file_lock_fail_fast = v;
    return this;
  }

  IWKVOptions randomSeed(long seed) {
    this.random_seed = seed;
    return this;
  }

  IWKVOptions walCRCOnCheckpoint(boolean v) {
    wal.check_crc_on_checkpoint = v;
    return this;
  }

  IWKVOptions walSavepointTimeoutSec(int v) {
    wal.savepoint_timeout_sec = v;
    return this;
  }

  IWKVOptions walCheckpointTimeoutSec(int v) {
    wal.checkpoint_timeout_sec = v;
    return this;
  }

  IWKVOptions walBufferSize(long v) {
    wal.buffer_sz = v;
    return this;
  }

  IWKVOptions walCheckpointBufferSize(long v) {
    wal.checkpoint_buffer_sz = v;
    return this;
  }

  @Override
  public String toString() {
    return new StringJoiner(", ", IWKVOptions.class.getSimpleName() + "[", "]").add("path='" + path + "'")
        .add("oflags=" + oflags).add("file_lock_fail_fast=" + file_lock_fail_fast).add("random_seed=" + random_seed)
        .add("wal=" + wal).toString();
  }

  public static final class WALOptions implements Serializable {

    private static final long serialVersionUID = 2406233154956721582L;

    private boolean check_crc_on_checkpoint;
    private int savepoint_timeout_sec;
    private int checkpoint_timeout_sec;
    private long buffer_sz;
    private long checkpoint_buffer_sz;

    public boolean getCheckCRCOnCheckpoint() {
      return this.check_crc_on_checkpoint;
    }

    public int getSavePointTimeoutSec() {
      return this.savepoint_timeout_sec;
    }

    public int getCheckPointTimeoutSec() {
      return this.checkpoint_timeout_sec;
    }

    public long getBufferSize() {
      return this.buffer_sz;
    }

    public long getCheckpointBufferSize() {
      return this.checkpoint_buffer_sz;
    }

    @Override
    public String toString() {
      return new StringJoiner(", ", WALOptions.class.getSimpleName() + "[", "]")
          .add("check_crc_on_checkpoint=" + check_crc_on_checkpoint)
          .add("savepoint_timeout_sec=" + savepoint_timeout_sec).add("checkpoint_timeout_sec=" + checkpoint_timeout_sec)
          .add("buffer_sz=" + buffer_sz).add("checkpoint_buffer_sz=" + checkpoint_buffer_sz).toString();
    }
  }
}
