package com.softmotions.ejdb2;

/**
 * Missing JSON value at key exception.
 */
public class JSONMissingException extends RuntimeException {

  private static final long serialVersionUID = -1180177018517230756L;

  public JSONMissingException(String key) {
    super("Missing JSON value at: '" + key + '\'');
  }

  public JSONMissingException(int index) {
    super("Missing JSON value at: '" + index + '\'');
  }
}
