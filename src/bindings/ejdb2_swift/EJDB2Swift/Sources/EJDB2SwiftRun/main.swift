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

  id = try db.put("foo", ["foo": "bar"])
  print("ID: \(id)")

  let res = try db.get("foo", 1)
  print("\(res)")

  print("Good!")
} catch {
  print("Unexpected error: \(error).")
}