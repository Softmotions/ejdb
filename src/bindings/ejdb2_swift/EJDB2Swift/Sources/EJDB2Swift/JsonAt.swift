import Foundation

public func jsonAt(obj: Any, pointer: String) throws -> Any? {
  if pointer.isEmpty {
    return nil
  }
  let val: Any
  if obj is String {
    val = try JSONSerialization.jsonObject(with: (obj as! String).data(using: .utf8)!)
  } else {
    val = obj
  }
  if val is [AnyHashable: Any] || val is [Any] {
    // todo:
  }
  return nil
}

fileprivate func pointer() {
  // todo:
}

fileprivate func traverse() {
  // todo:
}