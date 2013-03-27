package org.ejdb.bson;

import java.io.OutputStream;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public final class BSON {

    public static final byte NULL = (byte) 0x0A;
    public static final byte INT = (byte) 0x10;
    public static final byte LONG = (byte) 0x11;
    public static final byte DOUBLE = (byte) 0x01;
    public static final byte STRING = (byte) 0x02;
    public static final byte BOOLEAN = (byte) 0x08;
    public static final byte OBJECT_ID = (byte) 0x07;
    public static final byte OBJECT = (byte) 0x03;
    public static final byte BINARY = (byte) 0x05;
    public static final byte ARRAY = (byte) 0x04;
    public static final byte DATE = (byte) 0x09;
    public static final byte REGEX = (byte) 0x0B;

    private BSON() {
    }

    public static byte[] encode(BSONObject obj){
        return new BSONEncoder().encode(obj);
    }

    public static BSONObject decode(byte[] data) {
        return new BSONDecoder().decode(data);
    }
}
