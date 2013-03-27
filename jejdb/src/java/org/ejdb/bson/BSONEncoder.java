package org.ejdb.bson;

import org.ejdb.bson.io.OutputBuffer;
import org.ejdb.bson.types.ObjectId;
import org.ejdb.bson.util.RegexFlag;

import java.lang.reflect.Array;
import java.util.Date;
import java.util.Iterator;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.regex.Pattern;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
class BSONEncoder {

    private OutputBuffer output;

    public byte[] encode(BSONObject object) throws IllegalStateException {
        if (isBusy()) {
            throw new IllegalStateException("other encoding in process");
        }

        if (object == null) {
            throw new IllegalArgumentException("can not serialize null document object");
        }

        output = new OutputBuffer();
        this.handleObject(object);
        byte[] result = output.getResult();
        output = null;

        return result;
    }

    public boolean isBusy() {
        return output != null;
    }

    protected void handleObject(BSONObject object) {
        int start = output.getPosition();

        output.writeInt(0);

        for (String field : object.keySet()) {
            writeField(field, object.get(field));
        }

        output.write((byte) 0x00);

        int end = output.getPosition();

        output.writeIntAt(start, end - start);
    }

    protected void writeFieldSpec(byte type, String name) {
        output.write(type);
        output.writeString(name);
    }

    protected void writeField(String name, Object value) {
        if (null == value) {
            writeNull(name);
        } else if (value instanceof Number) {
            writeNumber(name, (Number) value);
        } else if (value instanceof String) {
            writeString(name, (String) value);
        } else if (value instanceof Character) {
            writeString(name, value.toString());
        } else if (value instanceof Boolean) {
            writeBoolean(name, (Boolean) value);
        } else if (value instanceof ObjectId) {
            writeObjectId(name, (ObjectId) value);
        } else if (value instanceof BSONObject) {
            writeObject(name, (BSONObject) value);
        } else if (value instanceof Map) {
            writeObject(name, new BSONObject((Map) value));
        } else if (value instanceof byte[]) {
            writeBinary(name, (byte[]) value);
        } else if (value instanceof Iterable) {
            writeArray(name, ((Iterable) value).iterator());
        } else if (value.getClass().isArray()) {
            writeArray(name, wrapArrayIterator(value));
        } else if (value instanceof Date) {
            writeDate(name, (Date) value);
        } else if (value instanceof Pattern) {
            writeRegex(name, (Pattern) value);
        } else {
            throw new IllegalArgumentException("can not serialize object: " + value.getClass().getName());
        }
    }

    protected void writeNull(String name) {
        writeFieldSpec(BSON.NULL, name);
    }

    protected void writeNumber(String name, Number value) {
        if (value instanceof Integer || value instanceof Short || value instanceof Byte || value instanceof AtomicInteger) {
            writeFieldSpec(BSON.INT, name);
            output.writeInt(value.intValue());
        } else if (value instanceof Long || value instanceof AtomicLong) {
            writeFieldSpec(BSON.LONG, name);
            output.writeLong(value.longValue());
        } else if (value instanceof Double || value instanceof Float) {
            writeFieldSpec(BSON.DOUBLE, name);
            output.writeDouble(value.doubleValue());
        } else {
            throw new IllegalArgumentException("can not serialize object: " + value.getClass().getName());
        }
    }

    protected void writeString(String name, String value) {
        writeFieldSpec(BSON.STRING, name);

        int sp = output.getPosition();
        output.writeInt(0);
        int length = output.writeString(value);
        output.writeIntAt(sp, length);
    }

    protected void writeBoolean(String name, Boolean value) {
        writeFieldSpec(BSON.BOOLEAN, name);
        output.write((byte) (value ? 0x01 : 0x00));
    }

    protected void writeObjectId(String name, ObjectId value) {
        writeFieldSpec(BSON.OBJECT_ID, name);
        output.write(value.toByteArray());
    }

    protected void writeObject(String name, BSONObject value) {
        writeFieldSpec(BSON.OBJECT, name);
        handleObject(value);
    }

    protected void writeBinary(String name, byte[] value) {
        writeFieldSpec(BSON.BINARY, name);
        output.writeInt(value.length);
        output.write((byte) 0x00);
        output.write(value);
    }

    protected void writeArray(String name, Iterator<Object> value) {
        writeFieldSpec(BSON.ARRAY, name);

        int sp = output.getPosition();
        output.writeInt(0);
        int i = 0;
        while (value.hasNext()) {
            writeField(String.valueOf(i), value.next());
            ++i;
        }
        output.write((byte) 0x00);
        output.writeIntAt(sp, output.getPosition() - sp);
    }

    protected void writeDate(String name, Date value) {
        writeFieldSpec(BSON.DATE, name);
        output.writeLong(value.getTime());
    }

    protected void writeRegex(String name, Pattern value) {
        writeFieldSpec(BSON.REGEX, name);
        output.writeString(value.pattern());
        output.writeString(RegexFlag.regexFlagsToString(value.flags()));
    }

    protected Iterator<Object> wrapArrayIterator(final Object array) {
        final int size = Array.getLength(array);
        return new Iterator<Object>() {
            private int position;

            public boolean hasNext() {
                return position < size;
            }

            public Object next() {
                if (!hasNext()) {
                    throw new NoSuchElementException();
                }

                return Array.get(array, position++);
            }

            public void remove() {
                throw new UnsupportedOperationException();
            }
        };
    }
}
