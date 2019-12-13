import EJDB2Swift
import Foundation

print("EJDB2 version: \(EJDB2Swift.version)")

do {
  let db = try EJDB2Builder("swift.db").open()

  db.close()
} catch {
  print("Unexpected error: \(error).")
}