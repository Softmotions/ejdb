package com.softmotions.ejdb2;

import java.io.IOException;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicInteger;

import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableMap;

public class EJDB2DBModule extends ReactContextBaseJavaModule {

  @SuppressWarnings("StaticCollection")
  static Map<Integer, DbEntry> dbmap = new ConcurrentHashMap<>();

  static AtomicInteger dbkeys = new AtomicInteger();

  private final Executor executor;

  public EJDB2DBModule(ReactApplicationContext reactContext, Executor executor) {
    super(reactContext);
    this.executor = executor;
  }

  @Override
  public String getName() {
    return "EJDB2DB";
  }

  @ReactMethod
  public void open(String path, ReadableMap opts, Promise promise) {
    executor.execute(() -> openImpl(path, opts, promise));
  }

  private synchronized void openImpl(String path_, ReadableMap opts, Promise promise) {
    try {
      String path = normalizePath(path_);
      DbEntry dbe = null;
      for (DbEntry v : dbmap.values()) {
        if (v.path.equals(path)) {
          dbe = v;
          break;
        }
      }
      if (dbe != null) {
        dbe.countOpen();
        promise.resolve(dbe.handle);
        return;
      }
      // Now open new database
      EJDB2Builder b = new EJDB2Builder(path);
      if (opts.hasKey("readonly") && !opts.isNull("readonly") && opts.getBoolean("readonly"))
        b.readonly();
      if (opts.hasKey("truncate") && !opts.isNull("truncate") && opts.getBoolean("truncate"))
        b.truncate();
      if (opts.hasKey("wal_enabled") && !opts.isNull("wal_enabled"))
        b.noWAL(!opts.getBoolean("wal_enabled"));
      else
        b.withWAL();
      if (opts.hasKey("wal_check_crc_on_checkpoint") && !opts.isNull("wal_check_crc_on_checkpoint"))
        b.walCRCOnCheckpoint(opts.getBoolean("wal_check_crc_on_checkpoint"));
      if (opts.hasKey("wal_checkpoint_buffer_sz") && !opts.isNull("wal_checkpoint_buffer_sz"))
        b.walCheckpointBufferSize(opts.getInt("wal_checkpoint_buffer_sz"));
      if (opts.hasKey("wal_checkpoint_timeout_sec") && !opts.isNull("wal_checkpoint_timeout_sec"))
        b.walCheckpointTimeoutSec(opts.getInt("wal_checkpoint_timeout_sec"));
      if (opts.hasKey("wal_savepoint_timeout_sec") && !opts.isNull("wal_savepoint_timeout_sec"))
        b.walSavepointTimeoutSec(opts.getInt("wal_savepoint_timeout_sec"));
      if (opts.hasKey("wal_wal_buffer_sz") && !opts.isNull("wal_wal_buffer_sz"))
        b.walBufferSize(opts.getInt("wal_wal_buffer_sz"));
      if (opts.hasKey("document_buffer_sz") && !opts.isNull("document_buffer_sz"))
        b.documentBufferSize(opts.getInt("document_buffer_sz"));
      if (opts.hasKey("sort_buffer_sz") && !opts.isNull("sort_buffer_sz"))
        b.sortBufferSize(opts.getInt("sort_buffer_sz"));

      final Integer handle = dbkeys.incrementAndGet();
      dbmap.put(handle, new DbEntry(b.open(), handle, path));

      promise.resolve(handle);
    } catch (EJDB2Exception e) {
      promise.reject(String.valueOf(e.getCode()), e.toString());
    } catch (Exception e) {
      promise.reject(null, e.toString());
    }
  }

  @ReactMethod
  public synchronized void close(Integer handle, Promise promise) {
    try {
      if (db(handle).close())
        dbmap.remove(handle);
    } catch (EJDB2Exception e) {
      promise.reject(String.valueOf(e.getCode()), e.toString());
    } catch (Exception e) {
      promise.reject(null, e.toString());
    }
    promise.resolve(null);
  }

  public void context(Promise promise) {

  }

  @ReactMethod
  public void info(Integer handle, Promise promise) {
    thread(handle, promise, db -> promise.resolve(db.infoAsString()));
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public Integer createQuery(Integer handle, String query, String collection) {
    return EJDB2JQLModule.createQuery(db(handle).db, query, collection);
  }

  @ReactMethod
  public void put(Integer handle, String collection, String json, Integer id, Promise promise) {
    thread(handle, promise, db -> promise.resolve(String.valueOf(db.put(collection, json, id.longValue()))));
  }

  @ReactMethod
  public void patch(Integer handle, String collection, String json, Integer id, Promise promise) {
    thread(handle, promise, db -> {
      db.patch(collection, json, id.longValue());
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void patchOrPut(Integer handle, String collection, String json, Integer id, Promise promise) {
    thread(handle, promise, db -> {
      db.patchOrPut(collection, json, id.longValue());
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void get(Integer handle, String collection, Integer id, Promise promise) {
    thread(handle, promise, db -> promise.resolve(db.getAsString(collection, id.longValue())));
  }

  @ReactMethod
  public void del(Integer handle, String collection, Integer id, Promise promise) {
    thread(handle, promise, db -> {
      db.del(collection, id.longValue());
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void renameCollection(Integer handle, String oldCollectionName, String newCollectionName, Promise promise) {
    thread(handle, promise, db -> {
      db.renameCollection(oldCollectionName, newCollectionName);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void removeCollection(Integer handle, String collection, Promise promise) {
    thread(handle, promise, db -> {
      db.removeCollection(collection);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void ensureStringIndex(Integer handle, String collection, String path, Boolean unique, Promise promise) {
    thread(handle, promise, db -> {
      db.ensureStringIndex(collection, path, unique);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void removeStringIndex(Integer handle, String collection, String path, Boolean unique, Promise promise) {
    thread(handle, promise, db -> {
      db.removeStringIndex(collection, path, unique);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void ensureIntIndex(Integer handle, String collection, String path, Boolean unique, Promise promise) {
    thread(handle, promise, db -> {
      db.ensureIntIndex(collection, path, unique);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void removeIntIndex(Integer handle, String collection, String path, Boolean unique, Promise promise) {
    thread(handle, promise, db -> {
      db.removeIntIndex(collection, path, unique);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void ensureFloatIndex(Integer handle, String collection, String path, Boolean unique, Promise promise) {
    thread(handle, promise, db -> {
      db.ensureFloatIndex(collection, path, unique);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void removeFloatIndex(Integer handle, String collection, String path, Boolean unique, Promise promise) {
    thread(handle, promise, db -> {
      db.removeFloatIndex(collection, path, unique);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void onlineBackup(Integer handle, String targetFile, Promise promise) {
    thread(handle, promise, db -> {
      promise.resolve(String.valueOf(db.onlineBackup(targetFile)));
    });
  }

  private DbEntry db(Integer handle) throws RuntimeException {
    DbEntry dbe = dbmap.get(handle);
    if (dbe == null)
      throw new IllegalStateException("Database handle already disposes");
    return dbe;
  }

  private void immediate(Integer handle, Promise promise, DbLogic fn) {
    try {
      fn.apply(db(handle).db);
    } catch (EJDB2Exception e) {
      promise.reject(String.valueOf(e.getCode()), e.toString());
    } catch (Exception e) {
      promise.reject(null, e.toString());
    }
  }

  private void thread(Integer handle, Promise promise, DbLogic fn) {
    executor.execute(() -> immediate(handle, promise, fn));
  }

  private String normalizePath(String path) {
    return this.getReactApplicationContext().getDatabasePath(path).getAbsolutePath();
  }

  private interface DbLogic {
    void apply(EJDB2 db) throws Exception;
  }

  private static class DbEntry {
    DbEntry(EJDB2 db, Integer handle, String path) {
      this.db = db;
      this.counter = new AtomicInteger(1);
      this.path = path;
      this.handle = handle;
    }

    final EJDB2 db;
    private final String path;
    private final Integer handle;
    private final AtomicInteger counter;

    void countOpen() {
      this.counter.incrementAndGet();
    }

    boolean close() {
      int cnt = this.counter.decrementAndGet();
      if (cnt <= 0)
        db.close();
      return cnt <= 0;
    }
  }
}