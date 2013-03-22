package org.ejdb.driver.test;

import junit.framework.TestCase;

import org.ejdb.driver.EJDB;

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

    public void testDBOpen() throws Exception {
    }
}
