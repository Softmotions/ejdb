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

		if (ejdbisopen(db)) {
			if (!ejdbclose(db)) {
				set_ejdb_error(env, db);
			}
			if (NULL != db) {
				ejdbdel(db);
			}
		}

		(*env)->SetLongField(env, obj, dbpID, (jlong)NULL);
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
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			return;
		}

		if (!ejdbsyncdb(db)) {
			set_ejdb_error(env, db);
		}
};

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    ensureDB
* Signature: (Ljava/lang/Object;)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_ensureDB
	(JNIEnv *env, jobject obj, jobject opts) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dboID = (*env)->GetFieldID(env, clazz, "db", "Lorg/ejdb/driver/EJDB;");
		jobject dbo = (*env)->GetObjectField(env, obj, dboID);
		jclass dclazz = (*env)->GetObjectClass(env, dbo);
		jfieldID dbpID = (*env)->GetFieldID(env, dclazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, dbo, dbpID);

		EJDB* db = (EJDB*)dbp;
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			return;
		}

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);

		EJCOLLOPTS jcopts;
		memset(&jcopts, 0, sizeof (jcopts));
		// todo: open options

		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		EJCOLL *coll = ejdbcreatecoll(db, cname, &jcopts);
		(*env)->ReleaseStringUTFChars(env, colname, cname);

		if (!coll) {
			set_ejdb_error(env, db);
			return;
		}
};


/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    dropDB
* Signature: (Z)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_dropDB
	(JNIEnv *env, jobject obj, jboolean prune) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dboID = (*env)->GetFieldID(env, clazz, "db", "Lorg/ejdb/driver/EJDB;");
		jobject dbo = (*env)->GetObjectField(env, obj, dboID);
		jclass dclazz = (*env)->GetObjectClass(env, dbo);
		jfieldID dbpID = (*env)->GetFieldID(env, dclazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, dbo, dbpID);

		EJDB* db = (EJDB*)dbp;
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			return;
		}

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);

		const char *cname;
		cname = (*env)->GetStringUTFChars(env, colname, NULL);
		bool status = ejdbrmcoll(db, cname, (prune == JNI_TRUE));
		(*env)->ReleaseStringUTFChars(env, colname, cname);

		if (!status) {
			set_ejdb_error(env, db);
			return;
		}
};

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    syncDB
* Signature: ()V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_syncDB
	(JNIEnv *env, jobject obj) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dboID = (*env)->GetFieldID(env, clazz, "db", "Lorg/ejdb/driver/EJDB;");
		jobject dbo = (*env)->GetObjectField(env, obj, dboID);
		jclass dclazz = (*env)->GetObjectClass(env, dbo);
		jfieldID dbpID = (*env)->GetFieldID(env, dclazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, dbo, dbpID);

		EJDB* db = (EJDB*)dbp;
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			return;
		}

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
		jfieldID dboID = (*env)->GetFieldID(env, clazz, "db", "Lorg/ejdb/driver/EJDB;");
		jobject dbo = (*env)->GetObjectField(env, obj, dboID);
		jclass dclazz = (*env)->GetObjectClass(env, dbo);
		jfieldID dbpID = (*env)->GetFieldID(env, dclazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, dbo, dbpID);

		EJDB* db = (EJDB*)dbp;
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			return NULL;
		}

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
		jfieldID dboID = (*env)->GetFieldID(env, clazz, "db", "Lorg/ejdb/driver/EJDB;");
		jobject dbo = (*env)->GetObjectField(env, obj, dboID);
		jclass dclazz = (*env)->GetObjectClass(env, dbo);
		jfieldID dbpID = (*env)->GetFieldID(env, dclazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, dbo, dbpID);

		EJDB* db = (EJDB*)dbp;
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			return NULL;
		}

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);
		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		// todo: check
		EJCOLL * coll = ejdbcreatecoll(db, cname, NULL);
		(*env)->ReleaseStringUTFChars(env, colname, cname);

		if (!coll) {
			set_ejdb_error(env, db);
			return NULL;
		}

		bson_oid_t oid;

		jbyte *bdata = (*env)->GetByteArrayElements(env, objdata, NULL);
		jsize blength = (*env)->GetArrayLength(env, objdata);
		bson *bson = bson_create_from_buffer(bdata, blength);
		bool status = ejdbsavebson(coll, bson, &oid);
		bson_del(bson);
		(*env)->ReleaseByteArrayElements(env, objdata, bdata, 0);

		if (!status) {
			set_ejdb_error(env, db);
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
* Signature: ([B)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_removeDB
	(JNIEnv *env, jobject obj, jbyteArray oidArray) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID dboID = (*env)->GetFieldID(env, clazz, "db", "Lorg/ejdb/driver/EJDB;");
		jobject dbo = (*env)->GetObjectField(env, obj, dboID);
		jclass dclazz = (*env)->GetObjectClass(env, dbo);
		jfieldID dbpID = (*env)->GetFieldID(env, dclazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, dbo, dbpID);

		EJDB* db = (EJDB*)dbp;
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			return;
		}

		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);
		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		EJCOLL * coll = ejdbgetcoll(db, cname);
		(*env)->ReleaseStringUTFChars(env, colname, cname);

		if (!coll) {
			return;
		}

		bson_oid_t *oid = (void*)(*env)->GetByteArrayElements(env, oidArray, NULL);
		bool status = ejdbrmbson(coll, oid);
		(*env)->ReleaseByteArrayElements(env, oidArray, (jbyte*)oid, 0);

		if (!status) {
			set_ejdb_error(env, db);
			return;
		}
}

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    setIndexDB
* Signature: (Ljava/lang/String;I)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_setIndexDB
	(JNIEnv *env, jobject obj, jstring pathstr, jint flags) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID colnameID = (*env)->GetFieldID(env, clazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, obj, colnameID);
		jfieldID dboID = (*env)->GetFieldID(env, clazz, "db", "Lorg/ejdb/driver/EJDB;");
		jobject dbo = (*env)->GetObjectField(env, obj, dboID);
		jclass dclazz = (*env)->GetObjectClass(env, dbo);
		jfieldID dbpID = (*env)->GetFieldID(env, dclazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, dbo, dbpID);

		EJDB* db = (EJDB*)dbp;
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			return;
		}

		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		EJCOLL * coll = ejdbcreatecoll(db, cname, NULL);
		(*env)->ReleaseStringUTFChars(env, colname, cname);

		if (!coll) {
			set_ejdb_error(env, db);
			return;
		}

		const char * path = (*env)->GetStringUTFChars(env, pathstr, NULL);
		bool status = ejdbsetindex(coll, path, flags);
		(*env)->ReleaseStringUTFChars(env, pathstr, path);

		if (!status) {
			set_ejdb_error(env, db);
			return;
		}
};

/*
* Class:     org_ejdb_driver_EJDBQuery
* Method:    executeDB
* Signature: (Lorg/bson/BSONObject;[Lorg/bson/BSONObject;Lorg/bson/BSONObject;I)Lorg/ejdb/driver/EJDBQuery$QResult;
*/
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBQuery_executeDB
	(JNIEnv *env, jobject obj, jobject qobj, jobjectArray qorarrobj, jobject hobj, jint flags) {
		jclass clazz = (*env)->GetObjectClass(env, obj);
		jfieldID coID = (*env)->GetFieldID(env, clazz, "coll", "Lorg/ejdb/driver/EJDBCollection;");
		jobject collobj = (*env)->GetObjectField(env, obj, coID);
		jclass cclazz = (*env)->GetObjectClass(env, collobj);
		jfieldID colnameID = (*env)->GetFieldID(env, cclazz, "cname", "Ljava/lang/String;");
		jstring colname = (*env)->GetObjectField(env, collobj, colnameID);
		jfieldID dboID = (*env)->GetFieldID(env, cclazz, "db", "Lorg/ejdb/driver/EJDB;");
		jobject dbo = (*env)->GetObjectField(env, collobj, dboID);
		jclass dclazz = (*env)->GetObjectClass(env, dbo);
		jfieldID dbpID = (*env)->GetFieldID(env, dclazz, "dbPointer", "J");
		jlong dbp = (*env)->GetLongField(env, dbo, dbpID);

		jclass jBSONClazz = (*env)->FindClass(env, "org/bson/BSON");
		jmethodID encodeMethodID = (*env)->GetStaticMethodID(env, jBSONClazz, "encode", "(Lorg/bson/BSONObject;)[B");

		jclass jQResultClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBQuery$QResult");
		jmethodID initQResultMethodID = (*env)->GetMethodID(env, jQResultClazz, "<init>", "(JJ)V");
		
		bson *qbson = NULL;
		bson *qorbsons = NULL;
		bson *qhbson = NULL;

		EJQ *q = NULL;

		jobject qresult = NULL;

		EJDB* db = (EJDB*)dbp;
		if (!ejdbisopen(db)) {
			set_error(env, 0, "EJDB not opened");
			goto finish;
		}

		jsize blength = 0;
		jbyte *bdata = NULL;
		jbyteArray bobjdata;

		bobjdata = (*env)->CallStaticObjectMethod(env, jBSONClazz, encodeMethodID, qobj);
		bdata = (*env)->GetByteArrayElements(env, bobjdata, NULL);
		blength = (*env)->GetArrayLength(env, bobjdata);
		qbson = bson_create_from_buffer(bdata, blength);
		(*env)->ReleaseByteArrayElements(env, bobjdata, bdata, 0);
		(*env)->DeleteLocalRef(env, bobjdata);

		// todo: check query bson
		if (!qbson) {
			//
			goto finish;
		}

		jsize qorz = NULL != qorarrobj ? (*env)->GetArrayLength(env, qorarrobj) : 0;
		if (qorz > 0) {
			qorbsons = (bson*)malloc(qorz * sizeof(bson));
			// todo: check memory allocation
			if (!qorbsons) {
				set_error(env, 0, "Not enought memory");
				goto finish;
			}

			for (jsize i = 0; i < qorz; ++i) {
				jobject qorobj = (*env)->GetObjectArrayElement(env, qorarrobj, i);

				bobjdata = (*env)->CallStaticObjectMethod(env, jBSONClazz, encodeMethodID, qorobj);
				bdata = (*env)->GetByteArrayElements(env, bobjdata, NULL);
				blength = (*env)->GetArrayLength(env, bobjdata);
				bson_create_from_buffer2(&qorbsons[i], bdata, blength);
				(*env)->ReleaseByteArrayElements(env, bobjdata, bdata, 0);
				(*env)->DeleteLocalRef(env, bobjdata);
			}
		}

		if (NULL != hobj){
			bobjdata = (*env)->CallStaticObjectMethod(env, jBSONClazz, encodeMethodID, hobj);
			bdata = (*env)->GetByteArrayElements(env, bobjdata, NULL);
			blength = (*env)->GetArrayLength(env, bobjdata);
			qhbson = bson_create_from_buffer(bdata, blength);
			(*env)->ReleaseByteArrayElements(env, bobjdata, bdata, 0);
			(*env)->DeleteLocalRef(env, bobjdata);
		}

		q = ejdbcreatequery(db, qbson, qorz > 0 ? qorbsons : NULL, qorz, qhbson);
		if (!q) {
			set_ejdb_error(env, db);
			goto finish;
		}

		const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
		EJCOLL *coll = ejdbgetcoll(db, cname);

		if (!coll) {
			bson_iterator it;
			//If we are in $upsert mode a new collection will be created
			if (bson_find(&it, qbson, "$upsert") == BSON_OBJECT) {
				coll = ejdbcreatecoll(db, cname, NULL);
				(*env)->ReleaseStringUTFChars(env, colname, cname);
				if (!coll) {
					set_ejdb_error(env, db);
					goto finish;
				}
			}
		} else {
			(*env)->ReleaseStringUTFChars(env, colname, cname);
		}

		uint32_t count = 0;
		TCLIST *qres = NULL;
		if (!coll) { //No collection -> no results
			qres = (flags & JBQRYCOUNT) ? NULL : tclistnew2(1); //empty results
		} else {
			qres = ejdbqryexecute(coll, q, &count, flags, NULL);
			if (ejdbecode(db) != TCESUCCESS) {
				set_ejdb_error(env, db);
				goto finish;
			}
		}
		
		qresult = (*env)->NewObject(env, jQResultClazz, initQResultMethodID, (jlong)count, (jlong)qres);

finish:
		// clear
		if (qbson) {
			bson_del(qbson);
		}
		if (qorbsons) {
			for (int i = 0; i < qorz; ++i) {
				bson_destroy(&qorbsons[i]);
			}
			free(qorbsons);
		}
		if (qhbson) {
			bson_del(qhbson);
		}
		if (q) {
			ejdbquerydel(q);
		}

		return qresult;
};
