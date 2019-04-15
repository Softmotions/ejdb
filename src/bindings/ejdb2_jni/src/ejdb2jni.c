#include <ejdb2.h>
#include <jni.h>

static jint throwNoClassDefError(JNIEnv *env, const char *message) {
  char *className = "java/lang/NoClassDefFoundError";
  jclass clazz = (*env)->FindClass(env, className);
  if (!clazz) {
    return JNI_ERR;
  }
  return (*env)->ThrowNew(env, clazz, message);
}

static jint throwEJDB2ExceptionRC(JNIEnv *env, iwrc rc) {
  const char *className = "com/softmotions/ejdb2/EJDB2Exception";
  jclass clazz = (*env)->FindClass(env, className);
  if (!clazz) {
    return throwNoClassDefError(env, className);
  }
  const char *msg = iwlog_ecode_explained(rc);
  return (*env)->ThrowNew(env, clazz, msg ? msg : "Unknown iwrc error");
}
