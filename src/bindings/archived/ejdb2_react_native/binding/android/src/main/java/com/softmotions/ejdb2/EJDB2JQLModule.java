package com.softmotions.ejdb2;

import java.io.UnsupportedEncodingException;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;

public class EJDB2JQLModule extends ReactContextBaseJavaModule {

  @SuppressWarnings("StaticCollection")
  static Map<Integer, JQL> jqlmap = new ConcurrentHashMap<>();

  static final AtomicInteger jqlkeys = new AtomicInteger(0);

  private final Executor executor;

  public EJDB2JQLModule(ReactApplicationContext reactContext, Executor executor) {
    super(reactContext);
    this.executor = executor;
  }

  @Override
  public String getName() {
    return "EJDB2JQL";
  }

  public static Integer createQuery(EJDB2 db, String query, String collection) throws EJDB2Exception {
    Integer id = jqlkeys.getAndIncrement();
    jqlmap.put(id, db.createQuery(query, collection));
    return id;
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public String getQuery(Integer jqlid) {
    return jql(jqlid).getQuery();
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public String getCollection(Integer jqlid) {
    return jql(jqlid).getCollection();
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setCollection(Integer jqlid, String collection) {
    jql(jqlid).setCollection(collection);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void withExplain(Integer jqlid) {
    jql(jqlid).withExplain();
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public String getExplainLog(Integer jqlid) {
    try {
      return jql(jqlid).getExplainLog();
    } catch (UnsupportedEncodingException ignored) {
      return null;
    }
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setSkip(Integer jqlid, String val) {
    jql(jqlid).setSkip(Long.parseLong(val));
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public String getSkip(Integer jqlid) {
    return String.valueOf(jql(jqlid).getSkip());
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setLimit(Integer jqlid, String val) {
    jql(jqlid).setLimit(Long.parseLong(val));
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public String getLimit(Integer jqlid) {
    return String.valueOf(jql(jqlid).getLimit());
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void psetString(Integer jqlid, Integer placeholder, String val) {
    jql(jqlid).setString(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setString(Integer jqlid, String placeholder, String val) {
    jql(jqlid).setString(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void psetLong(Integer jqlid, Integer placeholder, String val) {
    jql(jqlid).setLong(placeholder, Long.parseLong(val));
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setLong(Integer jqlid, String placeholder, String val) {
    jql(jqlid).setLong(placeholder, Long.parseLong(val));
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void psetJSON(Integer jqlid, Integer placeholder, String val) {
    jql(jqlid).setJSON(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setJSON(Integer jqlid, String placeholder, String val) {
    jql(jqlid).setJSON(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void psetRegexp(Integer jqlid, Integer placeholder, String val) {
    jql(jqlid).setRegexp(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setRegexp(Integer jqlid, String placeholder, String val) {
    jql(jqlid).setRegexp(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void psetDouble(Integer jqlid, Integer placeholder, Double val) {
    jql(jqlid).setDouble(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setDouble(Integer jqlid, String placeholder, Double val) {
    jql(jqlid).setDouble(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void psetBoolean(Integer jqlid, Integer placeholder, Boolean val) {
    jql(jqlid).setBoolean(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setBoolean(Integer jqlid, String placeholder, Boolean val) {
    jql(jqlid).setBoolean(placeholder, val);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void psetNull(Integer jqlid, Integer placeholder) {
    jql(jqlid).setNull(placeholder);
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void setNull(Integer jqlid, String placeholder) {
    jql(jqlid).setNull(placeholder);
  }

  @ReactMethod
  public void execute(Integer jqlid, String eventId, Promise promise) {
    final ReactContext ctx = getReactApplicationContext();
    final DeviceEventManagerModule.RCTDeviceEventEmitter module = ctx
        .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class);

    AtomicReference<WritableArray> holder = new AtomicReference<>(Arguments.createArray());

    thread(jqlid, promise, jql -> {
      jql.execute((id, json) -> {
        WritableArray wa = holder.get();
        if (wa.size() >= 64) {
          module.emit(eventId, wa);
          wa = Arguments.createArray();
          holder.set(wa);
        }
        WritableMap map = Arguments.createMap();
        map.putString("id", String.valueOf(id));
        map.putString("raw", json);
        wa.pushMap(map);
        return 1;
      });
      WritableArray wa = holder.get();
      wa.pushNull();
      holder.set(null);
      module.emit(eventId, wa);
      promise.resolve(null);
    });
  }

  @ReactMethod
  public void list(Integer jqlid, ReadableMap opts, Promise promise) {
    thread(jqlid, promise, jql -> {
      if (opts.hasKey("skip")) {
        jql.setSkip(Long.parseLong(opts.getString("skip")));
      }
      if (opts.hasKey("limit")) {
        jql.setLimit(Long.parseLong(opts.getString("limit")));
      }
      WritableArray ret = Arguments.createArray();
      jql.execute((id, json) -> {
        WritableMap map = Arguments.createMap();
        map.putString("id", String.valueOf(id));
        map.putString("raw", json);
        ret.pushMap(map);
        return 1;
      });
      promise.resolve(ret);
    });
  }

  @ReactMethod
  public void first(Integer jqlid, Promise promise) {
    thread(jqlid, promise, jql -> {
      Map.Entry<Long, String> f = jql.first();
      if (f.getValue() != null) {
        WritableMap map = Arguments.createMap();
        map.putInt("id", f.getKey().intValue());
        map.putString("raw", f.getValue());
        promise.resolve(map);
      } else {
        promise.resolve(null);
      }
    });
  }

  @ReactMethod
  public void executeScalarInt(Integer jqlid, Promise promise) {
    thread(jqlid, promise, jql -> promise.resolve(String.valueOf(jql.executeScalarInt())));
  }

  @ReactMethod
  public void reset(Integer jqlid, Promise promise) {
    immediate(jqlid, promise, jql -> {
      jql.reset();
      promise.resolve(null);
    });
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public void close(Integer jqlid) {
    JQL jql = jqlmap.get(jqlid);
    if (jql != null) {
      jqlmap.remove(jqlid);
      try {
        jql.close();
      } catch (Exception ignored) {
      }
    }
  }

  private JQL jql(Integer jqlid) throws RuntimeException {
    JQL jql = jqlmap.get(jqlid);
    if (jql == null) {
      throw new IllegalStateException("Query handle already disposed");
    }
    return jql;
  }

  private void immediate(Integer jqlid, Promise promise, JqlLogic fn) {
    try {
      fn.apply(jql(jqlid));
    } catch (EJDB2Exception e) {
      promise.reject(String.valueOf(e.getCode()), e.toString());
    } catch (Exception e) {
      promise.reject(null, e.toString());
    }
  }

  private void thread(Integer jqlid, Promise promise, JqlLogic fn) {
    executor.execute(() -> immediate(jqlid, promise, fn));
  }

  private interface JqlLogic {
    void apply(JQL jql) throws Exception;
  }
}