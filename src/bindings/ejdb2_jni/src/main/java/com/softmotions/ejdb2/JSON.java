package com.softmotions.ejdb2;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;

/**
 * JSON parser/container. Heavily based on https://github.com/json-iterator (MIT
 * licensed)
 */
public final class JSON {

  // todo: JSON pointer spec
  // todo: JSON encoder

  private static final ValueType[] valueTypes = new ValueType[256];
  private static final int[] hexDigits = new int['f' + 1];

  static {
    for (int i = 0; i < valueTypes.length; ++i) {
      valueTypes[i] = ValueType.INVALID;
    }
    valueTypes['"'] = ValueType.STRING;
    valueTypes['-'] = ValueType.NUMBER;
    valueTypes['0'] = ValueType.NUMBER;
    valueTypes['1'] = ValueType.NUMBER;
    valueTypes['2'] = ValueType.NUMBER;
    valueTypes['3'] = ValueType.NUMBER;
    valueTypes['4'] = ValueType.NUMBER;
    valueTypes['5'] = ValueType.NUMBER;
    valueTypes['6'] = ValueType.NUMBER;
    valueTypes['7'] = ValueType.NUMBER;
    valueTypes['8'] = ValueType.NUMBER;
    valueTypes['9'] = ValueType.NUMBER;
    valueTypes['t'] = ValueType.BOOLEAN;
    valueTypes['f'] = ValueType.BOOLEAN;
    valueTypes['n'] = ValueType.NULL;
    valueTypes['['] = ValueType.ARRAY;
    valueTypes['{'] = ValueType.OBJECT;
  }

  static {
    for (int i = 0; i < hexDigits.length; i++) {
      hexDigits[i] = -1;
    }
    for (int i = '0'; i <= '9'; ++i) {
      hexDigits[i] = (i - '0');
    }
    for (int i = 'a'; i <= 'f'; ++i) {
      hexDigits[i] = ((i - 'a') + 10);
    }
    for (int i = 'A'; i <= 'F'; ++i) {
      hexDigits[i] = ((i - 'A') + 10);
    }
  }

  public final Object value;
  public final ValueType type;

  private char[] reusableChars = new char[32];
  private byte[] buf;
  private int head;
  private int tail;

  public JSON(byte[] buf) {
    this.buf = buf;
    tail = buf.length;
    type = whatIsNext();
    value = read(type);
  }

  public JSON(String data) {
    this(data.getBytes());
  }

  private Object read(ValueType valueType) {
    try {
      switch (valueType) {
        case STRING:
          return readString();
        case NUMBER:
          return readNumber();
        case NULL:
          head += 4;
          return null;
        case BOOLEAN:
          return readBoolean();
        case ARRAY:
          return readArray(new ArrayList<Object>(4));
        case OBJECT:
          return readObject(new LinkedHashMap<String, Object>(4));
        default:
          throw reportError("read", "unexpected value type: " + valueType);
      }
    } catch (ArrayIndexOutOfBoundsException e) {
      throw reportError("read", "premature end");
    }
  }

  private Object read() {
    return read(whatIsNext());
  }

  private Map<String, Object> readObject(Map<String, Object> map) {
    byte c = nextToken();
    if ('{' == c) {
      c = nextToken();
      if ('"' == c) {
        unreadByte();
        String field = readString();
        if (nextToken() != ':') {
          throw reportError("readObject", "expect :");
        }
        map.put(field, read());
        while (nextToken() == ',') {
          field = readString();
          if (nextToken() != ':') {
            throw reportError("readObject", "expect :");
          }
          map.put(field, read());
        }
        return map;
      }
      if ('}' == c) {
        return map;
      }
      throw reportError("readObject", "expect \" after {");
    }
    if ('n' == c) {
      head += 3;
      return map;
    }
    throw reportError("readObject", "expect { or n");
  }

  private ArrayList<Object> readArray(ArrayList<Object> list) {
    byte c = nextToken();
    if (c == '[') {
      c = nextToken();
      if (c != ']') {
        unreadByte();
        list.add(read());
        while (nextToken() == ',') {
          list.add(read());
        }
        return list;
      }
      return list;
    }
    if (c == 'n') {
      return list;
    }
    throw reportError("readArray", "expect [ or n, but found: " + (char) c);
  }

  private boolean readBoolean() {
    byte c = nextToken();
    if ('t' == c) {
      head += 3; // true
      return true;
    }
    if ('f' == c) {
      head += 4; // false
      return false;
    }
    throw reportError("readBoolean", "expect t or f, found: " + c);
  }

  private Object readNumber() {
    NumberChars numberChars = readNumberImpl();
    String numberStr = new String(numberChars.chars, 0, numberChars.charsLength);
    Double number = Double.valueOf(numberStr);
    if (numberChars.dotFound) {
      return number;
    }
    double doubleNumber = number;
    if (doubleNumber == Math.floor(doubleNumber) && !Double.isInfinite(doubleNumber)) {
      long longNumber = Long.valueOf(numberStr);
      if (longNumber <= Integer.MAX_VALUE && longNumber >= Integer.MIN_VALUE) {
        return (int) longNumber;
      }
      return longNumber;
    }
    return number;
  }

  private NumberChars readNumberImpl() {
    int j = 0;
    boolean dotFound = false;
    for (;;) {
      for (int i = head; i < tail; i++) {
        if (j == reusableChars.length) {
          char[] newBuf = new char[reusableChars.length * 2];
          System.arraycopy(reusableChars, 0, newBuf, 0, reusableChars.length);
          reusableChars = newBuf;
        }
        byte c = buf[i];
        switch (c) {
          case '.':
          case 'e':
          case 'E':
            dotFound = true;
            // fallthrough
          case '-':
          case '+':
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            reusableChars[j++] = (char) c;
            break;
          default:
            head = i;
            NumberChars numberChars = new NumberChars();
            numberChars.chars = reusableChars;
            numberChars.charsLength = j;
            numberChars.dotFound = dotFound;
            return numberChars;
        }
      }

      head = tail;
      NumberChars numberChars = new NumberChars();
      numberChars.chars = reusableChars;
      numberChars.charsLength = j;
      numberChars.dotFound = dotFound;
      return numberChars;
    }
  }

  private String readString() {
    byte c = nextToken();
    if (c != '"') {
      if (c == 'n') {
        head += 3;
        return null;
      }
      throw reportError("readString", "expect string or null, but " + (char) c);
    }
    int j = parseString();
    return new String(reusableChars, 0, j);
  }

  private int parseString() {
    byte c;
    int i = head;
    int bound = reusableChars.length;
    for (int j = 0; j < bound; j++) {
      c = buf[i++];
      if (c == '"') {
        head = i;
        return j;
      }
      if ((c ^ '\\') < 1) {
        break;
      }
      reusableChars[j] = (char) c;
    }
    int alreadyCopied = 0;
    if (i > head) {
      alreadyCopied = i - head - 1;
      head = i - 1;
    }
    return readStringSlowPath(alreadyCopied);
  }

  private int translateHex(byte b) {
    int val = hexDigits[b];
    if (val == -1) {
      throw new IndexOutOfBoundsException(b + " is not valid hex digit");
    }
    return val;
  }

  private int readStringSlowPath(int j) {
    try {
      boolean isExpectingLowSurrogate = false;
      for (int i = head; i < tail;) {
        int bc = buf[i++];
        if (bc == '"') {
          head = i;
          return j;
        }
        if (bc == '\\') {
          bc = buf[i++];
          switch (bc) {
            case 'b':
              bc = '\b';
              break;
            case 't':
              bc = '\t';
              break;
            case 'n':
              bc = '\n';
              break;
            case 'f':
              bc = '\f';
              break;
            case 'r':
              bc = '\r';
              break;
            case '"':
            case '/':
            case '\\':
              break;
            case 'u':
              bc = (translateHex(buf[i++]) << 12) + (translateHex(buf[i++]) << 8) + (translateHex(buf[i++]) << 4)
                  + translateHex(buf[i++]);
              if (Character.isHighSurrogate((char) bc)) {
                if (isExpectingLowSurrogate) {
                  throw new JSONException("invalid surrogate");
                } else {
                  isExpectingLowSurrogate = true;
                }
              } else if (Character.isLowSurrogate((char) bc)) {
                if (isExpectingLowSurrogate) {
                  isExpectingLowSurrogate = false;
                } else {
                  throw new JSONException("invalid surrogate");
                }
              } else {
                if (isExpectingLowSurrogate) {
                  throw new JSONException("invalid surrogate");
                }
              }
              break;

            default:
              throw reportError("readStringSlowPath", "invalid escape character: " + bc);
          }
        } else if ((bc & 0x80) != 0) {
          final int u2 = buf[i++];
          if ((bc & 0xE0) == 0xC0) {
            bc = ((bc & 0x1F) << 6) + (u2 & 0x3F);
          } else {
            final int u3 = buf[i++];
            if ((bc & 0xF0) == 0xE0) {
              bc = ((bc & 0x0F) << 12) + ((u2 & 0x3F) << 6) + (u3 & 0x3F);
            } else {
              final int u4 = buf[i++];
              if ((bc & 0xF8) == 0xF0) {
                bc = ((bc & 0x07) << 18) + ((u2 & 0x3F) << 12) + ((u3 & 0x3F) << 6) + (u4 & 0x3F);
              } else {
                throw reportError("readStringSlowPath", "invalid unicode character");
              }
              if (bc >= 0x10000) {
                // check if valid unicode
                if (bc >= 0x110000) {
                  throw reportError("readStringSlowPath", "invalid unicode character");
                }
                // split surrogates
                final int sup = bc - 0x10000;
                if (reusableChars.length == j) {
                  char[] newBuf = new char[reusableChars.length * 2];
                  System.arraycopy(reusableChars, 0, newBuf, 0, reusableChars.length);
                  reusableChars = newBuf;
                }
                reusableChars[j++] = (char) ((sup >>> 10) + 0xd800);
                if (reusableChars.length == j) {
                  char[] newBuf = new char[reusableChars.length * 2];
                  System.arraycopy(reusableChars, 0, newBuf, 0, reusableChars.length);
                  reusableChars = newBuf;
                }
                reusableChars[j++] = (char) ((sup & 0x3ff) + 0xdc00);
                continue;
              }
            }
          }
        }
        if (reusableChars.length == j) {
          char[] newBuf = new char[reusableChars.length * 2];
          System.arraycopy(reusableChars, 0, newBuf, 0, reusableChars.length);
          reusableChars = newBuf;
        }
        reusableChars[j++] = (char) bc;
      }
      throw reportError("readStringSlowPath", "incomplete string");
    } catch (IndexOutOfBoundsException e) {
      throw reportError("readString", "incomplete string");
    }
  }

  private ValueType whatIsNext() {
    ValueType valueType = valueTypes[nextToken()];
    unreadByte();
    return valueType;
  }

  private void unreadByte() {
    if (head == 0) {
      throw reportError("unreadByte", "unread too many bytes");
    }
    head--;
  }

  private byte nextToken() {
    int i = head;
    for (;;) {
      byte c = buf[i++];
      switch (c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
          continue;
        default:
          head = i;
          return c;
      }
    }
  }

  private JSONException reportError(String op, String msg) {
    int peekStart = head - 10;
    if (peekStart < 0) {
      peekStart = 0;
    }
    int peekSize = head - peekStart;
    if (head > tail) {
      peekSize = tail - peekStart;
    }
    String peek = new String(buf, peekStart, peekSize);
    throw new JSONException(op + ": " + msg + ", head: " + head + ", peek: " + peek + ", buf: " + new String(buf));
  }

  public static enum ValueType {
    INVALID, STRING, NUMBER, NULL, BOOLEAN, ARRAY, OBJECT
  }

  private static final class NumberChars {
    char[] chars;
    int charsLength;
    boolean dotFound;
  }

  @Override
  public int hashCode() {
    return Objects.hash(type, value);
  }

  @Override
  public boolean equals(Object obj) {
    if (this == obj) {
      return true;
    }
    if (obj == null) {
      return false;
    }
    if (getClass() != obj.getClass()) {
      return false;
    }
    JSON other = (JSON) obj;
    return type == other.type && Objects.equals(value, other.value);
  }
}
