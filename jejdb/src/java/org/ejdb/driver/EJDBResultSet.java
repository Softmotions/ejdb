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

    protected native BSONObject getDB(int position);
    protected native int lengthDB();
    protected native void closeDB();

    public Iterator<BSONObject> iterator() {
        return this;
    }

    public boolean hasNext() {
        return position < this.lengthDB();
    }

    public BSONObject next() {
        if (!hasNext()) {
            throw new NoSuchElementException();
        }

        return getDB(position++);
    }

    public void remove() {
        throw new UnsupportedOperationException();
    }

    public void close() {
        this.closeDB();
    }

    @Override
    protected void finalize() throws Throwable {
        this.close();
        super.finalize();
    }
}
