#include <stdio.h>
#include "jejdb.h"

/*
 * Class:     org_ejdb_driver_EJDB
 * Method:    openDB
 * Signature: (Ljava/lang/String;I)J
 */
JNIEXPORT jlong JNICALL Java_org_ejdb_driver_EJDB_openDB
  (JNIEnv *env, jobject ojb, jstring path, jint mode) {
    EJDB* db = ejdbnew();

    const char *dbpath;
    dbpath = (*env)->GetStringUTFChars(env, path, NULL);
    ejdbopen(db, dbpath, mode);
    (*env)->ReleaseStringUTFChars(env, path, dbpath);

    return (jlong)db;
};

/*
 * Class:     org_ejdb_driver_EJDB
 * Method:    isOpenDB
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDB_isOpenDB
  (JNIEnv *env, jobject obj, jlong dbp) {
    EJDB* db = (EJDB*)dbp;

    return ejdbisopen(db);
};

/*
 * Class:     org_ejdb_driver_EJDB
 * Method:    closeDB
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDB_closeDB
  (JNIEnv *env, jobject obj, jlong dbp) {
    EJDB* db = (EJDB*)dbp;

    ejdbclose(db);
    ejdbdel(db);
};

/*
 * Class:     org_ejdb_driver_EJDBCollection
 * Method:    ensureCollectionDB
 * Signature: (JLjava/lang/String;Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDBCollection_ensureCollectionDB
  (JNIEnv *env, jobject obj, jlong dbp, jstring colname, jobject opts) {
    EJDB* db = (EJDB*)dbp;

    EJCOLLOPTS jcopts;
    memset(&jcopts, 0, sizeof (jcopts));

    const char *cname;
    cname = (*env)->GetStringUTFChars(env, colname, NULL);

    EJCOLL *coll = ejdbcreatecoll(db, cname, &jcopts);

    (*env)->ReleaseStringUTFChars(env, colname, cname);

    return !coll ? JNI_FALSE : JNI_TRUE;
};


/*
 * Class:     org_ejdb_driver_EJDBCollection
 * Method:    dropCollectionDB
 * Signature: (JLjava/lang/String;Z)Z
 */
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDBCollection_dropCollectionDB
  (JNIEnv *env, jobject obj, jlong dbp, jstring colname, jboolean prune) {
    EJDB* db = (EJDB*)dbp;

    const char *cname;
    cname = (*env)->GetStringUTFChars(env, colname, NULL);

    bool status = ejdbrmcoll(db, cname, (prune == JNI_TRUE));

    (*env)->ReleaseStringUTFChars(env, colname, cname);

    return status ? JNI_TRUE : JNI_FALSE;
};

/*
 * Class:     org_ejdb_driver_EJDBCollection
 * Method:    loadDB
 * Signature: (JLjava/lang/String;[B)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBCollection_loadDB
  (JNIEnv *env, jobject obj, jlong dbp, jstring colname, jbyteArray oidArray) {
    EJDB* db = (EJDB*)dbp;

    const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
    bson_oid_t *oid = (jbyte*)(*env)->GetByteArrayElements(env, oidArray, NULL);

    // todo: check
    EJCOLL * coll = ejdbgetcoll(db, cname);

    bson* bson = ejdbloadbson(coll, oid);

    (*env)->ReleaseStringUTFChars(env, colname, cname);
    (*env)->ReleaseByteArrayElements(env, oidArray, (jbyte*)oid, 0);

    if (!bson) {
        return NULL;
//        result = (*env)->NewByteArray(env, 0);
    }
        jclass clazz = (*env)->GetObjectClass(env, obj);
        jmethodID method = (*env)->GetStaticMethodID(env, clazz, "handleBSONData", "(Ljava/nio/ByteBuffer;)Ljava/lang/Object;");
        // todo: checks

//        printf("%d", bson_size(bson));

//        jbyteArray buff = (*env)->NewByteArray(env, bson_size(bson));
//        memcpy(buff, bson_data(bson), bson_size(bson));
//        jbyte* tbuff;
//        (*env)>GetByteArrayRegion(env, buff, 0, bson_size(bson), tbuff);
//        (*env)>SetByteArrayRegion(env, buff, (jsize)0, (jsize)bson_size(bson), (jbyte*)bson_data(bson));
//
//        printf("0x%x", bson_data(bson));

//        void *bb = malloc(bson_size(bson));
//        memcpy(bb, bson_data(bson), bson_size(bson));
//
        jobject buff = (*env)->NewDirectByteBuffer(env, (void*)bson_data(bson), bson_size(bson));
        jobject result = (*env)->CallStaticObjectMethod(env, clazz, method, buff);

//        free (bb);
        (*env)->DeleteLocalRef(env, buff);
//        (*env)->DeleteLocalRef(env, method);
//        (*env)->DeleteLocalRef(env, clazz);
        bson_del(bson);

    return result;
}

/*
 * Class:     org_ejdb_driver_EJDBCollection
 * Method:    saveDB
 * Signature: (JLjava/lang/String;[B)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBCollection_saveDB
  (JNIEnv *env, jobject obj, jlong dbp, jstring colname, jbyteArray objdata) {
    EJDB* db = (EJDB*)dbp;

    const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
    jbyte *bdata = (*env)->GetByteArrayElements(env, objdata, NULL);
    jsize blength = (*env)->GetArrayLength(env, objdata);

    bson_oid_t oid;

    // todo: check
    EJCOLL * coll = ejdbgetcoll(db, cname);

    bson *bson = bson_create_from_buffer(bdata, blength);
    bool ss = ejdbsavebson(coll, bson, &oid);
    bson_del(bson);

    (*env)->ReleaseStringUTFChars(env, colname, cname);
    (*env)->ReleaseByteArrayElements(env, objdata, bdata, 0);


    if (!ss) {
        // todo: error
        return NULL;
    }

    jclass clazz = (*env)->GetObjectClass(env, obj);
    jmethodID method = (*env)->GetStaticMethodID(env, clazz, "handleObjectIdData", "(Ljava/nio/ByteBuffer;)Ljava/lang/Object;");

        jobject buff = (*env)->NewDirectByteBuffer(env, (void*)&oid, sizeof(oid));
        jobject result = (*env)->CallStaticObjectMethod(env, clazz, method, buff);

//        free (bb);
        (*env)->DeleteLocalRef(env, buff);
//        (*env)->DeleteLocalRef(env, method);
//        (*env)->DeleteLocalRef(env, clazz);

    return result;

}
