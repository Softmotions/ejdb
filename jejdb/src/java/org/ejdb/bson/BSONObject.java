package org.ejdb.bson;

import org.ejdb.bson.types.ObjectId;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * BSON object.
 * <p/>
 * NOTE:
 *   {@link BSONObject#ID_KEY} must be valid {@link ObjectId} ({@link ObjectId} instance or valid <code>byte[]</code> or <code>String</code>)
 *
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class BSONObject {
    /**
     * ID-field name
     */
    public static final String ID_KEY = "_id";

    protected Map<String, Object> data;
    protected List<String> fields;

    {
        data = new HashMap<String, Object>();
        fields = new ArrayList<String>();
    }

    /**
     * Constructs new BSON object
     */
    public BSONObject() {
    }

    /**
     * Constructs new BSON object with specified id
     */
    public BSONObject(ObjectId oid) {
        this(ID_KEY, oid);
    }

    /**
     * Constructs new BSON object with initial data.
     * The same as:
     * <code>
     * BSONObject obj = new BSONObject();
     * obj.put(key, value);
     * </code>
     */
    public BSONObject(String key, Object value) {
        this.put(key, value);
    }

    /**
     * Constructs new BSON object and init data from specified Map.
     * The same as
     * <code>
     * BSONObject obj = new BSONObject();
     * obj.putAll(data);
     * </code>
     */
    public BSONObject(Map<String, Object> data) {
        this.putAll(data);
    }

    /**
     * Constructs new BSON object as copy of other BSON object.
     */
    public BSONObject(BSONObject src) {
        if (src != null) {
            this.putAll(src);
        }
    }

    protected Object registerField(String key, Object value) {
        if (!data.containsKey(key)) {
            fields.add(key);
        }

        data.put(key, value);
        return value;
    }

    /**
     * Add new key->value to BSON object.
     *
     * @return added value
     * @throws IllegalArgumentException if not valid ObjectId data passed as _id ({@link BSONObject#ID_KEY} field.
     */
    public Object put(String key, Object value) throws IllegalArgumentException {
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

        return registerField(key, value);
    }

    /**
     * The same as {@link BSONObject#put(String, Object)} but return <code>this</code>
     */
    public BSONObject append(String key, Object value) {
        this.put(key, value);

        return this;
    }

    /**
     * Adds key->value pair to BSON object from specified Map
     */
    public void putAll(Map<String, Object> values) {
        for (Map.Entry<String, Object> entry : values.entrySet()) {
            put(entry.getKey(), entry.getValue());
        }
    }

    /**
     * Adds key->value pair to BSON object from other BSON object
     */
    public void putAll(BSONObject object) {
        for (String field : object.fields) {
            this.put(field, object.get(field));
        }
    }

    /**
     * Returns fields in adding order
     * @return fields in adding order
     */
    public List<String> fields() {
        return Collections.unmodifiableList(fields);
    }

    /**
     * Returns id of BSON object (if specified)
     * @return id of BSON object (if specified)
     */
    public ObjectId getId() {
        return (ObjectId) get(ID_KEY);
    }

    /**
     * Returns value of specified field if exists, or <code>null</code> otherwise
     * @return value of specified field if exists, or <code>null</code> otherwise
     */
    public Object get(String key) {
        return data.get(key);
    }

    /**
     * Returns fields count
     * @return fields count
     */
    public int size() {
        return data.size();
    }

    /**
     * Checks field contains in BSON object
     */
    public boolean containsField(String key) {
        return data.containsKey(key);
    }

    /**
     * Removes field from Object
     */
    public void remove(String field) {
        if (data.containsKey(field)) {
            fields.remove(field);
            data.remove(field);
        }
    }

    /**
     * Removes all fields
     */
    public void clear() {
        fields.clear();
        data.clear();
    }

    /**
     * If returns <code>true</code> fields order will be checks on equal.
     */
    protected boolean isFieldsOrderImportant() {
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean equals(Object o) {
        if (this != o && (null == o || !(o instanceof BSONObject))) {
            return false;
        }

        BSONObject other = (BSONObject) o;
        Map<String, Object> thatData = other.data;

        if (thatData.size() != data.size()) {
            return false;
        }

        if ((isFieldsOrderImportant() || other.isFieldsOrderImportant()) && !fields.equals(other.fields())) {
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

    /**
     * {@inheritDoc}
     */
    @Override
    public int hashCode() {
        return data.hashCode();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("{");
        String delimiter = "";
        for (String field : fields) {
            sb.append(delimiter);
            sb.append(field).append(":").append(get(field));
            delimiter = ", ";
        }
        sb.append("}");
        return sb.toString();
    }
}
