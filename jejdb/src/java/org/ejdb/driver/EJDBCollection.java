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

    private long dbPointer;
    private String cname;

    EJDBCollection(long dbPointer, String cname) {
        this.dbPointer = dbPointer;
        this.cname = cname;
    }

    // todo: bson object for options
    protected native boolean ensureDB(Object opts);
    protected native boolean dropDB(boolean prune);
    protected native boolean syncDB();

    protected native Object loadDB(byte[] oid);
    protected native Object saveDB(byte[] objdata);
    protected native boolean removeDB(byte[] objdata);

    public boolean ensureExists() {
        return this.ensureExists(null);
    }

    public boolean ensureExists(Object opts) {
        return this.ensureDB(opts);
    }

    public boolean drop() {
        return this.drop(false);
    }

    public boolean drop(boolean prune) {
        return this.dropDB(prune);
    }

    public boolean sync() {
        return this.syncDB();
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

    public boolean remove(ObjectId oid) {
        return this.removeDB(oid.toByteArray());
    }

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
