package com.softmotions.ejdb2;

import java.io.ByteArrayOutputStream;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class TestEJDB2 {

  private TestEJDB2() {
  }

  public static void main(String[] args) throws Exception {
    try (EJDB2 db = new EJDB2Builder("test.db").truncate().withWAL().open()) {
      EJDB2Exception exception = null;
      String json = "{'foo':'bar'}".replace('\'', '"');
      String patch = "[{'op':'add', 'path':'/baz', 'value':'qux'}]".replace('\'', '"');

      long id = db.putNew("c1", json);
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
      assert (exception.getMessage().indexOf("IWKV_ERROR_NOTFOUND") != -1);
      //
    }
  }
}
