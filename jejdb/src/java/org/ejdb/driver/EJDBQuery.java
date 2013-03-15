package org.ejdb.driver;

import org.bson.BSONObject;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBQuery {

    // Query search mode flags
    public static final int JBQRYCOUNT = 1; /*< Query only count(*) */


    private EJDBCollection coll;
    private BSONObject query;
    private BSONObject[] qors;
    private BSONObject hints;
    private int flags;

    EJDBQuery(EJDBCollection coll, BSONObject query, BSONObject[] qors, BSONObject hints, int flags) {
        this.coll = coll;
        this.query = query;
        this.qors = qors;
        this.hints = hints;
        this.flags = flags;
    }

    public void execute() {
        QResult qResult = this.executeDB(query, qors, hints, flags);
        // TODO: create EJDBResultSet from QResult
    }


    protected native QResult executeDB(BSONObject query, BSONObject[] qors, BSONObject hints, int flags);

    private static class QResult {
        private long count;
        private long rsPointer;

        private QResult(long count, long rsPointer) {
            this.count = count;
            this.rsPointer = rsPointer;
        }
    }
}
