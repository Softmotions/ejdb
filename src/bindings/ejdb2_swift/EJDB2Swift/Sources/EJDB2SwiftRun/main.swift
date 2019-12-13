import EJDB2Swift
import Foundation

print("EJDB2 version: \(EJDB2Swift.version)")

do {
  let db = try EJDB2Builder("swift.db").withTruncate().open()
  defer {
    try? db.close()
  }

  let id = try db.put("foo", "{\"foo\": \"bar\"}")
  print("ID: \(id)")

  print("Good!")
} catch {
  print("Unexpected error: \(error).")
}