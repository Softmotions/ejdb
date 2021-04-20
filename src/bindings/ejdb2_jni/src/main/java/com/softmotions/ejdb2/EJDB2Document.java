package com.softmotions.ejdb2;

import com.softmotions.ejdb2.JSON.ValueType;

public final class EJDB2Document {

  public final long id;

  public final JSON json;

  EJDB2Document(long id, byte[] data) {
    this(id, new JSON(data));
  }

  EJDB2Document(long id, String data) {
    this(id, new JSON(data));
  }

  EJDB2Document(long id, JSON json) {
    if (json.type != ValueType.OBJECT && json.type != ValueType.NULL) {
      throw new IllegalArgumentException("json is not an object or null");
    }
    this.id = id;
    this.json = json;
  }

  @Override
  public String toString() {
    return "[" + id + ' ' + json.toString() + ']';
  }

  @Override
  public int hashCode() {
    return json.hashCode();
  }
}
