package com.softmotions.ejdb2;

import java.io.IOException;
import java.io.StringWriter;
import java.io.UnsupportedEncodingException;
import java.io.Writer;
import java.net.URLDecoder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;

/**
 * JSON parser/container.
 *
 * Based on:
 *
 * - https://github.com/json-iterator (MIT)
 *
 * - https://github.com/ralfstx/minimal-json (MIT)
 */
public final class JSON {

  public static ObjectBuilder buildObject() {
    return new ObjectBuilder();
  }

  public static ArrayBuilder buildArray() {
    return new ArrayBuilder();
  }

  public static JSON fromString(String val) {
    return new JSON(val);
  }

  public static JSON fromBytes(byte[] bytes) {
    return new JSON(bytes);
  }

  public static JSON fromMap(Map<String, Object> map) {
    return new JSON(ValueType.OBJECT, map);
  }

  public static JSON fromList(List<Object> list) {
    return new JSON(ValueType.ARRAY, list);
  }

  private static final ValueType[] valueTypes = new ValueType[256];
  private static final int[] hexDigits = new int['f' + 1];
  private static JSON UNKNOWN = new JSON(ValueType.UNKNOWN, null);

  static {
    for (int i = 0; i < valueTypes.length; ++i) {
      valueTypes[i] = ValueType.UNKNOWN;
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
    for (int i = 0; i < hexDigits.length; ++i) {
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
  private byte[] buf = new byte[0];
  private int head;
  private int tail;

  JSON(byte[] buf) {
    this.buf = buf;
    tail = buf.length;
    type = whatIsNext();
    value = read(type);
    reset();
  }

  JSON(String data) {
    this(data.getBytes());
  }

  private JSON(ValueType type, Object value) {
    this.type = type;
    this.value = value;
  }

  public void write(Writer w) {
    if (type != ValueType.UNKNOWN) {
      writeTo(w, value);
    }
  }

  @Override
  public String toString() {
    StringWriter sw = new StringWriter();
    write(sw);
    return sw.toString();
  }

  public boolean isUnknown() {
    return type == ValueType.UNKNOWN;
  }

  public boolean isNull() {
    return type == ValueType.NULL;
  }

  public boolean isNumber() {
    return type == ValueType.NUMBER;
  }

  public boolean isBoolean() {
    return type == ValueType.BOOLEAN;
  }

  public boolean isString() {
    return type == ValueType.STRING;
  }

  public boolean isObject() {
    return type == ValueType.OBJECT;
  }

  public boolean isArray() {
    return type == ValueType.ARRAY;
  }

  public Builder modify() {
    return new Builder(this);
  }

  public JSON get(String key) {
    if (type == ValueType.ARRAY) {
      try {
        return get(Integer.parseInt(key));
      } catch (NumberFormatException ignored) {
        return UNKNOWN;
      }
    }
    if (type != ValueType.OBJECT) {
      return UNKNOWN;
    }
    Object v = ((Map<String, Object>) value).get(key);
    return new JSON(ValueType.getTypeOf(v), v);
  }

  public JSON get(int index) {
    if (type == ValueType.OBJECT) {
      return get(String.valueOf(index));
    }
    if (type != ValueType.ARRAY) {
      return UNKNOWN;
    }
    List<Object> list = (List<Object>) value;
    if (index < 0 || index >= list.size()) {
      return UNKNOWN;
    }
    Object v = list.get(index);
    return new JSON(ValueType.getTypeOf(v), v);
  }

  public <T> T cast() {
    return (T) value;
  }

  public <T> T orDefault(T defaultValue) {
    return type != ValueType.UNKNOWN ? (T) value : defaultValue;
  }

  public <T> T orDefaultNotNull(T defaultValue) {
    return type != ValueType.UNKNOWN && type != ValueType.NULL ? (T) value : defaultValue;
  }

  public String asStringOr(String fallbackValue) {
    return type == ValueType.STRING ? (String) value : fallbackValue;
  }

  public String asString() {
    return asStringOr(null);
  }

  public String asTextOr(String fallbackValue) {
    if (type == ValueType.UNKNOWN) {
      return fallbackValue;
    } else {
      return String.valueOf(value);
    }
  }

  public String asText() {
    return asTextOr(null);
  }

  public Boolean asBooleanOr(Boolean fallbackValue) {
    return type == ValueType.BOOLEAN ? (Boolean) value : fallbackValue;
  }

  public Boolean asBoolean() {
    return asBooleanOr(null);
  }

  public Number asNumberOr(Number fallbackValue) {
    return type == ValueType.NUMBER ? (Number) value : fallbackValue;
  }

  public Number asNumber() {
    return asNumberOr(null);
  }

  public Integer asIntegerOr(Integer fallbackValue) {
    Number n = asNumberOr(fallbackValue);
    return n != null ? n.intValue() : null;
  }

  public Integer asInteger() {
    return asIntegerOr(null);
  }

  public Map<String, Object> asMapOr(Map<String, Object> fallbackValue) {
    return type == ValueType.OBJECT ? (Map<String, Object>) value : fallbackValue;
  }

  public Map<String, Object> asMapOrEmpty() {
    return asMapOr(Collections.emptyMap());
  }

  public List<Object> asListOr(List<Object> fallbackValue) {
    return type == ValueType.ARRAY ? (List<Object>) value : fallbackValue;
  }

  public List<Object> asListOrEmpty() {
    return asListOr(Collections.emptyList());
  }

  public JSON asKnownJsonOr(JSON fallbackValue) {
    return type == ValueType.UNKNOWN ? fallbackValue : this;
  }

  public JSON asKnownJson() {
    return asKnownJsonOr(null);
  }

  public JSON at(String pointer) {
    if (type != ValueType.OBJECT) {
      return UNKNOWN;
    }
    if (pointer == null || "/".equals(pointer) || pointer.isEmpty()) {
      return this;
    }
    return traverse(this, createPointer(pointer));
  }

  private JSON traverse(JSON obj, List<String> pp) {
    if (pp.isEmpty() || obj.isUnknown()) {
      return obj;
    }
    String key = pp.remove(0);
    if (obj.isObject() || obj.isArray()) {
      return traverse(obj.get(key), pp);
    } else {
      return UNKNOWN;
    }
  }

  private List<String> createPointer(String pointer) {
    if (pointer.charAt(0) == '#') {
      try {
        pointer = URLDecoder.decode(pointer, "UTF_8");
      } catch (UnsupportedEncodingException e) {
        throw new JSONException(e);
      }
    }
    if (pointer.isEmpty() || pointer.charAt(0) != '/') {
      throw new JSONException("Invalid JSON pointer: " + pointer);
    }
    String[] parts = pointer.substring(1).split("/");
    ArrayList<String> res = new ArrayList<>(parts.length);

    for (int i = 0, l = parts.length; i < l; ++i) {
      while (parts[i].contains("~1")) {
        parts[i] = parts[i].replace("~1", "/");
      }
      while (parts[i].contains("~0")) {
        parts[i] = parts[i].replace("~0", "~");
      }
      res.add(parts[i]);
    }
    return res;
  }

  private static void writeTo(Writer w, Object val) {
    try {
      write(new JsonWriter(w), val);
    } catch (IOException e) {
      throw new JSONException(e);
    }
  }

  private static void write(JsonWriter w, Object val) throws IOException {
    if (val == null) {
      w.writeLiteral("null");
      return;
    }
    final Class clazz = val.getClass();
    if (clazz == String.class) {
      w.writeString((String) val);
      return;
    } else if (clazz == Boolean.class) {
      Boolean bv = (Boolean) val;
      w.writeLiteral(bv ? "true" : "false");
      return;
    }
    if (val instanceof Map) {
      final Map<String, Object> map = (Map<String, Object>) val;
      Iterator<Entry<String, Object>> iter = map.entrySet().iterator();
      w.writeObjectOpen();
      if (iter.hasNext()) {
        Entry<String, Object> v = iter.next();
        w.writeMemberName(v.getKey());
        w.writeMemberSeparator();
        write(w, v.getValue());
        while (iter.hasNext()) {
          v = iter.next();
          w.writeObjectSeparator();
          w.writeMemberName(v.getKey());
          w.writeMemberSeparator();
          write(w, v.getValue());
        }
      }
      w.writeObjectClose();
    } else if (val instanceof List) {
      final List<Object> list = (List<Object>) val;
      w.writeArrayOpen();
      Iterator<Object> iter = list.iterator();
      if (iter.hasNext()) {
        write(w, iter.next());
        while (iter.hasNext()) {
          w.writeArraySeparator();
          write(w, iter.next());
        }
      }
      w.writeArrayClose();
    } else if (val instanceof Number) {
      w.writeNumber((Number) val);
    } else {
      w.writeString(val.toString());
    }
  }

  private void reset() {
    buf = null;
    head = 0;
    tail = 0;
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
      for (int i = head; i < tail; ++i) {
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
    for (int j = 0; j < bound; ++j) {
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

  public static enum ValueType {
    UNKNOWN, STRING, NUMBER, NULL, BOOLEAN, ARRAY, OBJECT;

    static ValueType getTypeOf(Object v) {
      if (v == null) {
        return NULL;
      }
      Class clazz = v.getClass();
      if (clazz == String.class) {
        return STRING;
      }
      if (clazz == Boolean.class) {
        return BOOLEAN;
      }
      if (v instanceof Number) {
        return NUMBER;
      }
      if (v instanceof Map) {
        return OBJECT;
      }
      if (v instanceof List) {
        return ARRAY;
      }
      return UNKNOWN;
    }
  }

  private static final class NumberChars {
    char[] chars;
    int charsLength;
    boolean dotFound;
  }

  private static final class JsonWriter {
    private static final char[] QUOT_CHARS = { '\\', '"' };
    private static final char[] BS_CHARS = { '\\', '\\' };
    private static final char[] LF_CHARS = { '\\', 'n' };
    private static final char[] CR_CHARS = { '\\', 'r' };
    private static final char[] TAB_CHARS = { '\\', 't' };

    // In JavaScript, U+2028 and U+2029 characters count as line endings and must be
    // encoded.
    // http://stackoverflow.com/questions/2965293/javascript-parse-error-on-u2028-unicode-character
    private static final char[] UNICODE_2028_CHARS = { '\\', 'u', '2', '0', '2', '8' };
    private static final char[] UNICODE_2029_CHARS = { '\\', 'u', '2', '0', '2', '9' };
    private static final char[] HEX_DIGITS = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
        'e', 'f' };

    private final Writer writer;

    JsonWriter(Writer writer) {
      this.writer = writer;
    }

    void writeLiteral(String value) throws IOException {
      writer.write(value);
    }

    void writeNumber(Number value) throws IOException {
      String s = value.toString();
      if (s.indexOf('.') > 0 && s.indexOf('e') < 0 && s.indexOf('E') < 0) {
        while (s.endsWith("0")) {
          s = s.substring(0, s.length() - 1);
        }
        if (s.endsWith(".")) {
          s = s.substring(0, s.length() - 1);
        }
      }
      writer.write(s);
    }

    void writeString(String string) throws IOException {
      writer.write('"');
      writeJsonString(string);
      writer.write('"');
    }

    void writeArrayOpen() throws IOException {
      writer.write('[');
    }

    void writeArrayClose() throws IOException {
      writer.write(']');
    }

    void writeArraySeparator() throws IOException {
      writer.write(',');
    }

    void writeObjectOpen() throws IOException {
      writer.write('{');
    }

    void writeObjectClose() throws IOException {
      writer.write('}');
    }

    void writeMemberName(String name) throws IOException {
      writer.write('"');
      writeJsonString(name);
      writer.write('"');
    }

    void writeMemberSeparator() throws IOException {
      writer.write(':');
    }

    void writeObjectSeparator() throws IOException {
      writer.write(',');
    }

    void writeJsonString(String string) throws IOException {
      int length = string.length();
      int start = 0;
      for (int index = 0; index < length; ++index) {
        char[] replacement = getReplacementChars(string.charAt(index));
        if (replacement != null) {
          writer.write(string, start, index - start);
          writer.write(replacement);
          start = index + 1;
        }
      }
      writer.write(string, start, length - start);
    }

    static char[] getReplacementChars(char ch) {
      if (ch > '\\') {
        if (ch < '\u2028' || ch > '\u2029') {
          // The lower range contains 'a' .. 'z'. Only 2 checks required.
          return null;
        }
        return ch == '\u2028' ? UNICODE_2028_CHARS : UNICODE_2029_CHARS;
      }
      if (ch == '\\') {
        return BS_CHARS;
      }
      if (ch > '"') {
        // This range contains '0' .. '9' and 'A' .. 'Z'. Need 3 checks to get here.
        return null;
      }
      if (ch == '"') {
        return QUOT_CHARS;
      }
      if (ch > 0x001f) { // CONTROL_CHARACTERS_END
        return null;
      }
      if (ch == '\n') {
        return LF_CHARS;
      }
      if (ch == '\r') {
        return CR_CHARS;
      }
      if (ch == '\t') {
        return TAB_CHARS;
      }
      return new char[] { '\\', 'u', '0', '0', HEX_DIGITS[ch >> 4 & 0x000f], HEX_DIGITS[ch & 0x000f] };
    }
  }

  public static final class Builder {
    final JSON json;
    final ObjectBuilder o;
    final ArrayBuilder a;

    Builder(JSON json) {
      this.json = json;
      if (json.type == ValueType.OBJECT) {
        o = new ObjectBuilder((Map<String, Object>) json.value);
        a = null;
      } else if (json.type == ValueType.ARRAY) {
        a = new ArrayBuilder((List<Object>) json.value);
        o = null;
      } else {
        throw new JSONException("JSON value must be either Object or Array");
      }
    }

    public ObjectBuilder addObject() {
      return getA().addObject();
    }

    public ArrayBuilder addObject(JSON val) {
      return getA().addObject(val);
    }

    public ArrayBuilder addArray() {
      return getA().addArray();
    }

    public ArrayBuilder addArray(JSON val) {
      return getA().addArray(val);
    }

    public ArrayBuilder add(String val) {
      return getA().add(val);
    }

    public ArrayBuilder add(Number val) {
      return getA().add(val);
    }

    public ArrayBuilder add(Boolean val) {
      return getA().add(val);
    }

    public ArrayBuilder addNull() {
      return getA().addNull();
    }

    public int length() {
      return getA().length();
    }

    public Object get(int index) {
      return getA().get(index);
    }

    public ArrayBuilder delete(int index) {
      return getA().delete(index);
    }

    public ObjectBuilder putObject(String key) {
      return getO().putObject(key);
    }

    public ObjectBuilder putObject(String key, JSON val) {
      return getO().putObject(key, val);
    }

    public ArrayBuilder putArray(String key) {
      return getO().putArray(key);
    }

    public ObjectBuilder putArray(String key, JSON val) {
      return getO().putArray(key, val);
    }

    public ObjectBuilder put(String key, String val) {
      return getO().put(key, val);
    }

    public ObjectBuilder put(String key, Number val) {
      return getO().put(key, val);
    }

    public ObjectBuilder put(String key, Boolean val) {
      return getO().put(key, val);
    }

    public ObjectBuilder putNull(String key) {
      return getO().putNull(key);
    }

    public ObjectBuilder delete(String key) {
      return getO().delete(key);
    }

    public Iterable<String> keys() {
      return getO().keys();
    }

    public JSON toJSON() {
      return json;
    }

    @Override
    public String toString() {
      return json.toString();
    }

    private ArrayBuilder getA() {
      if (a == null) {
        throw new JSONException("JSON value is not an Array");
      }
      return a;
    }

    private ObjectBuilder getO() {
      if (o == null) {
        throw new JSONException("JSON value is not an Object");
      }
      return o;
    }
  }

  public static final class ArrayBuilder {

    final List<Object> value;

    private JSON json;

    ArrayBuilder(List<Object> value) {
      this.value = value;
    }

    ArrayBuilder() {
      this(new ArrayList<>());
    }

    public ObjectBuilder addObject() {
      ObjectBuilder b = new ObjectBuilder();
      value.add(b.value);
      return b;
    }

    public ArrayBuilder addObject(JSON val) {
      if (val.type == ValueType.NULL) {
        return addNull();
      } else if (val.type != ValueType.OBJECT) {
        throw new JSONException("Value must be an Object");
      }
      value.add(val.value);
      return this;
    }

    public ArrayBuilder addArray() {
      ArrayBuilder b = new ArrayBuilder();
      value.add(b.value);
      return b;
    }

    public ArrayBuilder addArray(JSON val) {
      if (val.type == ValueType.NULL) {
        return addNull();
      } else if (val.type != ValueType.ARRAY) {
        throw new JSONException("Value must be an Array");
      }
      value.add(val.value);
      return this;
    }

    public ArrayBuilder add(String val) {
      value.add(val);
      return this;
    }

    public ArrayBuilder add(Number val) {
      value.add(val);
      return this;
    }

    public ArrayBuilder add(Boolean val) {
      value.add(val);
      return this;
    }

    public ArrayBuilder addNull() {
      value.add(null);
      return this;
    }

    public int length() {
      return value.size();
    }

    public Object get(int index) {
      return index >= 0 && value.size() > index ? value.get(index) : null;
    }

    public ArrayBuilder delete(int index) {
      value.remove(index);
      return this;
    }

    public JSON toJSON() {
      if (json == null) {
        json = new JSON(ValueType.ARRAY, value);
      }
      return json;
    }

    @Override
    public String toString() {
      StringWriter sw = new StringWriter();
      writeTo(sw, value);
      return sw.toString();
    }
  }

  public static final class ObjectBuilder {

    final Map<String, Object> value;

    private JSON json;

    ObjectBuilder(Map<String, Object> value) {
      this.value = value;
    }

    ObjectBuilder() {
      this(new LinkedHashMap<>());
    }

    public ObjectBuilder putObject(String key) {
      ObjectBuilder b = new ObjectBuilder();
      value.put(key, b.value);
      return b;
    }

    public ObjectBuilder putObject(String key, JSON val) {
      if (val.type == ValueType.NULL) {
        return putNull(key);
      }
      if (val.type != ValueType.OBJECT) {
        throw new JSONException("Value must be an Object");
      }
      value.put(key, val.value);
      return this;
    }

    public ArrayBuilder putArray(String key) {
      ArrayBuilder b = new ArrayBuilder();
      value.put(key, b.value);
      return b;
    }

    public ObjectBuilder putArray(String key, JSON val) {
      if (val.type == ValueType.NULL) {
        return putNull(key);
      }
      if (val.type != ValueType.ARRAY) {
        throw new JSONException("Value must be an Array");
      }
      value.put(key, val.value);
      return this;
    }

    public ObjectBuilder put(String key, String val) {
      value.put(key, val);
      return this;
    }

    public ObjectBuilder put(String key, Number val) {
      value.put(key, val);
      return this;
    }

    public ObjectBuilder put(String key, Boolean val) {
      value.put(key, val);
      return this;
    }

    public ObjectBuilder putNull(String key) {
      value.put(key, null);
      return this;
    }

    public ObjectBuilder delete(String key) {
      value.remove(key);
      return this;
    }

    public Iterable<String> keys() {
      return value.keySet();
    }

    public JSON toJSON() {
      if (json == null) {
        json = new JSON(ValueType.OBJECT, value);
      }
      return json;
    }

    @Override
    public String toString() {
      StringWriter sw = new StringWriter();
      writeTo(sw, value);
      return sw.toString();
    }
  }
}
