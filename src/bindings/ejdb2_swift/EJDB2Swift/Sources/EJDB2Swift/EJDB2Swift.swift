import EJDB2

public enum EJDB2Swift {

  public static var versionFull: String {
    return String(cString: ejdb_version_full())
  }

  public static var version: (major: Int, minor: Int, patch: Int) {
    return (Int(ejdb_version_major()), Int(ejdb_version_minor()), Int(ejdb_version_patch()))
  }
}

/// EJDB2 document items
public struct JBDOC {
  let id: Int64
  let json: String

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

  mutating func withReadonly() -> EJDB2Builder {
    readonly = true
    return self
  }

  mutating func withTruncate() -> EJDB2Builder {
    truncate = true
    return self
  }

  mutating func withWalDisabled() -> EJDB2Builder {
    walEnabled = false
    return self
  }
}

/// EJDB2
public class EJDB2 {

  internal private(set) var handle: OpaquePointer?

}