import Foundation
import XCTest

@testable import EJDB2Swift

final class EJDB2SwiftTests: XCTestCase {

  override func setUp() {
    super.setUp()
  }

  override func tearDown() {
    super.tearDown()
  }

  func testMain() throws {
    let db = try EJDB2Builder("test.db").withTruncate().open()
    defer {
      try? db.close()
    }
    var id = try db.put("mycoll", ["foo": "bar"])
    XCTAssertEqual(id, 1)

    var doc = try db.get("mycoll", id)
    XCTAssertEqual(doc.id, 1)
    XCTAssertEqual(doc.object as! Dictionary, ["foo": "bar"])

    id = try db.put("mycoll", #"{"foo":"baz"}"#, id)
    XCTAssertEqual(doc.id, 1)


  }

  static var allTests = [
    ("testMain", testMain),
  ]
}