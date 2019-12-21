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
  }

  static var allTests = [
    ("testMain", testMain),
  ]
}