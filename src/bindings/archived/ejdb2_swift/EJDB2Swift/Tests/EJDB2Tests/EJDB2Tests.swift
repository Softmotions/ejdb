import CEJDB2
import Foundation
import XCTest

@testable import EJDB2

final class EJDB2Tests: XCTestCase {

  override func setUp() {
    super.setUp()
  }

  override func tearDown() {
    super.tearDown()
  }

  func testMain() throws {
    var db = try EJDB2Builder("test.db").withTruncate().open()
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
    //XCTAssertEqual(info.size, 8192)
    XCTAssertEqual(info.collections.count, 1)

    var (cnt, log) = try db.createQuery("@mycoll/* | count").execute(log: true)
    XCTAssertEqual(cnt, 2)
    XCTAssertEqual(log, "[INDEX] NO [COLLECTOR] PLAIN\n")

    id = try db.put("mycoll", ["zzz": "bbb"])
    doc = try db.get("mycoll", id)
    XCTAssertNotNil(doc)

    try db.del("mycoll", id)
    XCTAssertThrowsError(try db.get("mycoll", id)) { err in
      let jbe = err as! EJDB2Error
      XCTAssertTrue(jbe.isNotFound)
    }

    try db.ensureStringIndex("mycoll", "/foo", unique: true)
    info = try db.info()
    XCTAssertEqual(info.collections[0].indexes[0].ptr, "/foo")

    (cnt, log) = try db.createQuery("@mycoll/[foo = :?] | count").setString(0, "baz").execute(log: true)
    XCTAssertEqual(cnt, 1)
    XCTAssertEqual(
      log,
      "[INDEX] MATCHED  UNIQUE|STR|2 /foo EXPR1: \'foo = :?\' INIT: IWKV_CURSOR_EQ\n[INDEX] SELECTED UNIQUE|STR|2 /foo EXPR1: \'foo = :?\' INIT: IWKV_CURSOR_EQ\n [COLLECTOR] PLAIN\n"
    )

    doc = try db.createQuery("@mycoll/[foo re :?]").setRegexp(0, ".*").firstRequired()
    XCTAssertEqual(doc.object as! [String: String], ["foo": "zaz"])

    try db.renameCollection("mycoll", "mycoll2")
    cnt = try db.createQuery("@mycoll2/* | count").executeScalarInt()
    XCTAssertEqual(cnt, 2)

    try db.removeCollection("mycoll2")
    cnt = try db.createQuery("@mycoll2/* | count").executeScalarInt()
    XCTAssertEqual(cnt, 0)

    info = try db.info()
    XCTAssertEqual(info.collections.count, 0)

    for i in 1...1000 {
      try db.put("cc1", ["name": "v\(i)"])
    }
    cnt = try db.createQuery("@cc1/* | count").executeScalarInt()
    XCTAssertEqual(cnt, 1000)

    let ts0 = UInt64(Date().timeIntervalSince1970 * 1000)
    let ts = try db.onlineBackup("hello-bkp.db")
    XCTAssertTrue(ts0 < ts)
    try db.close()

    db = try EJDB2Builder("hello-bkp.db").open()
    cnt = try db.createQuery("@cc1/* | count").executeScalarInt()
    XCTAssertEqual(cnt, 1000)
  }

  func testJsonAt() throws {
    XCTAssertNil(try jsonAt(["foo": "baz"], "/bar"))
    XCTAssertEqual(try jsonAt(["foo": "baz"], "/foo"), "baz")
    XCTAssertEqual(
      try jsonAt(
        [
          "foo": [
            "bar": [
              "baz": [
                ["gaz": 33]
              ]
            ]
          ],
        ], "/foo/bar/baz/0/gaz"), 33)
    XCTAssertEqual(try jsonAt(["c%d": 1], "#/c%25d"), 1)
    XCTAssertEqual(try jsonAt(["~foo~": "bar"], "/~0foo~0"), "bar")
    XCTAssertEqual(try jsonAt(["/foo/": "bar"], "/~1foo~1"), "bar")
    XCTAssertEqual(try jsonAt(["": "bar"], "/"), "bar")
    XCTAssertEqual(try jsonAt([" ": "bar"], "/ "), "bar")
    XCTAssertEqual(try jsonAt(["bar": ["": "baz"]], "/bar/"), "baz")
    XCTAssertEqual(try jsonAt(["foo": "bar"], ""), ["foo": "bar"])
  }

  static var allTests = [
    ("testJsonAt", testJsonAt),
    ("testMain", testMain),
  ]
}