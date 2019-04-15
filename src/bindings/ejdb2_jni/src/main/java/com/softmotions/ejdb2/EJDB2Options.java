package com.softmotions.ejdb2;

import java.io.Serializable;
import java.util.StringJoiner;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class EJDB2Options implements Serializable {

    private static final long serialVersionUID = 6475291112728462635L;

    private boolean no_wal;

    private int sort_buffer_sz;

    private int document_buffer_sz;

    private final IWKVOptions iwkv;

    private final EJDB2HttpOptions http;

    public IWKVOptions getIWKVOptions() {
        return iwkv;
    }

    public EJDB2HttpOptions getHttpOptions() {
        return http;
    }

    public boolean isNoWal() {
        return no_wal;
    }

    public int getSortBufferSize() {
        return sort_buffer_sz;
    }

    public int getDocumentBufferSize() {
        return document_buffer_sz;
    }

    public EJDB2Options(String path) {
        iwkv = new IWKVOptions(path);
        http = new EJDB2HttpOptions();
    }

    public EJDB2Options noWal(boolean v) {
        this.no_wal = v;
        return this;
    }

    public EJDB2Options sortBufferSize(int v) {
        this.sort_buffer_sz = v;
        return this;
    }

    public EJDB2Options documentBufferSize(int v) {
        this.document_buffer_sz = v;
        return this;
    }

    public EJDB2Options truncate() {
        iwkv.truncate();
        return this;
    }

    public EJDB2Options readonly() {
        iwkv.readonly();
        return this;
    }

    EJDB2Options fileLockFailFast(boolean v) {
        iwkv.fileLockFailFast(v);
        return this;
    }

    EJDB2Options randomSeed(long seed) {
        iwkv.randomSeed(seed);
        return this;
    }

    EJDB2Options walEnabled(boolean v) {
        iwkv.walEnabled(v);
        return this;
    }

    EJDB2Options walCRCOnCheckpoint(boolean v) {
        iwkv.walCRCOnCheckpoint(v);
        return this;
    }

    EJDB2Options walSavepointTimeoutSec(int v) {
        iwkv.walSavepointTimeoutSec(v);
        return this;
    }

    EJDB2Options walCheckpointTimeoutSec(int v) {
        iwkv.walCheckpointTimeoutSec(v);
        return this;
    }

    EJDB2Options walBufferSize(long v) {
        iwkv.walBufferSize(v);
        return this;
    }

    EJDB2Options walCheckpointBufferSize(long v) {
        iwkv.walCheckpointBufferSize(v);
        return this;
    }

    public EJDB2Options httpEnabled(boolean v) {
        http.enabled = v;
        return this;
    }

    public EJDB2Options httpPort(int v) {
        http.port = v;
        return this;
    }

    public EJDB2Options httpBind(String v) {
        http.bind = v;
        return this;
    }

    public EJDB2Options httpAccessToken(String v) {
        http.access_token = v;
        return this;
    }

    public EJDB2Options httpReadAnon(boolean v) {
        http.read_anon = v;
        return this;
    }

    public EJDB2Options httpMaxBodySize(int v) {
        http.max_body_size = v;
        return this;
    }

    void check() throws EJDB2Exception {
        // todo
    }

    @Override
    public String toString() {
        return new StringJoiner(", ", EJDB2Options.class.getSimpleName() + "[", "]")
                .add("no_wal=" + no_wal)
                .add("sort_buffer_sz=" + sort_buffer_sz)
                .add("document_buffer_sz=" + document_buffer_sz)
                .add("iwkv=" + iwkv)
                .add("http=" + http)
                .toString();
    }

    public static class EJDB2HttpOptions implements Serializable {

        private static final long serialVersionUID = 5957416367411081107L;

        boolean enabled;
        int port;
        String bind;
        String access_token;
        boolean read_anon;
        int max_body_size;

        public boolean isEnabled() {
            return enabled;
        }

        public int getPort() {
            return port;
        }

        public String getBind() {
            return bind;
        }

        public String getAccessToken() {
            return access_token;
        }

        boolean getReadAnon() {
            return read_anon;
        }

        int getMaxBodySize() {
            return max_body_size;
        }

        @Override
        public String toString() {
            return new StringJoiner(", ", EJDB2HttpOptions.class.getSimpleName() + "[", "]")
                    .add("enabled=" + enabled)
                    .add("port=" + port)
                    .add("bind='" + bind + "'")
                    .add("access_token='" + access_token + "'")
                    .add("read_anon=" + read_anon)
                    .add("max_body_size=" + max_body_size)
                    .toString();
        }
    }
}
