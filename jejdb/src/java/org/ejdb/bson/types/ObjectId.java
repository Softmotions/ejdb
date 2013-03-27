package org.ejdb.bson.types;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class ObjectId {

    private byte[] data;

    public ObjectId(byte[] data) {
        if (data == null || data.length != 12) {
            throw new IllegalArgumentException("unexpected ObjectId data");
        }

        this.data = new byte[12];
        System.arraycopy(data, 0, this.data, 0, 12);
    }

    public ObjectId(String value) {
        if (!isValid(value)) {
            throw new IllegalArgumentException("unexpected ObjectId data");
        }

        this.data = new byte[12];
        for (int i = 0; i < 12; ++i) {
            this.data[i] = Byte.parseByte(value.substring(i << 1, i << 1 + 2), 16);
        }
    }

    public byte[] toByteArray() {
        byte[] result = new byte[12];
        System.arraycopy(data, 0, result, 0, 12);
        return result;
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
