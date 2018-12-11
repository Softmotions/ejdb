#pragma once
#ifndef JBLOB_H
#define JBLOB_H

#include <iowow/iwlog.h>
#include <iowow/iwpool.h>
#include <iowow/iwxstr.h>
#include <stdbool.h>

IW_EXTERN_C_START

struct _JBL;
typedef struct _JBL *JBL;

typedef enum {
  _JBL_ERROR_START = (IW_ERROR_START + 10000UL),
  JBL_ERROR_INVALID_BUFFER,             /**< Invalid JBL buffer (JBL_ERROR_INVALID_BUFFER) */
  JBL_ERROR_CREATION,                   /**< Cannot create JBL object (JBL_ERROR_CREATION) */
  JBL_ERROR_INVALID,                    /**< Invalid JBL object (JBL_ERROR_INVALID) */
  JBL_ERROR_PARSE_JSON,                 /**< Failed to parse JSON string (JBL_ERROR_PARSE_JSON) */
  JBL_ERROR_PARSE_UNQUOTED_STRING,      /**< Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING) */
  JBL_ERROR_PARSE_INVALID_CODEPOINT,    /**< Invalid unicode codepoint/escape sequence (JBL_ERROR_PARSE_INVALID_CODEPOINT) */
  JBL_ERROR_PARSE_INVALID_UTF8,         /**< Invalid utf8 string (JBL_ERROR_PARSE_INVALID_UTF8) */
  JBL_ERROR_JSON_POINTER,               /**< Invalid JSON pointer (rfc6901) path (JBL_ERROR_JSON_POINTER) */
  JBL_ERROR_PATH_NOTFOUND,              /**< JSON object not matched the path specified (JBL_ERROR_PATH_NOTFOUND) */
  JBL_ERROR_PATCH_INVALID,              /**< Invalid JSON patch specified (JBL_ERROR_PATCH_INVALID) */
  JBL_ERROR_PATCH_INVALID_OP,           /**< Invalid JSON patch operation specified (JBL_ERROR_PATCH_INVALID_OP) */
  JBL_ERROR_PATCH_NOVALUE,              /**< No value specified in JSON patch (JBL_ERROR_PATCH_NOVALUE) */
  JBL_ERROR_PATCH_TARGET_INVALID,       /**< Could not find target object to set value (JBL_ERROR_PATCH_TARGET_INVALID) */
  JBL_ERROR_PATCH_INVALID_ARRAY_INDEX,  /**< Invalid array index in JSON patch path (JBL_ERROR_PATCH_INVALID_ARRAY_INDEX) */
  JBL_ERROR_PATCH_TEST_FAILED,          /**< JSON patch test operation failed (JBL_ERROR_PATCH_TEST_FAILED) */
  _JBL_ERROR_END
} jbl_ecode;


typedef uint8_t jbl_print_flags_t;
#define JBL_PRINT_PRETTY      ((jbl_print_flags_t) 0x01U)
#define JBL_PRINT_CODEPOINTS  ((jbl_print_flags_t) 0x02U)

typedef uint8_t jbn_visitor_cmd_t;
#define JBL_VCMD_OK           ((jbn_visitor_cmd_t) 0x00U)
#define JBL_VCMD_TERMINATE    ((jbn_visitor_cmd_t) 0x01U)
#define JBL_VCMD_SKIP_NESTED  ((jbn_visitor_cmd_t) 0x02U)
#define JBN_VCMD_DELETE       ((jbn_visitor_cmd_t) 0x04U)

typedef enum {
  JBV_NONE = 0, // Do not reorder
  JBV_NULL,

  JBV_BOOL,
  JBV_I64,
  JBV_F64,
  JBV_STR,

  JBV_OBJECT, // Do not reorder
  JBV_ARRAY,
} jbl_type_t;

typedef struct _JBL_NODE {
  struct _JBL_NODE *next;
  struct _JBL_NODE *prev;
  struct _JBL_NODE *parent; /**< Optional parent */
  const char *key;
  int klidx;
  uint32_t flags;           /**< Utility node flags */

  // Do not sort/add members after this point (offsetof usage below)
  struct _JBL_NODE *child;
  int vsize;
  jbl_type_t type;
  union {
    const char *vptr;
    bool vbool;
    int64_t vi64;
    double vf64;
  };
} *JBL_NODE;

typedef enum {
  JBP_ADD = 1,
  JBP_REMOVE,
  JBP_REPLACE,
  JBP_COPY,
  JBP_MOVE,
  JBP_TEST
} jbp_patch_t;

typedef struct _JBL_PATCH {
  jbp_patch_t op;
  const char *path;
  const char *from;
  const char *vjson;
  JBL_NODE vnode;
} JBL_PATCH;

/**
 * @brief JSON pointer
 */
typedef struct _JBL_PTR {
  uint64_t op;      /**< Opaque data associated with pointer */
  int cnt;          /**< Number of nodes */
  int sz;           /**< Size of JBL_PTR allocated area */
  char *n[1];       /**< Path nodes */
} *JBL_PTR;

typedef iwrc(*jbl_json_printer)(const char *data, int size, char ch, int count, void *op);

IW_EXPORT iwrc jbl_create_empty_object(JBL *jblp);

IW_EXPORT iwrc jbl_create_empty_array(JBL *jblp);

IW_EXPORT iwrc jbl_from_buf_keep(JBL *jblp, void *buf, size_t bufsz, bool keep_on_destroy);

IW_EXPORT iwrc jbl_from_buf_keep_onstack(JBL jbl, void *buf, size_t bufsz);

IW_EXPORT iwrc jbl_from_buf_keep_onstack2(JBL jbl, void *buf);

IW_EXPORT iwrc jbl_from_json(JBL *jblp, const char *jsonstr);

IW_EXPORT jbl_type_t jbl_type(JBL jbl);

IW_EXPORT size_t jbl_count(JBL jbl);

IW_EXPORT size_t jbl_size(JBL jbl);

IW_EXPORT int32_t jbl_get_i32(JBL jbl);

IW_EXPORT int64_t jbl_get_i64(JBL jbl);

IW_EXPORT double jbl_get_f64(JBL jbl);

IW_EXPORT char *jbl_get_str(JBL jbl);

IW_EXPORT size_t jbl_copy_strn(JBL jbl, char *buf, size_t bufsz);

IW_EXPORT iwrc jbl_at(JBL jbl, const char *path, JBL *res);

IW_EXPORT iwrc jbl_at2(JBL jbl, JBL_PTR jp, JBL *res);

IW_EXPORT iwrc jbl_as_buf(JBL jbl, void **buf, size_t *size);

IW_EXPORT iwrc jbl_as_json(JBL jbl, jbl_json_printer pt, void *op, jbl_print_flags_t pf);

IW_EXPORT iwrc jbl_fstream_json_printer(const char *data, int size, char ch, int count, void *op);

IW_EXPORT iwrc jbl_xstr_json_printer(const char *data, int size, char ch, int count, void *op);

IW_EXPORT void jbl_destroy(JBL *jblp);

//--- JBL_NODE

IW_EXPORT iwrc jbl_to_node(JBL jbl, JBL_NODE *node, IWPOOL *pool);

IW_EXPORT iwrc jbl_node_from_json(const char *json, JBL_NODE *node, IWPOOL *pool);

IW_EXPORT iwrc jbl_node_as_json(JBL_NODE node, jbl_json_printer pt, void *op, jbl_print_flags_t pf);

IW_EXPORT iwrc jbl_from_node(JBL jbl, JBL_NODE node);

IW_EXPORT int jbl_compare_nodes(JBL_NODE n1, JBL_NODE n2, iwrc *rcp);

IW_EXPORT void jbl_add_item(JBL_NODE parent, JBL_NODE node);

IW_EXPORT void jbl_remove_item(JBL_NODE parent, JBL_NODE child);

IW_EXPORT JBL_NODE jbl_node_detach(JBL_NODE target, JBL_PTR path);

IW_EXPORT void jbl_node_reset_data(JBL_NODE node);

IW_EXPORT iwrc jbl_ptr_alloc(const char *path, JBL_PTR *jpp);

IW_EXPORT iwrc jbl_ptr_alloc_pool(const char *path, JBL_PTR *jpp, IWPOOL *pool);

IW_EXPORT int jbl_ptr_cmp(JBL_PTR p1, JBL_PTR p2);

IW_EXPORT iwrc jbl_ptr_serialize(JBL_PTR ptr, IWXSTR *xstr);

/**
 * @brief JBL_NODE visitor context
 */
typedef struct _JBN_VCTX {
  JBL_NODE root;  /**< Root node from which started visitor */
  void *op;       /**< Arbitrary opaque data */
  void *result;
  bool terminate;
  IWPOOL *pool;   /**< Pool placeholder, initialization is responsibility of `JBN_VCTX` creator */
} JBN_VCTX;


/**
 * Call with lvl: `-1` means end of visiting whole object tree.
 */
typedef jbn_visitor_cmd_t (*JBN_VISITOR)(int lvl, JBL_NODE n, const char *key, int klidx, JBN_VCTX *vctx, iwrc *rc);

IW_EXPORT iwrc jbn_visit(JBL_NODE node, int lvl, JBN_VCTX *vctx, JBN_VISITOR visitor);

//--- PATCHING

IW_EXPORT iwrc jbl_patch_auto(JBL_NODE root, JBL_NODE patch, IWPOOL *pool);

IW_EXPORT iwrc jbl_patch_node(JBL_NODE root, const JBL_PATCH *patch, size_t cnt);

IW_EXPORT iwrc jbl_patch(JBL jbl, const JBL_PATCH *patch, size_t cnt);

IW_EXPORT iwrc jbl_patch_from_json(JBL jbl, const char *patchjson);

IW_EXPORT iwrc jbl_merge_patch_node(JBL_NODE root, const char *patchjson, IWPOOL *pool);

IW_EXPORT iwrc jbl_merge_patch(JBL jbl, const char *patchjson);


IW_EXPORT iwrc jbl_init(void);

IW_EXTERN_C_END
#endif
