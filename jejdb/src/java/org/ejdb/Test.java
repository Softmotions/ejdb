package org.ejdb;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;
import org.bson.types.BasicBSONList;
import org.bson.types.ObjectId;
import org.ejdb.driver.EJDB;
import org.ejdb.driver.EJDBCollection;
import org.ejdb.driver.EJDBDriver;

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
            BSONObject bson = coll.load(new ObjectId("513f04d563f08b6400000000"));
            if (bson != null) {
                System.out.println(bson.toString());
            }

            Random rand = new Random();

            List<ObjectId> oids = new ArrayList<ObjectId>(500);

            int ri;

            for (int i = 0; i < 5/*00*/; ++ i) {
                ri = rand.nextInt();
//                System.out.println("Random: " + ri);

                ObjectId oid = coll.save(new BasicBSONObject("test", ri));
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
        } finally {
            db.close();
            System.out.println("DB closed");
        }
    }
}

