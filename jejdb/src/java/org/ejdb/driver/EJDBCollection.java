package org.ejdb.driver;

import org.bson.BSON;
import org.bson.BSONObject;
import org.bson.types.ObjectId;

import java.nio.ByteBuffer;
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

    private EJDB db;
    private String cname;

    EJDBCollection(EJDB db, String cname) {
        this.db = db;
        this.cname = cname;
    }

    protected native void ensureDB(Options opts);
    protected native void dropDB(boolean prune);
    protected native void syncDB();

    protected native BSONObject loadDB(ObjectId oid);
    protected native ObjectId saveDB(BSONObject obj);
    protected native void removeDB(ObjectId oid);

    protected native void setIndexDB(String path, int flags);
    protected native void txControlDB(int mode);

    public void ensureExists() {
        this.ensureExists(null);
    }

    public void ensureExists(Options opts) {
        this.ensureDB(opts);
    }

    public void drop() {
        this.drop(false);
    }

    public void drop(boolean prune) {
        this.dropDB(prune);
    }

    public void sync() {
        this.syncDB();
    }

    public BSONObject load(ObjectId oid) {
        return this.loadDB(oid);
    }

    public ObjectId save(BSONObject object) {
        return this.saveDB(object);
    }

    public List<ObjectId> save(List<BSONObject> objects) {
        List<ObjectId> result = new ArrayList<ObjectId>(objects.size());

        for (BSONObject object : objects) {
            result.add(this.save(object));
        }

        return result;
    }

    public void remove(ObjectId oid) {
        this.removeDB(oid);
    }

    public void setIndex(String path, int flags) {
        this.setIndexDB(path, flags);
    }

    public EJDBQuery createQuery(BSONObject query, BSONObject[] qors, BSONObject hints, int flags) {
        return new EJDBQuery(this, query, qors, hints, flags);
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
