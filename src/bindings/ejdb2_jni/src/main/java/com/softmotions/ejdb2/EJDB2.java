package com.softmotions.ejdb2;

import java.io.ByteArrayOutputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

/**
 * EJDB2 JNI Wrapper.
 *
 * Database opened by {@link EJDB2Builder#open()} helper.
 *
 * Example:
 *
 * <pre>
 * {@code
 *  try (EJDB2 db = new EJDB2Builder("my.db").open() {
 *    ...
 *  }
 * }
 * </pre>
 *
 * In order to release memory resources and avoiding data lost every opened
 * database instance should be closed with {@link EJDB2#close()}.
 *
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public final class EJDB2 implements AutoCloseable {

  static {
    System.loadLibrary("ejdb2jni");
  }

  @SuppressWarnings("unused")
  private long _handle;

  private final EJDB2Builder options;

  /**
   * Returns options used to build this database instance.
   *
   * @return
   */
  public EJDB2Builder getOptions() {
    return options;
  }

  EJDB2(EJDB2Builder options) throws EJDB2Exception {
    this.options = options;
    _open(options);
  }

  /**
   * Closes database instance and releases all resources.
   */
  @Override
  public void close() {
    _dispose();
  }

  /**
   * Create a query instance. Query can be reused multiple times with various
   * placeholder parameters. See JQL specification:
   * https://github.com/Softmotions/ejdb/blob/master/README.md#jql
   * <p>
   * Note: collection name must be encoded in query, eg: {@code @mycoll/[foo=bar]}
   * </p>
   */
  public JQL createQuery(String query) throws EJDB2Exception {
    return new JQL(this, query, null);
  }

  /**
   * Create a query instance. Query can be reused multiple times with various
   * placeholder parameters. See JQL specification:
   * https://github.com/Softmotions/ejdb/blob/master/README.md#jql
   *
   * If {@code collection} is not null it will be used for query. In this case
s   * collection name encoded in query will not be taken into account.
   *
   * @param collection Optional collection name
   */
  public JQL createQuery(String query, String collection) throws EJDB2Exception {
    return new JQL(this, query, collection);
  }

  /**
   * Persists {@code json} document into {@code collection}.
   *
   * @param collection Collection name
   * @param json       JSON document
   * @return Generated identifier for document
   * @throws EJDB2Exception
   */
  public long put(String collection, String json) throws EJDB2Exception {
    return _put(collection, json, 0);
  }

  /**
   * Persists {@code json} document under specified {@code id}.
   * <p>
   * If {@code id} is zero a new document identifier will be generated.
   *
   * @param collection Collection name
   * @param json       JSON document
   * @param id         Document id. If zero a new identifier will be genareted
   * @return Document identifier
   * @throws EJDB2Exception
   */
  public long put(String collection, String json, long id) throws EJDB2Exception {
    return _put(collection, json, id);
  }

  /**
   * Removes a document identified by given {@code id} from collection
   * {@code coll}.
   * <p>
   * If document is not found {@link EJDB2Exception} will be thrown:
   *
   * <pre>
   * {@code
   *  code: 75001
   *  message: Key not found. (IWKV_ERROR_NOTFOUND)
   * }
   * </pre>
   *
   * @param collection Collection name
   * @param id         Document id
   * @throws EJDB2Exception
   */
  public void del(String collection, long id) throws EJDB2Exception {
    _del(collection, id);
  }

  /**
   * Apply rfc6902/rfc6901 JSON patch to the document identified by {@code id}.
   *
   * @param collection Collection name
   * @param patch      JSON patch
   * @param id         Document id
   * @throws EJDB2Exception
   */
  public void patch(String collection, String patch, long id) throws EJDB2Exception {
    _patch(collection, patch, id);
  }

  /**
   * Write document identified by {@code id} into {@code out}.
   * <p>
   * If document is not found {@link EJDB2Exception} will be thrown:
   *
   * <pre>
   * {@code
   *  code: 75001
   *  message: Key not found. (IWKV_ERROR_NOTFOUND)
   * }
   * </pre>
   *
   * @param collection
   * @param id
   * @param out
   */
  public void get(String collection, long id, OutputStream out) {
    _get(collection, id, out, false);
  }

  /**
   * Same as {@link #get(String, long, OutputStream)} but returns prettified JSON
   * document.
   *
   * @param collection Collection name
   * @param id         Document id
   * @param out        Output stream to write
   */
  public void getPrettified(String collection, long id, OutputStream out) {
    _get(collection, id, out, true);
  }

  /**
   * Returns JSON document describind database structure.
   * <p>
   * Example database metadata:
   *
   * <pre>
   * {@code
   *   {
   *    "version": "2.0.0", // EJDB engine version
   *    "file": "db.jb",    // Path to storage file
   *    "size": 16384,      // Storage file size in bytes
   *    "collections": [    // List of collections
   *     {
   *      "name": "c1",     // Collection name
   *      "dbid": 3,        // Collection database ID
   *      "rnum": 2,        // Number of documents in collection
   *      "indexes": [      // List of collections indexes
   *       {
   *        "ptr": "/n",    // rfc6901 JSON pointer to indexed field
   *        "mode": 8,      // Index mode. Here is EJDB_IDX_I64
   *        "idbf": 96,     // Index database flags. See iwdb_flags_t
   *        "dbid": 4,      // Index database ID
   *        "rnum": 2       // Number records stored in index database
   *       }
   *      ]
   *     }
   *    ]
   *   }
   * }
   * </pre>
   *
   * @param out Output stream to write
   * @throws EJDB2Exception
   */
  public void info(OutputStream out) throws EJDB2Exception {
    _info(out);
  }

  /**
   * Returns JSON document describind database structure. Same as
   * {@link #info(OutputStream)} but returns JSON data as string.
   *
   * @throws EJDB2Exception
   */
  public String infoAsString() throws EJDB2Exception {
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    info(bos);
    return bos.toString(StandardCharsets.UTF_8);
  }

  /**
   * Removes {@code collection} from database and all its documents.
   *
   * @param collection Collection name
   * @return Current database instance
   * @throws EJDB2Exception
   */
  public EJDB2 removeCollection(String collection) throws EJDB2Exception {
    _remove_collection(collection);
    return this;
  }

  /**
   * Create string index with specified parameters if it has not existed before.
   * <p>
   * Where {@code path} must be fully specified as rfc6901 JSON pointer.
   * <p>
   * Example document:
   *
   * <pre>
   * {@code
   *    "address" : {
   *      "street": "High Street"
   *    }
   * }
   * </pre>
   *
   * Call {@code ensureStringIndex("mycoll", "/address/street", true)} in order to
   * create unique index over all street names in nested address object.
   *
   * @param collection Collection name
   * @param path       JSON pointer path to indexed field
   * @param unique     If {@code true} an unique index will be created
   * @return Current database instance
   * @throws EJDB2Exception
   */
  public EJDB2 ensureStringIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _ensure_index(collection, path, 0x04 | (unique ? 0x01 : 0));
    return this;
  }

  /**
   * Removes collection index for JSON document field pointed by {@code path}
   *
   * @param collection Collection name
   * @param path       JSON pointer path to indexed field
   * @param unique     {@code true} for unique index
   * @return
   * @throws EJDB2Exception
   */
  public EJDB2 removeStringIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _remove_index(collection, path, 0x04 | (unique ? 0x01 : 0));
    return this;
  }

  /**
   * Create integer index with specified parameters if it has not existed before.
   *
   * @param collection Collection name
   * @param path       JSON pointer path to indexed field
   * @param unique     {@code true} for unique index
   * @return
   * @throws EJDB2Exception
   */
  public EJDB2 ensureIntIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _ensure_index(collection, path, 0x08 | (unique ? 0x01 : 0));
    return this;
  }

  /**
   * Removes collection index for JSON document field pointed by {@code path}
   *
   * @param collection Collection name
   * @param path       JSON pointer path to indexed field
   * @param unique     {@code true} for unique index
   * @return
   * @throws EJDB2Exception
   */
  public EJDB2 removeIntIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _remove_index(collection, path, 0x08 | (unique ? 0x01 : 0));
    return this;
  }

  /**
   * Create floating point number index with specified parameters if it has not
   * existed before.
   *
   * @param collection Collection name
   * @param path       JSON pointer path to indexed field
   * @param unique     {@code true} for unique index
   * @return
   * @throws EJDB2Exception
   */
  public EJDB2 ensureFloatIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _ensure_index(collection, path, 0x10 | (unique ? 0x01 : 0));
    return this;
  }

  public EJDB2 removeFloatIndex(String collection, String path, boolean unique) throws EJDB2Exception {
    _remove_index(collection, path, 0x10 | (unique ? 0x01 : 0));
    return this;
  }

  private native void _open(EJDB2Builder opts) throws EJDB2Exception;

  private native void _dispose() throws EJDB2Exception;

  private native long _put(String collection, String json, long id) throws EJDB2Exception;

  private native void _del(String collection, long id) throws EJDB2Exception;

  private native void _patch(String collection, String patch, long id) throws EJDB2Exception;

  private native void _get(String collection, long id, OutputStream out, boolean pretty) throws EJDB2Exception;

  private native void _info(OutputStream out) throws EJDB2Exception;

  private native void _remove_collection(String collection) throws EJDB2Exception;

  private native void _ensure_index(String collection, String path, int mode) throws EJDB2Exception;

  private native void _remove_index(String collection, String path, int mode) throws EJDB2Exception;
}
