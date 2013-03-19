package org.ejdb.driver;

import org.bson.BSONObject;
import org.bson.BasicBSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBQuery {

    // Query search mode flags
    protected static final int JBQRYCOUNT = 1; /*< Query only count(*) */


    private EJDBCollection coll;
    private BSONObject query;
    private List<BSONObject> qors;
    private BSONObject hints;
    private int flags;

    EJDBQuery(EJDBCollection coll, BSONObject query, BSONObject[] qors, BSONObject hints) {
        this.coll = coll;
        this.query = query;
        this.qors = new ArrayList<BSONObject>();
        if (qors != null) {
            this.qors.addAll(Arrays.asList(qors));
        }
        this.hints = hints;
    }

    public BSONObject getQueryObject() {
        return query;
    }

    // TODO
    public EJDBResultSet find() {
        return this.execute(hints, 0).getResultSet();
    }

    public BSONObject findOne() {
        Map hintsMap = hints != null ? hints.toMap() : new HashMap();
        hintsMap.put("$max", 1);

        EJDBResultSet rs = this.execute(new BasicBSONObject(hintsMap), 0).getResultSet();
        BSONObject result = rs.hasNext() ? rs.next() : null;
        rs.close();

        return result;
    }

    public int update() {
        return this.execute(hints, JBQRYCOUNT).getCount();
    }

    public int count() {
        return this.execute(hints, JBQRYCOUNT).getCount();
    }

    protected QResult execute(BSONObject hints, int flags) {
        BSONObject[] qors = new BSONObject[this.qors.size()];
        this.qors.toArray(qors);

        return this.execute(query, qors, hints, flags);
    }

    protected native QResult execute(BSONObject query, BSONObject[] qors, BSONObject hints, int flags);

    private static class QResult {
        private int count;
        private long rsPointer;

        private QResult(int count, long rsPointer) {
            this.count = count;
            this.rsPointer = rsPointer;
        }

        public int getCount() {
            return count;
        }

        public EJDBResultSet getResultSet() {
            return new EJDBResultSet(rsPointer);
        }
    }
}
