package org.ejdb.bson;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class BSONException extends RuntimeException {
    public BSONException() {
        super();
    }

    public BSONException(String message) {
        super(message);
    }

    public BSONException(String message, Throwable cause) {
        super(message, cause);
    }

    public BSONException(Throwable cause) {
        super(cause);
    }
}
