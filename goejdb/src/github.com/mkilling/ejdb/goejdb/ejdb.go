package goejdb

// #cgo LDFLAGS: -ltcejdb
// #include "../../../../../../tcejdb/ejdb.h"
import "C"

import (
	"errors"
	"fmt"
	"unsafe"
)

const (
	JBOREADER = C.JBOREADER
	JBOWRITER = C.JBOWRITER
	JBOCREAT  = C.JBOCREAT
	JBOTRUNC  = C.JBOTRUNC
	JBONOLCK  = C.JBONOLCK
	JBOLCKNB  = C.JBOLCKNB
	JBOTSYNC  = C.JBOTSYNC
)

const maxslice = 1<<31 - 1

type Ejdb struct {
	ptr *[0]byte
}

type EjdbError struct {
	ErrorCode int
	error
}

func new_ejdb() *Ejdb {
	ejdb := new(Ejdb)
	ejdb.ptr = C.ejdbnew()
	if ejdb.ptr == nil {
		return nil
	} else {
		return ejdb
	}
}

func Version() string {
	cs := C.ejdbversion()
	return C.GoString(cs)
}

func IsValidOidStr(oid string) bool {
	c_oid := C.CString(oid)
	res := C.ejdbisvalidoidstr(c_oid)
	C.free(unsafe.Pointer(c_oid))

	return bool(res)
}

func Open(path string, options int) (*Ejdb, *EjdbError) {
	ejdb := new_ejdb()
	if ejdb != nil {
		c_path := C.CString(path)
		defer C.free(unsafe.Pointer(c_path))
		C.ejdbopen(ejdb.ptr, c_path, C.int(options))
	}

	return ejdb, ejdb.check_error()
}

func (ejdb *Ejdb) check_error() *EjdbError {
	ecode := C.ejdbecode(ejdb.ptr)
	if ecode == 0 {
		return nil
	}
	c_msg := C.ejdberrmsg(ecode)
	msg := C.GoString(c_msg)
	return &EjdbError{int(ecode), errors.New(fmt.Sprintf("EJDB error: %v", msg))}
}

func (ejdb *Ejdb) IsOpen() bool {
	ret := C.ejdbisopen(ejdb.ptr)
	return bool(ret)
}

func (ejdb *Ejdb) Del() {
	C.ejdbdel(ejdb.ptr)
}

func (ejdb *Ejdb) Close() *EjdbError {
	C.ejdbclose(ejdb.ptr)
	return ejdb.check_error()
}

func (ejdb *Ejdb) GetColl(colname string) (*EjColl, *EjdbError) {
	c_colname := C.CString(colname)
	defer C.free(unsafe.Pointer(c_colname))

	ejcoll := new(EjColl)
	ejcoll.ejdb = ejdb
	ejcoll.ptr = C.ejdbgetcoll(ejdb.ptr, c_colname)

	return ejcoll, ejdb.check_error()
}

func (ejdb *Ejdb) GetColls() ([]*EjColl, *EjdbError) {
	ret := make([]*EjColl, 0)
	lst := C.ejdbgetcolls(ejdb.ptr)
	if lst == nil {
		return ret, ejdb.check_error()
	}

	for i := int(lst.start); i < int(lst.start)+int(lst.num); i++ {
		ptr := uintptr(unsafe.Pointer(lst.array)) + unsafe.Sizeof(C.TCLISTDATUM{})*uintptr(i)
		datum := (*C.TCLISTDATUM)(unsafe.Pointer(ptr))
		datum_ptr := unsafe.Pointer(datum.ptr)
		ret = append(ret, &EjColl{(*[0]byte)(datum_ptr), ejdb})
	}
	return ret, nil
}

func (ejdb *Ejdb) CreateColl(colname string, opts *EjCollOpts) (*EjColl, *EjdbError) {
	c_colname := C.CString(colname)
	defer C.free(unsafe.Pointer(c_colname))

	ret := new(EjColl)
	ret.ejdb = ejdb

	if opts != nil {
		var c_opts C.EJCOLLOPTS
		c_opts.large = C._Bool(opts.large)
		c_opts.compressed = C._Bool(opts.large)
		c_opts.records = C.int64_t(opts.records)
		c_opts.cachedrecords = C.int(opts.cachedrecords)
		ret.ptr = C.ejdbcreatecoll(ejdb.ptr, c_colname, &c_opts)
	} else {
		ret.ptr = C.ejdbcreatecoll(ejdb.ptr, c_colname, nil)
	}

	if ret.ptr != nil {
		return ret, nil
	} else {
		return nil, ejdb.check_error()
	}
}

func (ejdb *Ejdb) RmColl(colname string, unlinkfile bool) (bool, *EjdbError) {
	c_colname := C.CString(colname)
	defer C.free(unsafe.Pointer(c_colname))
	res := C.ejdbrmcoll(ejdb.ptr, c_colname, C._Bool(unlinkfile))
	if res {
		return bool(res), nil
	} else {
		return bool(res), ejdb.check_error()
	}
}

// EJDB_EXPORT bool ejdbsyncdb(EJDB *jb);
func (ejdb *Ejdb) Sync() (bool, *EjdbError) {
	ret := C.ejdbsyncdb(ejdb.ptr)
	if ret {
		return bool(ret), nil
	} else {
		return bool(ret), ejdb.check_error()
	}
}

// EJDB_EXPORT bson* ejdbmeta(EJDB *jb);
func (ejdb *Ejdb) Meta() ([]byte, *EjdbError) {
	bson := C.ejdbmeta(ejdb.ptr)
	err := ejdb.check_error()
	if err != nil {
		return make([]byte, 0), err
	}
	defer C.bson_del(bson)
	return bson_to_byte_slice(bson), nil
}
