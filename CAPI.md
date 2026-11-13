# C API

EJDB2 can be embedded into any `C` or `C++` application. The public API is documented
in the following installed headers:

* [ejdb2.h](https://github.com/Softmotions/ejdb/blob/master/src/ejdb2.h.in) — storage,
  documents, collections, indexes, queries and query execution.
* [jql.h](https://github.com/Softmotions/ejdb/blob/master/src/jql/jql.h) — JQL query
  objects and placeholder binding.
* [jbr.h](https://github.com/Softmotions/ejdb/blob/master/src/jbr/jbr.h) — embedded
  HTTP/WebSocket server (requires a build with `EJDB_ENABLE_HTTP`).
* `iowow/iwjson.h` — JSON documents (`JBL`/`JBL_NODE`), provided by the
  [iowow](https://github.com/Softmotions/iowow) dependency.

The library is also available through language bindings (`src/bindings`: Node.js, JNI
and Android wrappers).

## Example

```c
#include <ejdb2/ejdb2.h>

#define CHECK(rc_)          \
  if (rc_) {                 \
    iwlog_ecode_error3(rc_); \
    return 1;                \
  }

static iwrc documents_visitor(struct ejdb_exec *ctx, struct ejdb_doc* const doc, int64_t *step) {
  (void) ctx;
  (void) step;
  // Print document to stderr
  return jbl_as_json(doc->raw, jbl_fstream_json_printer, stderr, JBL_PRINT_PRETTY);
}

int main(void) {

  struct ejdb_opts opts = {
    .kv = {
      .path = "example.db",
      .oflags = IWKV_TRUNC
    }
  };
  struct ejdb *db;   // EJDB2 storage handle
  int64_t id;        // Document id placeholder
  struct jql *q = 0; // Query instance
  JBL jbl = 0;       // Json document

  iwrc rc = ejdb_init();
  CHECK(rc);

  rc = ejdb_open(&opts, &db);
  CHECK(rc);

  // First record
  rc = jbl_from_json(&jbl, "{\"name\":\"Bianca\", \"age\":4}");
  RCGO(rc, finish);
  rc = ejdb_put_new(db, "parrots", jbl, &id);
  RCGO(rc, finish);
  jbl_destroy(&jbl);

  // Second record
  rc = jbl_from_json(&jbl, "{\"name\":\"Darko\", \"age\":8}");
  RCGO(rc, finish);
  rc = ejdb_put_new(db, "parrots", jbl, &id);
  RCGO(rc, finish);
  jbl_destroy(&jbl);

  // Now execute a query
  rc =  jql_create(&q, "parrots", "/[age > :age]");
  RCGO(rc, finish);

  struct ejdb_exec ux = {
    .db = db,
    .q = q,
    .visitor = documents_visitor
  };

  // Set query placeholder value.
  // Actual query will be /[age > 3]
  rc = jql_set_i64(q, "age", 0, 3);
  RCGO(rc, finish);

  // Now execute the query
  rc = ejdb_exec(&ux);

finish:
  jql_destroy(&q);
  jbl_destroy(&jbl);
  ejdb_close(&db);
  CHECK(rc);
  return 0;
}
```

Build and run. If the installed header enables the block-based API (`IW_BLOCKS`, which
is the case for libraries built with blocks support), a blocks-capable compiler is
required:

```sh
# Blocks enabled header (default for clang builds):
clang -std=gnu11 -fblocks -o example1 example1.c $(pkg-config --cflags --libs libejdb2)

# Otherwise:
cc -std=gnu11 -o example1 example1.c $(pkg-config --cflags --libs libejdb2)

./example1
{
 "name": "Darko",
 "age": 8
}{
 "name": "Bianca",
 "age": 4
}
```

For static linking add `--static` to `pkg-config --libs`; the query pulls in the
`libiwnet`/`libiowow`/`libbinnc` (and, with blocks, `BlocksRuntime`) dependencies.
Without `pkg-config` link with `-lejdb2`.

## Lifecycle

```c
iwrc rc = ejdb_init();                 // Once per process, before any other call
rc = ejdb_open(&opts, &db);            // Open (and optionally start the HTTP server)
...
rc = ejdb_close(&db);                  // Close and release everything
```

* `ejdb_init()` must be called once before using any EJDB2 API function.
* A storage file can be opened by one process at a time.
* A database handle can be used from multiple threads; conflicting operations are
  serialized internally per collection. All application threads must stop using the
  handle (and the `iowow` API) before `ejdb_close()` is called.
* `ejdb_open()` with `opts->http.enabled` starts the HTTP/WebSocket endpoint; see the
  [network API](https://github.com/Softmotions/ejdb/blob/master/src/jbr/README.md).

### Open options

`struct ejdb_opts`:

Field | Description
--- | ---
`kv` | `struct iwkv_opts` storage options: `path` (database file), `oflags` (for example `IWKV_TRUNC` to reset the file), `file_lock_fail_fast`, etc.
`http` | HTTP/WebSocket server options (`struct ejdb_http`), disabled by default.
`no_wal` | Do not use the write-ahead log. Default: `false`.
`sort_buffer_sz` | Max sorting buffer size; overflow is spilled to a temporary file. Default: 16 MiB, min: 1 MiB.
`document_buffer_sz` | Initial buffer size for documents processed during queries. Default: 64 KiB, min: 16 KiB.

The `struct ejdb_opts` object may be stack allocated and released after `ejdb_open()`.

## Errors

All functions return `iwrc` (`0` on success, a non-zero error code otherwise) or a
`void`/value result. Errors are structured codes from several namespaces:
`IW_ERROR_*`, `IWKV_ERROR_*`, `JBL_ERROR_*`, `EJDB_ERROR_*`, `JQL_ERROR_*`, `JBR_ERROR_*`.

```c
iwrc rc = ejdb_get(db, "parrots", 1, &jbl);
if (rc) {
  iwlog_ecode_error3(rc);        // Log the error with its symbolic name
}
```

Helpers from `iowow` simplify error propagation: `RCC` (assign and jump on error),
`RCGO` (jump on error), `RCRET` (return on error), `RCB`/`RCA` (jump with
`IW_ERROR_ALLOC` when an allocation returns zero). `iwrc_strip_code()` removes the
embedded `errno` part of a code when a specific error constant has to be compared.

Frequent errors:

Constant | Meaning
--- | ---
`IWKV_ERROR_NOTFOUND` | Document or key not found
`IW_ERROR_NOT_EXISTS` | Collection does not exist
`EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED` | Unique index would be violated
`EJDB_ERROR_COLLECTION_NOT_FOUND` | Collection not found (`ejdb_rename_collection`)
`EJDB_ERROR_TARGET_COLLECTION_EXISTS` | Target collection already exists
`JQL_ERROR_QUERY_PARSE` | JQL query syntax error
`JQL_ERROR_NO_COLLECTION` | Query has no `@collection` anchor and no collection was given
`JQL_ERROR_UNSET_PLACEHOLDER` | A query placeholder was not bound

## Documents

Documents are represented by `JBL` (a compact binary JSON document) or `JBL_NODE`
(an in-memory tree of JSON values). Both are created from JSON text and serialized
back to JSON; helpers come from `iowow/iwjson.h`.

```c
JBL jbl = 0;
iwrc rc = jbl_from_json(&jbl, "{\"name\":\"Bianca\",\"age\":4}");
...
jbl_destroy(&jbl);      // No-op when the handle is zero
```

Function | Description
--- | ---
`jbl_from_json(&jbl, json)` | Parse a JSON text into `JBL`
`jbl_as_json(jbl, printer, op, flags)` | Serialize; printers: `jbl_xstr_json_printer`, `jbl_fstream_json_printer`, `jbl_count_json_printer`; flags: `JBL_PRINT_PRETTY`
`jbl_destroy(&jbl)` | Release a document
`jbn_*` | In-memory tree manipulation, see `iwjson.h`

### Persisting documents

Function | Description
--- | ---
`ejdb_put(db, coll, jbl, id)` | Store a document under the given identifier (replaces an existing one)
`ejdb_put_jbn(db, coll, jbn, id)` | Same for `JBL_NODE`
`ejdb_put_new(db, coll, jbl, &id)` | Store a document under a new auto-generated identifier
`ejdb_put_new_jbn(db, coll, jbn, &id)` | Same for `JBL_NODE`
`ejdb_get(db, coll, id, &jbl)` | Fetch a document; the result must be released with `jbl_destroy()`
`ejdb_del(db, coll, id)` | Remove a document
`ejdb_patch(db, coll, patchjson, id)` | Apply an RFC 7396/6902 patch to a stored document
`ejdb_patch_jbl`, `ejdb_patch_jbn` | Same with a `JBL`/`JBL_NODE` patch
`ejdb_merge_or_put(db, coll, patchjson, id)` | Atomically merge-patch an existing document or insert a new one
`ejdb_merge_or_put_jbl`, `ejdb_merge_or_put_jbn` | Same with a `JBL`/`JBL_NODE` patch

Collections are created implicitly by the first write operation.

## Collections and indexes

Function | Description
--- | ---
`ejdb_ensure_collection(db, coll)` | Create a collection if it does not exist
`ejdb_remove_collection(db, coll)` | Remove a collection; no error if it is missing
`ejdb_rename_collection(db, coll, new_coll)` | Rename a collection
`ejdb_ensure_index(db, coll, path, mode)` | Create an index if it does not exist
`ejdb_remove_index(db, coll, path, mode)` | Remove an index; `0` if the index does not exist, `IW_ERROR_NOT_EXISTS` if the collection does not exist
`ejdb_get_meta(db, &jbl)` | Database metadata as a JSON document

Index mode bits (`ejdb_idx_mode_t`):

Mode | Description
--- | ---
`EJDB_IDX_UNIQUE` (`0x01`) | Values must be unique
`EJDB_IDX_STR` (`0x04`) | Index for string values
`EJDB_IDX_I64` (`0x08`) | Index for 64 bit signed integer values
`EJDB_IDX_F64` (`0x10`) | Index for floating point values

An index path is an RFC 6901 JSON pointer to a concrete field, for example
`/address/street`; wildcards in the middle of a path are not allowed. A path can
point to an array field to index its elements.

```c
// Unique string index over nested field
rc = ejdb_ensure_index(db, "users", "/address/street", EJDB_IDX_UNIQUE | EJDB_IDX_STR);
```

## Queries

Queries are created with the JQL API (`jql.h`), optionally bound to placeholder values,
and executed with `ejdb_exec()`.

```c
struct jql *q = 0;
iwrc rc = jql_create(&q, "parrots", "/[age > :age]");
if (!rc) {
  rc = jql_set_i64(q, "age", 0, 3);      // Bind the `:age` placeholder
}
...
jql_destroy(&q);
```

Function | Description
--- | ---
`jql_create(&q, coll, query)` | Parse a query; `coll` may be zero if the query has an `@collection` anchor
`jql_create2(&q, coll, query, mode)` | Same with `JQL_KEEP_QUERY_ON_PARSE_ERROR`/`JQL_SILENT_ON_PARSE_ERROR`
`jql_error(q)` | Parse error text (with a `<---` marker) for a kept query
`jql_set_i64/f64/str/bool/null/json/regexp(...)` | Bind a placeholder value
`jql_set_*2(...)` | Variants with an ownership callback for the bound value
`jql_reset(q, reset_cache, reset_placeholders)` | Reuse a query object for the next invocation
`jql_destroy(&q)` | Release the query object

Placeholders are written `:name` or `:?`. `:?` placeholders are bound by index
(`jql_set_*(q, 0, index, value)`), named placeholders by name.

### Executing a query

`ejdb_exec()` streams matched documents to a visitor:

```c
typedef iwrc (*ejdb_exec_visitor)(struct ejdb_exec *ctx, struct ejdb_doc *doc, int64_t *step);

struct ejdb_exec ux = {
  .db = db,           // Database handle
  .q = q,             // Query object
  .visitor = visitor, // Called for each matched document
  .opaque = my_ctx,   // Optional visitor context
  .skip = 0,          // Overrides `skip` encoded in the query
  .limit = 0,         // Overrides `limit` encoded in the query; 0 means no limit
  .log = 0,           // Optional `IWXSTR` that receives the execution plan/index usage
  .cnt = 0            // Out: number of documents passed to the visitor
};
iwrc rc = ejdb_exec(&ux);
```

Visitor contract:

* `doc->raw` is the stored document; `doc->node` is set only when the query has an
  `apply` or projection section.
* `doc` and its data are valid only during the callback; copy what must outlive it.
* Set `*step` to `1` (default) to advance to the next document, `0` to stop iteration,
  or another value to move the cursor by that number of documents (used for advanced
  iteration over sorted results).
* `ejdb_exec()` applies modifications (`apply`/`upsert`/`del`) to the storage.

Collecting results into a list instead of streaming:

Function | Description
--- | ---
`ejdb_list(db, q, &first, limit, pool)` | Fill a linked list of documents using a caller-provided `iwpool` (not zero); the caller owns and destroys the pool
`ejdb_list2(db, coll, query, limit, &list)` | Same from a query text
`ejdb_list3(db, coll, query, limit, log, &list)` | Same with an execution plan buffer
`ejdb_list4(db, q, limit, log, &list)` | Same from a query object
`ejdb_list_destroy(&list)` | Release a result list

```c
struct ejdb_list *list = 0;
iwrc rc = ejdb_list3(db, "parrots", "/[age > 3]", 0 /* limit */, 0 /* optional IWXSTR log */, &list);
if (!rc) {
  for (struct ejdb_doc *doc = list->first; doc; doc = doc->next) {
    jbl_as_json(doc->raw, jbl_fstream_json_printer, stderr, JBL_PRINT_PRETTY);
  }
  ejdb_list_destroy(&list);
}
```

Collecting a whole result set can be memory consuming for large collections; prefer
`ejdb_exec()` with a visitor, or set a limit.

### Count and update helpers

Function | Description
--- | ---
`ejdb_count(db, q, &count, limit)` | Execute a query and return the number of matched documents
`ejdb_count2(db, coll, query, &count, limit)` | Same from a query text
`ejdb_update(db, q)` | Execute an update query (with an `apply` section)
`ejdb_update2(db, coll, query)` | Same from a query text

## Storage maintenance

Function | Description
--- | ---
`ejdb_online_backup(db, &ts, target_file)` | Consistent online backup; all records written before `ts` are included. Close open cursors or run it in a separate thread to avoid deadlocks
`ejdb_get_iwkv(db, &kv)` | Access the underlying IWKV storage (advanced use)
`ejdb_version_full()`, `ejdb_version_major()`, `ejdb_version_minor()`, `ejdb_version_patch()` | Library version
`ejdb_git_revision()` | Source revision the library was built from

## Thread safety and lifetime summary

Object | Ownership
--- | ---
`struct ejdb *` | Owned by the caller; released by `ejdb_close()`; shareable between threads
`JBL` | Owned by the caller; released by `jbl_destroy()`; returned by `ejdb_get()`/`ejdb_get_meta()`
`struct jql *` | Owned by the caller; released by `jql_destroy()`
`struct ejdb_list *` | Owned by the caller; released by `ejdb_list_destroy()`
`struct ejdb_doc` (visitor) | Valid only during the visitor callback
`struct ejdb_exec.pool` | Optional memory pool for apply/projection nodes owned by the caller
