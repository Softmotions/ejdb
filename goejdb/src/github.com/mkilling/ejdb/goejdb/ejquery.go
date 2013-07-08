package goejdb

// #cgo LDFLAGS: -ltcejdb
// #include "../../../../../../tcejdb/ejdb.h"
import "C"

import "unsafe"

const (
	JBQRYCOUNT   = C.JBQRYCOUNT
	JBQRYFINDONE = C.JBQRYFINDONE
)

type EjQuery struct {
	ptr  *[0]byte
	ejdb *Ejdb
}

// EJDB_EXPORT EJQ* ejdbcreatequery(EJDB *jb, bson *qobj, bson *orqobjs, int orqobjsnum, bson *hints);
func (ejdb *Ejdb) CreateQuery(query string, queries ...string) (*EjQuery, *EjdbError) {
	query_bson := bson_from_json(query)
	defer C.bson_destroy(query_bson)

	orqueries_count := len(queries)
	orqueries := C.malloc((C.size_t)(unsafe.Sizeof(new(C.bson))) * C.size_t(orqueries_count))
	defer C.free(orqueries)
	ptr_orqueries := (*[maxslice]*C.bson)(orqueries)
	for i, q := range queries {
		(*ptr_orqueries)[i] = bson_from_json(q)
		defer C.bson_destroy((*ptr_orqueries)[i])
	}

	q := C.ejdbcreatequery(ejdb.ptr, query_bson, (*C.bson)(orqueries), C.int(len(queries)), nil)
	if q == nil {
		return nil, ejdb.check_error()
	} else {
		return &EjQuery{ptr: q, ejdb: ejdb}, nil
	}
}

// EJDB_EXPORT EJQ* ejdbqueryhints(EJDB *jb, EJQ *q, const void *hintsbsdata);
func (q *EjQuery) SetHints(hints string) *EjdbError {
	bsdata := bson_from_json(hints).data
	ret := C.ejdbqueryhints(q.ejdb.ptr, q.ptr, unsafe.Pointer(bsdata))
	if ret == nil {
		return q.ejdb.check_error()
	} else {
		return nil
	}
}

func (q *EjQuery) Execute(coll *EjColl) ([][]byte, *EjdbError) {
	// execute query
	var count C.uint32_t
	res := C.ejdbqryexecute(coll.ptr, q.ptr, &count, 0, nil)
	defer C.ejdbqresultdispose(res)
	err := coll.ejdb.check_error()

	// return results
	ret := make([][]byte, 0)
	for i := 0; i < int(count); i++ {
		var size C.int
		bson_blob := C.ejdbqresultbsondata(res, C.int(i), &size)
		ret = append(ret, ((*[maxslice]byte)(bson_blob))[:int(size)])
	}

	return ret, err
}

func (q *EjQuery) ExecuteOne(coll *EjColl) (*[]byte, *EjdbError) {
	// execute query
	var count C.uint32_t
	res := C.ejdbqryexecute(coll.ptr, q.ptr, &count, JBQRYFINDONE, nil)
	defer C.ejdbqresultdispose(res)
	err := coll.ejdb.check_error()

	// return results
	if count == 0 {
		return nil, err
	} else {
		var size C.int
		bson_blob := C.ejdbqresultbsondata(res, 0, &size)
		ret := ((*[maxslice]byte)(bson_blob))[:int(size)]
		return &ret, err
	}
}

func (q *EjQuery) Count(coll *EjColl) (int, *EjdbError) {
	var count C.uint32_t
	C.ejdbqryexecute(coll.ptr, q.ptr, &count, JBQRYCOUNT, nil)
	err := coll.ejdb.check_error()
	return int(count), err
}

// EJDB_EXPORT void ejdbquerydel(EJQ *q);
func (q *EjQuery) Del() {
	C.ejdbquerydel(q.ptr)
}
