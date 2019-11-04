package com.softmotions.ejdb2;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public final class EJDB2Exception extends RuntimeException {

  private static final long serialVersionUID = 2380289380319769542L;

  private final long code;

  private final long errno;

  public long getCode() {
    return code;
  }

  public long getErrno() {
    return errno;
  }

  public EJDB2Exception(long code, long errno, String message, Throwable cause) {
    super("@ejdb IWRC:" + code + " errno:" + errno + ' ' + message, cause);
    this.code = code;
    this.errno = errno;
  }

  public EJDB2Exception(long code, long errno, String message) {
    super("@ejdb IWRC:" + code + " errno:" + errno + ' ' + message);
    this.code = code;
    this.errno = errno;
  }
}
