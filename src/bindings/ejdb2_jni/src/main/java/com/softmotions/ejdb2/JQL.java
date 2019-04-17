package com.softmotions.ejdb2;

import java.util.StringJoiner;

public class JQL {

  public final String jql;

  JQL(String jql) {
    this.jql = jql;
  }

  @Override
  public String toString() {
    return new StringJoiner(", ", JQL.class.getSimpleName() + "[", "]").toString();
  }
}