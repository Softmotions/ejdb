package goejdb

import (
	"fmt"
	"labix.org/v2/mgo/bson"
	"sync"
	"testing"
	"os"
)

type TestType struct {
	I int
	S string
	M map[string]string
}

func make_test_type() TestType {
	return TestType{I: 5, S: "mystring", M: map[string]string{"1": "one", "2": "two"}}
}

var i int = 0
var iMu sync.Mutex

func open() *Ejdb {
	iMu.Lock()
	i++
	ejdb, _ := Open(fmt.Sprintf("/tmp/my%d.ejdb", i), JBOWRITER|JBOCREAT|JBOTRUNC)
	iMu.Unlock()
	return ejdb
}

func TestVersion(t *testing.T) {
	Version()
}

func TestIsValidOidStr(t *testing.T) {
	if !IsValidOidStr("4fc62a0f4c114f273c000001") {
		t.Errorf("4fc62a0f4c114f273c000001 not recognized as a valid oid str though it should be")
	}

	if IsValidOidStr("invalidoid") {
		t.Errorf("invalidoid recognized as a valid oid str though it should not be")
	}
}

func TestOpen(t *testing.T) {
	ejdb := open()
	if ejdb == nil || !ejdb.IsOpen() {
		t.Errorf("Opening EJDB failed")
	}
}

func TestDel(t *testing.T) {
	ejdb := open()
	ejdb.Del()
}

func TestClose(t *testing.T) {
	ejdb := open()
	ejdb.Close()
	if ejdb.IsOpen() {
		t.Errorf("Closing EJDB failed")
	}
}

func TestCreateColl(t *testing.T) {
	ejdb := open()
	coll, err := ejdb.CreateColl("MyNewColl", nil)
	if err != nil {
		t.Errorf("CreateColl() failed with %v", err)
	}
	if coll == nil {
		t.Errorf("CreateColl() returned nil")
	}
}

func TestCreateCollWithOptions(t *testing.T) {
	ejdb := open()
	coll, err := ejdb.CreateColl("MyNewColl", &EjCollOpts{large: true, compressed: true, records: 1280000, cachedrecords: 0})
	if err != nil {
		t.Errorf("CreateColl() failed with %v", err)
	}
	if coll == nil {
		t.Errorf("CreateColl() returned nil")
	}
}

func TestGetColl(t *testing.T) {
	ejdb := open()
	ejdb.CreateColl("MyNewColl", nil)
	coll, err := ejdb.GetColl("MyNewColl")
	if err != nil {
		t.Errorf("GetColl() failed with %v", err)
	}
	if coll == nil {
		t.Errorf("GetColl() returned nil")
	}
}

func TestGetColls(t *testing.T) {
	ejdb := open()
	ejdb.CreateColl("MyNewColl", nil)
	ejdb.CreateColl("MyNewColl2", nil)

	colls, err := ejdb.GetColls()
	if err != nil {
		t.Errorf("GetColls() failed with %v", err)
	}
	if len(colls) != 2 {
		t.Errorf("expected GetColls() to return 2 collections, got %d", len(colls))
	}
}

func TestRmColl(t *testing.T) {
	ejdb := open()
	ejdb.CreateColl("MyNewColl", nil)
	ret, err := ejdb.RmColl("MyNewColl", true)
	if err != nil {
		t.Errorf("RmColl() failed with %v", err)
	}
	if ret == false {
		t.Errorf("RmColl() returned false though collection did exist and it should have returned true")
	}
}

func TestSaveBson(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	oid, _ := coll.SaveBson(bytes)
	if oid == "" {
		t.Errorf("SaveBson returned empty string instead of valid BSON OID")
	}
}

func TestLoadBson(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	oid, _ := coll.SaveBson(bytes)

	loaded := coll.LoadBson(oid)
	var out TestType
	bson.Unmarshal(loaded, &out)

	if out.I != 5 {
		t.Errorf("Unmarshalling int failed: expected 5 but was %v", out.I);
	}
	if out.S != "mystring" {
		t.Errorf("Unmarshalling string failed: expected \"mystring\" but was %v", out.S);
	}
	if out.M["2"] != "two" {
		t.Errorf("Unmarshalling map[string]string failed: expected entry [\"2\"] to be \"two\" but was %v", out.M["2"]);
	}
}

func TestRmBson(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	oid, _ := coll.SaveBson(bytes)
	coll.RmBson(oid)
}

func TestSyncColl(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	coll.SaveBson(bytes)
	ret, err := coll.Sync()
	if err != nil {
		t.Errorf("Collection.Sync() failed with %v", err)
	}
	if ret == false {
		t.Errorf("Collection.Sync() should have returned true but returned false")
	}
}

func TestSyncEjdb(t *testing.T) {
	ejdb := open()
	ejdb.CreateColl("MyNewColl", nil)
	ret, err := ejdb.Sync()
	if err != nil {
		t.Errorf("Ejdb.Sync() failed with %v", err)
	}
	if ret == false {
		t.Errorf("Ejdb.Sync() should have returned true but returned false")
	}
}

func TestEjdbMeta(t *testing.T) {
	ejdb := open()
	var m map[string]interface{}
	bs, _ := ejdb.Meta()
	bson.Unmarshal(bs, &m)
	if m["file"] == nil {
		t.Errorf("Metadata seems to be invalid, does not have an entry for [\"file\"]")
	}
}

func TestFind(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	coll.SaveBson(bytes)

	res1, err := coll.Find(`{"s" : "mystring"}`)
	if err != nil {
		t.Errorf("Find() failed with %v", err)
	}
	if len(res1) != 1 {
		fmt.Println(res1)
		t.Errorf("Find() did not find the right amount of entries. Expected 1 but got %v", len(res1))
	}
}

func TestFindShouldReturnEmptySliceOnNoResults(t *testing.T) {
	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)

	res1, err := coll.Find(`{"s" : "mystring"}`)
	if err != nil {
		t.Errorf("Find() failed with %v", err)
	}
	if len(res1) != 0 {
		fmt.Println(res1)
		t.Errorf("Find() did not find the right amount of entries. Expected 0 but got %v", len(res1))
	}
}

func TestCount(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	coll.SaveBson(bytes)
	coll.SaveBson(bytes)

	ct, err := coll.Count(`{"s" : "mystring"}`)
	if err != nil {
		t.Errorf("Count() failed with %v", err)
	}
	if ct != 2 {
		t.Errorf("Count() did not return the right result. Expected 2 but got %v", ct)
	}
}

func TestFindOneShouldReturnResult(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	coll.SaveBson(bytes)
	coll.SaveBson(bytes)

	ret, err := coll.FindOne(`{"s" : "mystring"}`)
	if err != nil {
		t.Errorf("FindOne() failed with %v", err)
	}
	if ret == nil {
		t.Fatalf("FindOne() failed. Returned nil though there should be a result.")
	}
	var tt TestType
	bson.Unmarshal(*ret, &tt)
	if tt.S != "mystring" {
		t.Errorf("FindOne() failed. Expected result to have entry [\"s\"] set to \"mystring\" but was \"%s\".", tt.S)
	}
}

func TestFindOneShouldReturnNilOnNoResult(t *testing.T) {
	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)

	ret, err := coll.FindOne(`{"s" : "mystring"}`)
	if err != nil {
		t.Errorf("FindOne() failed with %v", err)
	}
	if ret != nil {
		t.Fatalf("FindOne() failed. Returned non-nil result though it should have found nothing and returned nil.")
	}
}

func TestUpdate(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	coll.SaveBson(bytes)
	coll.SaveBson(bytes)

	ct, err := coll.Update(`{"$inc": {"i": 5}}`)
	if err != nil {
		t.Errorf("Update() failed with %v", err)
	}
	if ct != 2 {
		t.Errorf("Update() returned the wrong count. Expected 2 but got %d", ct)
	}

	// check the values after updating
	res, _ := coll.Find(`{}`)
	for _, r := range res {
		var tt TestType
		bson.Unmarshal(r, &tt)
		if tt.I != 10 {
			t.Errorf("Update() failed. Expected all \"I\" entries to have been updated to 10 but this one was %v", tt.I)
		}
	}
}

func TestSetIndex(t *testing.T) {
	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)
	for i := 0; i < 10; i++ {
		coll.SaveJson(fmt.Sprintf(`{"i": %d, "s": "number%d"`, i))
	}

	err1 := coll.SetIndex("i", JBIDXNUM)
	if err1 != nil {
		t.Errorf("SetIndex() with JBIDXNUM failed with %v", err1)
	}
	err2 := coll.SetIndex("s", JBIDXSTR)
	if err2 != nil {
		t.Errorf("SetIndex() with JBIDXSTR failed with %v", err2)
	}
}

func TestTransactions(t *testing.T) {
	bytes, _ := bson.Marshal(make_test_type())

	ejdb := open()
	coll, _ := ejdb.CreateColl("MyNewColl", nil)

	coll.BeginTransaction()
	coll.SaveBson(bytes)
	coll.SaveBson(bytes)
	coll.AbortTransaction()

	ct, _ := coll.Count(`{}`)
	if ct != 0 {
		t.Errorf("transaction not aborted successfully")
	}

	if coll.IsTransactionActive() == true {
		t.Errorf("IsTransactionActive() returned true though it should be false")
	}

	coll.BeginTransaction()
	coll.SaveBson(bytes)
	coll.SaveBson(bytes)

	if coll.IsTransactionActive() == false {
		t.Errorf("IsTransactionActive() returned false though it should be true")
	}

	coll.CommitTransaction()

	ct2, _ := coll.Count(`{}`)
	if ct2 != 2 {
		t.Errorf("transaction not committed successfully")
	}
}

func TestOneSnippetIntroFromReadme(t *testing.T) {
    jb, err := Open("addressbook", JBOWRITER | JBOCREAT | JBOTRUNC)
    if err != nil {
        os.Exit(1)
    }
    //Get or create collection 'contacts'
    coll, _ := jb.CreateColl("contacts", nil)

    //Insert one record:
    //JSON: {'name' : 'Bruce', 'phone' : '333-222-333', 'age' : 58}
    rec := map[string]interface{} {"name" : "Bruce", "phone" : "333-222-333", "age" : 58}
    bsrec, _ := bson.Marshal(rec)
    coll.SaveBson(bsrec)
    fmt.Printf("\nSaved Bruce")

    //Now execute query
    res, _ := coll.Find(`{"name" : {"$begin" : "Bru"}}`) //Name starts with 'Bru' string
    fmt.Printf("\n\nRecords found: %d\n", len(res))

    //Now print the result set records
    for _, bs := range res {
        var m map[string]interface{}
        bson.Unmarshal(bs, &m)
        fmt.Println(m)
    }

    //Close database
    jb.Close()
}

// long-running benchmarks to identify possible memory leaks

func BenchmarkSavingAndCounting(b *testing.B) {
	ejdb, _ := Open("/tmp/mybench.ejdb", JBOWRITER|JBOCREAT|JBOTRUNC|JBOTSYNC)
	coll, _ := ejdb.CreateColl("BenchmarkCollection", nil)
	bytes, _ := bson.Marshal(make_test_type())

	for i := 0; i < 100000; i++ {
		coll.SaveBson(bytes)
	}

	coll.Count(`{"s" : "mystring"}`)

	ejdb.Close()
}

func BenchmarkCounting(b *testing.B) {
	ejdb, _ := Open("/tmp/mybench2.ejdb", JBOWRITER|JBOCREAT|JBOTRUNC|JBOTSYNC)
	coll, _ := ejdb.CreateColl("BenchmarkCollection", nil)
	bytes, _ := bson.Marshal(make_test_type())
	coll.SaveBson(bytes)
	coll.Count(`{"s" : "mystring"}`)
	ejdb.Close()
}
