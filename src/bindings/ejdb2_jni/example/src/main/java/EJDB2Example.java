import com.softmotions.ejdb2.EJDB2;
import com.softmotions.ejdb2.EJDB2Builder;

/**
 * Minimal EJDB2 Java example.
 *
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class EJDB2Example {

  public static void main(String[] args) {
    try (EJDB2 db = new EJDB2Builder("example.db").truncate().open()) {
      long id = db.put("parrots", "{\"name\":\"Bianca\", \"age\": 4}");
      System.out.println("Bianca record: " + id);

      id = db.put("parrots", "{\"name\":\"Darko\", \"age\": 8}");
      System.out.println("Darko record: " + id);

      db.createQuery("@parrots/[age > :age]").setLong("age", 3).execute((doc) -> {
        System.out.println(String.format("Found %s", doc));
        return 1;
      });
    }
  }
}
