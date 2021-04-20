#include <ejdb2.h>
#include <jni.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "com_softmotions_ejdb2_EJDB2.h"
#include "com_softmotions_ejdb2_JQL.h"


#define JBN_JSON_FLUSH_BUFFER_SZ 4096

typedef struct JBN_STR {
  const char *utf;
  jstring     str;
} JBN_STR;

typedef enum {
  _JBN_ERROR_START = (IW_ERROR_START + 15000UL + 5000),
  JBN_ERROR_INVALID_FIELD,          /**< Failed to get class field (JBN_ERROR_INVALID_FIELD) */
  JBN_ERROR_INVALID_METHOD,         /**< Failed to get class method (JBN_ERROR_INVALID_METHOD) */
  JBN_ERROR_INVALID_OPTIONS,
  /**< Invalid com.softmotions.ejdb2.EJDB2Builder configuration provided
     (JBN_ERROR_INVALID_OPTIONS) */
  JBN_ERROR_INVALID_STATE,          /**< Invalid com.softmotions.ejdb2.EJDB2 JNI state (JBN_ERROR_INVALID_STATE) */
  JBN_ERROR_CREATION_OBJ,           /**< Failed to create/allocate JNI object (JBN_ERROR_CREATION_OBJ) */
  _JBN_ERROR_END,
} jbn_ecode_t;

static jclass k_EJDB2Exception_clazz;
static jmethodID k_EJDB2Exception_constructor; // EJDB2Exception(int code, String message)

static jclass k_EJDB2_clazz;
static jfieldID k_EJDB2_handle_fid;

static jclass k_JQL_clazz;
static jfieldID k_JQL_handle_fid;
static jfieldID k_JQL_db_fid;
static jfieldID k_JQL_query_fid;
static jfieldID k_JQL_collection_fid;
static jfieldID k_JQL_skip_fid;
static jfieldID k_JQL_limit_fid;

#define JBNFIELD(fid_, env_, clazz_, name_, type_)           \
  fid_ = (*(env_))->GetFieldID(env_, clazz_, name_, type_);

#define JBNFIELD2(fid_, env_, clazz_, name_, type_, label_)  \
  fid_ = (*(env_))->GetFieldID(env_, clazz_, name_, type_);  \
  if (!fid_) goto label_;

typedef struct JBN_JSPRINT_CTX {
  int     flush_buffer_sz;
  IWXSTR *xstr;
  iwrc (*flushFn)(struct JBN_JSPRINT_CTX *pctx);
  JNIEnv   *env;
  jclass    osClazz;
  jobject   osObj;
  jmethodID write_mid;
} JBN_JSPRINT_CTX;

static iwrc jbn_json_printer(const char *data, int size, char ch, int count, void *op) {
  JBN_JSPRINT_CTX *pctx = op;
  IWXSTR *xstr = pctx->xstr;
  if (!data) {
    if (count) {
      for (int i = 0; i < count; ++i) {
        iwrc rc = iwxstr_cat(xstr, &ch, 1);
        RCRET(rc);
      }
    }
  } else {
    if (size < 0) {
      size = (int) strlen(data);
    }
    if (!count) {
      count = 1;
    }
    for (int i = 0; i < count; ++i) {
      iwrc rc = iwxstr_cat(xstr, data, size);
      RCRET(rc);
    }
  }
  if (iwxstr_size(xstr) >= pctx->flush_buffer_sz) {
    iwrc rc = pctx->flushFn(pctx);
    RCRET(rc);
  }
  return 0;
}

IW_INLINE iwrc jbn_db(JNIEnv *env, jobject thisObj, EJDB *db) {
  *db = 0;
  jlong ptr = (*env)->GetLongField(env, thisObj, k_EJDB2_handle_fid);
  if (!ptr) {
    return JBN_ERROR_INVALID_STATE;
  }
  *db = (void*) ptr;
  return 0;
}

IW_INLINE iwrc jbn_jql_q(JNIEnv *env, jobject thisObj, JQL *q) {
  *q = 0;
  jlong ptr = (*env)->GetLongField(env, thisObj, k_JQL_handle_fid);
  if (!ptr) {
    return JBN_ERROR_INVALID_STATE;
  }
  *q = (void*) ptr;
  return 0;
}

static iwrc jbn_flush_to_stream(JBN_JSPRINT_CTX *pctx) {
  JNIEnv *env = pctx->env;
  IWXSTR *xstr = pctx->xstr;
  size_t xsz = iwxstr_size(xstr);
  if (xsz == 0) {
    return 0;
  }
  jbyteArray arr = (*env)->NewByteArray(env, xsz);
  if (!arr) {
    return JBN_ERROR_CREATION_OBJ;
  }
  (*env)->SetByteArrayRegion(env, arr, 0, xsz, (void*) iwxstr_ptr(xstr));
  iwxstr_clear(xstr);
  (*env)->CallVoidMethod(env, pctx->osObj, pctx->write_mid, arr);
  return 0;
}

static iwrc jbn_init_pctx(JNIEnv *env, JBN_JSPRINT_CTX *pctx, jobject thisObj, jobject osObj) {
  memset(pctx, 0, sizeof(*pctx));
  iwrc rc = 0;
  jclass osClazz = (*env)->GetObjectClass(env, osObj);
  jmethodID writeMid = (*env)->GetMethodID(env, osClazz, "write", "([B)V");
  IWXSTR *xstr = iwxstr_new();
  if (!xstr) {
    return iwrc_set_errno(rc, IW_ERROR_ALLOC);
  }
  pctx->xstr = xstr;
  pctx->flush_buffer_sz = JBN_JSON_FLUSH_BUFFER_SZ;
  pctx->env = env;
  pctx->osClazz = osClazz;
  pctx->osObj = osObj;
  pctx->write_mid = writeMid;
  pctx->flushFn = jbn_flush_to_stream;
  return rc;
}

static void jbn_destroy_pctx(JBN_JSPRINT_CTX *pctx) {
  if (pctx->xstr) {
    iwxstr_destroy(pctx->xstr);
    pctx->xstr = 0;
  }
}

static void jbn_throw_rc_exception(JNIEnv *env, iwrc rc, const char *msg_) {
  const char *msg;
  if (msg_) {
    msg = msg_;
  } else {
    msg = iwlog_ecode_explained(rc);
    if (!msg) {
      msg = "Unknown iwrc error";
    }
  }
  uint32_t eno = iwrc_strip_errno(&rc);
  jstring msgStr = (*env)->NewStringUTF(env, msg);
  jobject exObj = (*env)->NewObject(env, k_EJDB2Exception_clazz,
                                    k_EJDB2Exception_constructor, (jlong) rc, (jlong) eno, msgStr);
  if ((*env)->Throw(env, exObj) < 0) {
    iwlog_error("Failed to throw exception for EJDB2Exception: %s", msg);
  }
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1open(
  JNIEnv *env,
  jobject thisObj,
  jobject optsObj) {
  iwrc rc = 0;
  EJDB_OPTS opts = { 0 };
  JNIEnv e = *env;
  jfieldID fid;
  jobject iwkv, http, wal;
  jclass iwkvClazz, httpClazz, walClazz;
  jclass optsClazz = e->GetObjectClass(env, optsObj);

  int sc = 0;
  EJDB db = 0;
  JBN_STR strings[3] = { 0 };

  // opts
  JBNFIELD(fid, env, optsClazz, "no_wal", "Z");
  opts.no_wal = e->GetBooleanField(env, optsObj, fid);

  JBNFIELD(fid, env, optsClazz, "sort_buffer_sz", "I");
  opts.sort_buffer_sz = (uint32_t) e->GetIntField(env, optsObj, fid);

  JBNFIELD(fid, env, optsClazz, "document_buffer_sz", "I");
  opts.document_buffer_sz = (uint32_t) e->GetIntField(env, optsObj, fid);

  // iwkv
  JBNFIELD(fid, env, optsClazz, "iwkv", "Lcom/softmotions/ejdb2/IWKVOptions;");
  iwkv = e->GetObjectField(env, optsObj, fid);
  if (!iwkv) {
    jbn_throw_rc_exception(env, JBN_ERROR_INVALID_OPTIONS, 0);
    return;
  }
  iwkvClazz = e->GetObjectClass(env, iwkv);

  JBNFIELD(fid, env, iwkvClazz, "random_seed", "J");
  opts.kv.random_seed = (uint32_t) e->GetLongField(env, iwkv, fid);

  JBNFIELD(fid, env, iwkvClazz, "oflags", "J");
  opts.kv.oflags = (iwkv_openflags) e->GetLongField(env, iwkv, fid);

  JBNFIELD(fid, env, iwkvClazz, "file_lock_fail_fast", "Z");
  opts.kv.file_lock_fail_fast = e->GetBooleanField(env, iwkv, fid);

  JBNFIELD(fid, env, iwkvClazz, "path", "Ljava/lang/String;");
  strings[sc].str = e->GetObjectField(env, iwkv, fid);
  strings[sc].utf = strings[sc].str ? e->GetStringUTFChars(env, strings[sc].str, 0) : 0;
  opts.kv.path = strings[sc++].utf;
  if (!opts.kv.path) {
    rc = JBN_ERROR_INVALID_OPTIONS;
    goto finish;
  }

  // wal
  JBNFIELD2(fid, env, iwkvClazz, "wal", "Lcom/softmotions/ejdb2/IWKVOptions$WALOptions;", finish);
  wal = e->GetObjectField(env, iwkv, fid);
  if (!wal) {
    jbn_throw_rc_exception(env, JBN_ERROR_INVALID_OPTIONS, 0);
    goto finish;
  }
  walClazz = e->GetObjectClass(env, wal);

  JBNFIELD2(fid, env, walClazz, "check_crc_on_checkpoint", "Z", finish);
  opts.kv.wal.check_crc_on_checkpoint = e->GetBooleanField(env, wal, fid);

  JBNFIELD2(fid, env, walClazz, "savepoint_timeout_sec", "I", finish);
  opts.kv.wal.savepoint_timeout_sec = (uint32_t) e->GetIntField(env, wal, fid);

  JBNFIELD2(fid, env, walClazz, "checkpoint_timeout_sec", "I", finish);
  opts.kv.wal.checkpoint_timeout_sec = (uint32_t) e->GetIntField(env, wal, fid);

  JBNFIELD2(fid, env, walClazz, "buffer_sz", "J", finish);
  opts.kv.wal.wal_buffer_sz = (uint64_t) e->GetLongField(env, wal, fid);

  JBNFIELD2(fid, env, walClazz, "checkpoint_buffer_sz", "J", finish);
  opts.kv.wal.checkpoint_buffer_sz = (uint64_t) e->GetLongField(env, wal, fid);


  // http
  JBNFIELD2(fid, env, optsClazz, "http", "Lcom/softmotions/ejdb2/EJDB2Builder$EJDB2HttpOptions;", finish);
  http = e->GetObjectField(env, optsObj, fid);
  httpClazz = e->GetObjectClass(env, http);

  JBNFIELD2(fid, env, httpClazz, "enabled", "Z", finish);
  opts.http.enabled = e->GetBooleanField(env, http, fid);

  JBNFIELD2(fid, env, httpClazz, "port", "I", finish);
  opts.http.port = e->GetIntField(env, http, fid);

  JBNFIELD2(fid, env, httpClazz, "bind", "Ljava/lang/String;", finish);
  strings[sc].str = e->GetObjectField(env, http, fid);
  strings[sc].utf = strings[sc].str ? e->GetStringUTFChars(env, strings[sc].str, 0) : 0;
  opts.http.bind = strings[sc++].utf;

  JBNFIELD2(fid, env, httpClazz, "access_token", "Ljava/lang/String;", finish);
  strings[sc].str = e->GetObjectField(env, http, fid);
  strings[sc].utf = strings[sc].str ? e->GetStringUTFChars(env, strings[sc].str, 0) : 0;
  opts.http.access_token = strings[sc++].utf;
  opts.http.access_token_len = opts.http.access_token ? strlen(opts.http.access_token) : 0;

  JBNFIELD2(fid, env, httpClazz, "read_anon", "Z", finish);
  opts.http.read_anon = e->GetBooleanField(env, http, fid);

  JBNFIELD2(fid, env, httpClazz, "max_body_size", "I", finish);
  opts.http.max_body_size = (size_t) e->GetIntField(env, http, fid);

  rc = ejdb_open(&opts, &db);
  RCGO(rc, finish);

  e->SetLongField(env, thisObj, k_EJDB2_handle_fid, (jlong) db);

finish:
  for (int i = 0; i < (sizeof(strings) / sizeof(strings[0])); ++i) {
    if (strings[i].str) {
      e->ReleaseStringUTFChars(env, strings[i].str, strings[i].utf);
    }
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

// DISPOSE
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1dispose(JNIEnv *env, jobject thisObj) {
  jlong ptr = (*env)->GetLongField(env, thisObj, k_EJDB2_handle_fid);
  if (ptr) {
    (*env)->SetLongField(env, thisObj, k_EJDB2_handle_fid, 0);
    EJDB db = (void*) ptr;
    iwrc rc = ejdb_close(&db);
    if (rc) {
      jbn_throw_rc_exception(env, rc, 0);
    }
  }
}

// PUT
JNIEXPORT jlong JNICALL Java_com_softmotions_ejdb2_EJDB2__1put(
  JNIEnv *env,
  jobject thisObj,
  jstring coll_,
  jstring json_,
  jlong   id) {
  iwrc rc;
  EJDB db;
  JBL jbl = 0;
  jlong ret = id;

  const char *coll = (*env)->GetStringUTFChars(env, coll_, 0);
  const char *json = (*env)->GetStringUTFChars(env, json_, 0);
  if (!coll || !json) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }

  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = jbl_from_json(&jbl, json);
  RCGO(rc, finish);

  if (id > 0) {
    rc = ejdb_put(db, coll, jbl, id);
  } else {
    rc = ejdb_put_new(db, coll, jbl, &ret);
  }

finish:
  if (jbl) {
    jbl_destroy(&jbl);
  }
  if (coll) {
    (*env)->ReleaseStringUTFChars(env, coll_, coll);
  }
  if (json) {
    (*env)->ReleaseStringUTFChars(env, json_, json);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
  return ret;
}

JNIEXPORT jlong JNICALL Java_com_softmotions_ejdb2_EJDB2__1online_1backup(
  JNIEnv *env, jobject thisObj,
  jstring target_) {
  EJDB db;
  uint64_t ts = 0;
  const char *target = (*env)->GetStringUTFChars(env, target_, 0);

  iwrc rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = ejdb_online_backup(db, &ts, target);

finish:
  if (target) {
    (*env)->ReleaseStringUTFChars(env, target_, target);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
  return ts;
}

// GET
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1get(
  JNIEnv  *env,
  jobject  thisObj,
  jstring  coll_,
  jlong    id,
  jobject  osObj,
  jboolean pretty) {
  iwrc rc;
  EJDB db;
  JBL jbl = 0;
  JBN_JSPRINT_CTX pctx;

  const char *coll = (*env)->GetStringUTFChars(env, coll_, 0);
  if (!coll) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }

  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = jbn_init_pctx(env, &pctx, thisObj, osObj);
  RCGO(rc, finish);

  rc = ejdb_get(db, coll, (int64_t) id, &jbl);
  RCGO(rc, finish);

  rc = jbl_as_json(jbl, jbn_json_printer, &pctx, 0);
  RCGO(rc, finish);

  rc = pctx.flushFn(&pctx);

finish:
  if (coll) {
    (*env)->ReleaseStringUTFChars(env, coll_, coll);
  }
  if (jbl) {
    jbl_destroy(&jbl);
  }
  jbn_destroy_pctx(&pctx);
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

// INFO
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1info(
  JNIEnv *env,
  jobject thisObj,
  jobject osObj) {
  iwrc rc;
  EJDB db;
  JBL jbl = 0;
  JBN_JSPRINT_CTX pctx;

  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = jbn_init_pctx(env, &pctx, thisObj, osObj);
  RCGO(rc, finish);

  rc = ejdb_get_meta(db, &jbl);
  RCGO(rc, finish);

  rc = jbl_as_json(jbl, jbn_json_printer, &pctx, 0);
  RCGO(rc, finish);

  rc = pctx.flushFn(&pctx);

finish:
  if (jbl) {
    jbl_destroy(&jbl);
  }
  jbn_destroy_pctx(&pctx);
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

// DEL
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1del(
  JNIEnv *env,
  jobject thisObj,
  jstring coll_,
  jlong   id) {
  iwrc rc;
  EJDB db;
  const char *coll = (*env)->GetStringUTFChars(env, coll_, 0);
  if (!coll) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }
  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = ejdb_del(db, coll, (int64_t) id);

finish:
  if (coll) {
    (*env)->ReleaseStringUTFChars(env, coll_, coll);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

// RENAME COLLECTION
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1rename_1collection(
  JNIEnv *env, jobject thisObj,
  jstring oldColl_, jstring newColl_) {
  iwrc rc;
  EJDB db;
  const char *newColl = 0;
  const char *oldColl = (*env)->GetStringUTFChars(env, oldColl_, 0);
  if (!oldColl) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }
  newColl = (*env)->GetStringUTFChars(env, newColl_, 0);
  if (!newColl) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }
  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = ejdb_rename_collection(db, oldColl, newColl);

finish:
  if (oldColl) {
    (*env)->ReleaseStringUTFChars(env, oldColl_, oldColl);
  }
  if (newColl) {
    (*env)->ReleaseStringUTFChars(env, newColl_, newColl);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

// PATCH
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1patch(
  JNIEnv  *env,
  jobject  thisObj,
  jstring  coll_,
  jstring  patch_,
  jlong    id,
  jboolean upsert) {
  iwrc rc;
  EJDB db;
  const char *coll = (*env)->GetStringUTFChars(env, coll_, 0);
  const char *patch = (*env)->GetStringUTFChars(env, patch_, 0);
  if (!coll || !patch) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }
  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  if (upsert) {
    rc = ejdb_merge_or_put(db, coll, patch, (int64_t) id);
  } else {
    rc = ejdb_patch(db, coll, patch, (int64_t) id);
  }

finish:
  if (coll) {
    (*env)->ReleaseStringUTFChars(env, coll_, coll);
  }
  if (patch_) {
    (*env)->ReleaseStringUTFChars(env, patch_, patch);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1remove_1collection(
  JNIEnv *env,
  jobject thisObj,
  jstring coll_) {
  iwrc rc;
  EJDB db;
  const char *coll = (*env)->GetStringUTFChars(env, coll_, 0);
  if (!coll) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }
  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = ejdb_remove_collection(db, coll);

finish:
  if (coll) {
    (*env)->ReleaseStringUTFChars(env, coll_, coll);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1ensure_1index(
  JNIEnv *env,
  jobject thisObj,
  jstring coll_,
  jstring path_,
  jint    mode) {

  iwrc rc;
  EJDB db;
  const char *coll = (*env)->GetStringUTFChars(env, coll_, 0);
  const char *path = (*env)->GetStringUTFChars(env, path_, 0);
  if (!coll || !path) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }
  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = ejdb_ensure_index(db, coll, path, (ejdb_idx_mode_t) mode);

finish:
  if (coll) {
    (*env)->ReleaseStringUTFChars(env, coll_, coll);
  }
  if (path) {
    (*env)->ReleaseStringUTFChars(env, path_, path);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2__1remove_1index(
  JNIEnv *env,
  jobject thisObj,
  jstring coll_,
  jstring path_,
  jint    mode) {
  iwrc rc;
  EJDB db;
  const char *coll = (*env)->GetStringUTFChars(env, coll_, 0);
  const char *path = (*env)->GetStringUTFChars(env, path_, 0);
  if (!coll || !path) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }
  rc = jbn_db(env, thisObj, &db);
  RCGO(rc, finish);

  rc = ejdb_remove_index(db, coll, path, (ejdb_idx_mode_t) mode);

finish:
  if (coll) {
    (*env)->ReleaseStringUTFChars(env, coll_, coll);
  }
  if (path) {
    (*env)->ReleaseStringUTFChars(env, path_, path);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

// JQL INIT
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1init(
  JNIEnv *env,
  jobject thisObj,
  jobject dbObj,
  jstring queryStr,
  jstring collStr) {
  EJDB db;
  iwrc rc;
  JQL q = 0;
  const char *query = 0, *coll = 0;

  if (!dbObj || !queryStr) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }

  rc = jbn_db(env, dbObj, &db);
  RCGO(rc, finish);

  query = (*env)->GetStringUTFChars(env, queryStr, 0);
  if (!query) {
    rc = IW_ERROR_INVALID_ARGS;
    goto finish;
  }

  if (collStr) {
    coll = (*env)->GetStringUTFChars(env, collStr, 0);
  }
  rc = jql_create2(&q, coll, query, JQL_KEEP_QUERY_ON_PARSE_ERROR | JQL_SILENT_ON_PARSE_ERROR);
  RCGO(rc, finish);

  (*env)->SetLongField(env, thisObj, k_JQL_handle_fid, (jlong) q);
  if (!coll) {
    collStr = (*env)->NewStringUTF(env, jql_collection(q));
    (*env)->SetObjectField(env, thisObj, k_JQL_collection_fid, collStr);
  }

finish:
  if (query) {
    (*env)->ReleaseStringUTFChars(env, queryStr, query);
  }
  if (coll) {
    (*env)->ReleaseStringUTFChars(env, collStr, coll);
  }
  if (rc) {
    if (q && (rc == JQL_ERROR_QUERY_PARSE)) {
      jbn_throw_rc_exception(env, rc, jql_error(q));
    } else {
      jbn_throw_rc_exception(env, rc, 0);
    }
    if (q) {
      jql_destroy(&q);
    }
  }
}

// JQL RESET
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1reset(JNIEnv *env, jobject thisObj) {
  jlong ptr = (*env)->GetLongField(env, thisObj, k_JQL_handle_fid);
  if (ptr) {
    JQL q = (void*) ptr;
    jql_reset(q, true, true);
  }
}

// JQL DESTROY
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1destroy(JNIEnv *env, jclass clazz, jlong handle) {
  if (handle) {
    JQL q = (void*) handle;
    jql_destroy(&q);
  }
}

typedef struct JBN_EXEC_CTX {
  JNIEnv   *env;
  jobject   cbObj;
  jclass    cbClazz;
  jmethodID cbMid;
} JBN_EXEC_CTX;

static iwrc jbn_exec_visitor(struct _EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  iwrc rc = 0;
  jstring json = 0;
  JBN_EXEC_CTX *ectx = ux->opaque;
  JNIEnv *env = ectx->env;
  IWXSTR *xstr = iwxstr_new2(jbl_size(doc->raw) * 2);
  if (!xstr) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (doc->node) {
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
  } else {
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
  }
  RCGO(rc, finish);

  json = (*env)->NewStringUTF(env, iwxstr_ptr(xstr));
  if (!json) {
    if (!(*env)->ExceptionOccurred(env)) {
      rc = JBN_ERROR_CREATION_OBJ;
    }
    goto finish;
  }
  int64_t llv = (*env)->CallLongMethod(env, ectx->cbObj, ectx->cbMid, (jlong) doc->id, json);
  if (llv < -2) {
    *step = 0;
  } else {
    *step = llv;
  }

finish:
  if (json) {
    (*env)->DeleteLocalRef(env, json);
  }
  iwxstr_destroy(xstr);
  return rc;
}

// JQL EXECUTE
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1execute(
  JNIEnv *env,
  jobject thisObj,
  jobject dbObj,
  jobject cbObj,
  jobject logStreamObj) {
  iwrc rc;
  EJDB db;
  JQL q;
  IWXSTR *log = 0;

  if (!dbObj) {
    jbn_throw_rc_exception(env, IW_ERROR_INVALID_ARGS, 0);
    return;
  }

  rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);

  rc = jbn_db(env, dbObj, &db);
  RCGO(rc, finish);

  JBN_EXEC_CTX ectx = {
    .env   = env,
    .cbObj = cbObj
  };

  if (cbObj) {
    ectx.cbClazz = (*env)->GetObjectClass(env, cbObj);
    ectx.cbMid = (*env)->GetMethodID(env, ectx.cbClazz, "onRecord", "(JLjava/lang/String;)J");
    if (!ectx.cbMid) {
      goto finish;
    }
  }

  jlong skip = (*env)->GetLongField(env, thisObj, k_JQL_skip_fid);
  jlong limit = (*env)->GetLongField(env, thisObj, k_JQL_limit_fid);
  if (logStreamObj) {
    log = iwxstr_new();
    if (!log) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
  }

  EJDB_EXEC ux = {
    .db      = db,
    .q       = q,
    .skip    = skip > 0 ? skip : 0,
    .limit   = limit > 0 ? limit : 0,
    .opaque  = &ectx,
    .visitor = cbObj ? jbn_exec_visitor : 0,
    .log     = log
  };

  rc = ejdb_exec(&ux);
  RCGO(rc, finish);

  if (log) { // Send query execution log
    size_t xsz = iwxstr_size(log);
    jclass logStreamClazz = (*env)->GetObjectClass(env, logStreamObj);
    jmethodID writeMid = (*env)->GetMethodID(env, logStreamClazz, "write", "([B)V");
    if (!writeMid) {
      goto finish;
    }
    jbyteArray arr = (*env)->NewByteArray(env, xsz);
    if (!arr) {
      goto finish;
    }
    (*env)->SetByteArrayRegion(env, arr, 0, xsz, (void*) iwxstr_ptr(log));
    (*env)->CallVoidMethod(env, logStreamObj, writeMid, arr);
  }

finish:
  if (log) {
    iwxstr_destroy(log);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

// JQL EXECUTE SCALAR LONG
JNIEXPORT jlong JNICALL Java_com_softmotions_ejdb2_JQL__1execute_1scalar_1long(
  JNIEnv *env,
  jobject thisObj,
  jobject dbObj,
  jobject logStreamObj) {
  iwrc rc;
  EJDB db;
  JQL q;
  IWXSTR *log = 0;
  jlong ret = 0;

  if (!dbObj) {
    jbn_throw_rc_exception(env, IW_ERROR_INVALID_ARGS, 0);
    return 0;
  }

  rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);

  rc = jbn_db(env, dbObj, &db);
  RCGO(rc, finish);

  jlong skip = (*env)->GetLongField(env, thisObj, k_JQL_skip_fid);
  jlong limit = (*env)->GetLongField(env, thisObj, k_JQL_limit_fid);
  if (logStreamObj) {
    log = iwxstr_new();
    if (!log) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
  }

  EJDB_EXEC ux = {
    .db    = db,
    .q     = q,
    .skip  = skip > 0 ? skip : 0,
    .limit = limit > 0 ? limit : 0,
    .log   = log
  };

  rc = ejdb_exec(&ux);
  RCGO(rc, finish);

  if (log) { // Send query execution log
    size_t xsz = iwxstr_size(log);
    jclass logStreamClazz = (*env)->GetObjectClass(env, logStreamObj);
    jmethodID writeMid = (*env)->GetMethodID(env, logStreamClazz, "write", "([B)V");
    if (!writeMid) {
      goto finish;
    }
    jbyteArray arr = (*env)->NewByteArray(env, xsz);
    if (!arr) {
      goto finish;
    }
    (*env)->SetByteArrayRegion(env, arr, 0, xsz, (void*) iwxstr_ptr(log));
    (*env)->CallVoidMethod(env, logStreamObj, writeMid, arr);
  }

  ret = ux.cnt;

finish:
  if (log) {
    iwxstr_destroy(log);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
  return ret;
}

static void jbn_free_json_node(void *ptr, void *op) {
  IWPOOL *pool = op;
  if (pool) {
    iwpool_destroy(pool);
  }
}

static void jbn_free_str(void *ptr, void *op) {
  free(ptr);
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1set_1string(
  JNIEnv *env,
  jobject thisObj,
  jint    pos,
  jstring placeholder_,
  jstring val_,
  jint    type) {
  JQL q;
  iwrc rc;
  const char *placeholder = 0, *val;

  if (!val_) {
    jbn_throw_rc_exception(env, IW_ERROR_INVALID_ARGS, 0);
    return;
  }
  val = (*env)->GetStringUTFChars(env, val_, 0);

  rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);

  if (placeholder_) {
    placeholder = (*env)->GetStringUTFChars(env, placeholder_, 0);
  }
  if (type == 1) {  // JSON
    JBL_NODE node;
    IWPOOL *pool = iwpool_create(1024);
    if (!pool) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    rc = jbn_from_json(val, &node, pool);
    if (rc) {
      iwpool_destroy(pool);
      goto finish;
    }
    rc = jql_set_json2(q, placeholder, pos, node, jbn_free_json_node, pool);
    if (rc) {
      iwpool_destroy(pool);
      goto finish;
    }
  } else if (type == 2) { // Regexp
    char *str = strdup(val);
    if (!str) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    rc = jql_set_regexp2(q, placeholder, pos, str, jbn_free_str, 0);
    if (rc) {
      free(str);
      goto finish;
    }
  } else { // All other cases
    char *str = strdup(val);
    if (!str) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    rc = jql_set_str2(q, placeholder, pos, str, jbn_free_str, 0);
    if (rc) {
      free(str);
      goto finish;
    }
  }

finish:
  if (val) {
    (*env)->ReleaseStringUTFChars(env, val_, val);
  }
  if (placeholder) {
    (*env)->ReleaseStringUTFChars(env, placeholder_, placeholder);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1set_1long(
  JNIEnv *env,
  jobject thisObj,
  jint    pos,
  jstring placeholder_,
  jlong   val) {

  JQL q;
  iwrc rc;
  const char *placeholder = 0;

  rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);

  if (placeholder_) {
    placeholder = (*env)->GetStringUTFChars(env, placeholder_, 0);
  }

  rc = jql_set_i64(q, placeholder, pos, val);

finish:
  if (placeholder) {
    (*env)->ReleaseStringUTFChars(env, placeholder_, placeholder);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1set_1double(
  JNIEnv *env,
  jobject thisObj,
  jint    pos,
  jstring placeholder_,
  jdouble val) {
  JQL q;
  iwrc rc;
  const char *placeholder = 0;

  rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);

  if (placeholder_) {
    placeholder = (*env)->GetStringUTFChars(env, placeholder_, 0);
  }

  rc = jql_set_f64(q, placeholder, pos, val);

finish:
  if (placeholder) {
    (*env)->ReleaseStringUTFChars(env, placeholder_, placeholder);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1set_1boolean(
  JNIEnv  *env,
  jobject  thisObj,
  jint     pos,
  jstring  placeholder_,
  jboolean val) {
  JQL q;
  iwrc rc;
  const char *placeholder = 0;

  rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);

  if (placeholder_) {
    placeholder = (*env)->GetStringUTFChars(env, placeholder_, 0);
  }

  rc = jql_set_bool(q, placeholder, pos, val);

finish:
  if (placeholder) {
    (*env)->ReleaseStringUTFChars(env, placeholder_, placeholder);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_JQL__1set_1null(
  JNIEnv *env,
  jobject thisObj,
  jint    pos,
  jstring placeholder_) {
  JQL q;
  iwrc rc;
  const char *placeholder = 0;

  rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);

  if (placeholder_) {
    placeholder = (*env)->GetStringUTFChars(env, placeholder_, 0);
  }

  rc = jql_set_null(q, placeholder, pos);

finish:
  if (placeholder) {
    (*env)->ReleaseStringUTFChars(env, placeholder_, placeholder);
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
}

JNIEXPORT jlong JNICALL Java_com_softmotions_ejdb2_JQL__1get_1limit(JNIEnv *env, jobject thisObj) {
  JQL q;
  int64_t limit = 0;
  iwrc rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);
  rc = jql_get_limit(q, &limit);

finish:
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
  return (jlong) limit;
}

JNIEXPORT jlong JNICALL Java_com_softmotions_ejdb2_JQL__1get_1skip(JNIEnv *env, jobject thisObj) {
  JQL q;
  int64_t skip = 0;
  iwrc rc = jbn_jql_q(env, thisObj, &q);
  RCGO(rc, finish);
  rc = jql_get_skip(q, &skip);

finish:
  if (rc) {
    jbn_throw_rc_exception(env, rc, 0);
  }
  return (jlong) skip;
}

static const char *jbn_ecodefn(locale_t locale, uint32_t ecode) {
  if (!((ecode > _JBN_ERROR_START) && (ecode < _JBN_ERROR_END))) {
    return 0;
  }
  switch (ecode) {
    case JBN_ERROR_INVALID_FIELD:
      return "Failed to get class field (JBN_ERROR_INVALID_FIELD)";
    case JBN_ERROR_INVALID_METHOD:
      return "Failed to get class method (JBN_ERROR_INVALID_METHOD)";
    case JBN_ERROR_INVALID_OPTIONS:
      return "Invalid com.softmotions.ejdb2.EJDB2Builder configuration provided (JBN_ERROR_INVALID_OPTIONS)";
    case JBN_ERROR_INVALID_STATE:
      return "Invalid com.softmotions.ejdb2.EJDB2 JNI state. Database closed? (JBN_ERROR_INVALID_STATE)";
    case JBN_ERROR_CREATION_OBJ:
      return "Failed to create/allocate JNI object (JBN_ERROR_CREATION_OBJ)";
  }
  return 0;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env;
  if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }
  static volatile int jbn_ecodefn_initialized = 0;
  if (__sync_bool_compare_and_swap(&jbn_ecodefn_initialized, 0, 1)) {
    iwrc rc = ejdb_init();
    if (rc) {
      iwlog_ecode_error3(rc);
      return JNI_ERR;
    }
    iwlog_register_ecodefn(jbn_ecodefn);
  }

  jclass clazz = (*env)->FindClass(env, "com/softmotions/ejdb2/EJDB2");
  if (!clazz) {
    iwlog_error2("Cannot find com.softmotions.ejdb2.EJDB2 class");
    return -1;
  }
  k_EJDB2_clazz = (*env)->NewGlobalRef(env, clazz);
  k_EJDB2_handle_fid = (*env)->GetFieldID(env, k_EJDB2_clazz, "_handle", "J");


  clazz = (*env)->FindClass(env, "com/softmotions/ejdb2/EJDB2Exception");
  if (!clazz) {
    iwlog_error2("Cannot find com.softmotions.ejdb2.EJDB2Exception class");
    return -1;
  }
  k_EJDB2Exception_clazz = (*env)->NewGlobalRef(env, clazz);
  k_EJDB2Exception_constructor = (*env)->GetMethodID(env, k_EJDB2Exception_clazz,
                                                     "<init>", "(JJLjava/lang/String;)V");
  if (!k_EJDB2Exception_constructor) {
    iwlog_error2("Cannot find com.softmotions.ejdb2.EJDB2Exception#<init>(long,String)");
    return -1;
  }

  clazz = (*env)->FindClass(env, "com/softmotions/ejdb2/JQL");
  if (!clazz) {
    iwlog_error2("Cannot find com.softmotions.ejdb2.JQL class");
    return -1;
  }
  k_JQL_clazz = (*env)->NewGlobalRef(env, clazz);
  k_JQL_handle_fid = (*env)->GetFieldID(env, k_JQL_clazz, "_handle", "J");
  k_JQL_db_fid = (*env)->GetFieldID(env, k_JQL_clazz, "db", "Lcom/softmotions/ejdb2/EJDB2;");
  k_JQL_query_fid = (*env)->GetFieldID(env, k_JQL_clazz, "query", "Ljava/lang/String;");
  k_JQL_collection_fid = (*env)->GetFieldID(env, k_JQL_clazz, "collection", "Ljava/lang/String;");
  k_JQL_skip_fid = (*env)->GetFieldID(env, k_JQL_clazz, "skip", "J");
  k_JQL_limit_fid = (*env)->GetFieldID(env, k_JQL_clazz, "limit", "J");

  return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) { // Not really useless
  JNIEnv *env;
  if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
    return;
  }
  if (k_EJDB2_clazz) {
    (*env)->DeleteGlobalRef(env, k_EJDB2_clazz);
  }
  if (k_JQL_clazz) {
    (*env)->DeleteGlobalRef(env, k_JQL_clazz);
  }
  if (k_EJDB2Exception_clazz) {
    (*env)->DeleteGlobalRef(env, k_EJDB2Exception_clazz);
  }
}
