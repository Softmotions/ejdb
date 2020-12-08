package com.softmotions.ejdb2;

public class JSONException extends RuntimeException {

  private static final long serialVersionUID = -1180877018017230756L;

  public JSONException() {
  }

  public JSONException(String message) {
    super(message);
  }

  public JSONException(String message, Throwable cause) {
    super(message, cause);
  }

  public JSONException(Throwable cause) {
    super(cause);
  }
}
