package org.ejdb.driver;

import org.bson.BSONObject;
import org.bson.types.ObjectId;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBCollection {

    /**
     * Drop index.
     */
    public final static int JBIDXDROP = 1 << 0;
    /**
     * Drop index for all types.
     */
    public final static int JBIDXDROPALL = 1 << 1;
    /**
     * Optimize index.
     */
    public final static int JBIDXOP = 1 << 2;
    /**
     * Rebuild index.
     */
    public final static int JBIDXREBLD = 1 << 3;
    /**
     * Number index.
     */
    public final static int JBIDXNUM = 1 << 4;
    /**
     * String index.
     */
    public final static int JBIDXSTR = 1 << 5;
    /**
     * Array token index.
     */
    public final static int JBIDXARR = 1 << 6;
    /**
     * Case insensitive string index
     */
    public final static int JBIDXISTR = 1 << 7;

    // transaction control options (inner use only)
    protected final static int JBTXBEGIN = 1 << 0;
    protected final static int JBTXCOMMIT = 1 << 1;
    protected final static int JBTXROLLBACK = 1 << 2;
    protected final static int JBTXSTATUS = 1 << 3;

    private EJDB db;
    private String cname;
    private boolean exists;
    private Options options;
    private Collection<Index> indexes;

    EJDBCollection(EJDB db, String cname) {
        this.db = db;
        this.cname = cname;
    }

    public EJDB getDB() {
        return db;
    }

    public String getName() {
        return cname;
    }

    public boolean isExists() {
        return exists;
    }

    public Options getOptions() {
        return options;
    }

    public Collection<Index> getIndexes() {
        return indexes;
    }

    public void ensureExists() {
        this.ensureExists(null);
    }

    public native void ensureExists(Options opts);

    public void drop() {
        this.drop(false);
    }

    public native void drop(boolean prune);

    public native void sync();

    public native void updateMeta();

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

    protected native boolean txControl(int mode);


    @Override
    public String toString() {
        final StringBuilder sb = new StringBuilder();
        sb.append("EJDBCollection");
        sb.append("{cname='").append(cname).append('\'');
        sb.append(", options=").append(options);
        sb.append(", indexes=").append(indexes);
        sb.append('}');
        return sb.toString();
    }

    public static class Options {
        private long buckets;
        private boolean compressed;
        private boolean large;
        private long records;
        private int cachedRecords;

        public Options() {
        }

        public Options(boolean compressed, boolean large, long records, int cachedRecords) {
            this.compressed = compressed;
            this.large = large;
            this.records = records;
            this.cachedRecords = cachedRecords;
        }

        public long getBuckets() {
            return buckets;
        }

        public boolean isCompressed() {
            return compressed;
        }

        public boolean isLarge() {
            return large;
        }

        public int getCachedRecords() {
            return cachedRecords;
        }

        @Override
        public String toString() {
            final StringBuilder sb = new StringBuilder();
            sb.append("Options");
            sb.append("{buckets=").append(buckets);
            sb.append(", compressed=").append(compressed);
            sb.append(", large=").append(large);
            sb.append(", cachedRecords=").append(cachedRecords);
            sb.append('}');
            return sb.toString();
        }
    }

    public static enum IndexType {
        Lexical,
        Numeric,
        Token;
    }

    public static class Index {
        private String name;
        private String field;
        private IndexType type;
        private String file;
        private int records;

        public String getName() {
            return name;
        }

        public String getField() {
            return field;
        }

        public IndexType getType() {
            return type;
        }

        public String getFile() {
            return file;
        }

        public int getRecords() {
            return records;
        }

        @Override
        public String toString() {
            final StringBuilder sb = new StringBuilder();
            sb.append("Index");
            sb.append("{name='").append(name).append('\'');
            sb.append(", field='").append(field).append('\'');
            sb.append(", type=").append(type);
            if (file != null) {
                sb.append(", file='").append(file).append('\'');
            }
            sb.append(", records=").append(records);
            sb.append('}');
            return sb.toString();
        }
    }
}
