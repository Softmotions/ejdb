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

    // todo: get dbPointer from db object
    private EJDB db;
    private String cname;

    EJDBCollection(EJDB db, String cname) {
        this.db = db;
        this.cname = cname;
    }

    // todo: bson object for options
    protected native void ensureDB(Object opts);
    protected native void dropDB(boolean prune);
    protected native void syncDB();

    protected native Object loadDB(byte[] oid);
    protected native Object saveDB(byte[] objdata);
    protected native void removeDB(byte[] objdata);

    protected native void setIndexDB(String path, int flags);

    public void ensureExists() {
        this.ensureExists(null);
    }

    public void ensureExists(Object opts) {
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
        return (BSONObject) this.loadDB(oid.toByteArray());
    }

    public ObjectId save(BSONObject object) {
        return (ObjectId) this.saveDB(BSON.encode(object));
    }

    public List<ObjectId> save(List<BSONObject> objects) {
        List<ObjectId> result = new ArrayList<ObjectId>(objects.size());

        for (BSONObject object : objects) {
            result.add(this.save(object));
        }

        return result;
    }

    public void remove(ObjectId oid) {
        this.removeDB(oid.toByteArray());
    }

    public void setIndex(String path, int flags) {
        this.setIndexDB(path, flags);
    }


    public EJDBQuery createQuery(BSONObject query, BSONObject[] qors, BSONObject hints, int flags) {
        return new EJDBQuery(this, query, qors, hints, flags);
    }

    ////////////////////////////////////////////////////
    private static Object handleBSONData(ByteBuffer data) {
        byte[] tmp = new byte[data.limit()];
        data.get(tmp);

        return BSON.decode(tmp);
    }

    private static Object handleObjectIdData(ByteBuffer data) {
        byte[] tmp = new byte[data.limit()];
        data.get(tmp);

        return new ObjectId(tmp);
    }
}
