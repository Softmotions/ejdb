package org.ejdb;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.ejdb.driver.EJDB;
import org.ejdb.driver.EJDBCollection;
import org.ejdb.driver.EJDBQuery;
import org.ejdb.driver.EJDBResultSet;

import java.io.IOException;
import java.util.Random;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class Test2 {
    public static int INDEX = -1;

    public static final int TEST_COUNT = 15;
    public static final Random random = new Random(System.currentTimeMillis());

    public static void main(String[] args) throws IOException {
        EJDB db = new EJDB();

        try {
            db.open("test5");
            System.out.println("test EJDB opened: " + db);

            EJDBCollection test = db.getCollection("test");

            System.out.println(test);

            test.setIndex("random", EJDBCollection.JBIDXNUM);

            test.updateMeta();

            System.out.println(test);

            test.drop(true);
            System.out.println("test collection dropped");
            test.ensureExists();
            System.out.println("test collection created");

            for (int i = 0; i < TEST_COUNT; ++i) {
                test.save(getTestObject());
            }

            System.out.println("test objects saved");

            test.sync();
            System.out.println("test collection synced");

            EJDBQuery query = test.createQuery(new BasicBSONObject());

            System.out.println("Objects matched: " + query.count());

            EJDBResultSet rs = query.find();
            for (BSONObject r : rs) {
                System.out.println(r);
            }
            rs.close();

            System.out.println();
            System.out.println(query.findOne());
            System.out.println();

            query = test.createQuery(new BasicBSONObject("randomBoolean", true));
            System.out.println("Objects with 'randomBoolean==true': " + query.count());
            rs = query.find();
            for (BSONObject r : rs) {
                System.out.println(r);
            }
            rs.close();

            query.getQueryObject().put("smallRandom", new BasicBSONObject("$lt", 6));
            rs = query.find();
            System.out.println("Objects with 'randomBoolean==true && smallRandom<6': " + rs.length());
            for (BSONObject r : rs) {
                System.out.println(r);
            }
            rs.close();
        } finally {
            db.close();
        }
    }

    private static BSONObject getTestObject() {
        BSONObject bsonObject = new BasicBSONObject();

        ++INDEX;

        bsonObject.put("name", "Object#" + INDEX);
        bsonObject.put("time", System.currentTimeMillis());
        bsonObject.put("index", INDEX);
        bsonObject.put("random", (random.nextBoolean() ? -1 : 1) * random.nextInt(65536));
        bsonObject.put("randomBoolean", random.nextBoolean());
        bsonObject.put("randomDouble", random.nextDouble());
        bsonObject.put("smallRandom", random.nextInt(10));

        return bsonObject;
    }
}
