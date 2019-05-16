package com.softmotions.ejdb2.example;

import com.softmotions.ejdb2.EJDB2;
import com.softmotions.ejdb2.EJDB2Builder;
import com.softmotions.ejdb2.EJDB2Exception;
import com.softmotions.ejdb2.JQLCallback;

/**
 * Minimal EJDB2 Java example.
 *
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class EJDB2Example {

  public static void main(String[] args) {
    try {
      EJDB2 db = new EJDB2Builder("example.db").truncate().open();
      long id = db.put("parrots", "{\"name\":\"Bianca\", \"age\": 4}");
      System.out.println("Bianca record: " + id);

      id = db.put("parrots", "{\"name\":\"Darko\", \"age\": 8}");
      System.out.println("Darko record: " + id);

      db.createQuery("@parrots/[age > :age]").setLong("age", 3).execute(new JQLCallback() {
        @Override
        public long onRecord(long docId, String doc) {
          System.out.println(String.format("Found %d %s", docId, doc));
          return 1;
        }
      });
      db.close();
    } catch (EJDB2Exception ex) {
      System.out.println(String.format("Error %s", ex.getMessage()));
    }
  }
}