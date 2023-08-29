package com.softmotions.ejdb2;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;

import com.softmotions.ejdb2.JSON.ObjectBuilder;
import com.softmotions.ejdb2.JSON.ValueType;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class TestEJDB2 {

  private TestEJDB2() {
  }

  private static void jsonBasicTest() throws Exception {
    JSON json = JSON.fromString("{\"foo\":\"bar\"}");
    assert json.type == ValueType.OBJECT;
    assert json.value != null;
    Map<String, Object> map = (Map<String, Object>) json.value;
    assert map.get("foo").equals("bar");

    ObjectBuilder b = JSON.object();
    b.put("foo", "bar").putArray("baz").add(1).add("one").toJSON();

    json = b.toJSON().at("/baz/1");
    assert json.isString();
    assert "one".equals(json.value);
  }

  private static void dbTest() throws Exception {
    try (EJDB2 db = new EJDB2Builder("test.db").truncate().withWAL().open()) {
      EJDB2Exception exception = null;
      String json = "{'foo':'bar'}".replace('\'', '"');
      String patch = "[{'op':'add', 'path':'/baz', 'value':'qux'}]".replace('\'', '"');

      long id = db.put("c1", json);
      assert (id == 1L);

      ByteArrayOutputStream bos = new ByteArrayOutputStream();
      db.get("c1", 1L, bos);
      assert (bos.toString().equals(json));

      db.patch("c1", patch, id);
      bos.reset();
      db.get("c1", 1L, bos);
      assert (bos.toString().equals("{'foo':'bar','baz':'qux'}".replace('\'', '"')));
      bos.reset();

      db.del("c1", id);

      exception = null;
      try {
        db.get("c1", id, bos);
      } catch (EJDB2Exception e) {
        exception = e;
      }
      assert (exception != null);
      assert (exception.getMessage().contains("IWKV_ERROR_NOTFOUND"));

      // JQL resources can be closed explicitly or garbage collected
      JQL q = db.createQuery("@mycoll/*");
      assert (q.getDb() != null);
      assert (Objects.equals(q.getCollection(), "mycoll"));

      id = db.put("mycoll", "{'foo':'bar'}".replace('\'', '"'));
      assert (id == 1);

      exception = null;
      try {
        db.put("mycoll", "{\"");
      } catch (EJDB2Exception e) {
        exception = e;
      }
      assert (exception != null && exception.getMessage() != null);
      assert (exception.getCode() == 76005);
      assert (exception.getMessage().contains("JBL_ERROR_PARSE_UNQUOTED_STRING"));

      db.put("mycoll", "{'foo':'baz'}".replace('\'', '"'));

      Map<Long, String> results = new LinkedHashMap<>();
      q.executeRaw((docId, doc) -> {
        assert (docId > 0 && doc != null);
        results.put(docId, doc);
        return 1;
      });
      assert (results.size() == 2);
      assert (Objects.equals(results.get(1L), "{\"foo\":\"bar\"}"));
      assert (Objects.equals(results.get(2L), "{\"foo\":\"baz\"}"));
      results.clear();

      try (JQL q2 = db.createQuery("/[foo=:?]", "mycoll").setString(0, "zaz")) {
        q2.executeRaw((docId, doc) -> {
          results.put(docId, doc);
          return 1;
        });
      }
      assert (results.isEmpty());

      try (JQL q2 = db.createQuery("/[foo=:val]", "mycoll").setString("val", "bar")) {
        q2.executeRaw((docId, doc) -> {
          results.put(docId, doc);
          return 1;
        });
      }
      assert (results.size() == 1);
      assert (Objects.equals(results.get(1L), "{\"foo\":\"bar\"}"));

      exception = null;
      try {
        db.createQuery("@mycoll/[");
      } catch (EJDB2Exception e) {
        exception = e;
      }
      assert (exception != null && exception.getMessage() != null);
      assert (exception.getCode() == 87001);
      assert (exception.getMessage().contains("@mycoll/[ <---"));

      long count = db.createQuery("@mycoll/* | count").executeScalarInt();
      assert (count == 2);

      q.withExplain().execute();
      assert (q.getExplainLog().contains("[INDEX] NO [COLLECTOR] PLAIN"));

      json = db.infoAsString();
      assert (json != null);
      assert (json.contains("{\"name\":\"mycoll\",\"dbid\":4,\"rnum\":2,\"indexes\":[]}"));

      // Indexes
      db.ensureStringIndex("mycoll", "/foo", true);
      json = db.infoAsString();
      assert (json.contains("\"indexes\":[{\"ptr\":\"/foo\",\"mode\":5,\"idbf\":0,\"dbid\":5,\"rnum\":2}]"));

      db.patch("mycoll", patch, 2);

      json = db.createQuery("@mycoll/[foo=:?] and /[baz=:?]").setString(0, "baz").setString(1, "qux").firstValue();
      assert ("{\"foo\":\"baz\",\"baz\":\"qux\"}".equals(json));

      json = db.createQuery("@mycoll/[foo re :?]").setRegexp(0, ".*").firstValue();
      assert ("{\"foo\":\"baz\",\"baz\":\"qux\"}".equals(json));

      db.removeStringIndex("mycoll", "/foo", true);
      json = db.infoAsString();
      assert (json.contains("{\"name\":\"mycoll\",\"dbid\":4,\"rnum\":2,\"indexes\":[]}"));

      db.removeCollection("mycoll");
      db.removeCollection("c1");
      json = db.infoAsString();
      assert (json.contains("\"collections\":[]"));

      // Test rename collection
      bos.reset();
      db.put("cc1", "{\"foo\": 1}");
      db.renameCollection("cc1", "cc2");
      db.get("cc2", 1, bos);
      assert (bos.toString().equals("{\"foo\":1}"));

      // Check limit
      q = db.createQuery("@mycoll/* | limit 2 skip 3");
      assert (q.getLimit() == 2);
      assert (q.getSkip() == 3);

      // Test #333 
      db.put("test333", "{\"foo\":1.1}");

      long ts0 = System.currentTimeMillis();
      long ts = db.onlineBackup("test-bkp.db");
      assert (ts > ts0);
      assert (new File("test-bkp.db").exists());
    }

    try (EJDB2 db = new EJDB2Builder("test-bkp.db").withWAL().open()) {
      String val = db.getAsString("cc2", 1);
      assert (val.equals("{\"foo\":1}"));
    }
  }

  public static void main(String[] args) throws Exception {
    jsonBasicTest();
    dbTest();
  }
}
