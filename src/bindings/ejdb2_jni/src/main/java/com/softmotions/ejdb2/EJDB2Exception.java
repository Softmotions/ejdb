package com.softmotions.ejdb2;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class EJDB2Exception extends RuntimeException {

    public EJDB2Exception(String message) {
        super(message);
    }

    public EJDB2Exception(String message, Throwable cause) {
        super(message, cause);
    }
}
