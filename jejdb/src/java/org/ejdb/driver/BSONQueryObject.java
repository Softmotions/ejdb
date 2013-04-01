package org.ejdb.driver;

import org.ejdb.bson.BSONObject;
import org.ejdb.bson.types.ObjectId;

import java.util.Map;

/**
 * BSON object for EJDB queries (limitation checks for {@link BSONObject#ID_KEY} field)
 *
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class BSONQueryObject extends BSONObject {

    public BSONQueryObject() {
        super();
    }

    public BSONQueryObject(String key, Object value) {
        super(key, value);
    }

    public BSONQueryObject(Map<String, Object> data) {
        super(data);
    }

    public BSONQueryObject(BSONObject src) {
        super(src);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Object put(String key, Object value) {
        return registerField(key, value);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public BSONQueryObject append(String key, Object value) {
        super.append(key, value);
        return this;
    }

    /**
     * {@inheritDoc}
     *
     * @deprecated BSON Query objects can not contains dedicated ObjectID
     */
    @Deprecated
    @Override
    public ObjectId getId() {
        return null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected boolean isFieldsOrderImportant() {
        return true;
    }
}
