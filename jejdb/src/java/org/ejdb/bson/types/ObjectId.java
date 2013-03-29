package org.ejdb.bson.types;

import java.util.Arrays;

/**
 * BSON Object ID
 *
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class ObjectId {

    private byte[] data;

    /**
     * Read ObjectId from byte array
     *
     * @throws IllegalStateException if not valid ObjectId data passed
     */
    public ObjectId(byte[] data) throws IllegalArgumentException {
        if (data == null || data.length != 12) {
            throw new IllegalArgumentException("unexpected ObjectId data");
        }

        this.data = new byte[12];
        System.arraycopy(data, 0, this.data, 0, 12);
    }

    /**
     * Read ObjectId from string
     *
     * @throws IllegalStateException if not valid ObjectId data passed
     */
    public ObjectId(String value) {
        if (!isValid(value)) {
            throw new IllegalArgumentException("unexpected ObjectId data");
        }

        this.data = new byte[12];
        for (int i = 0; i < 12; ++i) {
            this.data[i] = Byte.parseByte(value.substring(i << 1, i << 1 + 2), 16);
        }
    }

    /**
     * Export ObjectId to plain byte array
     */
    public byte[] toByteArray() {
        byte[] result = new byte[12];
        System.arraycopy(data, 0, result, 0, 12);
        return result;
    }

    @Override
    public boolean equals(Object o) {
        return this == o || null != o && o instanceof ObjectId && Arrays.equals(data, ((ObjectId) o).data);

    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(data);
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder(34);
        builder.append("ObjectId(");
        for (byte b : data) {
            builder.append(String.format("%02x", b));
        }
        builder.append(")");

        return builder.toString();
    }

    /**
     * Checks string on valid ObjectId data
     */
    public static boolean isValid(String value) {
        if (value == null || value.length() != 24) {
            return false;
        }

        value = value.toUpperCase();
        for (int i = 0; i < value.length(); ++i) {
            char c = value.charAt(i);
            if ((c < '0' || c > '9') && (c < 'A' || c > 'F')) {
                return false;
            }
        }

        return true;
    }
}
