package org.ejdb.driver.test;

import junit.framework.*;
import junit.framework.Test;

import org.ejdb.bson.BSON;
import org.ejdb.bson.BSONObject;
import org.ejdb.bson.types.ObjectId;
import org.ejdb.driver.BSONQueryObject;
import org.ejdb.driver.EJDBQueryBuilder;

import java.util.Arrays;
import java.util.Date;
import java.util.Enumeration;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBQueryBuilderTest extends TestCase {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }

    public void testQB1() throws Exception {
        BSONQueryObject query = new BSONQueryObject("name", "Andy")
                .append("_id", "510f7fa91ad6270a00000000")
                .append("age", new BSONQueryObject().append("$gt", 20).append("$lt", 40))
                .append("score", new BSONQueryObject("$in", new Object[]{1, 22.12333, 1362835380447L, null}));

        BSONQueryObject hints = new BSONQueryObject("$max", 232);

        EJDBQueryBuilder qb = new EJDBQueryBuilder();

        qb.field("name", "Andy");
        qb.field("_id").eq("510f7fa91ad6270a00000000");
        qb.field("age").gt(20).field("age").lt(40);
        qb.field("score").in(Arrays.<Object>asList(1, 22.12333, 1362835380447L, null));
        qb.setMaxResults(232);

        assertEquals(query, qb.getMainQuery());
        assertEquals(hints, qb.getQueryHints());

        BSONObject decoded = BSON.decode(BSON.encode(qb.getMainQuery()));

        assertFalse(query.equals(decoded)); // after decoding BSON string field _id will be contain ObjectId!
        query.put("_id", new ObjectId("510f7fa91ad6270a00000000"));
        assertEquals(query, decoded);
    }

    public void testQB2() throws Exception {
        BSONQueryObject query = new BSONQueryObject()
                .append("name", "Andy")
                .append("bdata", new Date(1362835380447L))
                .append("car", new BSONQueryObject().append("name", "Lamborghini").append("maxspeed", 320))
                .append("dst", null);

        EJDBQueryBuilder qb = new EJDBQueryBuilder();

        qb.field("name", "Andy")
                .field("bdata").eq(new Date(1362835380447L));
        qb.field("car").field("name").eq("Lamborghini")
                .field("car").field("maxspeed").eq(320);
        qb.field("dst", null);

        assertEquals(query, qb.getMainQuery());
        assertEquals(new BSONObject(), qb.getQueryHints());

        BSONObject decoded = BSON.decode(BSON.encode(qb.getMainQuery()));

        assertEquals(query, decoded);
    }
}
