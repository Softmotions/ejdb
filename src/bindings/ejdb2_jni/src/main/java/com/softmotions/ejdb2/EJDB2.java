package com.softmotions.ejdb2;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class EJDB2 implements AutoCloseable {

    static {
        loadLibrary();
    }

    private static void loadLibrary() {
        System.loadLibrary("ejdb2jni");
    }

    private long _handle;

    private final EJDB2Options opts;

    public EJDB2(EJDB2Options opts) throws EJDB2Exception {
        this.opts = opts;
        open(opts);
    }

    @Override
    public void close() throws Exception {
        dispose();
    }

    private native void open(EJDB2Options opts) throws EJDB2Exception;

    private native void dispose() throws EJDB2Exception;
}
