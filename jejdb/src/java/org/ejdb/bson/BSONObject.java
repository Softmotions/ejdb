package org.ejdb.bson;

import org.ejdb.bson.types.ObjectId;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class BSONObject {
    private static final String ID_KEY = "_id";

    private Map<String, Object> data;

    {
        data = new HashMap<String, Object>();
    }

    public BSONObject() {
    }

    public BSONObject(ObjectId oid) {
        this(ID_KEY, oid);
    }

    public BSONObject(String key, Object value) {
        this.put(key, value);
    }

    public BSONObject(Map<String, Object> data) {
        this.putAll(data);
    }

    public Object put(String key, Object value) {
        if (ID_KEY.equals(key)) {
            if (value instanceof ObjectId) {
                // noop
            } else if (value instanceof byte[]) {
                value = new ObjectId((byte[]) value);
            } else if (value instanceof String) {
                value = new ObjectId((String) value);
            } else {
                throw new IllegalArgumentException("expected ObjectId");
            }
        }

        return data.put(key, value);
    }

    public BSONObject append(String key, Object value) {
        this.put(key, value);

        return this;
    }

    public void putAll(Map<String, Object> values) {
        for (Map.Entry<String, Object> entry : values.entrySet()) {
            put(entry.getKey(), entry.getValue());
        }
    }

    public void putAll(BSONObject object) {
        this.putAll(object.asMap());
    }

    public Map<String, Object> asMap() {
        return new HashMap<String, Object>(data);
    }

    public Set<String> keySet() {
        return new HashSet<String>(data.keySet());
    }

    public Object get(String key) {
        return data.get(key);
    }

    public boolean containsField(String key) {
        return data.containsKey(key);
    }

    @Override
    public String toString() {
        return data.toString();
    }
}
