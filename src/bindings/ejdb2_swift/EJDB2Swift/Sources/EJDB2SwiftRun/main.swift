import EJDB2Swift
import Foundation

print("EJDB2 version: \(EJDB2Swift.version)")
do {
  let db = try EJDB2Builder("swift.db").withTruncate().open()
  defer {
    try? db.close()
  }

  var id = try db.put("foo", #"{"foo": "bar"}"#)
  print("ID: \(id)")

  id = try db.put("foo", ["foo": "baz"])
  print("ID: \(id)")

  let res = try db.get("foo", 1)
  print("\(res)")

  let q = try db.createQuery("@foo/*")

  try q.execute { doc in
    print("Doc: \(doc)")
    return true
  }

  let log = try q.execute(log: true) { doc in
    return true
  }
  print("Log: \(log!)")

  var cnt = try q.executeScalarInt()
  print("Count: \(cnt)")

  cnt = try db.createQuery("/[foo=:?]", "foo").setString(0, "bar").executeScalarInt()
  print("Count: \(cnt)")

  print("Good!")
} catch {
  print("Unexpected error: \(error).")
}