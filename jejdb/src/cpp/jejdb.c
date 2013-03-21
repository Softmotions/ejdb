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

static void set_ejdb_to_object(JNIEnv *env, jobject obj, EJDB *db) {
	jclass clazz = (*env)->GetObjectClass(env, obj);
	jfieldID dbpID = (*env)->GetFieldID(env, clazz, "dbPointer", "J");
	(*env)->SetLongField(env, obj, dbpID, (jlong)db);
};

static EJDB *get_ejdb_from_object(JNIEnv *env, jobject obj) {
	jclass jEJDBQueryClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBQuery");
	if ((*env)->IsInstanceOf(env, obj, jEJDBQueryClazz)) {
		jfieldID collID = (*env)->GetFieldID(env, jEJDBQueryClazz, "coll", "Lorg/ejdb/driver/EJDBCollection;");
		obj = (*env)->GetObjectField(env, obj, collID);

		if (NULL == obj) {
			return 0;
		}
	}

	jclass jEJDBCollectionClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBCollection");
	if ((*env)->IsInstanceOf(env, obj, jEJDBCollectionClazz)) {
		jfieldID dbID = (*env)->GetFieldID(env, jEJDBCollectionClazz, "db", "Lorg/ejdb/driver/EJDB;");
		obj = (*env)->GetObjectField(env, obj, dbID);

		if (NULL == obj) {
			return 0;
		}
	}

	jclass jEJDBClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDB");
	if ((*env)->IsInstanceOf(env, obj, jEJDBClazz)) {
		jfieldID dbpID = (*env)->GetFieldID(env, jEJDBClazz, "dbPointer", "J");
		return (EJDB *)(*env)->GetLongField(env, obj, dbpID);
	}

	return (EJDB *)NULL;
};

static jstring get_coll_name(JNIEnv *env, jobject obj) {
	jclass jEJDBQueryClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBQuery");
	if ((*env)->IsInstanceOf(env, obj, jEJDBQueryClazz)) {
		jfieldID collID = (*env)->GetFieldID(env, jEJDBQueryClazz, "coll", "Lorg/ejdb/driver/EJDBCollection;");
		obj = (*env)->GetObjectField(env, obj, collID);
	}

	jclass jEJDBCollectionClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBCollection");
	if ((*env)->IsInstanceOf(env, obj, jEJDBCollectionClazz)) {
		jfieldID colnameID = (*env)->GetFieldID(env, jEJDBCollectionClazz, "cname", "Ljava/lang/String;");
		return (*env)->GetObjectField(env, obj, colnameID);
	}

	return NULL;
};

static void fill_ejdb_collopts(JNIEnv *env, jobject obj, EJCOLLOPTS *ejcopts) {
	memset(ejcopts, 0, sizeof (*ejcopts));

	if (NULL == obj) {
		return;
	}

	jclass clazz = (*env)->GetObjectClass(env, obj);

	jfieldID recordsFID = (*env)->GetFieldID(env, clazz, "records", "J");
	jfieldID cachedrecordsFID = (*env)->GetFieldID(env, clazz, "cachedRecords", "I");
	jfieldID largeFID = (*env)->GetFieldID(env, clazz, "large", "Z");
	jfieldID compressedFID = (*env)->GetFieldID(env, clazz, "compressed", "Z");

	jlong records = (*env)->GetLongField(env, obj, recordsFID);
	jint cachedrecords = (*env)->GetIntField(env, obj, cachedrecordsFID);
	jboolean large = (*env)->GetBooleanField(env, obj, largeFID);
	jboolean compressed = (*env)->GetBooleanField(env, obj, compressedFID);

	ejcopts->records = records > 0 ? records : 0;
	ejcopts->cachedrecords = cachedrecords > 0 ? cachedrecords : 0;
	ejcopts->large = (JNI_TRUE == large);
	ejcopts->compressed = (JNI_TRUE == compressed);
};

static bson *encode_bson(JNIEnv *env, jobject jbson, bson *out) {
	jclass jBSONClazz = (*env)->FindClass(env, "org/bson/BSON");
	jmethodID encodeMethodID = (*env)->GetStaticMethodID(env, jBSONClazz, "encode", "(Lorg/bson/BSONObject;)[B");

	jbyteArray bobjdata = (*env)->CallStaticObjectMethod(env, jBSONClazz, encodeMethodID, jbson);
	jbyte *bdata = (*env)->GetByteArrayElements(env, bobjdata, NULL);
	jsize blength = (*env)->GetArrayLength(env, bobjdata);
	if (NULL == out) {
		out = bson_create_from_buffer(bdata, blength);
	} else {
		bson_create_from_buffer2(out, bdata, blength);
	}
	(*env)->ReleaseByteArrayElements(env, bobjdata, bdata, 0);
	(*env)->DeleteLocalRef(env, bobjdata);

	return out;
};

static jobject decode_bson_from_buffer(JNIEnv *env, const void *buffer, jsize size) {
	jclass jBSONClazz = (*env)->FindClass(env, "org/bson/BSON");
	jmethodID decodeMethodID = (*env)->GetStaticMethodID(env, jBSONClazz, "decode", "([B)Lorg/bson/BSONObject;");

	jbyteArray jbsdata = (*env)->NewByteArray(env, size);
	(*env)->SetByteArrayRegion(env, jbsdata, 0, size, (jbyte*)buffer);
	jobject result = (*env)->CallStaticObjectMethod(env, jBSONClazz, decodeMethodID, jbsdata);
	(*env)->DeleteLocalRef(env, jbsdata);

	return result;
};

static jobject decode_bson(JNIEnv *env, bson *bson) {
	return decode_bson_from_buffer(env, (void *)bson_data(bson), bson_size(bson));
};

static TCLIST *get_rs_from_object(JNIEnv *env, jobject obj) {
	jclass clazz = (*env)->GetObjectClass(env, obj);
	jfieldID rspID = (*env)->GetFieldID(env, clazz, "rsPointer", "J");

	return (TCLIST *)(*env)->GetLongField(env, obj, rspID);
};

static void set_rs_to_object(JNIEnv *env, jobject obj, TCLIST *rs) {
	jclass clazz = (*env)->GetObjectClass(env, obj);
	jfieldID rspID = (*env)->GetFieldID(env, clazz, "rsPointer", "J");

	(*env)->SetLongField(env, obj, rspID, (jlong)rs);
};

static void update_coll_meta(JNIEnv *env, jobject obj, EJCOLL *coll) {
	jclass clazz = (*env)->GetObjectClass(env, obj);
	jfieldID existsID = (*env)->GetFieldID(env, clazz, "exists", "Z");
	(*env)->SetBooleanField(env, obj, existsID, coll ? JNI_TRUE : JNI_FALSE);

	jclass optionsClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBCollection$Options");
	jfieldID optionsID = (*env)->GetFieldID(env, clazz, "options", "Lorg/ejdb/driver/EJDBCollection$Options;");
	jobject jopts = (*env)->GetObjectField(env, obj, optionsID);
	
	if (!jopts) {
		jmethodID initOptions = (*env)->GetMethodID(env, optionsClazz, "<init>", "()V");
		jopts = (*env)->NewObject(env, optionsClazz, initOptions);
		(*env)->SetObjectField(env, obj, optionsID, jopts);
	}

	jfieldID bucketsID = (*env)->GetFieldID(env, optionsClazz, "buckets", "J");
	jfieldID cachedID = (*env)->GetFieldID(env, optionsClazz, "cachedRecords", "I");
	jfieldID largeID = (*env)->GetFieldID(env, optionsClazz, "large", "Z");
	jfieldID compressedID = (*env)->GetFieldID(env, optionsClazz, "compressed", "Z");

	(*env)->SetLongField(env, jopts, bucketsID, coll->tdb->hdb->bnum);
    (*env)->SetIntField(env, jopts, cachedID, coll->tdb->hdb->rcnum);
	(*env)->SetBooleanField(env, jopts, largeID, coll->tdb->opts & TDBTLARGE ? JNI_TRUE : JNI_FALSE);
	(*env)->SetBooleanField(env, jopts, compressedID, coll->tdb->opts & TDBTDEFLATE ? JNI_TRUE : JNI_FALSE);

	// TODO: indexes
	jclass indexClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBCollection$Index");
	jclass indexTypeClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBCollection$IndexType");
	jmethodID initIndex = (*env)->GetMethodID(env, indexClazz, "<init>", "()V");
	jfieldID indexesID = (*env)->GetFieldID(env, clazz, "indexes", "Ljava/util/Collection;");

	jclass arrayListClazz = (*env)->FindClass(env, "Ljava/util/ArrayList;");
	jmethodID initArrayList = (*env)->GetMethodID(env, arrayListClazz, "<init>", "(I)V");
	jmethodID addToList = (*env)->GetMethodID(env, arrayListClazz, "add", "(Ljava/lang/Object;)Z");

	jfieldID fieldID = (*env)->GetFieldID(env, indexClazz, "field", "Ljava/lang/String;");
	jfieldID nameID = (*env)->GetFieldID(env, indexClazz, "name", "Ljava/lang/String;");
	jfieldID fileID = (*env)->GetFieldID(env, indexClazz, "file", "Ljava/lang/String;");
	jfieldID typeID = (*env)->GetFieldID(env, indexClazz, "type", "Lorg/ejdb/driver/EJDBCollection$IndexType;");
	jfieldID recordsID = (*env)->GetFieldID(env, indexClazz, "records", "I");

	jobject indexes = (*env)->NewObject(env, arrayListClazz, initArrayList, coll->tdb->inum);

	for (int j = 0; j < coll->tdb->inum; ++j) {
		TDBIDX *idx = (coll->tdb->idxs + j);
		assert(idx);
		if (idx->type != TDBITLEXICAL && idx->type != TDBITDECIMAL && idx->type != TDBITTOKEN) {
			continue;
		}

		jobject index = (*env)->NewObject(env, indexClazz, initIndex);

		(*env)->SetObjectField(env, index, fieldID, (*env)->NewStringUTF(env, idx->name+1));
		(*env)->SetObjectField(env, index, nameID, (*env)->NewStringUTF(env, idx->name));

		jfieldID indexTypeID = NULL;
		switch (idx->type) {
			case TDBITLEXICAL:
				indexTypeID = (*env)->GetStaticFieldID(env, indexTypeClazz, "Lexical", "Lorg/ejdb/driver/EJDBCollection$IndexType;");
				break;
			case TDBITDECIMAL:
				indexTypeID = (*env)->GetStaticFieldID(env, indexTypeClazz, "Numeric", "Lorg/ejdb/driver/EJDBCollection$IndexType;");
				break;
			case TDBITTOKEN:
				indexTypeID = (*env)->GetStaticFieldID(env, indexTypeClazz, "Token", "Lorg/ejdb/driver/EJDBCollection$IndexType;");
				break;
		}
		if (indexTypeID) {
			(*env)->SetObjectField(env, index, typeID, (*env)->GetStaticObjectField(env, indexTypeClazz, indexTypeID));
		}

		TCBDB *idb = (TCBDB*) idx->db;
		if (idb) {
			(*env)->SetIntField(env, index, recordsID, idb->rnum);
			(*env)->SetObjectField(env, index, fileID, (*env)->NewStringUTF(env, idb->hdb->path));
		}

		(*env)->CallBooleanMethod(env, indexes, addToList, index);
	}

	(*env)->SetObjectField(env, obj, indexesID, indexes);
};

/*
* Class:     org_ejdb_driver_EJDB
* Method:    open
* Signature: (Ljava/lang/String;I)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDB_open (JNIEnv *env, jobject obj, jstring path, jint mode) {

	EJDB* db = get_ejdb_from_object(env, obj);
	if (ejdbisopen(db)) {
		set_error(env, 0, "EJDB already opened");
		return;
	}
	
	db = ejdbnew();

	if (!db) {
		set_error(env, 0, "Could not create EJDB");
		return;
	}

	const char *dbpath = (*env)->GetStringUTFChars(env, path, NULL);
	bool status = ejdbopen(db, dbpath, mode);
	(*env)->ReleaseStringUTFChars(env, path, dbpath);

	if (!status) {
		set_ejdb_error(env, db);
		return;
	}

	set_ejdb_to_object(env, obj, db);
	Java_org_ejdb_driver_EJDB_updateMeta(env, obj);
	return;
};

/*
* Class:     org_ejdb_driver_EJDB
* Method:    isOpen
* Signature: ()Z
*/
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDB_isOpen (JNIEnv *env, jobject obj) {
	return ejdbisopen(get_ejdb_from_object(env, obj));
};

/*
* Class:     org_ejdb_driver_EJDB
* Method:    close
* Signature: ()V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDB_close (JNIEnv *env, jobject obj) {
	EJDB* db = get_ejdb_from_object(env, obj);

	if (ejdbisopen(db)) {
		if (!ejdbclose(db)) {
			set_ejdb_error(env, db);
		}
		if (NULL != db) {
			ejdbdel(db);
		}
	}

	set_ejdb_to_object(env, obj, NULL);
	Java_org_ejdb_driver_EJDB_updateMeta(env, obj);
};

/*
* Class:     org_ejdb_driver_EJDB
* Method:    sync
* Signature: ()V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDB_sync (JNIEnv *env, jobject obj) {
	EJDB* db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return;
	}

	if (!ejdbsyncdb(db)) {
		set_ejdb_error(env, db);
	}
};

/*
 * Class:     org_ejdb_driver_EJDB
 * Method:    updateMeta
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDB_updateMeta (JNIEnv *env, jobject obj) {
	jclass clazz = (*env)->GetObjectClass(env, obj);
	EJDB *db = get_ejdb_from_object(env, obj);

	jfieldID pathID = (*env)->GetFieldID(env, clazz, "path", "Ljava/lang/String;");
	(*env)->SetObjectField(env, obj, pathID, db ? (*env)->NewStringUTF(env, db->metadb->hdb->path) : NULL);

	jfieldID collectionMapID = (*env)->GetFieldID(env, clazz, "collections", "Ljava/util/Map;");
	jobject collmapold = (*env)->GetObjectField(env, obj, collectionMapID);
	jclass mapClazz = (*env)->GetObjectClass(env, collmapold);
	jmethodID getFromMap = (*env)->GetMethodID(env, mapClazz, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");

	jclass hashMapClazz = (*env)->FindClass(env, "java/util/HashMap");
	jmethodID initHashMap = (*env)->GetMethodID(env, hashMapClazz, "<init>", "(I)V");
	jmethodID putIntoMap = (*env)->GetMethodID(env, hashMapClazz, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
	jobject collmapnew = (*env)->NewObject(env, hashMapClazz, initHashMap, !db ? 0 : db->cdbsnum);

	if (!db) {
		(*env)->SetObjectField(env, obj, collectionMapID, collmapnew);
		return;
	} 

	TCLIST *colls = ejdbgetcolls(db);
    if (!colls) {
        set_ejdb_error(env, db);
        return;
    }

	jclass collectionClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBCollection");
	jmethodID initCollection = (*env)->GetMethodID(env, collectionClazz, "<init>", "(Lorg/ejdb/driver/EJDB;Ljava/lang/String;)V");

    for (int i = 0; i < TCLISTNUM(colls); ++i) {
		EJCOLL *coll = (EJCOLL*) TCLISTVALPTR(colls, i);

		jstring cname = (*env)->NewStringUTF(env, coll->cname);//, coll->cnamesz);

		jobject cobj = (*env)->CallObjectMethod(env, collmapold, getFromMap, cname);
		if (!cobj) {
			cobj = (*env)->NewObject(env, collectionClazz, initCollection, obj, cname);
		}

		update_coll_meta(env, cobj, coll);
		
		(*env)->CallObjectMethod(env, collmapnew, putIntoMap, cname, cobj);
	}

	(*env)->SetObjectField(env, obj, collectionMapID, collmapnew);
};

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    ensureExists
* Signature: (Lorg/ejdb/driver/EJDBCollection$Options;)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_ensureExists (JNIEnv *env, jobject obj, jobject opts) {
	EJDB* db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return;
	}

	jstring colname = get_coll_name(env, obj);

	EJCOLLOPTS ejcopts;
	fill_ejdb_collopts(env, opts, &ejcopts);

	const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
	EJCOLL *coll = ejdbcreatecoll(db, cname, &ejcopts);
	(*env)->ReleaseStringUTFChars(env, colname, cname);

	if (!coll) {
		set_ejdb_error(env, db);
		return;
	}
};

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    drop
* Signature: (Z)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_drop (JNIEnv *env, jobject obj, jboolean prune) {
	EJDB* db = get_ejdb_from_object(env, obj);		
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return;
	}

	jstring colname = get_coll_name(env, obj);

	const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
	bool status = ejdbrmcoll(db, cname, (prune == JNI_TRUE));
	(*env)->ReleaseStringUTFChars(env, colname, cname);

	if (!status) {
		set_ejdb_error(env, db);
		return;
	}
};

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    sync
* Signature: ()V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_sync (JNIEnv *env, jobject obj) {
	EJDB* db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return;
	}

	jstring colname = get_coll_name(env, obj);

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
 * Method:    updateMeta
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_updateMeta (JNIEnv *env, jobject obj) {
	EJDB *db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return;
	}

	jstring colname = get_coll_name(env, obj);

	const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
	EJCOLL * coll = ejdbgetcoll(db, cname);
	(*env)->ReleaseStringUTFChars(env, colname, cname);

	update_coll_meta(env, obj, coll);
};

/*
 * Class:     org_ejdb_driver_EJDBCollection
 * Method:    load
 * Signature: (Lorg/bson/types/ObjectId;)Lorg/bson/BSONObject;
 */
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBCollection_load (JNIEnv *env, jobject obj, jobject joid) {
	EJDB* db = get_ejdb_from_object(env, obj);		
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return NULL;
	}

	jstring colname = get_coll_name(env, obj);

	const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
	EJCOLL * coll = ejdbgetcoll(db, cname);
	(*env)->ReleaseStringUTFChars(env, colname, cname);

	if (!coll) {
		return NULL;
	}

	jclass jObjectIdClazz = (*env)->FindClass(env, "org/bson/types/ObjectId");
	jmethodID encodeMethodID = (*env)->GetMethodID(env, jObjectIdClazz, "toByteArray", "()[B");
	jbyteArray joiddata = (*env)->CallObjectMethod(env, joid, encodeMethodID);
	bson_oid_t *oid = (bson_oid_t*)(*env)->GetByteArrayElements(env, joiddata, NULL);
	bson* bson = ejdbloadbson(coll, oid);
	(*env)->ReleaseByteArrayElements(env, joiddata, (jbyte*)oid, 0);
	(*env)->DeleteLocalRef(env, joiddata);

	if (!bson) {
		return NULL;
	}

	jobject result = decode_bson(env, bson);
	bson_del(bson);

	return result;
}

/*
 * Class:     org_ejdb_driver_EJDBCollection
 * Method:    save
 * Signature: (Lorg/bson/BSONObject;)Lorg/bson/types/ObjectId;
 */
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBCollection_save (JNIEnv *env, jobject obj, jobject jdata) {
	EJDB* db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return NULL;
	}

	jstring colname = get_coll_name(env, obj);

	const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
	// todo: check
	EJCOLL * coll = ejdbcreatecoll(db, cname, NULL);
	(*env)->ReleaseStringUTFChars(env, colname, cname);

	if (!coll) {
		set_ejdb_error(env, db);
		return NULL;
	}

	bson_oid_t oid;

	bson *bson = encode_bson(env, jdata, NULL);
	bool status = ejdbsavebson(coll, bson, &oid);
	bson_del(bson);

	if (!status) {
		set_ejdb_error(env, db);
		return NULL;
	}

	jclass jObjectIdClazz = (*env)->FindClass(env, "org/bson/types/ObjectId");
	jmethodID initMethodID = (*env)->GetMethodID(env, jObjectIdClazz, "<init>", "([B)V");

	jbyteArray joiddata = (*env)->NewByteArray(env, sizeof(oid));
	(*env)->SetByteArrayRegion(env, joiddata, 0, sizeof(oid), (jbyte*)&oid);
	jobject result = (*env)->NewObject(env, jObjectIdClazz, initMethodID, joiddata);

	(*env)->DeleteLocalRef(env, joiddata);

	return result;
}

/*
 * Class:     org_ejdb_driver_EJDBCollection
 * Method:    remove
 * Signature: (Lorg/bson/types/ObjectId;)V
 */
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_remove (JNIEnv *env, jobject obj, jobject joid) {
	EJDB* db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return;
	}

	jstring colname = get_coll_name(env, obj);
	const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
	EJCOLL * coll = ejdbgetcoll(db, cname);
	(*env)->ReleaseStringUTFChars(env, colname, cname);

	if (!coll) {
		return;
	}

	jclass jObjectIdClazz = (*env)->FindClass(env, "org/bson/types/ObjectId");
	jmethodID encodeMethodID = (*env)->GetMethodID(env, jObjectIdClazz, "toByteArray", "()[B");
	jbyteArray joiddata = (*env)->CallObjectMethod(env, joid, encodeMethodID);
	bson_oid_t *oid = (bson_oid_t*)(*env)->GetByteArrayElements(env, joiddata, NULL);
	bool status = ejdbrmbson(coll, oid);
	(*env)->ReleaseByteArrayElements(env, joiddata, (jbyte*)oid, 0);
	(*env)->DeleteLocalRef(env, joiddata);

	if (!status) {
		set_ejdb_error(env, db);
		return;
	}
}

/*
* Class:     org_ejdb_driver_EJDBCollection
* Method:    setIndex
* Signature: (Ljava/lang/String;I)V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBCollection_setIndex (JNIEnv *env, jobject obj, jstring pathstr, jint flags) {
	EJDB* db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return;
	}

	jstring colname = get_coll_name(env, obj);
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
* Method:    execute
* Signature: (Lorg/bson/BSONObject;[Lorg/bson/BSONObject;Lorg/bson/BSONObject;I)Lorg/ejdb/driver/EJDBQuery$QResult;
*/
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBQuery_execute (JNIEnv *env, jobject obj, jobject qobj, jobjectArray qorarrobj, jobject hobj, jint flags) {
	jclass jQResultClazz = (*env)->FindClass(env, "org/ejdb/driver/EJDBQuery$QResult");
	jmethodID initQResultMethodID = (*env)->GetMethodID(env, jQResultClazz, "<init>", "(IJ)V");

	bson *qbson = NULL;
	bson *qorbsons = NULL;
	bson *qhbson = NULL;

	EJQ *q = NULL;

	jobject qresult = NULL;

	EJDB* db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		goto finish;
	}

	qbson = encode_bson(env, qobj, NULL);

	// todo: check query bson
	if (!qbson) {
		//
		goto finish;
	}

	jsize qorz = NULL != qorarrobj ? (*env)->GetArrayLength(env, qorarrobj) : 0;
	if (qorz > 0) {
		qorbsons = (bson*)malloc(qorz * sizeof(bson));
		if (!qorbsons) {
			set_error(env, 0, "Not enought memory");
			goto finish;
		}

		for (jsize i = 0; i < qorz; ++i) {
			jobject qorobj = (*env)->GetObjectArrayElement(env, qorarrobj, i);
			encode_bson(env, qorobj, &qorbsons[i]);
		}
	}

	if (NULL != hobj){
		qhbson = encode_bson(env, hobj, NULL);
	}

	q = ejdbcreatequery(db, qbson, qorz > 0 ? qorbsons : NULL, qorz, qhbson);
	if (!q) {
		set_ejdb_error(env, db);
		goto finish;
	}

	jstring colname = get_coll_name(env, obj);

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

	qresult = (*env)->NewObject(env, jQResultClazz, initQResultMethodID, (jint)count, (jlong)qres);

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

/*
* Class:     org_ejdb_driver_EJDBResultSet
* Method:    get
* Signature: (I)Lorg/bson/BSONObject;
*/
JNIEXPORT jobject JNICALL Java_org_ejdb_driver_EJDBResultSet_get (JNIEnv *env, jobject obj, jint indx) {
	TCLIST *rs = get_rs_from_object(env, obj);

	if (!rs) {
		set_error(env, 0, "Cursor closed");
		return NULL;
	}

	if (indx < 0 || indx >= TCLISTNUM(rs)) {
		set_error(env, 0, "Invalid cursor position");
		return NULL;
	}

	int bdatasz;
	void *bdata;

	TCLISTVAL(bdata, rs, indx, bdatasz);

	return decode_bson_from_buffer(env, bdata, bdatasz);
};

/*
* Class:     org_ejdb_driver_EJDBResultSet
* Method:    length
* Signature: ()I
*/
JNIEXPORT jint JNICALL Java_org_ejdb_driver_EJDBResultSet_length (JNIEnv *env, jobject obj) {
	TCLIST *rs = get_rs_from_object(env, obj);

	if (!rs) {
		return 0;
	}

	return TCLISTNUM(rs);
};

/*
* Class:     org_ejdb_driver_EJDBResultSet
* Method:    close
* Signature: ()V
*/
JNIEXPORT void JNICALL Java_org_ejdb_driver_EJDBResultSet_close (JNIEnv *env, jobject obj) {
	TCLIST *rs = get_rs_from_object(env, obj);

	if (!rs) {
		return;
	}

	tclistdel(rs);

	set_rs_to_object(env, obj, NULL);
};

/*
 * Class:     org_ejdb_driver_EJDBCollection
 * Method:    txControl
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_org_ejdb_driver_EJDBCollection_txControl (JNIEnv *env, jobject obj, jint mode) {
	EJDB* db = get_ejdb_from_object(env, obj);
	if (!ejdbisopen(db)) {
		set_error(env, 0, "EJDB not opened");
		return JNI_FALSE;
	}

	jstring colname = get_coll_name(env, obj);
	const char *cname = (*env)->GetStringUTFChars(env, colname, NULL);
	EJCOLL * coll = ejdbcreatecoll(db, cname, NULL);
	(*env)->ReleaseStringUTFChars(env, colname, cname);

	if (!coll) {
		set_ejdb_error(env, db);
		return JNI_FALSE;
	}

	bool status = false, txActive = true;

	switch (mode) {
	case org_ejdb_driver_EJDBCollection_JBTXBEGIN : 
		status = ejdbtranbegin(coll);
		break;

	case org_ejdb_driver_EJDBCollection_JBTXCOMMIT : 
		status = ejdbtrancommit(coll);
		break;

	case org_ejdb_driver_EJDBCollection_JBTXROLLBACK : 
		status = ejdbtranabort(coll);
		break;

	case org_ejdb_driver_EJDBCollection_JBTXSTATUS :
		txActive = false;
		status = ejdbtranstatus(coll, &txActive);
		break;

	default:
		set_error(env, 0, "Unexpected txControl option");
		return JNI_FALSE;
	}

	if (!status) {
		set_ejdb_error(env, db);
		return JNI_FALSE;
	}

	return txActive ? JNI_TRUE : JNI_FALSE;
};
