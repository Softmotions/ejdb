package org.ejdb.bson.io;

import org.ejdb.bson.BSONException;
import org.ejdb.bson.BSONObject;

import java.io.UnsupportedEncodingException;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class InputBuffer {
    private byte[] data;
    private int offset;
    private int position;
    private int limit;

    private InputBuffer(byte[] data, int offset, int limit) {
        this.data = data;
        this.position = 0;
        this.offset = offset;
        this.limit = limit;
    }

    public byte read() {
        ensure(1);

        byte result = data[offset + position];
        position += 1;

        return result;
    }

    public int readInt() {
        ensure(4);

        int result = 0;
        for (int i = 0; i < 4; ++i) {
            result |= (0xFF & data[offset + position + i]) << (i * 8);
        }
        position += 4;

        return result;
    }

    public long readLong() {
        ensure(8);

        long result = 0;
        for (int i = 0; i < 8; ++i) {
            result |= (0xFFL & data[offset + position + i]) << (i * 8);
        }
        position += 8;

        return result;
    }

    public byte[] readBytes(int count) {
        ensure(count);
        byte[] bytes = new byte[count];
        System.arraycopy(data, offset + position, bytes, 0, count);
        position += count;

        return bytes;
    }

    public String readString() {
        return readString(0);
    }

    public String readString(int length) {
        if (length > 0) {
            ensure(length);
            if ((byte) 0x00 != data[offset + position + length - 1]) {
                throw new BSONException("unexpected end of string");
            }

            try {
                String s = new String(data, offset + position, length - 1, "UTF-8");
                position += length;
                return s;
            } catch (UnsupportedEncodingException e) {
                throw new BSONException("can not decode string", e);
            }
        }

        length = 0;
        while (position + length < limit && data[offset + position + length] != (byte)0x00) {
            ++length;
        }

        if (position + length > limit) {
            throw new BSONException("unexpected end of string");
        }

        String s = new String(data, offset + position, length);
        position += length + 1;
        return s;
    }

    public boolean isAvailable() {
        return position < limit;
    }

    public InputBuffer subBuffer(int limit) {
        ensure(limit);

        InputBuffer result = new InputBuffer(data, offset + position, limit);
        position += limit;

        return result;
    }

    protected void ensure(int size) {
        if (size > limit - position) {
            throw new BSONException("can not allocate sub buffer: not enought bytes");
        }
    }

    public static InputBuffer createFromByteArray(byte[] data) {
        return new InputBuffer(data, 0, data.length);
    }
}
