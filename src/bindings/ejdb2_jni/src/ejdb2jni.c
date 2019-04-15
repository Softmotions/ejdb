#include <ejdb2.h>
#include <jni.h>
#include "com_softmotions_ejdb2_EJDB2.h"

typedef struct JbnStr {
  const char *utf;
  jstring str;
} JbnStr;

typedef enum {
  _JBN_ERROR_START = (IW_ERROR_START + 15000UL + 5000),
  JBN_ERROR_INVALID_OPTIONS,        /**< Invalid com.softmotions.ejdb2.EJDB2Options instance provided  (JBN_ERROR_INVALID_OPTIONS) */
  _JBN_ERROR_END,
} jbn_ecode_t;


#define JBNFIELD(fid_, env_, clazz_, name_, type_, rc_)     \
  fid_ = (*(env_))->GetFieldID(env_, clazz_, name_, type_); \
  if (!fid_) {                                              \
    jbn_throw_rc_exception(env, rc_);                       \
    return;                                                 \
  }

#define JBNFIELD2(fid_, env_, clazz_, name_, type_, rc_, label_)  \
  fid_ = (*(env_))->GetFieldID(env_, clazz_, name_, type_);       \
  if (!fid_) {                                                    \
    jbn_throw_rc_exception(env, rc_);                             \
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
 * Signature: (Lcom/softmotions/ejdb2/EJDB2Options;)V
 */
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2_open(JNIEnv *env, jobject thisObj, jobject optsObj) {
  iwrc rc = 0;
  EJDB_OPTS opts = {0};
  JNIEnv e = *env;
  jobject iwkv, http, wal;
  jclass iwkvClazz;
  jclass optsClazz = e->GetObjectClass(env, optsObj);

  int sc = 0;
  JbnStr strings[2] = {0};

  jfieldID fid = e->GetFieldID(env, optsClazz, "iwkv", "Lcom/softmotions/ejdb2/IWKVOptions;");
  JBNFIELD(fid, env, optsClazz, "iwkv", "Lcom/softmotions/ejdb2/IWKVOptions;", JBN_ERROR_INVALID_OPTIONS);

  iwkv = e->GetObjectField(env, optsObj, fid);
  if (!iwkv) {
    jbn_throw_rc_exception(env, JBN_ERROR_INVALID_OPTIONS);
    return;
  }
  iwkvClazz = e->GetObjectClass(env, iwkv);

  JBNFIELD(fid, env, optsClazz, "http", "Lcom/softmotions/ejdb2/EJDB2Options$EJDB2HttpOptions;",
           JBN_ERROR_INVALID_OPTIONS);
  http = e->GetObjectField(env, optsObj, fid);

  JBNFIELD(fid, env, optsClazz, "no_wal", "Z", JBN_ERROR_INVALID_OPTIONS);
  opts.no_wal = e->GetBooleanField(env, optsObj, fid);

  JBNFIELD(fid, env, optsClazz, "sort_buffer_sz", "I", JBN_ERROR_INVALID_OPTIONS);
  opts.sort_buffer_sz = (uint32_t) e->GetIntField(env, optsClazz, fid);

  JBNFIELD(fid, env, optsClazz, "document_buffer_sz", "I", JBN_ERROR_INVALID_OPTIONS);
  opts.document_buffer_sz = (uint32_t) e->GetIntField(env, optsClazz, fid);

  JBNFIELD(fid, env, iwkvClazz, "wal", "Lcom/softmotions/ejdb2/IWKVOptions$WALOptions;", JBN_ERROR_INVALID_OPTIONS);
  wal = e->GetObjectField(env, iwkv, fid);

  JBNFIELD(fid, env, iwkvClazz, "path", "Ljava/lang/String;", JBN_ERROR_INVALID_OPTIONS);
  strings[sc].str = e->GetObjectField(env, iwkv, fid);
  strings[sc].utf = e->GetStringUTFChars(env, strings[sc].str, 0);
  sc++;
  opts.kv.path = strings[sc].utf;
  if (!opts.kv.path) {
    rc = JBN_ERROR_INVALID_OPTIONS;
    goto finish;
  }

finish:
  for (int i = 0; i < (sizeof(strings) / sizeof(strings[0])); ++i)   {
    if (strings[i].str) {
      (*env)->ReleaseStringUTFChars(env, strings[i].str, strings[i].utf);
    }
  }
  if (rc) {
    jbn_throw_rc_exception(env, rc);
  }
}

/*
 * Class:     com_softmotions_iowow_IWKV
 * Method:    dispose
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_softmotions_ejdb2_EJDB2_dispose(JNIEnv *env, jobject thisObj) {

}

static const char *jbn_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JBN_ERROR_START && ecode < _JBN_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case JBN_ERROR_INVALID_OPTIONS:
      return "Invalid com.softmotions.ejdb2.EJDB2Options instance provided  (JBN_ERROR_INVALID_OPTIONS)";
  }
  return 0;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  static volatile int jbn_ecodefn_initialized = 0;
  if (__sync_bool_compare_and_swap(&jbn_ecodefn_initialized, 0, 1)) {
    iwrc rc = ejdb_init();
    if (rc) {
      iwlog_ecode_error3(rc);
      return JNI_ERR;
    }
    iwlog_register_ecodefn(jbn_ecodefn);
  }
  return 0;
}


JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
}
