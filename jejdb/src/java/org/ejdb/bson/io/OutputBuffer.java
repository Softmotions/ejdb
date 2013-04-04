package org.ejdb.bson.io;

import org.ejdb.bson.BSONException;

import java.io.UnsupportedEncodingException;

/**
 * Utility class for serialize BSON object
 *
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

    /**
     * Returns current position in output
     * @return current position in output
     */
    public int getPosition() {
        return position;
    }

    /**
     * Sets position
     */
    public void setPosition(int position) {
        this.position = position;
    }

    /**
     * Returns actual (full) size of buffer (currently writed bytes)
     * @return actual (full) size of buffer (currently writed bytes)
     */
    public int getActualSize() {
        return actualSize;
    }

    /**
     * Returns buffer as byte array
     * @return buffer as byte array
     */
    public byte[] getResult() {
        byte[] result = new byte[getActualSize()];
        System.arraycopy(buffer, 0, result, 0, getActualSize());
        return result;
    }

    /**
     * Writes single byte to buffer
     */
    public void write(byte data) {
        ensureLength(1);
        buffer[position++] = data;
        actualSize = Math.max(actualSize, position);
    }

    /**
     * Writes byte array to buffer
     */
    public void write(byte[] data) {
        this.write(data, 0, data.length);
    }

    /**
     * Writes part of byte array to buffer
     *
     * @param data   source byte array
     * @param offset start position in source
     * @param length count bytes to write
     */
    public void write(byte[] data, int offset, int length) {
        ensureLength(length);
        System.arraycopy(data, offset, buffer, position, length);
        position += length;
        actualSize = Math.max(actualSize, position);
    }

    /**
     * Writes integer value (4 bytes) at specified position
     * @param position position to write
     * @param value value
     */
    public void writeIntAt(int position, int value) {
        int save = getPosition();
        setPosition(position);
        writeInt(value);
        setPosition(save);
    }

    /**
     * Writes integer value to buffer as 4 bytes
     */
    public void writeInt(int value) {
        this.write(new byte[]{
                (byte) ((value >>> 0) & 0xFF),
                (byte) ((value >>> 8) & 0xFF),
                (byte) ((value >>> 16) & 0xFF),
                (byte) ((value >>> 24) & 0xFF)
        });
    }

    /**
     * Writes long value to buffer as 8 bytes
     */
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

    /**
     * Writes double value to buffers as 8 bytes
     */
    public void writeDouble(double value) {
        this.writeLong(Double.doubleToRawLongBits(value));
    }

    /**
     * Writes {@link String} to buffer as c-style string (null-terminated)
     * @return count of writed bytes
     */
    public int writeString(String value) {
        int start = getPosition();
        try {
            this.write(value.getBytes("UTF-8"));
        } catch (UnsupportedEncodingException e) {
            throw new BSONException("can not encode string", e);
        }
        this.write((byte) 0x00);

        return getPosition() - start;
    }

    /**
     * Checks internal array size to hold needed data and expand it if need.
     */
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
