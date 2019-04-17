package com.softmotions.ejdb2;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class EJDB2Exception extends RuntimeException {

  private static final long serialVersionUID = 2380289380319769542L;

  public EJDB2Exception(String message) {
    super(message);
  }

  public EJDB2Exception(String message, Throwable cause) {
    super(message, cause);
  }
}
