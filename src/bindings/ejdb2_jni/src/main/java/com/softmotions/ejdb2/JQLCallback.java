package com.softmotions.ejdb2;

interface JQLCallback {

  boolean onRecord(long id, String json);
}