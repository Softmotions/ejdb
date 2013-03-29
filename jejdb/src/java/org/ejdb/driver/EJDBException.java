package org.ejdb.driver;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBException extends RuntimeException {
    private int code;

    public EJDBException() {
        super();
    }

    public EJDBException(String message) {
        super(message);
    }

    public EJDBException(String message, Throwable cause) {
        super(message, cause);
    }

    public EJDBException(Throwable cause) {
        super(cause);
    }

    public EJDBException(int code, String message) {
        super(message);
        this.code = code;
    }

    public int getCode() {
        return code;
    }
}