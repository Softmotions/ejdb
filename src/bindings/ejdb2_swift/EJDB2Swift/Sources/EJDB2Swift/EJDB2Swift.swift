import EJDB2
import Foundation

public enum EJDB2Swift {

  public static var versionFull: String {
    return String(cString: ejdb_version_full())
  }

  public static var version: (major: Int, minor: Int, patch: Int) {
    return (Int(ejdb_version_major()), Int(ejdb_version_minor()), Int(ejdb_version_patch()))
  }
}

/// EJDB2 document items
public struct JBDOC: CustomStringConvertible {
  public let id: Int64
  public let json: String
  var _object: Any? = nil

  public var description: String {
    return "JBDOC: \(id) \(json)"
  }

  public var object: Any? {
    mutating get {
      if _object != nil {
        return _object
      }
      let data = json.data(using: String.Encoding.utf8)!
      do {
        self._object = try JSONSerialization.jsonObject(with: data)
      } catch {  // ignored
      }
      return _object
    }
  }

  public init(id: Int64, json: String) {
    self.id = id
    self.json = json
  }
}

/// EJDB2 instance builder
public struct EJDB2Builder {

  /// Database path
  let path: String

  /// Open database in readonly mode
  var readonly: Bool = false

  /// Truncate database on open
  var truncate: Bool = false

  /// WAL enabled
  var walEnabled = true

  var walCheckCRCOnCheckpoint: Bool?

  var walCheckpointBufferSize: Int?

  var walCheckpointTimeout: Int?

  var walSavepointTimeout: Int?

  var walBufferSize: Int?

  var documentBufferSize: Int?

  var sortBufferSize: Int?

  init(path: String) {
    self.path = path
  }

  mutating func withReadonly(v: Bool = true) -> EJDB2Builder {
    readonly = v
    return self
  }

  mutating func withTruncate(v: Bool = true) -> EJDB2Builder {
    truncate = v
    return self
  }

  mutating func withWalEnabled(v: Bool = true) -> EJDB2Builder {
    walEnabled = v
    return self
  }

  mutating func withWalCheckCRCOnCheckpoint(v: Bool = true) -> EJDB2Builder {
    walCheckCRCOnCheckpoint = v
    return self
  }

  mutating func withWalCheckpointBufferSize(v: Int) -> EJDB2Builder {
    walCheckpointBufferSize = v
    return self
  }

  mutating func withWalCheckpointTimeout(v: Int) -> EJDB2Builder {
    walCheckpointTimeout = v
    return self
  }

  mutating func withWalBufferSize(v: Int) -> EJDB2Builder {
    walBufferSize = v
    return self
  }
}

/// EJDB2
public class EJDB2 {

  internal private(set) var handle: OpaquePointer?

}