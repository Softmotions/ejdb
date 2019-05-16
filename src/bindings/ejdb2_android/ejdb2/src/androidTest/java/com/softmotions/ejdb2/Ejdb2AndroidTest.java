package com.softmotions.ejdb2;

import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.UnsupportedEncodingException;
import java.util.LinkedHashMap;
import java.util.Map;

import static org.junit.Assert.*;

@RunWith(AndroidJUnit4.class)
public class Ejdb2AndroidTest {

  private EJDB2 db;

  @Before
  public void start() {
    try {
      File dir = InstrumentationRegistry.getTargetContext().getCacheDir();
      File path = new File(dir, "test.db");
      db = new EJDB2Builder(path.getAbsolutePath()).truncate().open();
    } catch (EJDB2Exception e) {
      e.printStackTrace();
      fail();
    }
  }

  @After
  public void shutdown() {
    db.close();
  }

  @Test
  public void ejdb2tests() {
    try {
      EJDB2Exception exception;
      String json = "{'foo':'bar'}".replace('\'', '"');
      String patch = "[{'op':'add', 'path':'/baz', 'value':'qux'}]".replace('\'', '"');

      long id = db.put("c1", json);
      assertEquals(1L, id);

      ByteArrayOutputStream bos = new ByteArrayOutputStream();
      db.get("c1", 1L, bos);
      assertEquals(bos.toString(), json);

      db.patch("c1", patch, id);
      bos.reset();
      db.get("c1", 1L, bos);
      assertEquals(bos.toString(), "{'foo':'bar','baz':'qux'}".replace('\'', '"'));
      bos.reset();

      db.del("c1", id);

      exception = null;
      try {
        db.get("c1", id, bos);
      } catch (EJDB2Exception e) {
        exception = e;
      }
      assertNotNull(exception);
      assertTrue(exception.getMessage().contains("IWKV_ERROR_NOTFOUND"));

      // JQL resources can be closed explicitly or garbage collected
      JQL q = db.createQuery("@mycoll/*");
      assertNotNull(q.getDb());
      assertEquals(q.getCollection(), "mycoll");

      id = db.put("mycoll", "{'foo':'bar'}".replace('\'', '"'));
      assertEquals(1, id);

      exception = null;
      try {
        db.put("mycoll", "{\"");
      } catch (EJDB2Exception e) {
        exception = e;
      }
      assertTrue(exception != null && exception.getMessage() != null);
      assertEquals(86005, exception.getCode());
      assertTrue(exception.getMessage().contains("JBL_ERROR_PARSE_UNQUOTED_STRING"));

      db.put("mycoll", "{'foo':'baz'}".replace('\'', '"'));

      final Map<Long, String> results = new LinkedHashMap<>();
      q.execute(new JQLCallback() {
        @Override
        public long onRecord(long docId, String doc) {
          assertTrue(docId > 0 && doc != null);
          results.put(docId, doc);
          return 1;
        }
      });
      assertEquals(2, results.size());
      assertEquals(results.get(1L), "{\"foo\":\"bar\"}");
      assertEquals(results.get(2L), "{\"foo\":\"baz\"}");
      results.clear();

      try {
        JQL q2 = db.createQuery("/[foo=:?]", "mycoll").setString(0, "zaz");
        q2.execute(new JQLCallback() {
          @Override
          public long onRecord(long docId, String doc) {
            results.put(docId, doc);
            return 1;
          }
        });
        q2.close();
      } catch (Exception e) {
        e.printStackTrace();
        fail();
      }
      assertTrue(results.isEmpty());

      try {
        JQL q2 = db.createQuery("/[foo=:val]", "mycoll").setString("val", "bar");
        q2.execute(new JQLCallback() {
          @Override
          public long onRecord(long docId, String doc) {
            results.put(docId, doc);
            return 1;
          }
        });
        q2.close();
      } catch (Exception e) {
        e.printStackTrace();
        fail();
      }
      assertEquals(1, results.size());
      assertEquals(results.get(1L), "{\"foo\":\"bar\"}");

      exception = null;
      try {
        db.createQuery("@mycoll/[");
      } catch (EJDB2Exception e) {
        exception = e;
      }
      assertTrue(exception != null && exception.getMessage() != null);
      assertEquals(87001, exception.getCode());
      assertTrue(exception.getMessage().contains("@mycoll/[ <---"));


      long count = db.createQuery("@mycoll/* | count").executeScalarInt();
      assertEquals(2, count);

      q.withExplain().execute();
      assertTrue(q.getExplainLog().contains("[INDEX] NO [COLLECTOR] PLAIN"));

      json = db.infoAsString();
      assertNotNull(json);
      assertTrue(json.contains("{\"name\":\"mycoll\",\"dbid\":4,\"rnum\":2,\"indexes\":[]}"));

      // Indexes
      db.ensureStringIndex("mycoll", "/foo", true);
      json = db.infoAsString();
      assertTrue(json.contains("\"indexes\":[{\"ptr\":\"/foo\",\"mode\":5,\"idbf\":0,\"dbid\":5,\"rnum\":2}]"));


      db.patch("mycoll", patch, 2);

      json = db.createQuery("@mycoll/[foo=:?] and /[baz=:?]")
              .setString(0, "baz")
              .setString(1, "qux")
              .firstJson();
      assertEquals("{\"foo\":\"baz\",\"baz\":\"qux\"}", json);

      json = db.createQuery("@mycoll/[foo re :?]").setRegexp(0, ".*").firstJson();
      assertEquals("{\"foo\":\"baz\",\"baz\":\"qux\"}", json);

      db.removeStringIndex("mycoll", "/foo", true);
      json = db.infoAsString();
      assertTrue(json.contains("{\"name\":\"mycoll\",\"dbid\":4,\"rnum\":2,\"indexes\":[]}"));

      db.removeCollection("mycoll");
      db.removeCollection("c1");
      json = db.infoAsString();
      assertTrue(json.contains("\"collections\":[]"));
    } catch (UnsupportedEncodingException | EJDB2Exception e) {
      e.printStackTrace();
      fail();
    }
  }
}
