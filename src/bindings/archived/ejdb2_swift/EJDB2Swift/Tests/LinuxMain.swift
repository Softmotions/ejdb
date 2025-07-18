import XCTest
import EJDB2Tests

var tests = [XCTestCaseEntry]()
tests += EJDB2Tests.allTests()
XCTMain(tests)
