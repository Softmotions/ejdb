package org.ejdb.driver;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDB {

    /**
     * Open as a reader.
     */
    public static final int JBOREADER = 1 << 0;

    /**
     * Open as a writer.
     */
    public static final int JBOWRITER = 1 << 1;

    /**
     * Create if db file not exists.
     */
    public static final int JBOCREAT = 1 << 2;

    /**
     * Truncate db on open.
     */
    public static final int JBOTRUNC = 1 << 3;

    /**
     * Open without locking.
     */
    public static final int JBONOLCK = 1 << 4;

    /**
     * Lock without blocking.
     */
    public static final int JBOLCKNB = 1 << 5;

    /**
     * Synchronize every transaction.
     */
    public static final int JBOTSYNC = 1 << 6;

    /**
     * Default open mode
     */
    public static final int JBO_DEFAULT = (JBOWRITER | JBOCREAT | JBOTSYNC);

    static {
        System.loadLibrary("jejdb");
    }

    private long dbPointer;

    private String path;

    public String getPath() {
        return path;
    }

    public void open(String path) {
        this.open(path, JBO_DEFAULT);
    }

    public native void open(String path, int mode);

    public native boolean isOpen();

    public native void close();

    public native void sync();

    public EJDBCollection ensureCollection(String cname) {
        return this.ensureCollection(cname, null);
    }

    public EJDBCollection ensureCollection(String cname, EJDBCollection.Options opts) {
        EJDBCollection collection = getCollection(cname);
        collection.ensureExists(opts);

        return collection;
    }

    public void dropCollection(String cname) {
        this.dropCollection(cname, false);
    }

    public void dropCollection(String cname, boolean prune) {
        getCollection(cname).drop(prune);
    }

    public EJDBCollection getCollection(String cname) {
        return this.getCollection(cname, false);
    }

    public EJDBCollection getCollection(String cname, boolean ecreate) {
        return this.getCollection(cname, ecreate, null);
    }

    public EJDBCollection getCollection(String cname, boolean ecreate, EJDBCollection.Options opts) {
        EJDBCollection collection = new EJDBCollection(this, cname);

        if (ecreate) {
            collection.ensureExists(opts);
        }

        return collection;
    }

    //////////////////////////////////////////
    @Override
    protected void finalize() throws Throwable {
        this.close();
        super.finalize();
    }

}
