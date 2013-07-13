package goejdb

// #cgo LDFLAGS: -ltcejdb
// #include "../../../../../../tcejdb/ejdb.h"
import "C"

import (
	"errors"
	"fmt"
	"unsafe"
)

// Database open modes
const (
	// Open as a reader.
	JBOREADER = C.JBOREADER
	// Open as a writer.
	JBOWRITER = C.JBOWRITER
	// Create if db file not exists.
	JBOCREAT  = C.JBOCREAT
	// Truncate db on open.
	JBOTRUNC  = C.JBOTRUNC
	// Open without locking.
	JBONOLCK  = C.JBONOLCK
	// Lock without blocking.
	JBOLCKNB  = C.JBOLCKNB
	// Synchronize every transaction.
	JBOTSYNC  = C.JBOTSYNC
)

// Error codes
const (
    // Invalid collection name.
    JBEINVALIDCOLNAME = C.JBEINVALIDCOLNAME
    // Invalid bson object.
    JBEINVALIDBSON = C.JBEINVALIDBSON
    // Invalid bson object id.
    JBEINVALIDBSONPK = C.JBEINVALIDBSONPK
    // Invalid query control field starting with '$'.
    JBEQINVALIDQCONTROL = C.JBEQINVALIDQCONTROL
    // $strand, $stror, $in, $nin, $bt keys requires not empty array value.
    JBEQINOPNOTARRAY = C.JBEQINOPNOTARRAY
    // Inconsistent database metadata.
    JBEMETANVALID = C.JBEMETANVALID
    // Invalid field path value.
    JBEFPATHINVALID = C.JBEFPATHINVALID
    // Invalid query regexp value.
    JBEQINVALIDQRX = C.JBEQINVALIDQRX
    // Result set sorting error.
    JBEQRSSORTING = C.JBEQRSSORTING
    // Query generic error.
    JBEQERROR = C.JBEQERROR
    // Updating failed.
    JBEQUPDFAILED = C.JBEQUPDFAILED
    // Only one $elemMatch allowed in the fieldpath.
    JBEQONEEMATCH = C.JBEQONEEMATCH
    // $fields hint cannot mix include and exclude fields
    JBEQINCEXCL = C.JBEQINCEXCL
    // action key in $do block can only be one of: $join
    JBEQACTKEY = C.JBEQACTKEY
    // Exceeded the maximum number of collections per database
    JBEMAXNUMCOLS = C.JBEMAXNUMCOLS
)

const maxslice = 1<<31 - 1

// An EJDB database
type Ejdb struct {
	ptr *[0]byte
}

type EjdbError struct {
	// Error code returned by EJDB
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

// Returns EJDB library version string. Eg: "1.1.13"
func Version() string {
	cs := C.ejdbversion()
	return C.GoString(cs)
}

// Return true if passed `oid` string cat be converted to valid 12 bit BSON object identifier (OID).
func IsValidOidStr(oid string) bool {
	c_oid := C.CString(oid)
	res := C.ejdbisvalidoidstr(c_oid)
	C.free(unsafe.Pointer(c_oid))

	return bool(res)
}

// Returns a new open EJDB database.
// path is the path to the database file.
// options specify the open mode bitmask flags.
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

// Return true if database is in open state, false otherwise
func (ejdb *Ejdb) IsOpen() bool {
	ret := C.ejdbisopen(ejdb.ptr)
	return bool(ret)
}

// Delete database object. If the database is not closed, it is closed implicitly.
// Note that the deleted object and its derivatives can not be used anymore
func (ejdb *Ejdb) Del() {
	C.ejdbdel(ejdb.ptr)
}

// Close a table database object. If a writer opens a database but does not close it appropriately, the database will be broken.
// If successful return true, otherwise return false.
func (ejdb *Ejdb) Close() *EjdbError {
	C.ejdbclose(ejdb.ptr)
	return ejdb.check_error()
}

// Retrieve collection handle for collection specified `collname`.
// If collection with specified name does't exists it will return nil.
func (ejdb *Ejdb) GetColl(colname string) (*EjColl, *EjdbError) {
	c_colname := C.CString(colname)
	defer C.free(unsafe.Pointer(c_colname))

	ejcoll := new(EjColl)
	ejcoll.ejdb = ejdb
	ejcoll.ptr = C.ejdbgetcoll(ejdb.ptr, c_colname)

	return ejcoll, ejdb.check_error()
}

// Return a slice containing shallow copies of all collection handles (EjColl) currently open.
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

// Same as GetColl() but automatically creates new collection if it doesn't exists.
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

// Removes collections specified by `colname`.
// If `unlinkfile` is true the collection db file and all of its index files will be removed.
// If removal was successful return true, otherwise return false.
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

// Synchronize entire EJDB database and all of its collections with storage.
func (ejdb *Ejdb) Sync() (bool, *EjdbError) {
	ret := C.ejdbsyncdb(ejdb.ptr)
	if ret {
		return bool(ret), nil
	} else {
		return bool(ret), ejdb.check_error()
	}
}

// Gets description of EJDB database and its collections as a BSON object.
func (ejdb *Ejdb) Meta() ([]byte, *EjdbError) {
	bson := C.ejdbmeta(ejdb.ptr)
	err := ejdb.check_error()
	if err != nil {
		return make([]byte, 0), err
	}
	defer C.bson_del(bson)
	return bson_to_byte_slice(bson), nil
}
