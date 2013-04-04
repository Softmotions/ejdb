package org.ejdb.driver.test;

import junit.framework.TestCase;

import org.ejdb.bson.BSONObject;
import org.ejdb.bson.types.ObjectId;
import org.ejdb.driver.EJDB;
import org.ejdb.driver.EJDBCollection;
import org.ejdb.driver.EJDBQuery;
import org.ejdb.driver.EJDBQueryBuilder;
import org.ejdb.driver.EJDBResultSet;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.regex.Pattern;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBTest extends TestCase {

    private EJDB db;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        db = new EJDB();
        assertFalse(db.isOpen());
        db.open("jejdb-test");
        assertTrue(db.isOpen());
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();

        assertNotNull(db);
        assertTrue(db.isOpen());
        db.close();
        assertFalse(db.isOpen());
    }

    public void testLowLevel() throws Exception {
        assertTrue(db.isOpen());

        EJDBCollection coll = db.getCollection("testcoll");

        assertNotNull(coll);
        assertFalse(coll.isExists());

        coll.ensureExists();
        assertTrue(coll.isExists());

        BSONObject obj = new BSONObject("test", "test").append("test2", 123);

        ObjectId oid = coll.save(obj);
        assertNotNull(oid);
        assertEquals(oid, obj.getId());

        BSONObject lobj = coll.load(oid);
        assertNotNull(lobj);
        assertEquals(obj, lobj);

        EJDBQueryBuilder qb;

        qb = new EJDBQueryBuilder();
        EJDBQuery query = coll.createQuery(qb);
        EJDBResultSet rs = query.find();
        assertEquals(rs.length(), 1);
        for (BSONObject r : rs) {
            assertNotNull(r);
            assertEquals(r, lobj);
        }
        rs.close();

        assertEquals(lobj, query.findOne());

        qb = new EJDBQueryBuilder();
        EJDBQuery query2 = db.getCollection("test2").createQuery(qb);
        assertNull(query2.findOne());

        assertEquals(query.count(), 1);
        assertEquals(query2.count(), 0);

        qb = new EJDBQueryBuilder();
        qb.field("test", "test")
                .excludeField("test2");

        EJDBQuery query3 = coll.createQuery(new BSONObject("test", "test"), new BSONObject("$fields", new BSONObject("test2", 0)));
        assertEquals(query3.count(), 1);

        rs = query3.find();
        assertEquals(rs.length(), 1);
        rs.close();

        coll.remove(oid);
        lobj = coll.load(oid);
        assertNull(lobj);

        db.sync();

        db.ensureCollection("test2", new EJDBCollection.Options(false, false, 90000, 0));
        db.dropCollection("test2", true);
    }

    public void testQueries() throws Exception {
        assertTrue(db.isOpen());

        BSONObject obj1 = new BSONObject("name", "Grenny")
                .append("type", "African Grey")
                .append("male", true)
                .append("age", 1)
                .append("birthdate", new Date())
                .append("likes", new String[]{"green color", "night", "toys"})
                .append("extra1", null);

        BSONObject obj2 = new BSONObject("name", "Bounty")
                .append("type", "Cockatoo")
                .append("male", false)
                .append("age", 15)
                .append("birthdate", new Date())
                .append("likes", new String[]{"sugar cane"})
                .append("extra1", null);

        EJDBCollection parrots = db.getCollection("parrots");

        List<ObjectId> ss = parrots.save(Arrays.asList(obj1, null, obj2));
        assertEquals(ss.size(), 3);
        assertNotNull(ss.get(0));
        assertNull(ss.get(1));
        assertNotNull(ss.get(2));

        BSONObject obj12 = parrots.load(ss.get(0));
        assertNotNull(obj12);
        assertEquals(ss.get(0), obj12.getId());
        assertEquals(obj1, obj12);

        EJDBQueryBuilder qb;

        qb = new EJDBQueryBuilder();
        EJDBQuery query = parrots.createQuery(qb);
        EJDBResultSet rs = query.find();
        assertEquals(2, rs.length());
        rs.close();

        qb = new EJDBQueryBuilder();
        qb.field("name", Pattern.compile("(grenny|bounty)", Pattern.CASE_INSENSITIVE))
                .orderBy().asc("name");

        query = parrots.createQuery(qb);

        rs = query.find();
        assertEquals(rs.length(), 2);
        BSONObject robj1 = rs.next();
        assertEquals("Bounty", robj1.get("name"));
        assertEquals(15, robj1.get("age"));
        rs.close();

        qb = new EJDBQueryBuilder();
        qb
                .or().field("name", "Grenny")
                .or().field("name", "Bounty");
        qb.orderBy().asc("name");
        query = parrots.createQuery(qb);

        rs = query.find();
        assertEquals(2, rs.length());
        rs.close();

        qb = new EJDBQueryBuilder();
        qb
                .or().field("name", "Grenny");
        qb.orderBy().asc("name");
        query = parrots.createQuery(qb);
        assertEquals(1, query.count());
    }

    public void testIndexes() throws Exception {
        assertTrue(db.isOpen());

        BSONObject sally = new BSONObject("name", "Sally").append("mood", "Angry");
        BSONObject molly = new BSONObject("name", "Molly").append("mood", "Very angry").append("secret", null);

        EJDBCollection birds = db.getCollection("birds");
        birds.save(Arrays.asList(sally, molly));

        ByteArrayOutputStream log;

        EJDBQuery query = birds.createQuery(new EJDBQueryBuilder().field("name", "Molly"));

        query.find(log = new ByteArrayOutputStream());
        assertTrue(log.toString().contains("RUN FULLSCAN"));

        birds.ensureStringIndex("name");

        query.find(log = new ByteArrayOutputStream());
        assertTrue(log.toString().contains("MAIN IDX: 'sname'"));
        assertFalse(log.toString().contains("RUN FULLSCAN"));
    }

    public void testTransactions() throws Exception {
        assertTrue(db.isOpen());

        ObjectId boid;
        BSONObject bar = new BSONObject("foo", "bar");

        EJDBCollection bars = db.getCollection("bars");

        assertFalse(bars.isTransactionActive());
        bars.beginTransaction();
        assertTrue(bars.isTransactionActive());
        boid = bars.save(bar);
        assertNotNull(boid);
        assertNotNull(bars.load(boid));
        bars.rollbackTransaction();
        assertFalse(bars.isTransactionActive());
        assertNull(bars.load(boid));

        assertFalse(bars.isTransactionActive());
        bars.beginTransaction();
        assertTrue(bars.isTransactionActive());
        boid = bars.save(bar);
        assertNotNull(boid);
        assertNotNull(bars.load(boid));
        bars.commitTransaction();
        assertFalse(bars.isTransactionActive());
        BSONObject bar2 = bars.load(boid);
        assertNotNull(bar2);
        assertEquals(bar2.getId(), boid);
        assertEquals(bar2, bar);
    }
}
