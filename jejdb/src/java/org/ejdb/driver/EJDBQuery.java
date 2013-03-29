package org.ejdb.driver;

import org.ejdb.bson.BSONObject;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * EJDB Query object.
 *
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBQuery {

    /* Query only count(*) */
    protected static final int JBQRYCOUNT = 1;

    private EJDBCollection coll;
    private EJDBQueryBuilder query;
    private int flags;


    EJDBQuery(EJDBCollection coll, EJDBQueryBuilder query) {
        this.coll = coll;
        this.query = query;
    }

    /**
     * Execute query
     */
    public EJDBResultSet find() throws EJDBException {
        try {
            return this.find(null);
        } catch (IOException ignored) {
            // unpossible (log is null)
        }

        return null;
    }
    /**
     * Execute query
     */
    public EJDBResultSet find(OutputStream log) throws EJDBException, IOException {
        return this.execute(query.getQueryHints(), 0, log).getResultSet();
    }

    /**
     * Same as  {@link org.ejdb.driver.EJDBQuery#find()} but retrieves only one matching JSON object.
     */
    public BSONObject findOne() throws EJDBException {
        try {
            return this.findOne(null);
        } catch (IOException ignored) {
            // unpossible (log is null)
        }

        return null;
    }
    /**
     * Same as  {@link org.ejdb.driver.EJDBQuery#find()} but retrieves only one matching JSON object.
     */
    public BSONObject findOne(OutputStream log) throws EJDBException, IOException {
        BSONQueryObject hints = new BSONQueryObject(query.getQueryHints()).append("$max", 1);

        EJDBResultSet rs = this.execute(hints, 0, null).getResultSet();
        BSONObject result = rs.hasNext() ? rs.next() : null;
        rs.close();

        return result;
    }

    /**
     * Executes update query
     *
     * @return count of affected objects
     */
    public int update() throws EJDBException {
        try {
            return this.update(null);
        } catch (IOException ignored) {
            // unpossible (log is null)
        }

        return 0;
    }
    /**
     * Executes update query
     *
     * @return count of affected objects
     */
    public int update(OutputStream log) throws EJDBException, IOException {
        return this.execute(query.getQueryHints(), JBQRYCOUNT, log).getCount();
    }

    /**
     * Convenient count(*) operation
     */
    public int count() throws EJDBException {
        try {
            return this.count(null);
        } catch (IOException ignored) {
            // unpossible (log is null)
        }

        return 0;
    }

    /**
     * Convenient count(*) operation
     */
    public int count(OutputStream log) throws EJDBException, IOException {
        return this.execute(query.getQueryHints(), JBQRYCOUNT, log).getCount();
    }

    protected QResult execute(BSONObject hints, int flags, OutputStream log) throws EJDBException, IOException {
        return this.execute(query.getMainQuery(), query.getOrQueries(), hints, flags, log);
    }

    protected native QResult execute(BSONObject query, BSONObject[] qors, BSONObject hints, int flags, OutputStream log) throws EJDBException, IOException;

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
