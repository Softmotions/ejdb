#include <ejdb2.h>
#include <jni.h>
#include <string.h>
#include "com_softmotions_ejdb2_EJDB2.h"

typedef struct JbnStr {
  const char *utf;
  jstring str;
} JbnStr;

typedef enum {
  _JBN_ERROR_START = (IW_ERROR_START + 15000UL + 5000),
  JBN_ERROR_INVALID_FIELD,          /**< Failed to get class field (JBN_ERROR_INVALID_FIELD) */
  JBN_ERROR_INVALID_OPTIONS,        /**< Invalid com.softmotions.ejdb2.EJDB2Builder configuration provided (JBN_ERROR_INVALID_OPTIONS) */
  _JBN_ERROR_END,
} jbn_ecode_t;


#define JBNFIELD(fid_, env_, clazz_, name_, type_)     \
  fid_ = (*(env_))->GetFieldID(env_, clazz_, name_, type_); \
  if (!fid_) {                                              \
    jbn_throw_rc_exception(env, JBN_ERROR_INVALID_FIELD);   \
    return;                                                 \
  }

#define JBNFIELD2(fid_, env_, clazz_, name_, type_, label_)  \
  fid_ = (*(env_))->GetFieldID(env_, clazz_, name_, type_);       \
  if (!fid_) {                                                    \
    jbn_throw_rc_exception(env, JBN_ERROR_INVALID_FIELD);         \
    goto label_;                                                  \
  }

static jint jbn_throw_noclassdef(JNIEnv *env, const char *message) {
  char *className = "java/lang/NoClassDefFoundError";
  jclass clazz = (*env)->FindClass(env, className);
  if (!clazz) {
    return JNI_ERR;
  }
  return (*env)->ThrowNew(env, clazz, message);
}

static jint jbn_throw_rc_exception(JNIEnv *env, iwrc rc) {
  const char *className = "com/softmotions/ejdb2/EJDB2Exception";
  jclass clazz = (*env)->FindClass(env, className);
  if (!clazz) {
    return jbn_throw_noclassdef(env, className);
  }
  const char *msg = iwlog_ecode_explained(rc);
  return (*env)->ThrowNew(env, clazz, msg ? msg : "Unknown iwrc error");
}

/*
 * Class:     com_softmotions_ejdb2_EJDB2
 * Method:    open
 * Signature: (Lcom/softmotions/ejdb2/EJDB2Builder;)V
 */
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2_open(JNIEnv *env, jobject thisObj, jobject optsObj) {
  iwrc rc = 0;
  EJDB_OPTS opts = {0};
  JNIEnv e = *env;
  jfieldID fid;
  jobject iwkv, http, wal;
  jclass iwkvClazz, httpClazz, walClazz;
  jclass optsClazz = e->GetObjectClass(env, optsObj);
  jclass thisClazz = e->GetObjectClass(env, thisObj);

  int sc = 0;
  EJDB db = 0;
  JbnStr strings[3] = {0};

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
    jbn_throw_rc_exception(env, JBN_ERROR_INVALID_OPTIONS);
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
    jbn_throw_rc_exception(env, JBN_ERROR_INVALID_OPTIONS);
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
  opts.http.enabled = e->GetBooleanField(env, wal, fid);

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
  opts.http.read_anon = e->GetBooleanField(env, wal, fid);

  JBNFIELD2(fid, env, httpClazz, "max_body_size", "I", finish);
  opts.http.max_body_size = (size_t) e->GetIntField(env, http, fid);

  JBNFIELD2(fid, env, thisClazz, "_handle", "J", finish);

  rc = ejdb_open(&opts, &db);
  RCGO(rc, finish);

  e->SetLongField(env, thisObj, fid, (jlong) db);

finish:
  for (int i = 0; i < (sizeof(strings) / sizeof(strings[0])); ++i)   {
    if (strings[i].str) {
      e->ReleaseStringUTFChars(env, strings[i].str, strings[i].utf);
    }
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc);
  }
}

/*
 * Class:     com_softmotions_ejdb2_EJDB2
 * Method:    dispose
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2_dispose(JNIEnv *env, jobject thisObj) {
  jfieldID fid;
  jclass thisClazz = (*env)->GetObjectClass(env, thisObj);
  JBNFIELD(fid, env, thisClazz, "_handle", "J");
  jlong ptr = (*env)->GetLongField(env, thisObj, fid);
  if (ptr) {
    (*env)->SetLongField(env, thisObj, fid, 0);
    EJDB db = (void *) ptr;
    iwrc rc = ejdb_close(&db);
    if (rc) {
      jbn_throw_rc_exception(env, rc);
    }
  }
}

static const char *jbn_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JBN_ERROR_START && ecode < _JBN_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case JBN_ERROR_INVALID_FIELD:
      return "Failed to get class field (JBN_ERROR_INVALID_FIELD)";
    case JBN_ERROR_INVALID_OPTIONS:
      return "Invalid com.softmotions.ejdb2.EJDB2Builder configuration provided (JBN_ERROR_INVALID_OPTIONS)";
  }
  return 0;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv* env;
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
  // todo: register natives?
  return JNI_VERSION_1_6;
}
