package org.ejdb;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.BasicBSONList;
import org.bson.types.ObjectId;
import org.ejdb.driver.EJDB;
import org.ejdb.driver.EJDBCollection;
import org.ejdb.driver.EJDBDriver;
import org.ejdb.driver.EJDBQuery;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class Test {
    public static void main(String[] args) throws InterruptedException, IOException {
        EJDB db = new EJDB();

        try {
            db.open("test");

            System.out.println("DB opened");
//            System.in.read();

//            db.ensureCollection("test");
//
//            System.out.println("collection 'test' created");
//            System.in.read();

//            db.ensureCollection("test2");
//
//            System.out.println("collection 'test2' created");
////            System.in.read();
//
            db.dropCollection("test");
//
//            System.out.println("collection 'test' dropped");
////            System.in.read();
//
//            db.dropCollection("test2", true);
//
//            System.out.println("collection 'test2' dropped");
//            System.in.read();


            EJDBCollection coll = db.getCollection("test");
            coll.ensureExists();
            coll.drop();


            BSONObject b = new BasicBSONObject("name", "name");
            EJDBQuery query = coll.createQuery(b, new BSONObject[]{b,b,b,b}, b);

//            if (true) {
//                return;
//            }


//
//            db.close();
//
//            db.isOpen();

            coll.setIndex("test", EJDBCollection.JBIDXDROPALL | EJDBCollection.JBIDXISTR);

//            if (true) {
//                return;
//            }
//
//            coll.createQuery(null);

            BSONObject bson = coll.load(new ObjectId("513f04d563f08b6400000000"));
            if (bson != null) {
                System.out.println(bson);
            }

            Random rand = new Random();

            List<ObjectId> oids = new ArrayList<ObjectId>(500);

            int ri;

            for (int i = 0; i < 5/*00*/; ++ i) {
                ri = rand.nextInt();
//                System.out.println("Random: " + ri);

                ObjectId oid = coll.save(new BasicBSONObject("random", ri));
                if (oid == null) {
                    System.out.println("Error saving");
                } else {
                    bson = coll.load(oid);
                    if (bson == null) {
                        System.out.println("Error loading");
                    } else {
                        System.out.println(bson);
                        oids.add(oid);
                    }
                }
            }

            List<ObjectId> roids = new ArrayList<ObjectId>(oids.size());
            List<ObjectId> nroids = new ArrayList<ObjectId>(oids.size());

            for (ObjectId oid : oids) {
                coll.remove(oid);
                System.out.println("Object removed #" + oid);
                (true ? roids : nroids).add(oid);
            }

            System.out.println("Check removed (" + roids.size() + ")");
            for (ObjectId oid : roids) {
                bson = coll.load(oid);
                if (bson != null) {
                    System.out.println("Achtung! Object exists: " + bson);
                } else {
                    System.out.println("Ok! Object not loaded. (#" + oid + ")");
                }
            }

            System.out.println("Check not removed (" + nroids.size() + ")");
            for (ObjectId oid : nroids) {
                bson = coll.load(oid);
                if (bson != null) {
                    System.out.println("Achtung! Object exists: " + bson);
                } else {
                    System.out.println("That ok! Object not loaded. (#" + oid + ")");
                }
            }

//            int i = 0;

//            while(true) {
//                for (ObjectId oid : oids) {
//                    bson = coll.load(oid);
//                }
//            }
//
            BasicBSONList list = new BasicBSONList();

            list.add(new BasicBSONObject("random", rand.nextInt()));
            list.add(new BasicBSONObject("random", rand.nextInt()));
            list.add(new BasicBSONObject("random", rand.nextInt()));
            list.add(new BasicBSONObject("random", rand.nextInt()));
            list.add(new BasicBSONObject("random", rand.nextInt()));

            ObjectId oid = coll.save(list);
            if (oid == null) {
                System.out.println("Error saving");
            } else {
                bson = coll.load(oid);
                if (bson == null) {
                    System.out.println("Error loading");
                } else {
                    System.out.println(bson.toString());
                    oids.add(oid);
                }
            }

            System.out.println("Collection sync: ");coll.sync();


            List<BSONObject> objs = new ArrayList<BSONObject>(5);
            for(int i=0; i < 5; ++i) {
                objs.add(new BasicBSONObject("random", rand.nextBoolean()));
            }

            oids = coll.save(objs);
            for (ObjectId oid2 : oids) {
                if (oid2 == null) {
                    System.out.println("Error saving");
                } else {
                    bson = coll.load(oid2);
                    if (bson == null) {
                        System.out.println("Error loading");
                    } else {
                        System.out.println(bson);
                    }
                }
            }

            System.out.println("DB sync: ");db.sync();

        } finally {
            db.close();
            System.out.println("DB closed");
        }
    }
}

