package org.ejdb.bson.io;

import java.io.UnsupportedEncodingException;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class OutputBuffer {
    public static final int BUFFER_DEFAULT_LENGTH = 512;

    private byte[] buffer;
    private int position;
    private int actualSize;

    {
        buffer = new byte[BUFFER_DEFAULT_LENGTH];
        position = 0;
        actualSize = 0;
    }

    public int getPosition() {
        return position;
    }

    public void setPosition(int position) {
        this.position = position;
    }

    public int getActualSize() {
        return actualSize;
    }

    public byte[] getResult() {
        byte[] result = new byte[getActualSize()];
        System.arraycopy(buffer, 0, result, 0, getActualSize());
        return result;
    }

    public void write(byte data) {
        ensureLength(1);
        buffer[position++] = data;
        actualSize = Math.max(actualSize, position);
    }

    public void write(byte[] data) {
        this.write(data, 0, data.length);
    }

    public void write(byte[] data, int offset, int length) {
        ensureLength(length);
        System.arraycopy(data, offset, buffer, position, length);
        position += length;
        actualSize = Math.max(actualSize, position);
    }

    public void writeIntAt(int position, int value) {
        int save = getPosition();
        setPosition(position);
        writeInt(value);
        setPosition(save);
    }

    public void writeInt(int value) {
        this.write(new byte[]{
                (byte) ((value >>> 0) & 0xFF),
                (byte) ((value >>> 8) & 0xFF),
                (byte) ((value >>> 16) & 0xFF),
                (byte) ((value >>> 24) & 0xFF)
        });
    }

    public void writeLong(long value) {
        this.write(new byte[]{
                (byte) ((value >>> 0) & 0xFF),
                (byte) ((value >>> 8) & 0xFF),
                (byte) ((value >>> 16) & 0xFF),
                (byte) ((value >>> 24) & 0xFF),
                (byte) ((value >>> 32) & 0xFF),
                (byte) ((value >>> 40) & 0xFF),
                (byte) ((value >>> 48) & 0xFF),
                (byte) ((value >>> 56) & 0xFF),
        });
    }

    public void writeDouble(double value) {
        this.writeLong(Double.doubleToRawLongBits(value));
    }

    public int writeString(String value) {
        int start = getPosition();
        try {
            this.write(value.getBytes("UTF-8"));
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        }
        this.write((byte) 0x00);

        return getPosition() - start;
    }

    protected void ensureLength(int need) {
        if (need <= buffer.length - position) {
            return;
        }

        int newSize = (int) Math.floor(((double) (need + position - buffer.length)) / BUFFER_DEFAULT_LENGTH) * BUFFER_DEFAULT_LENGTH;
        byte[] newBuffer = new byte[newSize];
        System.arraycopy(buffer, 0, newBuffer, 0, position);
        buffer = newBuffer;
    }
}
