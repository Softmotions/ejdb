package com.softmotions.ejdb2;

public interface JQLCallback {

  long onRecord(long id, String json);
}