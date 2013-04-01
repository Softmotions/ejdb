package org.ejdb.driver;

import org.ejdb.bson.BSONObject;

import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBResultSet implements Iterable<BSONObject>, Iterator<BSONObject> {
    private transient long rsPointer;

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

    /**
     * {@inheritDoc}
     */
    public Iterator<BSONObject> iterator() {
        return this;
    }

    /**
     * {@inheritDoc}
     */
    public boolean hasNext() {
        return position < this.length();
    }

    /**
     * {@inheritDoc}
     */
    public BSONObject next() throws EJDBException {
        if (!hasNext()) {
            throw new NoSuchElementException();
        }

        return get(position++);
    }

    /**
     * {@inheritDoc}
     */
    public void remove() {
        throw new UnsupportedOperationException();
    }

    /**
     * Close result set
     */
    public native void close() throws EJDBException;

    /**
     * {@inheritDoc}
     */
    @Override
    protected void finalize() throws Throwable {
        this.close();
        super.finalize();
    }
}
