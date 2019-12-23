import Foundation

class CString {
  init(_ string: String?, keep: Bool = false) {
    self.keep = keep
    let s = string ?? ""
    (_len, buffer) = s.withCString {
      let len = Int(strlen($0) + 1)
      let buffer = strcpy(UnsafeMutablePointer<Int8>.allocate(capacity: len), $0)!
      return (len, buffer)
    }
  }

  deinit {
    if !keep {
      buffer.deallocate()
    }
  }

  var count: Int {
    return _len > 0 ? _len - 1 : 0
  }

  var bufferOrNil: UnsafeMutablePointer<Int8>? {
    return _len == 0 ? nil : buffer
  }

  let buffer: UnsafeMutablePointer<Int8>
  private let keep: Bool
  private let _len: Int
}

extension String {
  func encodeUrl() -> String? {
    return self.addingPercentEncoding(withAllowedCharacters: NSCharacterSet.urlQueryAllowed)
  }

  func decodeUrl() -> String? {
    return self.removingPercentEncoding
  }
}

extension String {

  var length: Int {
    return count
  }

  subscript(i: Int) -> String {
    return self[i..<i + 1]
  }

  func substring(fromIndex: Int) -> String {
    return self[min(fromIndex, length)..<length]
  }

  func substring(toIndex: Int) -> String {
    return self[0..<max(0, toIndex)]
  }

  subscript(r: Range<Int>) -> String {
    let range = Range(
      uncheckedBounds: (
        lower: max(0, min(length, r.lowerBound)),
        upper: min(length, max(0, r.upperBound))
      ))
    let start = index(startIndex, offsetBy: range.lowerBound)
    let end = index(start, offsetBy: range.upperBound - range.lowerBound)
    return String(self[start..<end])
  }
}