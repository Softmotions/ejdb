package goejdb

// #cgo LDFLAGS: -ltcejdb
// #include "../../../../../../tcejdb/ejdb.h"
import "C"

import "unsafe"

const (
	JBIDXDROP    = C.JBIDXDROP
	JBIDXDROPALL = C.JBIDXDROPALL
	JBIDXOP      = C.JBIDXOP
	JBIDXREBLD   = C.JBIDXREBLD
	JBIDXNUM     = C.JBIDXNUM
	JBIDXSTR     = C.JBIDXSTR
	JBIDXARR     = C.JBIDXARR
	JBIDXISTR    = C.JBIDXISTR
)

type EjColl struct {
	ptr  *[0]byte
	ejdb *Ejdb
}

type EjCollOpts struct {
	large         bool
	compressed    bool
	records       int
	cachedrecords int
}

func (coll *EjColl) save_c_bson(c_bson *C.bson) (string, *EjdbError) {
	var c_oid C.bson_oid_t
	C.ejdbsavebson(coll.ptr, c_bson, &c_oid)
	return bson_oid_to_string(&c_oid), coll.ejdb.check_error()
}

func (coll *EjColl) SaveBson(bsdata []byte) (string, *EjdbError) {
	c_bson := bson_from_byte_slice(bsdata)
	defer C.bson_destroy(c_bson)
	return coll.save_c_bson(c_bson)
}

func (coll *EjColl) SaveJson(j string) (string, *EjdbError) {
	c_bson := bson_from_json(j)
	defer C.bson_destroy(c_bson)
	return coll.save_c_bson(c_bson)
}

// EJDB_EXPORT bool ejdbrmbson(EJCOLL *coll, bson_oid_t *oid);
func (coll *EjColl) RmBson(oid string) bool {
	c_oid := bson_oid_from_string(&oid)
	ret := C.ejdbrmbson(coll.ptr, c_oid)
	coll.ejdb.check_error()
	return bool(ret)
}

// EJDB_EXPORT bson* ejdbloadbson(EJCOLL *coll, const bson_oid_t *oid);
func (coll *EjColl) LoadBson(oid string) []byte {
	c_oid := bson_oid_from_string(&oid)
	bson := C.ejdbloadbson(coll.ptr, c_oid)
	defer C.bson_del(bson)
	coll.ejdb.check_error()

	return bson_to_byte_slice(bson)
}

// EJDB_EXPORT EJQRESULT ejdbqryexecute(EJCOLL *jcoll, const EJQ *q, uint32_t *count, int qflags, TCXSTR *log);
func (coll *EjColl) Find(query string, queries ...string) ([][]byte, *EjdbError) {
	q, err := coll.ejdb.CreateQuery(query, queries...)
	defer q.Del()
	if err != nil {
		return nil, err
	} else {
		return q.Execute(coll)
	}
}

func (coll *EjColl) FindOne(query string, queries ...string) (*[]byte, *EjdbError) {
	q, err := coll.ejdb.CreateQuery(query, queries...)
	defer q.Del()
	if err != nil {
		return nil, err
	} else {
		return q.ExecuteOne(coll)
	}
}

func (coll *EjColl) Count(query string, queries ...string) (int, *EjdbError) {
	q, err := coll.ejdb.CreateQuery(query, queries...)
	if err != nil {
		return 0, err
	}
	defer q.Del()
	return q.Count(coll)
}

// EJDB_EXPORT uint32_t ejdbupdate(EJCOLL *jcoll, bson *qobj, bson *orqobjs, int orqobjsnum, bson *hints, TCXSTR *log);
func (coll *EjColl) Update(query string, queries ...string) (int, *EjdbError) {
	query_bson := bson_from_json(query)
	defer C.bson_destroy(query_bson)

	orqueries := C.malloc((C.size_t)(unsafe.Sizeof(new(C.bson))) * C.size_t(len(queries)))
	defer C.free(orqueries)
	ptr_orqueries := (*[maxslice]*C.bson)(orqueries)
	for i, q := range queries {
		(*ptr_orqueries)[i] = bson_from_json(q)
		defer C.bson_destroy((*ptr_orqueries)[i])
	}

	count := C.ejdbupdate(coll.ptr, query_bson, (*C.bson)(orqueries), C.int(len(queries)), nil, nil)
	return int(count), coll.ejdb.check_error()
}

// EJDB_EXPORT bool ejdbsetindex(EJCOLL *coll, const char *ipath, int flags);
func (coll *EjColl) SetIndex(ipath string, flags int) *EjdbError {
	c_ipath := C.CString(ipath)
	defer C.free(unsafe.Pointer(c_ipath))
	res := C.ejdbsetindex(coll.ptr, c_ipath, C.int(flags))
	if res {
		return nil
	} else {
		return coll.ejdb.check_error()
	}
}

// EJDB_EXPORT bool ejdbtranbegin(EJCOLL *coll);
func (coll *EjColl) BeginTransaction() *EjdbError {
	res := C.ejdbtranbegin(coll.ptr)
	if res {
		return nil
	} else {
		return coll.ejdb.check_error()
	}
}

//EJDB_EXPORT bool ejdbtrancommit(EJCOLL *coll);
func (coll *EjColl) CommitTransaction() *EjdbError {
	res := C.ejdbtrancommit(coll.ptr)
	if res {
		return nil
	} else {
		return coll.ejdb.check_error()
	}
}

// EJDB_EXPORT bool ejdbtranabort(EJCOLL *coll);
func (coll *EjColl) AbortTransaction() *EjdbError {
	res := C.ejdbtranabort(coll.ptr)
	if res {
		return nil
	} else {
		return coll.ejdb.check_error()
	}
}

// EJDB_EXPORT bool ejdbtranstatus(EJCOLL *jcoll, bool *txactive);
func (coll *EjColl) IsTransactionActive() bool {
	var ret C.bool
	C.ejdbtranstatus(coll.ptr, &ret)
	return bool(ret)
}

// EJDB_EXPORT void ejdbqresultdispose(EJQRESULT qr);

// EJDB_EXPORT bool ejdbsyncoll(EJCOLL *jcoll);
func (coll *EjColl) Sync() (bool, *EjdbError) {
	ret := C.ejdbsyncoll(coll.ptr)
	if ret {
		return bool(ret), nil
	} else {
		return bool(ret), coll.ejdb.check_error()
	}
}
