package org.ejdb.driver;

import org.bson.BSON;
import org.bson.BSONObject;
import org.bson.types.ObjectId;

import java.awt.image.DataBufferByte;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.util.Arrays;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBCollection {

    private long dbp;
    private String name;

    EJDBCollection(long dbp, String name) {
        this.dbp = dbp;
        this.name = name;
    }

    // todo: bson object for options
    protected native boolean ensureCollectionDB(long dbp, String cname, Object opts);

    protected native boolean dropCollectionDB(long dbp, String cname, boolean prune);

    protected native Object loadDB(long dbp, String cname, byte[] oid);
    protected native Object saveDB(long dbp, String cname, byte[] objdata);

    public boolean ensureExists() {
        return this.ensureExists(null);
    }

    public boolean ensureExists(Object opts) {
        return this.ensureCollectionDB(dbp, name, opts);
    }

    public boolean drop() {
        return this.drop(false);
    }

    public boolean drop(boolean prune) {
        return this.dropCollectionDB(dbp, name, prune);
    }


    public BSONObject load(ObjectId oid) {
        return (BSONObject) this.loadDB(dbp, name, oid.toByteArray());
//        byte[] data = this.loadDB(dbp, name, oid.toByteArray());
//        if (data.length > 0) {
//            return BSON.decode(data);
//        } else {
//            return null;
//        }
    }

    public ObjectId save(BSONObject object) {
        return (ObjectId) this.saveDB(dbp, name, BSON.encode(object));
    }

    private static Object handleBSONData(ByteBuffer data) {
        byte[] tmp = new byte[data.limit()];
        data.get(tmp);

//        System.out.println(data.getClass().getName());
//        System.out.println(((MappedByteBuffer)data).load().array());
        //        System.out.println(Arrays.toString(data.array()));
        return BSON.decode(tmp);

//        return null;
    }

    private static Object handleObjectIdData(ByteBuffer data) {
        byte[] tmp = new byte[data.limit()];
        data.get(tmp);

        return new ObjectId(tmp);
    }
}
