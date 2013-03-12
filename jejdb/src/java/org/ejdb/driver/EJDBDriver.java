package org.ejdb.driver;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBDriver {
//
//    // Open modes
//    public static final int JBOREADER = 1 << 0; /**< Open as a reader. */
//    public static final int JBOWRITER = 1 << 1; /**< Open as a writer. */
//    public static final int JBOCREAT = 1 << 2; /**< Create if db file not exists. */
//    public static final int JBOTRUNC = 1 << 3; /**< Truncate db on open. */
//    public static final int JBONOLCK = 1 << 4; /**< Open without locking. */
//    public static final int JBOLCKNB = 1 << 5; /**< Lock without blocking. */
//    public static final int JBOTSYNC = 1 << 6; /**< Synchronize every transaction. */
//
//    public static final int JBO_DEFAULT = (JBOWRITER | JBOCREAT | JBOTSYNC);
//
//
//    static {
//        System.loadLibrary("jejdb");
//    }
//
//    private native long openDB(String path, int mode);
//    private native void closeDB(long dbp);
//
//    private Long dbp;
//
//    public void open(String path) {
//        this.open(path, JBO_DEFAULT);
//    }
//
//    public void open(String path, int mode) {
//        dbp = this.openDB(path, mode);
//    }
//
//    public void close() {
//        if (dbp != null) {
//            this.closeDB(dbp);
//        }
//        dbp = null;
//    }
//
//    @Override
//    protected void finalize() throws Throwable {
//        this.close();
//        super.finalize();
//    }
}
