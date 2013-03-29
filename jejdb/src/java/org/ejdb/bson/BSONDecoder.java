package org.ejdb.bson;

import org.ejdb.bson.io.InputBuffer;
import org.ejdb.bson.types.ObjectId;
import org.ejdb.bson.util.RegexFlag;

import java.util.Date;
import java.util.regex.Pattern;

/**
 * Default BSON object decoder (from plain byte array to Java object)
 *
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
class BSONDecoder {

    private InputBuffer input;

    /**
     * Decode BSON object
     *
     * @throws IllegalStateException if other decoding process active with this decoder
     */
    public BSONObject decode(byte[] data) throws IllegalStateException {
        if (isBusy()) {
            throw new IllegalStateException("other decoding in process");
        }

        if (data == null) {
            throw new IllegalArgumentException("can not read object from null");
        }

        input = InputBuffer.createFromByteArray(data);
        BSONObject result = read();
        input = null;

        return result;
    }

    /**
     * @return <code>true</code> if decoder currently in use
     */
    public boolean isBusy() {
        return input != null;
    }


    protected BSONObject read() {
        int length = input.readInt();

        BSONObject result = this.readObject(input.subBuffer(length - 5));

        if (0x00 != input.read()) {
            throw new BSONException("unexpected end of document");
        }

        return result;
    }

    protected BSONObject readObject(InputBuffer input) {
        BSONObject result = new BSONObject();
        while (input.isAvailable()) {
            byte type = input.read();
            String name = input.readString();
            switch (type) {
                case BSON.DOUBLE:
                    result.put(name, Double.longBitsToDouble(input.readLong()));
                    break;

                case BSON.STRING:
                    int strlen = input.readInt();
                    result.put(name, input.readString(strlen));
                    break;

                case BSON.OBJECT:
                    int objlen = input.readInt();
                    result.put(name, readObject(input.subBuffer(objlen - 5)));
                    input.read();
                    break;

                case BSON.ARRAY:
                    int arrlen = input.readInt();
                    BSONObject arrObject = readObject(input.subBuffer(arrlen - 5));
                    Object[] array = new Object[arrObject.size()];
                    for (int i = 0; i < array.length; ++i) {
                        array[i] = arrObject.get(String.valueOf(i));
                    }
                    result.put(name, array);
                    input.read();
                    break;

                case BSON.BINARY:
                    int binlen = input.readInt();
                    byte subtype = input.read();
                    if (0x00 != subtype) {
                        throw new BSONException("unexpected binary type: " + subtype);
                    }
                    result.put(name, input.readBytes(binlen));
                    break;

                case BSON.OBJECT_ID:
                    result.put(name, new ObjectId(input.readBytes(12)));
                    break;

                case BSON.BOOLEAN:
                    byte bvalue = input.read();
                    if (0x00 != bvalue && 0x01 != bvalue) {
                        throw new BSONException("unexpected boolean value");
                    }
                    result.put(name, 0x01 == bvalue);
                    break;

                case BSON.DATE:
                    result.put(name, new Date(input.readLong()));
                    break;

                case BSON.NULL:
                    result.put(name, null);
                    break;

                case BSON.REGEX:
                    //noinspection MagicConstant
                    result.put(name, Pattern.compile(input.readString(), RegexFlag.stringToRegexFlags(input.readString())));
                    break;

                case BSON.INT:
                    result.put(name, input.readInt());
                    break;

                case BSON.LONG:
                    result.put(name, input.readLong());
                    break;

                default:
                    throw new BSONException("unexpected type: " + type);
            }
        }

        return result;
    }
}
