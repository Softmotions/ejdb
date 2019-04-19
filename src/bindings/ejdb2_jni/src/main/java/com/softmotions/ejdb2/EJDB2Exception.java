package com.softmotions.ejdb2;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public final class EJDB2Exception extends RuntimeException {

  private static final long serialVersionUID = 2380289380319769542L;

  private long code;

  public long getCode() {
    return code;
  }

  public EJDB2Exception(long code, String message, Throwable cause) {
    super(message, cause);
    this.code = code;
  }

  public EJDB2Exception(long code, String message) {
    super(message);
    this.code = code;
  }
}
