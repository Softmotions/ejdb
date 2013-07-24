package org.ejdb.driver;

import org.ejdb.bson.BSONObject;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

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
    public static final int JBO_DEFAULT = (JBOWRITER | JBOCREAT);

    static {
        System.loadLibrary("jejdb");
    }

    private transient long dbPointer;

    private String path;
    private Map<String, EJDBCollection> collections;

    {
        collections = new HashMap<String, EJDBCollection>();
    }

    /**
     * Returns EJDB path
     * @return EJDB path
     */
    public String getPath() {
        return path;
    }

    /**
     * Open database using default open mode.
     * <p/>
     * Default open mode: JBOWRITER | JBOCREAT
     *
     * @param path EJDB path
     * @throws EJDBException
     */
    public void open(String path) throws EJDBException {
        this.open(path, JBO_DEFAULT);
    }

    /**
     * Open database.
     * <p/>
     * Default open mode: JBOWRITER | JBOCREAT
     *
     * @param path EJDB path
     * @param mode Open mode
     * @throws EJDBException
     */
    public native void open(String path, int mode) throws EJDBException;

    /**
     * Check if database in opened state.
     */
    public native boolean isOpen();

    /**
     * Close database.
     * If database was not opened it does nothing.
     *
     * @throws EJDBException
     */
    public native void close() throws EJDBException;

    /**
     * Synchronize entire EJDB database and all its collections with storage.
     *
     * @throws EJDBException
     */
    public native void sync() throws EJDBException;

    /**
     * Update description of EJDB database and its collections.
     *
     * @throws EJDBException
     */
    public native void updateMeta() throws EJDBException;

    /**
     * Automatically creates new collection if it does't exists with using default collection options
     *
     * @param cname Collection name
     * @throws EJDBException
     * @see org.ejdb.driver.EJDBCollection#ensureExists()
     */
    public EJDBCollection ensureCollection(String cname) throws EJDBException {
        return this.ensureCollection(cname, null);
    }

    /**
     * Automatically creates new collection if it does't exists.
     * Collection options `opts` are applied only for newly created collection.
     * For existing collections `opts` takes no effect.
     *
     * @param cname Collection name
     * @param opts  Collection options
     * @throws EJDBException
     * @see EJDBCollection#ensureExists(org.ejdb.driver.EJDBCollection.Options)
     */
    public EJDBCollection ensureCollection(String cname, EJDBCollection.Options opts) throws EJDBException {
        EJDBCollection collection = getCollection(cname);
        collection.ensureExists(opts);

        return collection;
    }

    /**
     * Drop collection by name.
     *
     * @param cname Collection name
     * @throws EJDBException
     * @see org.ejdb.driver.EJDBCollection#drop()
     */
    public void dropCollection(String cname) throws EJDBException {
        this.dropCollection(cname, false);
    }

    /**
     * Drop collection.
     *
     * @param cname Collection name
     * @param prune If true the collection data will erased from disk.
     * @throws EJDBException
     * @see org.ejdb.driver.EJDBCollection#drop(boolean)
     */
    public void dropCollection(String cname, boolean prune) throws EJDBException {
        getCollection(cname).drop(prune);
    }

    /**
     * Returns collection object (without automatical creation)
     *
     * @param cname Collection name
     * @return collection object
     * @throws EJDBException
     * @see EJDB#getCollection(String, boolean, org.ejdb.driver.EJDBCollection.Options)
     */
    public EJDBCollection getCollection(String cname) throws EJDBException {
        return this.getCollection(cname, false);
    }

    /**
     * Returns collection object
     *
     * @param cname   Collection name
     * @param ecreate Automatically collection creation flag
     * @return collection object
     * @throws EJDBException
     * @see EJDB#getCollection(String, boolean, org.ejdb.driver.EJDBCollection.Options)
     */
    public EJDBCollection getCollection(String cname, boolean ecreate) throws EJDBException {
        return this.getCollection(cname, ecreate, null);
    }

    /**
     * Returns collection object
     *
     * @param cname   Collection name
     * @param ecreate Automatically collection creation flag
     * @param opts    Collection options
     * @return collection object
     * @throws EJDBException
     */
    public EJDBCollection getCollection(String cname, boolean ecreate, EJDBCollection.Options opts) throws EJDBException {
        if (!this.isOpen()) {
            throw new EJDBException(0, "EJDB not opened");
        }

        EJDBCollection collection = collections.containsKey(cname) ? collections.get(cname) : new EJDBCollection(this, cname);

        if (ecreate) {
            collection.ensureExists(opts);
        }

        return collection;
    }

    /**
     * Executes ejdb database command.
     *
     * Supported commands:
     * 1) Exports database collections data. See ejdbexport() method.
     *      "export" : {
     *          "path" : string,                    //Exports database collections data
     *          "cnames" : [string array]|null,     //List of collection names to export
     *          "mode" : int|null                   //Values: null|`JBJSONEXPORT` See ejdb.h#ejdbexport() method
     *      }
     *
     *      Command response:
     *      {
     *          "log" : string,        //Diagnostic log about executing this command
     *          "error" : string|null, //ejdb error message
     *          "errorCode" : int|0,   //ejdb error code
     *      }
     *
     * 2) Imports previously exported collections data into ejdb.
     *      "import" : {
     *          "path" : string                     //The directory path in which data resides
     *          "cnames" : [string array]|null,     //List of collection names to import
     *          "mode" : int|null                   //Values: null| JBIMPORTUPDATE`|`JBIMPORTREPLACE` See ejdb.h#ejdbimport() method
     *      }
     *
     *      Command response:
     *      {
     *          "log" : string,        //Diagnostic log about executing this command
     *          "error" : string|null, //ejdb error message
     *          "errorCode" : int|0,   //ejdb error code
     *      }
     *
     * @param command Command BSON object
     * @return command response BSON object
     * @throws EJDBException
     */
    public native BSONObject executeCommand(BSONObject command) throws EJDBException;

    /**
     * Returns names of existed collections
     */
    public Collection<String> getCollectionNames() {
        return collections.keySet();
    }

    /**
     * Returns collection objects for all existed collections
     */
    public Collection<EJDBCollection> getCollections() {
        return collections.values();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String toString() {
        final StringBuilder sb = new StringBuilder();
        sb.append("EJDB");
        sb.append("{path='").append(path).append('\'');
        sb.append(", collections=").append(collections.values());
        sb.append('}');
        return sb.toString();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void finalize() throws Throwable {
        this.close();
        super.finalize();
    }
}
