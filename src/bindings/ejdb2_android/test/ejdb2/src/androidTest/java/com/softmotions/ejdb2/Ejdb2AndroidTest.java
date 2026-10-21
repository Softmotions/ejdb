package com.softmotions.ejdb2;

import android.content.Context;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

/**
 * Android binding test: loads {@code libejdb2jni.so} for the ABI of the test
 * device/emulator and runs the JNI binding test suite against a database in the
 * application cache directory.
 *
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
@RunWith(AndroidJUnit4.class)
public class Ejdb2AndroidTest {

  private EJDB2 db;

  private File dbFile;

  private File backupFile;

  @Before
  public void start() throws EJDB2Exception {
    Context ctx = InstrumentationRegistry.getInstrumentation().getTargetContext();
    File dir = ctx.getCacheDir();
    dbFile = new File(dir, "test.db");
    db = new EJDB2Builder(dbFile.getAbsolutePath()).truncate().withWAL().open();
  }

  @After
  public void shutdown() {
    if (db != null) {
      db.close();
      db = null;
    }
    if (dbFile != null) {
      File[] files = dbFile.getParentFile().listFiles(
          (d, name) -> name.startsWith(dbFile.getName()));
      if (files != null) {
        for (File f : files) {
          assertTrue("Failed to remove " + f, f.delete() || !f.exists());
        }
      }
    }
    if (backupFile != null) {
      backupFile.delete();
    }
  }

  @Test
  public void documentsTest() throws Exception {
    String json = "{'foo':'bar'}".replace('\'', '"');
    String patch = "[{'op':'add', 'path':'/baz', 'value':'qux'}]".replace('\'', '"');

    long id = db.put("c1", json);
    assertEquals(1L, id);

    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    db.get("c1", 1L, bos);
    assertEquals(json, bos.toString());

    db.patch("c1", patch, id);
    bos.reset();
    db.get("c1", 1L, bos);
    assertEquals("{'foo':'bar','baz':'qux'}".replace('\'', '"'), bos.toString());
    bos.reset();

    db.del("c1", id);

    EJDB2Exception exception = null;
    try {
      db.get("c1", id, bos);
    } catch (EJDB2Exception e) {
      exception = e;
    }
    assertNotNull(exception);
    assertTrue(exception.getMessage().contains("IWKV_ERROR_NOTFOUND"));
  }

  @Test
  public void queryTest() throws Exception {
    // JQL resources can be closed explicitly or garbage collected
    JQL q = db.createQuery("@mycoll/*");
    assertNotNull(q.getDb());
    assertEquals("mycoll", q.getCollection());

    long id = db.put("mycoll", "{\"foo\":\"bar\"}");
    assertEquals(1, id);

    EJDB2Exception exception = null;
    try {
      db.put("mycoll", "{\"");
    } catch (EJDB2Exception e) {
      exception = e;
    }
    assertNotNull(exception);
    assertNotNull(exception.getMessage());
    assertEquals(76005, exception.getCode());
    assertTrue(exception.getMessage().contains("JBL_ERROR_PARSE_UNQUOTED_STRING"));

    db.put("mycoll", "{\"foo\":\"baz\"}");

    final Map<Long, String> results = new LinkedHashMap<>();
    q.executeRaw((docId, doc) -> {
      assertTrue(docId > 0 && doc != null);
      results.put(docId, doc);
      return 1;
    });
    assertEquals(2, results.size());
    assertEquals("{\"foo\":\"bar\"}", results.get(1L));
    assertEquals("{\"foo\":\"baz\"}", results.get(2L));
    results.clear();

    try (JQL q2 = db.createQuery("/[foo=:?]", "mycoll").setString(0, "zaz")) {
      q2.executeRaw((docId, doc) -> {
        results.put(docId, doc);
        return 1;
      });
    }
    assertTrue(results.isEmpty());

    try (JQL q2 = db.createQuery("/[foo=:val]", "mycoll").setString("val", "bar")) {
      q2.executeRaw((docId, doc) -> {
        results.put(docId, doc);
        return 1;
      });
    }
    assertEquals(1, results.size());
    assertEquals("{\"foo\":\"bar\"}", results.get(1L));

    exception = null;
    try {
      db.createQuery("@mycoll/[");
    } catch (EJDB2Exception e) {
      exception = e;
    }
    assertNotNull(exception);
    assertNotNull(exception.getMessage());
    assertEquals(87001, exception.getCode());
    assertTrue(exception.getMessage().contains("@mycoll/[ <---"));

    long count = db.createQuery("@mycoll/* | count").executeScalarInt();
    assertEquals(2, count);

    q.withExplain().execute();
    assertTrue(q.getExplainLog().contains("[INDEX] NO [COLLECTOR] PLAIN"));

    // The collection dbid depends on the collection creation history, so the
    // assertions below intentionally do not pin it.
    String json = db.infoAsString();
    assertNotNull(json);
    assertTrue(json.contains("\"name\":\"mycoll\""));
    assertTrue(json.contains("\"rnum\":2"));
    assertTrue(json.contains("\"indexes\":[]"));

    // Indexes
    db.ensureStringIndex("mycoll", "/foo", true);
    json = db.infoAsString();
    assertTrue(json.contains("\"ptr\":\"/foo\""));
    assertTrue(json.contains("\"mode\":5"));
    assertTrue(json.contains("\"indexes\":["));

    String patch = "[{\"op\":\"add\", \"path\":\"/baz\", \"value\":\"qux\"}]";
    db.patch("mycoll", patch, 2);

    final String patchMatch = db.createQuery("@mycoll/[foo=:?] and /[baz=:?]")
        .setString(0, "baz")
        .setString(1, "qux")
        .firstValue();
    assertEquals("{\"foo\":\"baz\",\"baz\":\"qux\"}", patchMatch);

    String reMatch = db.createQuery("@mycoll/[foo re :?]").setRegexp(0, ".*").firstValue();
    assertEquals("{\"foo\":\"baz\",\"baz\":\"qux\"}", reMatch);

    db.removeStringIndex("mycoll", "/foo", true);
    json = db.infoAsString();
    assertTrue(json.contains("\"indexes\":[]"));

    db.removeCollection("mycoll");
    db.removeCollection("c1");
    json = db.infoAsString();
    assertTrue(json.contains("\"collections\":[]"));
  }

  @Test
  public void collectionsAndBackupTest() throws Exception {
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    db.put("cc1", "{\"foo\": 1}");
    db.renameCollection("cc1", "cc2");
    db.get("cc2", 1, bos);
    assertEquals("{\"foo\":1}", bos.toString());

    JQL q = db.createQuery("@mycoll/* | limit 2 skip 3");
    assertEquals(2, q.getLimit());
    assertEquals(3, q.getSkip());

    db.put("test333", "{\"foo\":1.1}");
    backupFile = new File(dbFile.getParentFile(), "test-bkp.db");
    File backup = backupFile;
    long ts0 = System.currentTimeMillis();
    long ts = db.onlineBackup(backup.getAbsolutePath());
    assertTrue(ts > ts0);
    assertTrue(backup.exists());

    db.close();
    db = new EJDB2Builder(backup.getAbsolutePath()).withWAL().open();
    assertEquals("{\"foo\":1}", db.getAsString("cc2", 1));
    assertTrue(Objects.equals(db.getAsString("test333", 1), "{\"foo\":1.1}"));
  }

  @Test
  public void jsonTest() {
    JSON json = JSON.fromString("{\"foo\":\"bar\"}");
    assertEquals(JSON.ValueType.OBJECT, json.type);
    assertNotNull(json.value);
    Map<?, ?> map = (Map<?, ?>) json.value;
    assertEquals("bar", map.get("foo"));

    JSON.ObjectBuilder b = JSON.object();
    b.put("foo", "bar").putArray("baz").add(1).add("one").toJSON();
    json = b.toJSON().at("/baz/1");
    assertTrue(json.isString());
    assertEquals("one", json.value);
  }
}
