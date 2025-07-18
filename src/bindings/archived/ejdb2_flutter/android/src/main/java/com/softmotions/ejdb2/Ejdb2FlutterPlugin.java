package com.softmotions.ejdb2;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

import android.app.Activity;
import android.content.Context;
import android.util.Log;
import io.flutter.plugin.common.EventChannel;
import io.flutter.plugin.common.EventChannel.EventSink;
import io.flutter.plugin.common.EventChannel.StreamHandler;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.common.MethodChannel.MethodCallHandler;
import io.flutter.plugin.common.MethodChannel.Result;
import io.flutter.plugin.common.PluginRegistry.Registrar;

/**
 * Ejdb2FlutterPlugin
 */
public final class Ejdb2FlutterPlugin implements MethodCallHandler, StreamHandler {

  public static final String TAG = "Ejdb2FlutterPlugin";

  @SuppressWarnings("StaticCollection")
  static final Map<Integer, DbEntry> dbmap = new ConcurrentHashMap<>();

  static final AtomicInteger dbkeys = new AtomicInteger();

  static final Map<String, DbMethod> methods = new ConcurrentHashMap<>();

  static final Executor executor = new ThreadPoolExecutor(0, 5, 60L, TimeUnit.SECONDS,
                                                          new LinkedBlockingQueue<Runnable>());

  static {
    methods.put("executeFirst", thread(Ejdb2FlutterPlugin::executeFirst));
    methods.put("executeList", thread(Ejdb2FlutterPlugin::executeList));
    methods.put("executeScalarInt", thread(Ejdb2FlutterPlugin::executeScalarInt));
    methods.put("executeQuery", thread(Ejdb2FlutterPlugin::executeQuery));
    methods.put("onlineBackup", thread(Ejdb2FlutterPlugin::onlineBackup));
    methods.put("removeFloatIndex", thread(Ejdb2FlutterPlugin::removeFloatIndex));
    methods.put("ensureFloatIndex", thread(Ejdb2FlutterPlugin::ensureFloatIndex));
    methods.put("removeIntIndex", thread(Ejdb2FlutterPlugin::removeIntIndex));
    methods.put("ensureIntIndex", thread(Ejdb2FlutterPlugin::ensureIntIndex));
    methods.put("removeStringIndex", thread(Ejdb2FlutterPlugin::removeStringIndex));
    methods.put("ensureStringIndex", thread(Ejdb2FlutterPlugin::ensureStringIndex));
    methods.put("removeCollection", thread(Ejdb2FlutterPlugin::removeCollection));
    methods.put("renameCollection", thread(Ejdb2FlutterPlugin::renameCollection));
    methods.put("del", thread(Ejdb2FlutterPlugin::del));
    methods.put("get", thread(Ejdb2FlutterPlugin::get));
    methods.put("patch", thread(Ejdb2FlutterPlugin::patch));
    methods.put("put", thread(Ejdb2FlutterPlugin::put));
    methods.put("info", thread(Ejdb2FlutterPlugin::info));
    methods.put("open", thread(Ejdb2FlutterPlugin::open));
    methods.put("close", thread(Ejdb2FlutterPlugin::close));
  }

  Ejdb2FlutterPlugin(Registrar registrar) {
    this.registrar = registrar;
    this.methodChannel = new MethodChannel(registrar.messenger(), "ejdb2");
    this.methodChannel.setMethodCallHandler(this);
    this.eventChannel = new EventChannel(registrar.messenger(), "ejdb2/query");
    this.eventChannel.setStreamHandler(this);
  }

  final Registrar registrar;

  final MethodChannel methodChannel;

  final EventChannel eventChannel;

  EventSink events;

  static Ejdb2FlutterPlugin plugin;

  /**
   * Plugin registration.
   */
  public static void registerWith(Registrar registrar) {
    plugin = new Ejdb2FlutterPlugin(registrar);
  }

  @Override
  public void onMethodCall(MethodCall call, Result result) {
    DbMethod method = methods.get(call.method);
    if (method == null) {
      result.notImplemented();
      return;
    }
    @SuppressWarnings("unchecked")
    Object[] args = ((List) call.arguments).toArray();
    DbEntry dbe = null;
    if (args[0] != null) {
      dbe = dbmap.get(((Number) args[0]).intValue());
      if (dbe == null) {
        result.error("@ejdb", "Database handle already disposed", null);
        return;
      }
    }
    try {
      method.handle(new DbMethodCall(this, dbe, args, result));
    } catch (EJDB2Exception e) {
      result.error("@ejdb IWRC:" + e.getCode(), e.getMessage(), null);
    } catch (Exception e) {
      Log.e(TAG, e.getMessage(), e);
      result.error("@ejdb", e.toString(), null);
    }
  }

  @Override
  public void onListen(Object arguments, EventSink events) {
    this.events = events;
  }

  @Override
  public void onCancel(Object arguments) {
    this.events = null;
  }

  private static void executeFirst(DbMethodCall mc) throws Exception {
    executeListImpl(mc, 1L);
  }

  private static void executeList(DbMethodCall mc) throws Exception {
    executeListImpl(mc, null);
  }

  private static void executeListImpl(DbMethodCall mc, Long limit) throws Exception {
    prepareQuery(mc, (q, hook) -> {
      if (limit != null) {
        q.setLimit(limit);
      }
      final List<Object> res = new ArrayList<>((limit != null && limit < 1024) ? limit.intValue() : 64);
      q.execute((id, json) -> {
        res.add(id);
        res.add(json);
        return 1;
      });
      mc.successOnMainThread(res);
    });
  }

  private static void executeScalarInt(DbMethodCall mc) throws Exception {
    prepareQuery(mc, (q, hook) -> {
      Long res = q.executeScalarInt();
      mc.successOnMainThread(res);
    });
  }

  private static void executeQuery(DbMethodCall mc) throws Exception {
    prepareQuery(mc, (q, hook) -> {
      final AtomicReference<List<Object>> holder = new AtomicReference<>(new ArrayList<>());
      q.execute((id, json) -> {
        List<Object> batch = holder.get();
        if (batch.size() >= 128 && batch.size() % 2 == 0) {
          EventSink sink = mc.plugin.events;
          if (sink != null) {
            List<Object> res = batch;
            res.add(0, hook);
            mc.runOnMainThread(() -> sink.success(res));
          }
          batch = new ArrayList<>();
          holder.set(batch);
        }
        batch.add(id);
        batch.add(json);
        return 1;
      });
      // Flush rest of records
      List<Object> batch = holder.get();
      batch.add(0, hook);
      batch.add(true);
      holder.set(null);
      EventSink esink = mc.plugin.events;
      if (esink != null) {
        mc.runOnMainThread(() -> esink.success(batch));
      }
    });
  }

  private static void prepareQuery(DbMethodCall mc, DbQueryHandler qh) throws Exception {
    final String hook = cast(mc.args[1]);
    final String coll = cast(mc.args[2]);
    final String qtext = asString(mc.args[3], "");
    final Map<String, Object> qspec = cast(mc.args[4]);
    final List<List<Object>> params = cast(mc.args[5]);

    final EJDB2 db = mc.getDb();
    final JQL q = db.createQuery(qtext, coll);
    try {
      for (Map.Entry<String, Object> entry : qspec.entrySet()) {
        String key = entry.getKey();
        switch (key) {
          case "l": // limit
            q.setLimit(asLong(entry.getValue(), 0));
            break;
          case "s": // skip
            q.setSkip(asLong(entry.getValue(), 0));
            break;
        }
      }
      for (List<Object> pslot : params) {
        int type = asInt(pslot.get(0), 0);
        Object plh = pslot.get(1);
        Object val = pslot.get(2);
        if (type == 0 || val == null) {
          if (plh instanceof Number) {
            q.setNull(((Number) plh).intValue());
          } else {
            q.setNull((String) plh);
          }
        } else {
          switch (type) {
            case 1: // String
              if (plh instanceof Number) {
                q.setString(((Number) plh).intValue(), asString(val, null));
              } else {
                q.setString((String) plh, asString(val, null));
              }
              break;
            case 2: // Long
              if (plh instanceof Number) {
                q.setLong(((Number) plh).intValue(), asLong(val, 0));
              } else {
                q.setLong((String) plh, asLong(val, 0));
              }
              break;
            case 3: // Double
              if (plh instanceof Number) {
                q.setDouble(((Number) plh).intValue(), asDouble(val, 0));
              } else {
                q.setDouble((String) plh, asDouble(val, 0));
              }
              break;
            case 4: // Boolean
              if (plh instanceof Number) {
                q.setBoolean(((Number) plh).intValue(), asBoolean(val, false));
              } else {
                q.setBoolean((String) plh, asBoolean(val, false));
              }
              break;
            case 5: // Regexp
              if (plh instanceof Number) {
                q.setRegexp(((Number) plh).intValue(), asString(val, null));
              } else {
                q.setRegexp((String) plh, asString(val, null));
              }
              break;
            case 6: // JSON
              if (plh instanceof Number) {
                q.setJSON(((Number) plh).intValue(), asString(val, null));
              } else {
                q.setJSON((String) plh, asString(val, null));
              }
              break;
          }
        }
      }

      qh.handle(q, hook);

    } finally {
      q.close();
    }
  }

  private static void onlineBackup(DbMethodCall mc) {
    String targetFile = asString(mc.args[1], null);
    Log.w(TAG, "Online backup into: " + targetFile);
    mc.successOnMainThread(mc.getDb().onlineBackup(targetFile));
  }

  private static void removeFloatIndex(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    String path = asString(mc.args[2], null);
    boolean unique = asBoolean(mc.args[3], false);
    mc.getDb().removeFloatIndex(coll, path, unique);
    mc.successOnMainThread(null);
  }

  private static void ensureFloatIndex(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    String path = asString(mc.args[2], null);
    boolean unique = asBoolean(mc.args[3], false);
    mc.getDb().ensureFloatIndex(coll, path, unique);
    mc.successOnMainThread(null);
  }

  private static void removeIntIndex(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    String path = asString(mc.args[2], null);
    boolean unique = asBoolean(mc.args[3], false);
    mc.getDb().removeIntIndex(coll, path, unique);
    mc.successOnMainThread(null);
  }

  private static void ensureIntIndex(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    String path = asString(mc.args[2], null);
    boolean unique = asBoolean(mc.args[3], false);
    mc.getDb().ensureIntIndex(coll, path, unique);
    mc.successOnMainThread(null);
  }

  private static void removeStringIndex(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    String path = asString(mc.args[2], null);
    boolean unique = asBoolean(mc.args[3], false);
    mc.getDb().removeStringIndex(coll, path, unique);
    mc.successOnMainThread(null);
  }

  private static void ensureStringIndex(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    String path = asString(mc.args[2], null);
    boolean unique = asBoolean(mc.args[3], false);
    mc.getDb().ensureStringIndex(coll, path, unique);
    mc.successOnMainThread(null);
  }

  private static void removeCollection(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    mc.getDb().removeCollection(coll);
    mc.successOnMainThread(null);
  }

  private static void renameCollection(DbMethodCall mc) {
    String oldName = asString(mc.args[1], null);
    String newName = asString(mc.args[2], null);
    mc.getDb().renameCollection(oldName, newName);
    mc.successOnMainThread(null);
  }

  private static void del(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    long id = asLong(mc.args[2], 0);
    mc.getDb().del(coll, id);
    mc.successOnMainThread(null);
  }

  private static void get(DbMethodCall mc) throws Exception {
    String coll = asString(mc.args[1], null);
    long id = asLong(mc.args[2], 0);
    mc.successOnMainThread(mc.getDb().getAsString(coll, id));
  }

  private static void patch(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    String json = asString(mc.args[2], null);
    long id = asLong(mc.args[3], 0);
    boolean upsert = asBoolean(mc.args[4], false);
    if (upsert) {
      mc.getDb().patchOrPut(coll, json, id);
    } else {
      mc.getDb().patch(coll, json, id);
    }
    mc.successOnMainThread(null);
  }

  private static void put(DbMethodCall mc) {
    String coll = asString(mc.args[1], null);
    String json = asString(mc.args[2], null);
    long id = asLong(mc.args[3], 0);
    mc.successOnMainThread(mc.getDb().put(coll, json, id));
  }

  private static void info(DbMethodCall mc) throws Exception {
    mc.successOnMainThread(mc.dbe.db.infoAsString());
  }

  private static void open(DbMethodCall mc) {
    synchronized (mc.plugin) {
      String path = normalizePath(mc.getAppContext(), (String) mc.args[1]);
      DbEntry dbe = null;
      for (DbEntry v : dbmap.values()) {
        if (v.path.equals(path)) {
          dbe = v;
          break;
        }
      }
      if (dbe != null) {
        dbe.countOpen();
        mc.successOnMainThread(dbe.handle);
        return;
      }
      Map<String, Object> opts = cast(mc.args[2]);
      EJDB2Builder b = new EJDB2Builder(path);
      if (asBoolean(opts.get("readonly"), false)) {
        b.readonly();
      }
      if (asBoolean(opts.get("truncate"), false)) {
        b.truncate();
      }
      if (asBoolean(opts.get("wal_enabled"), true)) {
        b.withWAL();
      }
      b.walCRCOnCheckpoint(asBoolean(opts.get("wal_check_crc_on_checkpoint"), false));
      if (opts.containsKey("wal_checkpoint_buffer_sz")) {
        b.walCheckpointBufferSize(asInt(opts.get("wal_checkpoint_buffer_sz"), 0));
      }
      if (opts.containsKey("wal_checkpoint_timeout_sec")) {
        b.walCheckpointTimeoutSec(asInt(opts.get("wal_checkpoint_timeout_sec"), 0));
      }
      if (opts.containsKey("wal_savepoint_timeout_sec")) {
        b.walSavepointTimeoutSec(asInt(opts.get("wal_savepoint_timeout_sec"), 0));
      }
      if (opts.containsKey("wal_wal_buffer_sz")) {
        b.walBufferSize(asInt(opts.get("wal_wal_buffer_sz"), 0));
      }
      if (opts.containsKey("document_buffer_sz")) {
        b.documentBufferSize(asInt(opts.get("document_buffer_sz"), 0));
      }
      if (opts.containsKey("sort_buffer_sz")) {
        b.sortBufferSize(asInt(opts.get("sort_buffer_sz"), 0));
      }

      final Integer handle = dbkeys.incrementAndGet();
      dbmap.put(handle, new DbEntry(b.open(), handle, path));
      mc.successOnMainThread(handle);
    }
  }

  private static void close(DbMethodCall mc) {
    synchronized (mc.plugin) {
      if (mc.dbe.close()) {
        dbmap.remove(mc.dbe.handle);
      }
    }
    mc.successOnMainThread(null);
  }

  @SuppressWarnings("unchecked")
  private static <T> T cast(Object obj) {
    return (T) obj;
  }

  private static boolean asBoolean(Object obj, boolean dv) {
    if (obj instanceof Boolean) {
      return ((Boolean) obj).booleanValue();
    } else {
      return obj != null ? Boolean.parseBoolean(obj.toString()) : dv;
    }
  }

  private static int asInt(Object obj, int dv) {
    if (obj instanceof Number) {
      return ((Number) obj).intValue();
    } else {
      return obj != null ? Integer.parseInt(obj.toString()) : dv;
    }
  }

  private static long asLong(Object obj, long dv) {
    if (obj instanceof Number) {
      return ((Number) obj).longValue();
    } else {
      return obj != null ? Long.parseLong(obj.toString()) : dv;
    }
  }

  private static double asDouble(Object obj, double dv) {
    if (obj instanceof Number) {
      return ((Number) obj).doubleValue();
    } else {
      return obj != null ? Double.parseDouble(obj.toString()) : dv;
    }
  }

  private static String asString(Object obj, String dv) {
    return obj != null ? obj.toString() : dv;
  }

  private static String normalizePath(Context ctx, String path) {
    return ctx.getDatabasePath(path).getAbsolutePath();
  }

  private static DbMethod thread(DbMethod dbm) {
    return mc -> executor.execute(() -> {
      try {
        dbm.handle(mc);
      } catch (EJDB2Exception e) {
        mc.errorOnMainThread("@ejdb IWRC:" + e.getCode(), e.getMessage(), null);
      } catch (Exception e) {
        Log.e(TAG, e.getMessage(), e);
        mc.errorOnMainThread("@ejdb", e.toString(), null);
      }
    });
  }

  private static final class DbMethodCall {
    private DbMethodCall(Ejdb2FlutterPlugin plugin, DbEntry dbe, Object[] args, Result result) {
      this.plugin = plugin;
      this.dbe = dbe;
      this.args = args;
      this.result = result;
    }

    private final Ejdb2FlutterPlugin plugin;
    private final DbEntry dbe;
    private final Object[] args;
    private final Result result;

    private EJDB2 getDb() {
      return dbe.db;
    }

    private Context getAppContext() {
      return plugin.registrar.context();
    }

    private Activity getActivity() {
      return plugin.registrar.activity();
    }

    private boolean runOnMainThread(Runnable action) {
      Activity activity = getActivity();
      if (activity != null) {
        activity.runOnUiThread(action);
        return true;
      } else {
        action.run();
      }
      return false;
    }

    private void successOnMainThread(Object val) {
      runOnMainThread(() -> result.success(val));
    }

    private void errorOnMainThread(String errorCode, String errorMessage, Object errorDetails) {
      runOnMainThread(() -> result.error(errorCode, errorMessage, errorDetails));
    }
  }

  private static final class DbEntry {
    DbEntry(EJDB2 db, Integer handle, String path) {
      this.db = db;
      this.counter = new AtomicInteger(1);
      this.path = path;
      this.handle = handle;
    }

    private final EJDB2 db;
    private final AtomicInteger counter;
    private final String path;
    private final Integer handle;

    void countOpen() {
      this.counter.incrementAndGet();
    }

    boolean close() {
      int cnt = this.counter.decrementAndGet();
      if (cnt <= 0) {
        db.close();
      }
      return cnt <= 0;
    }
  }

  private interface DbMethod {
    void handle(DbMethodCall mc) throws Exception;
  }

  private interface DbQueryHandler {
    void handle(JQL q, String hook) throws Exception;
  }
}
