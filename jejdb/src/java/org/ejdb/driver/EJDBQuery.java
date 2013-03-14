package org.ejdb.driver;

import org.bson.BSONObject;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBQuery {
    private EJDBCollection coll;
    private BSONObject query;

    private long qPointer;

    EJDBQuery(EJDBCollection coll, BSONObject query) {
        this.coll = coll;
        this.query = query;

        this.createDB(this.query, null, null);
    }

    protected native void createDB(BSONObject query, BSONObject[] qors, BSONObject hints);
    protected native Object executeDB();
    protected native void closeDB();

}
