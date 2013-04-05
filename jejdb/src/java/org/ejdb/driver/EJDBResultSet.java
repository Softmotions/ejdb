package org.ejdb.driver;

import org.ejdb.bson.BSONObject;

import java.io.Closeable;
import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.NoSuchElementException;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBResultSet implements Iterable<BSONObject>, Iterator<BSONObject>, Closeable {
    private transient long rsPointer;
    private transient Map<Integer, WeakReference<BSONObject>> cache;

    private int position;

    {
        cache = new HashMap<Integer, WeakReference<BSONObject>>();
    }

    EJDBResultSet(long rsPointer) {
        this.rsPointer = rsPointer;

        this.position = 0;
    }

    /**
     * Returns object by position
     */
    protected native BSONObject _get(int position) throws EJDBException;

    /**
     * Returns object by position
     */
    public BSONObject get(int position) throws EJDBException {
        BSONObject obj;
        WeakReference<BSONObject> wr;
        if (cache.containsKey(position) && (wr = cache.get(position)).get() != null) {
            obj = wr.get();
        } else {
            obj = _get(position);
            cache.put(position, new WeakReference<BSONObject>(obj));
        }

        return obj;
    }

    /**
     * Returns objects count in result set
     */
    public native int length();

    /**
     * {@inheritDoc}
     */
    public Iterator<BSONObject> iterator() {
        return new Iterator<BSONObject>() {
            private int pos = 0;

            public boolean hasNext() {
                return pos < EJDBResultSet.this.length();
            }

            public BSONObject next() {
                if (!hasNext()) {
                    throw new NoSuchElementException();
                }
                return get(pos++);
            }

            public void remove() {
                throw new UnsupportedOperationException();
            }
        };
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
     * {@inheritDoc}
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
