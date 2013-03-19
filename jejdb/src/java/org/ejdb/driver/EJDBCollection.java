package org.ejdb.driver;

import org.bson.BSONObject;
import org.bson.types.ObjectId;

import java.util.ArrayList;
import java.util.List;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBCollection {

    // Index modes, index types.
    public final static int JBIDXDROP = 1 << 0; /**< Drop index. */
    public final static int JBIDXDROPALL = 1 << 1; /**< Drop index for all types. */
    public final static int JBIDXOP = 1 << 2; /**< Optimize index. */
    public final static int JBIDXREBLD = 1 << 3; /**< Rebuild index. */
    public final static int JBIDXNUM = 1 << 4; /**< Number index. */
    public final static int JBIDXSTR = 1 << 5; /**< String index.*/
    public final static int JBIDXARR = 1 << 6; /**< Array token index. */
    public final static int JBIDXISTR = 1 << 7; /**< Case insensitive string index */

    protected final static int JBTXBEGIN = 1 << 0;
    protected final static int JBTXCOMMIT = 1 << 1;
    protected final static int JBTXROLLBACK = 1 << 2;
    protected final static int JBTXSTATUS = 1 << 3;

    private EJDB db;
    private String cname;

    EJDBCollection(EJDB db, String cname) {
        this.db = db;
        this.cname = cname;
    }

    protected native boolean txControl(int mode);

    public void ensureExists() {
        this.ensureExists(null);
    }

    public native void ensureExists(Options opts);

    public void drop() {
        this.drop(false);
    }

    public native void drop(boolean prune);

    public native void sync();

    public native BSONObject load(ObjectId oid);

    public native ObjectId save(BSONObject object);

    public List<ObjectId> save(List<BSONObject> objects) {
        List<ObjectId> result = new ArrayList<ObjectId>(objects.size());

        for (BSONObject object : objects) {
            result.add(this.save(object));
        }

        return result;
    }

    public native void remove(ObjectId oid);

    public native void setIndex(String path, int flags);

    public EJDBQuery createQuery(BSONObject query) {
        return new EJDBQuery(this, query, null, null);
    }

    public EJDBQuery createQuery(BSONObject query, BSONObject[] qors) {
        return new EJDBQuery(this, query, qors, null);
    }

    public EJDBQuery createQuery(BSONObject query, BSONObject hints) {
        return new EJDBQuery(this, query, null, hints);
    }

    public EJDBQuery createQuery(BSONObject query, BSONObject[] qors, BSONObject hints) {
        return new EJDBQuery(this, query, qors, hints);
    }

    public void beginTransaction() {
        this.txControl(JBTXBEGIN);
    }

    public void commitTransaction() {
        this.txControl(JBTXCOMMIT);
    }

    public void rollbakcTransaction() {
        this.txControl(JBTXROLLBACK);
    }

    public boolean isTransactionActive() {
        return this.txControl(JBTXSTATUS);
    }

    public static class Options {
        private boolean compressed;
        private boolean large;
        private long records;
        private int cachedrecords;

        public Options() {
        }

        public Options(boolean compressed, boolean large, long records, int cachedrecords) {
            this.compressed = compressed;
            this.large = large;
            this.records = records;
            this.cachedrecords = cachedrecords;
        }

        public boolean isCompressed() {
            return compressed;
        }

        public void setCompressed(boolean compressed) {
            this.compressed = compressed;
        }

        public boolean isLarge() {
            return large;
        }

        public void setLarge(boolean large) {
            this.large = large;
        }

        public long getRecords() {
            return records;
        }

        public void setRecords(long records) {
            this.records = records;
        }

        public int getCachedrecords() {
            return cachedrecords;
        }

        public void setCachedrecords(int cachedrecords) {
            this.cachedrecords = cachedrecords;
        }
    }
}
