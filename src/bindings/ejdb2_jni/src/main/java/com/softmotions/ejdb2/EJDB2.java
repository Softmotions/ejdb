package com.softmotions.ejdb2;

import java.io.Writer;
import java.nio.ByteBuffer;

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

    private final EJDB2Builder opts;


    public EJDB2(EJDB2Builder opts) throws EJDB2Exception {
        this.opts = opts;
        _open(opts);
    }

    @Override
    public void close() throws Exception {
        _dispose();
    }

    private native void _open(EJDB2Builder opts) throws EJDB2Exception;

    private native void _dispose() throws EJDB2Exception;

    // todo pending
    private native void del(String collection, long id) throws EJDB2Exception;

    private native long _put(String collection, ByteBuffer json) throws EJDB2Exception;

    private native void _patch(String collection, ByteBuffer patch, long id) throws EJDB2Exception;

    private native void _get(String collection, long id, Writer out) throws EJDB2Exception;

    private native void _info(Writer out) throws EJDB2Exception;

}
