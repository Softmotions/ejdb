package org.ejdb.bson;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class BSONDecoder {

    public BSONObject decode(byte[] data) {
        return new BSONObject();
    }

    public int readInt(byte[] data) {
        return readInt(data, 0);
    }

    public int readInt(byte[] data, int offset) {
        int result = 0;
        for (int i = 0; i < 4; ++i) {
            result |= (0xFF & data[offset + i]) << (i * 8);
        }

        return result;
    }

    public long readLong(byte[] data) {
        return readLong(data, 0);
    }

    public long readLong(byte[] data, int offset) {
        long result = 0;
        for (int i = 0; i < 8; ++i) {
            result |= (0xFFL & data[offset + i]) << (i * 8);
        }

        return result;
    }
}
