import EJDB2
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

    XCTAssertThrowsError(try db.put("mycoll", #"{""#)) { err in
      let jbe = err as! EJDB2Error
      XCTAssertEqual(jbe.code, UInt64(JBL_ERROR_PARSE_UNQUOTED_STRING.rawValue))
      XCTAssertEqual(
        "\(jbe)", "EJDB2Error: 86005 Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING)")
    }

    id = try db.put("mycoll", ["foo": "bar"])
    XCTAssertEqual(id, 2)

    var list = try db.createQuery("@mycoll/*").list()
    XCTAssertEqual(
      list.map() { $0.object } as! [[String: String]], [["foo": "bar"], ["foo": "baz"]])

    var first = try db.createQuery("@mycoll/*").first()
    XCTAssertEqual(first!.object as! [String: String], ["foo": "bar"])

    first = try db.createQuery("@mycoll/[zzz=bbb]").first()
    XCTAssertTrue(first == nil)

    XCTAssertThrowsError(try db.createQuery("@mycoll/[zzz=bbb]").firstRequired()) { err in
      let jbe = err as! EJDB2Error
      XCTAssertEqual(jbe.code, UInt64(IWKV_ERROR_NOTFOUND.rawValue))
    }

    doc = try db.createQuery("@mycoll/[foo=:? and foo=:bar]")
      .setString(0, "baz")
      .setString("bar", "baz")
      .firstRequired()
    XCTAssertEqual(doc.object as! [String: String], ["foo": "baz"])

    list = try db.createQuery("@mycoll/[foo != :?]")
      .setString(0, "zaz")
      .setSkip(1)
      .list()
    XCTAssertEqual(list.count, 1)
    XCTAssertEqual(list.map() { $0.object } as! [[String: String]], [["foo": "baz"]])

    var count = try db.createQuery("/[foo = :?] | apply :? | count", "mycoll")
      .setString(0, "bar")
      .setJson(1, ["foo": "zaz"])
      .executeScalarInt()
    XCTAssertEqual(count, 1)

    count = try db.createQuery("@mycoll/[foo = :?]").setString(0, "zaz").executeScalarInt()
    XCTAssertEqual(count, 1)

    XCTAssertThrowsError(try db.createQuery("@mycoll/[")) { err in
      let jbe = err as! EJDB2Error
      XCTAssertTrue(jbe.isInvalidQuery)
    }

    try db.patch("mycoll", [["op": "add", "path": "/baz", "value": "qux"]], 1)
    doc = try db.get("mycoll", 1)
    XCTAssertEqual(doc.object as! [String: String], ["foo": "baz", "baz": "qux"])

    var info = try db.info()
    XCTAssertEqual(info.file, "test.db")
    XCTAssertEqual(info.size, 8192)
    XCTAssertEqual(info.collections.count, 1)

  }

  static var allTests = [
    ("testMain", testMain),
  ]
}