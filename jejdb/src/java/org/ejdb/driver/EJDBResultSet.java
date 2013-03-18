package org.ejdb.driver;

import org.bson.BSONObject;

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

    public native BSONObject get(int position);
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

        return get(position++);
    }

    public void remove() {
        throw new UnsupportedOperationException();
    }

    public native void close();

    @Override
    protected void finalize() throws Throwable {
        this.close();
        super.finalize();
    }
}
