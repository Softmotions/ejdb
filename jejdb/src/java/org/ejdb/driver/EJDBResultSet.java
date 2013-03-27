package org.ejdb.driver;

import org.ejdb.bson.BSONObject;

import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBResultSet implements Iterable<BSONObject>, Iterator<BSONObject> {
    private long rsPointer;

    private int position;

    EJDBResultSet(long rsPointer) {
        this.rsPointer = rsPointer;

        this.position = 0;
    }

    /**
     * Returns object by position
     */
    public native BSONObject get(int position) throws EJDBException;

    /**
     * Returns objects count in result set
     */
    public native int length();

    public Iterator<BSONObject> iterator() {
        return this;
    }

    public boolean hasNext() {
        return position < this.length();
    }

    public BSONObject next() {
        if (!hasNext()) {
            throw new NoSuchElementException();
        }

        try {
            return get(position++);
        } catch (EJDBException e) {
            // TODO: ?
            throw new RuntimeException(e);
        }
    }

    public void remove() {
        throw new UnsupportedOperationException();
    }

    /**
     * Close result set
     */
    public native void close() throws EJDBException;

    @Override
    protected void finalize() throws Throwable {
        this.close();
        super.finalize();
    }
}
