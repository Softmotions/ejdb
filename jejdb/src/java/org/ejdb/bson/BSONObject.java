package org.ejdb.bson;

import org.ejdb.bson.types.ObjectId;

import java.lang.reflect.Array;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class BSONObject {
    public static final String ID_KEY = "_id";

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

    public ObjectId getId() {
        return (ObjectId) get(ID_KEY);
    }

    public int size() {
        return data.size();
    }

    public boolean containsField(String key) {
        return data.containsKey(key);
    }

    @Override
    public boolean equals(Object o) {
        if (this != o && (null == o || !(o instanceof BSONObject))) {
            return false;
        }

        Map<String, Object> thatData = ((BSONObject) o).data;

        if (thatData.size() != data.size()) {
            return false;
        }

        try {
            Iterator<Map.Entry<String, Object>> i = data.entrySet().iterator();
            while (i.hasNext()) {
                Map.Entry<String, Object> e = i.next();
                String key = e.getKey();
                Object value = e.getValue();
                if (value == null) {
                    if (!(thatData.get(key) == null && thatData.containsKey(key))) {
                        return false;
                    }
                } else {
                    if (!equalObjects(value, thatData.get(key))) {
                        return false;
                    }
                }
            }
        } catch (ClassCastException unused) {
            return false;
        } catch (NullPointerException unused) {
            return false;
        }

        return true;
    }

    private boolean equalObjects(Object o1, Object o2) {
        if (o1.getClass().isArray()) {
            int len = Array.getLength(o1);
            if (len != Array.getLength(o2)) {
                return false;
            }

            for (int i = 0; i < len; ++i) {
                Object item1 = Array.get(o1, i);
                Object item2 = Array.get(o2, i);

                boolean isEquals = item1 == null ? item2 == null : equalObjects(item1, item2);
                if (!isEquals) {
                    return false;
                }
            }

            return true;
        } else {
            return !o1.equals(o2) ? false : true;
        }
    }

    @Override
    public int hashCode() {
        return data.hashCode();
    }

    @Override
    public String toString() {
        return data.toString();
    }
}
