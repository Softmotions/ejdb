# C API

EJDB2 is an embeddable JSON database: an application links `libejdb2`, opens one or
more database files and works with JSON documents, collections, indexes and queries
through a plain C11 API. No server process is required (an embedded HTTP/WebSocket
endpoint is optional).

The public API is documented in the installed headers:

* [ejdb2.h](https://github.com/Softmotions/ejdb/blob/master/src/ejdb2.h.in) — storage,
  documents, collections, indexes, queries and query execution.
* [jql.h](https://github.com/Softmotions/ejdb/blob/master/src/jql/jql.h) — JQL query
  objects and placeholder binding.
* [jbr.h](https://github.com/Softmotions/ejdb/blob/master/src/jbr/jbr.h) — embedded
  HTTP/WebSocket server (requires a build with `EJDB_ENABLE_HTTP`).
* `iowow/iwjbl.h`, `iowow/iwjbn.h`, `iowow/iwjson.h` — JSON documents (`struct jbl`,
  `struct jbl_node`), provided by the [iowow](https://github.com/Softmotions/iowow)
  dependency.
* `iowow/iwkv.h`, `iowow/iwpool.h`, `iowow/iwxstr.h`, `iowow/iwlog.h` — storage,
  memory pools, string buffers and logging primitives used by the public API.

The library is also available through language bindings (`src/bindings`: Node.js, JNI
and Android wrappers).

Task | Start with
--- | ---
Open/close a database | `ejdb_init()`, `ejdb_open()`, `ejdb_close()`
Store and fetch documents | `jbl_from_json()`, `ejdb_put_new()`, `ejdb_get()`
Run a query | `jql_create()`, `ejdb_exec()`, `jql_destroy()`
Run a query and get a list | `ejdb_list2()` / `ejdb_list3()`, `ejdb_list_destroy()`
Count or update matched documents | `ejdb_count()`, `ejdb_update()`
Index a field | `ejdb_ensure_index()`
Inspect the query plan | `struct ejdb_exec.log`

## Quick start

```c
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <ejdb2/ejdb2.h>

#define CHECK(rc_)          \
  if (rc_) {                \
    iwlog_ecode_error3(rc_);\
    return 1;               \
  }

static iwrc documents_visitor(struct ejdb_exec *ctx, struct ejdb_doc *doc, int64_t *step) {
  (void) ctx;
  (void) step;
  printf("%" PRId64 " ", doc->id);
  iwrc rc = jbl_as_json(doc->raw, jbl_fstream_json_printer, stdout, 0);
  fputc('\n', stdout);
  return rc;
}

int main(void) {

  struct ejdb_opts opts = {
    .kv = {
      .path = "example.db",
      .oflags = IWKV_TRUNC
    }
  };
  struct ejdb *db = 0;   // EJDB2 storage handle
  struct jql *q = 0;     // Query instance
  struct jbl *jbl = 0;   // JSON document
  int64_t id = 0;        // Document id placeholder

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

  // Query over the collection with an unbound `:age` placeholder
  rc = jql_create(&q, "parrots", "/[age > :age]");
  RCGO(rc, finish);

  rc = jql_set_i64(q, "age", 0, 3);  // Actual query: /[age > 3]
  RCGO(rc, finish);

  struct ejdb_exec ux = {
    .db = db,
    .q = q,
    .visitor = documents_visitor
  };

  rc = ejdb_exec(&ux);

finish:
  jql_destroy(&q);
  jbl_destroy(&jbl);
  ejdb_close(&db);
  CHECK(rc);
  return 0;
}
```

Build and run:

```sh
# The installed header defines IW_BLOCKS when the library was built with blocks
# support; a blocks-capable compiler (clang) is required in that case:
clang -std=gnu11 -fblocks -o example1 example1.c $(pkg-config --cflags --libs libejdb2)

# Otherwise:
cc -std=gnu11 -o example1 example1.c $(pkg-config --cflags --libs libejdb2)

./example1
2 {"name":"Darko","age":8}
1 {"name":"Bianca","age":4}
```

The `1`/`2` prefixes are document identifiers (`doc->id`). Without an `asc`/`desc`
option a full scan visits documents in descending primary key order (see
[JQL performance tips](https://github.com/Softmotions/ejdb/blob/master/src/jql/README.md#performance-tip-physical-ordering-of-documents)),
which is why `Darko` comes first.

The examples below assume that `<stdio.h>`, `<stdlib.h>`, `<string.h>`,
`<inttypes.h>` and `<iowow/iwxstr.h>` are included where used.

## Building and linking

* `#include <ejdb2/ejdb2.h>` — the header is installed into
  `${prefix}/include/ejdb2` by `./build.sh --prefix=...`.
* `pkg-config --cflags --libs libejdb2` gives the compile and link flags.
* Static linking: add `--static` to `pkg-config --libs`. That pulls in
  `libiwnet`/`libiowow`/`libbinnc` and, with blocks support, `BlocksRuntime`:

  ```sh
  clang -std=gnu11 -fblocks example1.c \
    $(pkg-config --cflags --libs --static libejdb2) -lBlocksRuntime
  ```

* Without `pkg-config` link with `-lejdb2`.
* Header-only aliases such as `EJDB`, `EJDB_DOC`, `JBL`, `JQL` still exist, but new
  code may use the plain `struct` names. This document uses the plain names.

## Lifecycle

```c
iwrc rc = ejdb_init();                 // Once per process, before any other call
rc = ejdb_open(&opts, &db);            // Open (and optionally start the HTTP server)
...
rc = ejdb_close(&db);                  // Close and release everything
```

* `ejdb_init()` registers the error code namespaces. It is idempotent and is also
  called implicitly by `ejdb_open()`, but call it explicitly before using any other
  API function.
* A storage file can be opened by one process at a time. A second `ejdb_open()` of the
  same file blocks until the first handle is closed (see `kv.file_lock_fail_fast`).
* A database handle can be used from multiple threads. Conflicting operations are
  serialized internally with a read/write lock per collection.
* All application threads must stop using the handle (and the `iowow` API) before
  `ejdb_close()` is called. `ejdb_close()` sets the handle to zero.

### Open options

`struct ejdb_opts`:

Field | Description
--- | ---
`kv` | `struct iwkv_opts` storage options: `path` (database file, required), `oflags` (`IWKV_TRUNC` resets the file, `IWKV_RDONLY` opens read-only), `file_lock_fail_fast` (fail with an errno-class error instead of waiting for another process), `random_seed`, `fmt_version`, `wal`
`http` | HTTP/WebSocket server options (`struct ejdb_http`), disabled by default
`no_wal` | Do not use the write-ahead log. Default: `false`
`sort_buffer_sz` | Max sorting buffer size; overflow is spilled to a temporary file. Default: 16 MiB, min: 1 MiB
`document_buffer_sz` | Initial buffer size for documents processed during queries. Default: 64 KiB, min: 16 KiB

`struct ejdb_opts` may be stack allocated and released after `ejdb_open()`; the
library keeps its own copy of the structure.

```c
// Read-only access to an existing database
struct ejdb_opts opts = {
  .kv = {
    .path = "example.db",
    .oflags = IWKV_RDONLY,
    .file_lock_fail_fast = true
  }
};
```

A read-only database cannot be modified: write calls fail, and writes to a collection
that does not exist report `IW_ERROR_NOT_EXISTS`.

### Embedded HTTP/WebSocket endpoint

Enabling `opts.http` starts the same REST/WebSocket service that the `jbs` tool
provides. See the [embedded HTTP server](https://github.com/Softmotions/ejdb/blob/master/src/jbr/README.md#embedded-http-server)
section for the full option list; `ejdb_close()` stops the server and waits for it to
finish.

```c
struct ejdb_opts opts = {
  .kv = { .path = "example.db" },
  .http = {
    .enabled = true,
    .port = 9191,
    .access_token = "myaccess01",
    .read_anon = true,
    .blocking = false  // Serve in a background thread
  }
};
```

## Errors

All functions return `iwrc` (`0` on success, a non-zero error code otherwise) or a
`void`/value result. Errors are structured codes from several namespaces:
`IW_ERROR_*`, `IWKV_ERROR_*`, `JBL_ERROR_*`, `EJDB_ERROR_*`, `JQL_ERROR_*`, `JBR_ERROR_*`.

### Reporting and comparing errors

```c
iwrc rc = ejdb_get(db, "parrots", 1, &jbl);
if (rc) {
  // "Key not found. (IWKV_ERROR_NOTFOUND)"
  fprintf(stderr, "%s\n", iwlog_ecode_explained(rc));
  iwlog_ecode_error3(rc);  // Log with file/line and the symbolic code name
}
```

`iwlog_ecode_explained()` returns a human readable description of a code. Some codes
carry an embedded `errno` part; strip it with `iwrc_strip_code(&rc)` before comparing a
code with a specific constant:

```c
iwrc_strip_code(&rc);
if (rc == IWKV_ERROR_NOTFOUND) {
  // No such document
}
```

### Propagation macros

Helpers from `iowow` keep error handling out of the way:

Macro | Effect
--- | ---
`RCRET(rc)` | Return `rc` from the current function when it is non-zero
`RCGO(rc, label)` | Jump to `label` when `rc` is non-zero
`RCC(rc, label, expr)` | Assign `expr` to `rc` and jump when it is non-zero
`RCB(label, ptr)` | Jump when an allocation returned zero, setting `IW_ERROR_ALLOC`
`RCA(ptr, label)` | Same as `RCB` for an assignment expression
`RCBREAK(rc)` | Break out of a loop when `rc` is non-zero
`IWRC(expr, rc)` | Run `expr`, keep the first error in `rc` and log the rest
`WUR` | Function attribute marking the result as "must be used"

A typical helper ends with a single cleanup label:

```c
static iwrc store_parrot(struct ejdb *db, const char *json, int64_t *id) {
  struct jbl *jbl = 0;
  iwrc rc = jbl_from_json(&jbl, json);
  RCRET(rc);
  rc = ejdb_put_new(db, "parrots", jbl, id);
  jbl_destroy(&jbl);
  return rc;
}
```

*Always check the result of `jbl_*`/`jbn_*` creators before using the handle*: on error
the output handle is zero.

### Frequent errors

Constant | Meaning
--- | ---
`IWKV_ERROR_NOTFOUND` | Document or key not found
`IW_ERROR_NOT_EXISTS` | Collection does not exist
`IW_ERROR_INVALID_ARGS` | Required argument is zero or out of range
`EJDB_ERROR_INVALID_COLLECTION_NAME` | Empty name or longer than `EJDB_COLLECTION_NAME_MAX_LEN` (255)
`EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED` | Unique index would be violated
`EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE` | Index exists with a different uniqueness mode
`EJDB_ERROR_INVALID_INDEX_MODE` | Invalid index mode bits
`EJDB_ERROR_COLLECTION_NOT_FOUND` | Collection not found (`ejdb_rename_collection`)
`EJDB_ERROR_TARGET_COLLECTION_EXISTS` | Target collection already exists
`EJDB_ERROR_PATCH_JSON_NOT_OBJECT` | Merge patch for an absent document is not a JSON object
`JQL_ERROR_QUERY_PARSE` | JQL query syntax error
`JQL_ERROR_NO_COLLECTION` | Query has no `@collection` anchor and no collection was given
`JQL_ERROR_UNSET_PLACEHOLDER` | A query placeholder was not bound
`JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE` | Bound value type is not allowed here
`JQL_ERROR_REGEXP_INVALID` | Invalid regular expression in `jql_set_regexp()`

## Documents

A document is either a `struct jbl` (compact binary JSON, the storage format) or a
`struct jbl_node` (an in-memory tree of JSON values used to build and modify
documents). Both are converted from and to JSON text.

Type | Purpose | Lifetime
--- | --- | ---
`struct jbl` | Storage format; returned by `ejdb_get()`, `ejdb_list*()` | Released by `jbl_destroy()`
`struct jbl_node` | Mutable tree used by `apply`/projections and JSON builders | Released with its `struct iwpool` (or manually when `pool` is zero)

Function | Description
--- | ---
`jbl_from_json(&jbl, json)` | Parse a JSON text into `struct jbl`
`jbl_from_json_printf(&jbl, fmt, ...)` | Same with a `printf()` style format string
`jbn_from_json(json, &node, pool)` | Parse into a `struct jbl_node` allocated in `pool`
`jbl_to_node(jbl, &node, clone_strings, pool)` | Convert `jbl` into a node tree
`jbl_from_node(&jbl, node)` | Build a `struct jbl` from an object/array node
`jbl_fill_from_node(jbl, node)` | Refill an existing `struct jbl` from a node
`jbl_as_json(jbl, printer, op, flags)` | Serialize with a printer callback
`jbn_as_json(node, printer, op, flags)` | Serialize a node tree
`jbl_as_json_alloc(jbl, flags, &str)` | Serialize into a `malloc()`ed C string
`jbn_as_json_alloc(node, flags, &str)` | Same for a node tree
`jbl_destroy(&jbl)` | Release a document (no-op on a zero handle)

Printers: `jbl_fstream_json_printer` (`FILE *`), `jbl_xstr_json_printer`
(`struct iwxstr *`), `jbl_count_json_printer` (byte counter). Flags:
`JBL_PRINT_PRETTY`, `JBL_PRINT_PRETTY_INDENT2`, `JBL_PRINT_PRETTY_INDENT4`,
`JBL_PRINT_CODEPOINTS`.

Values are read either with typed accessors (`jbl_type()`, `jbl_get_i64()`,
`jbl_get_f64()`, `jbl_get_str()`, `jbl_get_str2()`) or with an RFC 6901 JSON pointer:

```c
struct jbl *age = 0;
iwrc rc = jbl_at(doc->raw, "/age", &age);
if (!rc && age) {   // `age` is zero when the path does not exist
  printf("age=%" PRId64 "\n", jbl_get_i64(age));
}
jbl_destroy(&age);  // Zero handles are ignored
```

Serializing a document into a C string (convenient for logging or IPC) — the result
must be released with `free()`:

```c
char *json = 0;
iwrc rc = jbl_as_json_alloc(jbl, JBL_PRINT_PRETTY, &json);
if (!rc) {
  puts(json);
  free(json);
}
```

### Persisting documents

Function | Description
--- | ---
`ejdb_put(db, coll, jbl, id)` | Store a document under the given identifier (replaces an existing one)
`ejdb_put_jbn(db, coll, jbn, id)` | Same for `struct jbl_node`
`ejdb_put_new(db, coll, jbl, &id)` | Store a document under a new auto-generated identifier
`ejdb_put_new_jbn(db, coll, jbn, &id)` | Same for `struct jbl_node`
`ejdb_get(db, coll, id, &jbl)` | Fetch a document; the result must be released with `jbl_destroy()`
`ejdb_del(db, coll, id)` | Remove a document
`ejdb_patch(db, coll, patchjson, id)` | Apply an RFC 7396/6902 patch to a stored document
`ejdb_patch_jbl`, `ejdb_patch_jbn` | Same with a `struct jbl`/`struct jbl_node` patch
`ejdb_merge_or_put(db, coll, patchjson, id)` | Atomically merge-patch an existing document or insert a new one
`ejdb_merge_or_put_jbl`, `ejdb_merge_or_put_jbn` | Same with a `struct jbl`/`struct jbl_node` patch

```c
struct jbl *jbl = 0;
int64_t id = 0;
iwrc rc = jbl_from_json(&jbl, "{\"name\":\"Bianca\",\"age\":4}");
RCC(rc, finish, ejdb_put_new(db, "parrots", jbl, &id));
jbl_destroy(&jbl);

// ... later, read it back
RCC(rc, finish, ejdb_get(db, "parrots", id, &jbl));

// ... and remove it
RCC(rc, finish, ejdb_del(db, "parrots", id));

finish:
  jbl_destroy(&jbl);
```

Tips:

* Documents are not retained by `ejdb_put*()`: the call serializes the document into
  the storage, so the `struct jbl` may be released immediately afterwards.
* Collections are created implicitly by the first write operation.
* `ejdb_get()` returns a document over a buffer owned by the returned `struct jbl`;
  release it with `jbl_destroy()`.
* Document identifiers are positive 64-bit integers. `ejdb_put_new()` allocates the
  next identifier of the collection; identifiers of deleted documents are not reused.
* `0` is not a valid document identifier. `ejdb_get(db, coll, 0, &jbl)` fails with
  `IW_ERROR_INVALID_ARGS`.

### Patching documents

`ejdb_patch()` accepts both patch flavours: a JSON array is an RFC 6902 JSON patch, a
JSON object is an RFC 7396 merge patch. `ejdb_merge_or_put()` additionally inserts the
patch as a new document when the identifier does not exist yet (atomically).

```c
// RFC 7396 merge patch: set `age`, delete `nick` (null removes the key), add `tags`
rc = ejdb_patch(db, "parrots",
                "{\"age\":5,\"nick\":null,\"tags\":[\"dev\"]}", id);

// RFC 6902 JSON patch: append to the `tags` array (`-` means the end of an array)
rc = ejdb_patch(db, "parrots",
                "[{\"op\":\"add\",\"path\":\"/tags/-\",\"value\":\"home\"}]", id);

// Merge patch, or insert {"name":"Ghost","age":1} under id 42
rc = ejdb_merge_or_put(db, "parrots", "{\"name\":\"Ghost\",\"age\":1}", 42);
```

Tips:

* An RFC 6902 `add` requires the parent container to exist (`/tags` above). The
  non-standard `add_create` op creates missing intermediate objects — see
  [JSON patch extensions](https://github.com/Softmotions/ejdb/blob/master/src/jql/README.md#non-standard-json-patch-extensions).
* Patching a missing document returns `IWKV_ERROR_NOTFOUND`; RFC 6902 `test`
  failures are reported as `JBL_ERROR_PATCH_TEST_FAILED`.
* `ejdb_merge_or_put()` fails with `EJDB_ERROR_PATCH_JSON_NOT_OBJECT` if the inserted
  patch is not a JSON object.

## Collections and indexes

Function | Description
--- | ---
`ejdb_ensure_collection(db, coll)` | Create a collection if it does not exist
`ejdb_remove_collection(db, coll)` | Remove a collection; no error if it is missing
`ejdb_rename_collection(db, coll, new_coll)` | Rename a collection
`ejdb_ensure_index(db, coll, path, mode)` | Create an index if it does not exist
`ejdb_remove_index(db, coll, path, mode)` | Remove an index; `0` if the index does not exist, `IW_ERROR_NOT_EXISTS` if the collection does not exist
`ejdb_get_meta(db, &jbl)` | Database metadata as a JSON document

Collection names are non-empty strings of at most `EJDB_COLLECTION_NAME_MAX_LEN` (255)
bytes.

Index mode bits (`ejdb_idx_mode_t`); exactly one of the type bits is required:

Mode | Description
--- | ---
`EJDB_IDX_UNIQUE` (`0x01`) | Values must be unique
`EJDB_IDX_STR` (`0x04`) | Index for string values
`EJDB_IDX_I64` (`0x08`) | Index for 64 bit signed integer values
`EJDB_IDX_F64` (`0x10`) | Index for floating point values

An index path is an RFC 6901 JSON pointer to a concrete field, for example
`/address/street`. Wildcards in the middle of a path are not allowed. A path can point
to an array field to index each of its elements.

```c
// Unique string index over a nested field
rc = ejdb_ensure_index(db, "users", "/address/street", EJDB_IDX_UNIQUE | EJDB_IDX_STR);

// Numeric index used by range queries and `asc`/`desc`
rc = ejdb_ensure_index(db, "users", "/age", EJDB_IDX_I64);

// Index every element of the `tags` array
rc = ejdb_ensure_index(db, "books", "/tags", EJDB_IDX_STR);
```

Tips:

* `ejdb_ensure_index()` is idempotent. An existing index with the same path but a
  different type/uniqueness fails with `EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE`.
* Creating an index over an existing collection builds the index for all documents.
  When importing a large batch, insert the documents first and create the indexes
  afterwards.
* A unique index is enforced by every write; a conflicting `ejdb_put*()` fails with
  `EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED`.
* Whether an index is actually used is reported by the query execution log, see
  [Query plan log](#query-plan-log-explain).

## Queries

Queries are created with the JQL API (`jql.h`), optionally bound to placeholder values,
and executed with `ejdb_exec()`. The query language itself — filters, `apply`,
projections, joins, sorting and options — is described in the
[JQL documentation](https://github.com/Softmotions/ejdb/blob/master/src/jql/README.md).

### Creating a query

Function | Description
--- | ---
`jql_create(&q, coll, query)` | Parse a query; `coll` may be zero if the query has an `@collection` anchor
`jql_create2(&q, coll, query, mode)` | Same with `JQL_KEEP_QUERY_ON_PARSE_ERROR`/`JQL_SILENT_ON_PARSE_ERROR`
`jql_collection(q)` | Collection name the query is bound to
`jql_first_anchor(q)` | First `@collection` anchor of the query, if any
`jql_error(q)` | Parse error text (with a `<---` marker) for a kept query
`jql_has_apply(q)`, `jql_has_apply_upsert(q)`, `jql_has_apply_delete(q)` | Query contains a modification section
`jql_has_projection(q)`, `jql_has_orderby(q)`, `jql_has_aggregate_count(q)` | Query shape inspection
`jql_get_skip(q, &skip)`, `jql_get_limit(q, &limit)` | Options encoded in the query
`jql_reset(q, reset_match_cache, reset_placeholders)` | Reuse a query object for the next invocation
`jql_destroy(&q)` | Release the query object

```c
struct jql *q = 0;

// Collection from the argument
iwrc rc = jql_create(&q, "parrots", "/[age > :age]");

// Collection from the query anchor, the `coll` argument is zero
rc = jql_create(&q, 0, "@parrots/[age > :age]");
```

Reporting a query syntax error with a caret marker:

```c
struct jql *q = 0;
iwrc rc = jql_create2(&q, "parrots", "/[age > ]",
                      JQL_KEEP_QUERY_ON_PARSE_ERROR | JQL_SILENT_ON_PARSE_ERROR);
if (rc) {
  fprintf(stderr, "%s\n", jql_error(q));  // jql_error() returns "" for a zero query
}
jql_destroy(&q);  // The kept query object must be destroyed in any case
```

A query object returned together with an error is kept **only** for error reporting;
it cannot be executed.

### Placeholder binding

Placeholders are written `:name` (named) or `:?` (positional). Positional
placeholders are bound by index (`jql_set_*(q, 0, index, value)`), named ones by name
(`jql_set_*(q, "name", 0, value)`). A query with an unbound placeholder fails with
`JQL_ERROR_UNSET_PLACEHOLDER`.

Function | Description
--- | ---
`jql_set_i64(q, ph, idx, v)` | 64-bit signed integer
`jql_set_f64(q, ph, idx, v)` | Floating point value
`jql_set_bool(q, ph, idx, v)` | Boolean
`jql_set_null(q, ph, idx)` | `null`
`jql_set_str(q, ph, idx, s)` | String, **not copied** — `s` must stay alive until the query releases it
`jql_set_str3(q, ph, idx, s, len)` | String of a given length, **copied** into the query
`jql_set_str2(q, ph, idx, s, freefn, op)` | String with an ownership callback
`jql_set_json(q, ph, idx, node)` | `struct jbl_node` value, **not copied** (typically allocated in a pool)
`jql_set_json_jbl(q, ph, idx, jbl)` | `struct jbl` value, converted and copied by the query
`jql_set_json2(q, ph, idx, node, freefn, op)` | Node value with an ownership callback
`jql_set_regexp(q, ph, idx, expr)` | Regular expression compiled from `expr`
`jql_set_regexp2(q, ph, idx, expr, freefn, op)` | Same with an ownership callback

Ownership rules for the `*2` variants: on success the query takes ownership of the
value and `op`, and `freefn(value, op)` is invoked when the placeholder is released
(`jql_reset()` with `reset_placeholders = true`, or `jql_destroy()`). On failure the
caller keeps ownership and `freefn` is **not** invoked.

```c
struct iwpool *pool = iwpool_create_empty();
struct jbl_node *tags = 0, *patch = 0;
struct jql *q = 0;

iwrc rc = jql_create(&q, "books", "/tags/[** in :tags] | apply :patch");
RCC(rc, finish, jbn_from_json("[\"bestseller\",\"dev\"]", &tags, pool));
RCC(rc, finish, jql_set_json(q, "tags", 0, tags));
RCC(rc, finish, jbn_from_json("{\"checked\":true}", &patch, pool));
RCC(rc, finish, jql_set_json(q, "patch", 0, patch));

finish:
  jql_destroy(&q);
  iwpool_destroy(pool);
```

Tips:

* `jql_set_str()` does not copy the string. For a temporary buffer use
  `jql_set_str3()` (copies) or keep the buffer alive until the query is destroyed.
* `jql_set_json()` does not copy the node. Allocate nodes in an `iwpool` that outlives
  all executions of the query; use `jql_set_json_jbl()` to get a private copy.
* An `apply`/`upsert` placeholder value must be bound as a JSON node
  (`jql_set_json()`/`jql_set_json_jbl()`); a plain string binding fails with
  `JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE`.
* Placeholders can be used in filters, projections, ordering clauses, `apply`, and
  `skip`/`limit`.

### Query object reuse

A query object can be executed many times with different placeholder values.
`ejdb_exec()` resets the internal match cache after each run but keeps the bound
placeholders, so only the changed values have to be re-bound:

```c
struct jql *q = 0;
iwrc rc = jql_create(&q, "parrots", "/[age > :age] | asc /age");
if (!rc) {
  for (int i = 0; i < 3; ++i) {
    struct ejdb_exec ux = {
      .db = db,
      .q = q
    };
    rc = jql_set_i64(q, "age", 0, i * 5);
    if (!rc) {
      rc = ejdb_exec(&ux);
    }
    if (rc) {
      break;
    }
    printf("age > %d: %" PRId64 " documents\n", i * 5, ux.cnt);
  }
}
jql_destroy(&q);
```

Use `jql_reset(q, reset_match_cache, reset_placeholders)` to release bound values
explicitly — pass `reset_placeholders = true` to drop all bindings, or `false` to only
clear the match cache.

### In-memory matching

`jql_matched(q, jbl, &matched)` checks a single document against the query filter
without touching the storage. This is handy when documents arrive from another source
and the same JQL syntax should be used for filtering:

```c
struct jql *q = 0;
struct jbl *doc = 0;
bool matched = false;
iwrc rc = jql_create(&q, "parrots", "/[age > :age]");
if (!rc) {
  rc = jql_set_i64(q, "age", 0, 3);
}
if (!rc) {
  rc = jbl_from_json(&doc, "{\"name\":\"Darko\",\"age\":8}");
}
if (!rc) {
  rc = jql_matched(q, doc, &matched);
}
if (!rc) {
  printf("matched=%d\n", matched);
}
jbl_destroy(&doc);
jql_destroy(&q);
```

## Executing queries

`ejdb_exec()` streams matched documents to a visitor:

```c
typedef iwrc (*ejdb_exec_visitor)(struct ejdb_exec *ctx, struct ejdb_doc *doc, int64_t *step);

struct ejdb_exec ux = {
  .db = db,           // Database handle, required
  .q = q,             // Query object, required
  .visitor = visitor, // Optional; without it documents are only matched
  .opaque = my_ctx,   // Optional user data passed to the visitor
  .skip = 0,          // Overrides `skip` encoded in the query
  .limit = 0,         // Overrides `limit` encoded in the query; 0 means "use the query value"
  .log = 0,           // Optional `struct iwxstr` receiving the execution plan
  .pool = 0,          // Optional pool for projected/patched node trees
  .cnt = 0            // Out: number of documents passed to the visitor
};
iwrc rc = ejdb_exec(&ux);
```

Behaviour:

* `ux.limit < 1` or `ux.skip < 1` mean "take the value from the query". A query
  without `limit` matches everything.
* `ux.cnt` counts the documents passed to the visitor. Without a visitor (as in
  `ejdb_count()`) it counts matched documents.
* Executing a query on a collection that does not exist is not an error: `ejdb_exec()`
  returns `0` and no documents. Use `ejdb_ensure_collection()` or a preceding write if
  a missing collection has to be detected.
* A query with an `apply`/`upsert`/`del` section takes a write lock on the collection
  and modifies the storage while documents are visited.
* Without a visitor the projection section is ignored (the query only matches and
  counts documents).

### Visitor contract

* `doc->id` is the document identifier, `doc->raw` is the stored document.
* `doc->node` is set only when the query has an `apply` or projection section. After
  an `apply` it holds the **modified** document, while `doc->raw` still holds the
  stored (pre-modification) one. Print/serialize `doc->node` when it is not zero.
* `doc->node` is valid only during the callback unless `ux.pool` is set (then node
  trees are allocated from that pool and stay valid until the pool is destroyed).
* `doc` and its data are valid only during the callback; copy everything that must
  outlive it.
* Return `0` to continue. Any non-zero return value aborts the execution and is
  returned by `ejdb_exec()`.
* `*step` controls the cursor: `1` (default) advances to the next document, `0` stops
  the iteration, positive values advance by that number of documents, negative values
  move backwards. Multi-step and backward movement is intended for custom navigation
  of sorted result sets (`asc`/`desc`); most visitors use `1` and `0`.
* `*step == -1` re-enters the visitor with the same document (useful when one document
  produces several output records).

```c
// Print either the projected/modified tree or the stored document
static iwrc print_doc_visitor(struct ejdb_exec *ctx, struct ejdb_doc *doc, int64_t *step) {
  (void) ctx;
  (void) step;
  if (doc->node) {
    return jbn_as_json(doc->node, jbl_fstream_json_printer, stdout, 0);
  }
  return jbl_as_json(doc->raw, jbl_fstream_json_printer, stdout, 0);
}

// Stop iterating once ten documents have been visited
static iwrc head_visitor(struct ejdb_exec *ctx, struct ejdb_doc *doc, int64_t *step) {
  (void) doc;
  int64_t *visited = ctx->opaque;
  if (++(*visited) >= 10) {
    *step = 0;
  }
  return 0;
}
```

```c
int64_t visited = 0;
struct ejdb_exec ux = {
  .db = db,
  .q = q,
  .visitor = head_visitor,
  .opaque = &visited
};
iwrc rc = ejdb_exec(&ux);
printf("visited=%" PRId64 "\n", ux.cnt);
```

### Query plan log (explain)

Setting `ux.log` to a `struct iwxstr` collects index selection and collector
decisions. It is the C API equivalent of the `explain` command of the WebSocket API:

```c
#include <iowow/iwxstr.h>

struct iwxstr *log = iwxstr_new();
struct jql *q = 0;
struct ejdb_list *list = 0;

iwrc rc = jql_create(&q, "parrots", "/[age > :age] | asc /age");
RCC(rc, finish, jql_set_i64(q, "age", 0, 3));
RCC(rc, finish, ejdb_list4(db, q, 0, log, &list));
printf("%s", iwxstr_ptr(log));

finish:
  ejdb_list_destroy(&list);
  jql_destroy(&q);
  iwxstr_destroy(log);
```

Example output for a query served by the `/age` index:

```
[INDEX] MATCHED  I64|4 /age EXPR1: 'age > :age' INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_PREV ORDERBY
[INDEX] SELECTED I64|4 /age EXPR1: 'age > :age' INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_PREV ORDERBY
 [COLLECTOR] PLAIN
```

`[INDEX] NO` followed by `[COLLECTOR] SORTER` means a full collection scan and (if the
query sorts) an in-memory or temporary-file sort. See
[JQL indexes and performance](https://github.com/Softmotions/ejdb/blob/master/src/jql/README.md#jql-indexes-and-performance-tips)
for the filters that can use an index.

### Collecting results into a list

Function | Description
--- | ---
`ejdb_list(db, q, &first, limit, pool)` | Fill a linked list of documents using a caller-provided `iwpool` (not zero); the caller owns and destroys the pool
`ejdb_list2(db, coll, query, limit, &list)` | Same from a query text
`ejdb_list3(db, coll, query, limit, log, &list)` | Same with an execution plan buffer
`ejdb_list4(db, q, limit, log, &list)` | Same from a query object
`ejdb_list_destroy(&list)` | Release a result list

```c
struct ejdb_list *list = 0;
iwrc rc = ejdb_list3(db, "parrots", "/[age > 3]", 0 /* no limit */, 0 /* no log */, &list);
if (!rc) {
  for (struct ejdb_doc *doc = list->first; doc; doc = doc->next) {
    printf("%" PRId64 " ", doc->id);
    jbl_as_json(doc->raw, jbl_fstream_json_printer, stdout, 0);
    putchar('\n');
  }
  ejdb_list_destroy(&list);
}
```

Notes:

* The `ejdb_list*()` helpers copy every matched document into a memory pool; prefer
  `ejdb_exec()` with a visitor (or a small `limit`) for large result sets.
* `ejdb_list2/3/4` create the pool themselves and release it in
  `ejdb_list_destroy()`. `ejdb_list()` does not allocate a `struct ejdb_list`: it
  returns the first document, and the caller supplied pool must stay alive while the
  list is walked.
* `list->q` is set only by `ejdb_list2()`/`ejdb_list3()`, which parse the query
  themselves. A query object passed to `ejdb_list()`/`ejdb_list4()` remains owned by
  the caller.

### Count and update helpers

Function | Description
--- | ---
`ejdb_count(db, q, &count, limit)` | Execute a query and return the number of matched documents
`ejdb_count2(db, coll, query, &count, limit)` | Same from a query text
`ejdb_update(db, q)` | Execute an update query (with an `apply` section)
`ejdb_update2(db, coll, query)` | Same from a query text

```c
int64_t count = 0;
iwrc rc = ejdb_count2(db, "parrots", "/[age > 3]", &count, 0);
```

The `*2` text variants create and execute the query in one step, so they cannot bind
placeholder values. Use the `struct jql` variants for queries with placeholders:

```c
struct jql *q = 0;
int64_t count = 0;
iwrc rc = jql_create(&q, "parrots", "/[age > :age]");
if (!rc) {
  rc = jql_set_i64(q, "age", 0, 3);
}
if (!rc) {
  rc = ejdb_count(db, q, &count, 0);
}
jql_destroy(&q);
```

Adding `count` to the options section (`"/[age > :age] | count"`) makes the engine
return only the count without converting documents for the visitor; this is the fastest
way to count or to perform a bulk `apply`/`del`.

## Blocks API

With `IW_BLOCKS` defined (the default for clang builds) the library exposes a
block-based iteration shortcut. The block receives a document and returns `true` to
continue or `false` to stop:

```c
#ifdef IW_BLOCKS
iwrc rc = ejdb_visit_block(db, q, ^bool(struct ejdb_doc *doc) {
  printf("%" PRId64 "\n", doc->id);
  return true;
});
#endif
```

`ejdb_visit_block2()` accepts a `struct ejdb_visit_block` with `db`, `q`, an optional
`pool` and `user_data`, and reports the number of visited documents in `num_visits`.
Note that variables captured by the block and modified inside it must be declared with
`__block`.

## Storage maintenance

Function | Description
--- | ---
`ejdb_online_backup(db, &ts, target_file)` | Consistent online backup; all records written before `ts` are included
`ejdb_get_iwkv(db, &kv)` | Access the underlying IWKV storage (advanced use)
`ejdb_get_meta(db, &jbl)` | Database structure as JSON (version, file, size, collections, indexes)
`ejdb_version_full()`, `ejdb_version_major()`, `ejdb_version_minor()`, `ejdb_version_patch()` | Library version
`ejdb_git_revision()` | Source revision the library was built from

```c
uint64_t ts = 0;
iwrc rc = ejdb_online_backup(db, &ts, "backup.db");
if (!rc) {
  printf("backup finished at %" PRIu64 "\n", ts);
}
```

The backup file is a regular database image that can be opened with `ejdb_open()`.
Reads and writes are allowed while the backup is running. Because the backup uses
database cursors internally, close your own cursors or run the backup from a dedicated
thread to avoid deadlocks.

Inspecting the database structure:

```c
struct jbl *meta = 0;
iwrc rc = ejdb_get_meta(db, &meta);
if (!rc) {
  char *json = 0;
  if (!jbl_as_json_alloc(meta, JBL_PRINT_PRETTY, &json)) {
    puts(json);
    free(json);
  }
}
jbl_destroy(&meta);
```

The metadata contains the engine `version`, the storage `file`/`size` and the list of
collections with their document counts (`rnum`) and indexes (`ptr`, `mode`).

## Common tasks

**Bulk insert.** Reuse one write call per document and create indexes after the import
(see [Collections and indexes](#collections-and-indexes)):

```c
int64_t id = 0;
for (int i = 0; i < 1000; ++i) {
  struct jbl *jbl = 0;
  iwrc rc = jbl_from_json_printf(&jbl, "{\"n\":%d}", i);
  if (!rc) {
    rc = ejdb_put_new(db, "numbers", jbl, &id);
  }
  jbl_destroy(&jbl);
  if (rc) {
    break;
  }
}
```

**Pagination.** `skip`/`limit` can be encoded in the query or overridden per
execution. `skip` is applied after filtering, so a deep page still scans the skipped
documents:

```c
struct jql *q = 0;
iwrc rc = jql_create(&q, "parrots", "/* | asc /age");
if (!rc) {
  for (int page = 0; page < 3; ++page) {
    struct ejdb_exec ux = {
      .db = db,
      .q = q,
      .skip = page * 50,
      .limit = 50,
      .visitor = print_doc_visitor
    };
    RCC(rc, finish, ejdb_exec(&ux));
  }
}

finish:
  jql_destroy(&q);
```

**Upsert by query.** Insert the document when nothing matches:

```c
struct iwpool *pool = iwpool_create_empty();
struct jbl_node *doc = 0;
struct jql *q = 0;

iwrc rc = jql_create(&q, "jobs", "@jobs/[title = :title] | upsert :doc");
RCC(rc, finish, jql_set_str(q, "title", 0, "Gardening"));
RCC(rc, finish, jbn_from_json("{\"title\":\"Gardening\",\"tags\":[\"outdoor\"]}", &doc, pool));
RCC(rc, finish, jql_set_json(q, "doc", 0, doc));
rc = ejdb_update(db, q);

finish:
  jql_destroy(&q);
  iwpool_destroy(pool);
```

**Bulk update with a count.** Store the changed documents without transferring them to
a visitor:

```c
struct iwpool *pool = iwpool_create_empty();
struct jbl_node *patch = 0;
struct jql *q = 0;
int64_t count = 0;

iwrc rc = jql_create(&q, "parrots", "/[age > :age] | apply :patch | count");
RCC(rc, finish, jql_set_i64(q, "age", 0, 3));
RCC(rc, finish, jbn_from_json("{\"adult\":true}", &patch, pool));
RCC(rc, finish, jql_set_json(q, "patch", 0, patch));
RCC(rc, finish, ejdb_count(db, q, &count, 0));
printf("updated=%" PRId64 "\n", count);

finish:
  jql_destroy(&q);
  iwpool_destroy(pool);
```

## Thread safety and lifetime summary

Object | Ownership
--- | ---
`struct ejdb *` | Owned by the caller; released by `ejdb_close()`; shareable between threads
`struct jbl *` | Owned by the caller; released by `jbl_destroy()`; returned by `ejdb_get()`/`ejdb_get_meta()`
`struct jbl_node *` | Released with its `struct iwpool`; a node bound with `jql_set_json()` must outlive the query executions
`struct jql *` | Owned by the caller; released by `jql_destroy()`
`struct ejdb_list *` | Owned by the caller; released by `ejdb_list_destroy()`
`struct ejdb_doc` (visitor) | Valid only during the visitor callback
`struct ejdb_exec.pool` | Optional memory pool for projected/patched nodes owned by the caller

* The database handle is thread-safe. Every collection has its own read/write lock:
  queries take a read lock, while `apply`/`upsert`/`del` queries and all write calls
  take a write lock.
* `ejdb_exec()` holds the collection lock for the whole execution. Do not call write
  API functions for the same collection from a visitor — collect the data and modify
  the storage after `ejdb_exec()` returns.
* A slow visitor slows down the query and blocks writers of that collection. Offload
  expensive work (network calls, large serialization) to another thread.
* Query objects, documents and result lists are not internally synchronized; use each
  of them from one thread at a time.
* Stop all threads before `ejdb_close()`.

## Tips and pitfalls

* **Initialize once.** `ejdb_open()` calls `ejdb_init()` implicitly, but call it
  explicitly before any other API function.
* **Zero-initialize output handles.** `jql_destroy()`, `jbl_destroy()` and
  `ejdb_list_destroy()` accept zero handles, which makes a single cleanup block
  possible.
* **Prefer `ejdb_exec()` over `ejdb_list*()`.** Lists copy every document; large
  collections will exhaust memory.
* **Add `count` when only a number is needed.** It avoids JSON conversion for
  `apply`/`del` queries and is the fastest way to count.
* **Bind placeholders instead of composing query text.** This avoids escaping bugs and
  lets the engine reuse the parsed query.
* **Use `jql_set_str3()` for temporary strings** and `jql_set_json_jbl()` for document
  values that must be copied.
* **`apply`/`upsert` values must be JSON nodes**, not strings.
* **`ejdb_patch()` does not insert.** Use `ejdb_merge_or_put()` when a missing document
  should be created.
* **A missing collection is not an error for queries.** `ejdb_exec()` returns `0` with
  an empty result; only write calls create collections.
* **Create indexes after a bulk import.** Building an index once over the final data
  set is faster than maintaining it during inserts.
* **Verify the plan with `ux.log`.** If the log says `[INDEX] NO`, the query does a full
  scan; add an index or rewrite the filter.
* **Print `doc->node` when it is set.** `doc->raw` still holds the pre-`apply` document.
* **Keep lifetimes explicit.** A document fetched with `ejdb_get()` is released by
  `jbl_destroy()`; node trees are released with their `iwpool`.
* **Log errors with `iwlog_ecode_error3()`/`iwlog_ecode_explained()`** to get the
  symbolic code name and description instead of a bare number.
* **Run memory-sensitive code under sanitizers.** `./build.sh -DENABLE_ASAN=1` builds
  the library and tests with AddressSanitizer.

## See also

* [JQL query language](https://github.com/Softmotions/ejdb/blob/master/src/jql/README.md)
* [HTTP REST/WebSocket API](https://github.com/Softmotions/ejdb/blob/master/src/jbr/README.md)
* [Test suite and tools](https://github.com/Softmotions/ejdb/blob/master/TESTS.md)
* [iowow — JSON, storage and utility library](https://github.com/Softmotions/iowow)
