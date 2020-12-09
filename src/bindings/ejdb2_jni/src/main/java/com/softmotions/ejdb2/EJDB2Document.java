package com.softmotions.ejdb2;

import java.util.Map;

import com.softmotions.ejdb2.JSON.ValueType;

public final class EJDB2Document {

  // todo: JSON pointer spec

  final long id;

  final Map<String, Object> value;

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
    value = (Map<String, Object>) json.value;
    this.id = id;
  }
}
