#include <stdio.h>
#include "jejdb.h"

static void set_error(JNIEnv * env, int code, const char *message) {
	jclass eclazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBException");
	jmethodID ecid = (*env)->GetMethodID(env, eclazz, "<init>", "(ILjava/lang/String;)V");
	jstring jmessage = (*env)->NewStringUTF(env, message);
	jobject exception = (*env)->NewObject(env, eclazz, ecid, (jint)code, jmessage);

	(*env)->Throw(env, exception);
};

static void set_ejdb_error(JNIEnv * env, EJDB *db) {
	int code = ejdbecode(db);
	set_error(env, code, ejdberrmsg(code));
};

/*
* Class:     org_ejdb_driver_EJDB
* Method:    openDB
* Signature: (Ljava/lang/String;I)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDB_openDB
	(JNIEnv *env, jobject obj, jstring path, jint mode) {
		EJDB* db = ejdbnew();

		if (!db) {
			set_error(env, 0, "Could not create EJDB");
			return;
		}

		const char *dbpath;
		dbpath = (*env)->GetStringUTFChars(env, path, NULL);
		bool status = ejdbopen(db, dbpath, mode);
		(*env)->ReleaseStringUTFChars(env, path, dbpath);

		if (!status) {
			set_ejdb_error(env, db);
			return;
		}

		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		(*env)->SetLongField(env, obj, dbpID, (jlong)db);

		return;
};

/*
* Class:     org_ejdb_driver_EJDB
* Method:    isOpenDB
* Signature: ()Z
*/
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDB_isOpenDB
	(JNIEnv *env, jobject obj) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		return ejdbisopen((EJDB*)dbp);
};

/*
* Class:     org_ejdb_driver_EJDB
* Method:    closeDB
* Signature: ()V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDB_closeDB
	(JNIEnv *env, jobject obj) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		EJDB* db = (EJDB*)dbp;

		if (!ejdbclose(db)) {
			set_ejdb_error(env, db);
		}
		ejdbdel(db);
};

/*
* Class:     org_ejdb_driver_EJDB
* Method:    syncDB
* Signature: ()V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDB_syncDB
	(JNIEnv *env, jobject obj) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		EJDB* db = (EJDB*)dbp;

		if (!ejdbsyncdb(db)) {
			set_ejdb_error(env, db);
		}
};

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    ensureDB
* Signature: (Ljava/lang/Object;)Z
*/
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDBCollection_ensureDB
	(JNIEnv *env, jobject obj, jobject opts) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		// todo: check null?
		EJDB* db = (EJDB*)dbp;

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);

		EJCOLLOPTS jcopts;
		memset(&jcopts, 0, sizeof (jcopts));

		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		EJCOLL *coll = ejdbcreatecoll(db, cname, &jcopts);
		(*env)->ReleaseStringUTFChars(env, colname, cname);

		return !coll ? JNI_FALSE : JNI_TRUE;
};


/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    dropDB
* Signature: (Z)Z
*/
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDBCollection_dropDB
	(JNIEnv *env, jobject obj, jboolean prune) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		// todo: check null?
		EJDB* db = (EJDB*)dbp;

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);

		const char *cname;
		cname = (*env)->GetStringUTFChars(env, colname, NULL);

		bool status = ejdbrmcoll(db, cname, (prune == JNI_TRUE));

		(*env)->ReleaseStringUTFChars(env, colname, cname);

		return status ? JNI_TRUE : JNI_FALSE;
};

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    syncDB
* Signature: ()V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_syncDB
	(JNIEnv *env, jobject obj) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		EJDB* db = (EJDB*)dbp;

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);

		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		EJCOLL *coll = ejdbgetcoll(db, cname);
		(*env)->ReleaseStringUTFChars(env, colname, cname);
		
		if (!coll) {
			set_error(env, 0, "Collection not found");
			return;
		}

		if (!ejdbsyncoll(coll)) {
			set_ejdb_error(env, db);
		}
};

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    loadDB
* Signature: ([B)Ljava/lang/Object;
*/
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBCollection_loadDB
	(JNIEnv *env, jobject obj, jbyteArray oidArray) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		EJDB* db = (EJDB*)dbp;

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);
		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		EJCOLL * coll = ejdbgetcoll(db, cname);
		(*env)->ReleaseStringUTFChars(env, colname, cname);

		if (!coll) {
			return NULL;
		}

		bson_oid_t *oid = (void*)(*env)->GetByteArrayElements(env, oidArray, NULL);
		bson* bson = ejdbloadbson(coll, oid);
		(*env)->ReleaseByteArrayElements(env, oidArray, (jbyte*)oid, 0);

		if (!bson) {
			return NULL;
		}

		jmethodID method = (*env)->GetStaticMethodID(env, clazz, "handleBSONData", "(Ljava/nio/ByteBuffer;)Ljava/lang/Object;");
		jobject buff = (*env)->NewDirectByteBuffer(env, (void*)bson_data(bson), bson_size(bson));
		jobject result = (*env)->CallStaticObjectMethod(env, clazz, method, buff);
		(*env)->DeleteLocalRef(env, buff);
		bson_del(bson);

		return result;
}

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    saveDB
* Signature: ([B)Ljava/lang/Object;
*/
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBCollection_saveDB
	(JNIEnv *env, jobject obj, jbyteArray objdata) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		// todo: check null?
		EJDB* db = (EJDB*)dbp;

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);

		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		jbyte *bdata = (*env)->GetByteArrayElements(env, objdata, NULL);
		jsize blength = (*env)->GetArrayLength(env, objdata);

		bson_oid_t oid;

		// todo: check
		EJCOLL * coll = ejdbgetcoll(db, cname);

		bson *bson = bson_create_from_buffer(bdata, blength);
		bool status = ejdbsavebson(coll, bson, &oid);
		bson_del(bson);

		(*env)->ReleaseStringUTFChars(env, colname, cname);
		(*env)->ReleaseByteArrayElements(env, objdata, bdata, 0);


		if (!status) {
			// todo: error?
			return NULL;
		}

		jmethodID method = (*env)->GetStaticMethodID(env, clazz, "handleObjectIdData", "(Ljava/nio/ByteBuffer;)Ljava/lang/Object;");

		jobject buff = (*env)->NewDirectByteBuffer(env, (void*)&oid, sizeof(oid));
		jobject result = (*env)->CallStaticObjectMethod(env, clazz, method, buff);

		(*env)->DeleteLocalRef(env, buff);

		return result;
}

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    removeDB
* Signature: ([B)Z
*/
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDBCollection_removeDB
	(JNIEnv *env, jobject obj, jbyteArray oidArray) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, obj, dbpID);

		// todo: check null?
		EJDB* db = (EJDB*)dbp;

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);

		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		bson_oid_t *oid = (void*)(*env)->GetByteArrayElements(env, oidArray, NULL);

		// todo: check
		EJCOLL * coll = ejdbgetcoll(db, cname);

		bool status = ejdbrmbson(coll, oid);

		(*env)->ReleaseStringUTFChars(env, colname, cname);
		(*env)->ReleaseByteArrayElements(env, oidArray, (jbyte*)oid, 0);

		return status ? JNI_TRUE : JNI_FALSE;
}

/*
* Class:     org_ejdb_driver_EJDBQuery
* Method:    createDB
* Signature: (Lorg/bson/BSONObject;)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBQuery_createDB
	(JNIEnv *env, jobject obj, jobject qobj) {
		// todo:
		return;
};