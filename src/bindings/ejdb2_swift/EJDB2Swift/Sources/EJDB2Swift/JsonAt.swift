import Foundation

public enum JsonAtError: Error {
  case invalidArgument(String)
}

public func jsonAt<T>(_ obj: Any, _ ptr: String) throws -> T? {
  if ptr.isEmpty {
    return obj as? T
  }
  let val: Any
  if obj is String {
    val = try JSONSerialization.jsonObject(with: (obj as! String).data(using: .utf8)!)
  } else {
    val = obj
  }
  if val is [AnyHashable: Any?] || val is [Any?] {
    var pp = try pointer(ptr)
    let tv = traverse(val, &pp)
    return tv is T? ? tv as! T? : tv as? T
  }
  return nil
}

fileprivate func pointer(_ pointer: String) throws -> [String] {
  let ptr = pointer.starts(with: "#") ? pointer.substring(fromIndex: 1).decodeUrl()! : pointer
  if ptr.isEmpty || ptr[0] != "/" {
    throw JsonAtError.invalidArgument("pointer")
  }
  if ptr.isEmpty {
    return [""]
  }

  return ptr.substring(fromIndex: 1)
    .split(separator: "/", omittingEmptySubsequences: false)
    .map {
      $0.replacingOccurrences(of: "~1", with: "/")
        .replacingOccurrences(of: "~0", with: "~")
    }
}

fileprivate func traverse(_ obj: Any?, _ pp: inout [String]) -> Any? {
  if pp.isEmpty {
    return obj
  }
  let key = pp.remove(at: 0)
  if obj is [AnyHashable: Any?] {
    let d = obj as! [AnyHashable: Any?]
    if d[key] == nil {
      return nil
    }
    return traverse(d[key]!, &pp)
  } else if obj is [Any?] {
    let a = obj as! [Any?]
    guard let ikey = Int(key) else {
      return nil
    }
    if ikey < 0 || ikey >= a.count {
      return nil
    }
    return traverse(a[ikey], &pp)
  }
  return nil
}