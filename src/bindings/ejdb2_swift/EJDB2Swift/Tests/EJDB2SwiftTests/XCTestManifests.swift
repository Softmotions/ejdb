import XCTest

#if !canImport(ObjectiveC)
public func allTests() -> [XCTestCaseEntry] {
    return [
        testCase(EJDB2SwiftTests.allTests),
    ]
}
#endif
