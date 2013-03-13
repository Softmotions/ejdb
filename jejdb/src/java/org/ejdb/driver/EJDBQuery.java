package org.ejdb.driver;

import org.bson.BSONObject;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBQuery {
    private long dbPointer;
    private String cname;

    private BSONObject query;

    EJDBQuery(long dbPointer, String cname, BSONObject query) {
        this.dbPointer = dbPointer;
        this.cname = cname;
        this.query = query;

        this.createDB(null);
    }

    protected native void createDB(BSONObject query);
}
