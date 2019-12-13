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

/// EJDB2 error code
public struct EJDB2Error: CustomStringConvertible, Error {

  internal init(_ code: UInt64) {
    self.code = code
    if let ptr = iwlog_ecode_explained(code) {
      self.message = String(cString: ptr)
    } else {
      self.message = nil
    }
  }

  public let code: UInt64

  public let message: String?

  public var description: String {
    return "EJDB2Error: \(code) \(message ?? "")"
  }
}

/// EJDB2 document items
public struct JBDOC: CustomStringConvertible {

  public init(id: Int64, json: String) {
    self.id = id
    self.json = json
  }

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
}

/// EJDB2 instance builder
public struct EJDB2Builder {

  /// Database path
  let path: String

  /// Open database in readonly mode
  var readonly: Bool?

  /// Truncate database on open
  var truncate: Bool?

  /// WAL enabled
  var walEnabled: Bool?

  var walCheckCRCOnCheckpoint: Bool?

  var walCheckpointBufferSize: UInt64?

  var walCheckpointTimeout: UInt32?

  var walSavepointTimeout: UInt32?

  var walBufferSize: Int?

  var httpEnabled: Bool?

  var httpPort: Int32?

  var httpBind: String?

  var httpAccessToken: String?

  var httpBlocking: Bool?

  var httpReadAnon: Bool?

  var httpMaxBodySize: UInt32?

  var documentBufferSize: UInt32?

  var sortBufferSize: UInt32?

  var randomSeed: UInt32?

  var fileLockFailFast: Bool?

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

  mutating func withWalCheckpointBufferSize(v: UInt64) -> EJDB2Builder {
    walCheckpointBufferSize = v
    return self
  }

  mutating func withWalCheckpointTimeout(v: UInt32) -> EJDB2Builder {
    walCheckpointTimeout = v
    return self
  }

  mutating func withWalBufferSize(v: Int) -> EJDB2Builder {
    walBufferSize = v
    return self
  }

  mutating func withHttpEnabled(v: Bool = true) -> EJDB2Builder {
    httpEnabled = v
    return self
  }

  mutating func withHttpPort(v: Int32) -> EJDB2Builder {
    httpPort = v
    return self
  }

  mutating func withHttpBind(v: String) -> EJDB2Builder {
    httpBind = v
    return self
  }

  mutating func withHttpAccessToken(v: String) -> EJDB2Builder {
    httpAccessToken = v
    return self
  }

  mutating func withHttpBlocking(v: Bool = true) -> EJDB2Builder {
    httpBlocking = v
    return self
  }

  mutating func withHttpReadAnon(v: Bool = true) -> EJDB2Builder {
    httpReadAnon = v
    return self
  }

  mutating func withHttpMaxBodySize(v: UInt32) -> EJDB2Builder {
    httpMaxBodySize = v
    return self
  }

  mutating func withSortBufferSize(v: UInt32) -> EJDB2Builder {
    sortBufferSize = v
    return self
  }

  mutating func withDocumentBufferSize(v: UInt32) -> EJDB2Builder {
    documentBufferSize = v
    return self
  }

  mutating func withRandomSeed(v: UInt32) -> EJDB2Builder {
    randomSeed = v
    return self
  }

  mutating func withFileLockFailFast(v: Bool = true) -> EJDB2Builder {
    fileLockFailFast = v
    return self
  }

  func open() throws -> EJDB2 {
    return try EJDB2(self)
  }

  private var iwkvOpenFlags: iwkv_openflags {
    var flags: iwkv_openflags = 0
    if self.readonly ?? false {
      flags |= IWKV_RDONLY
    }
    if self.truncate ?? false {
      flags |= IWKV_TRUNC
    }
    return flags
  }

  internal var opts: EJDB_OPTS {
    return EJDB_OPTS(
      kv: IWKV_OPTS(
        path: path,
        random_seed: randomSeed ?? 0,
        oflags: iwkvOpenFlags,
        file_lock_fail_fast: fileLockFailFast ?? false,
        wal: IWKV_WAL_OPTS(
          enabled: walEnabled ?? true,
          check_crc_on_checkpoint: walCheckCRCOnCheckpoint ?? false,
          savepoint_timeout_sec: walSavepointTimeout ?? 0,
          checkpoint_timeout_sec: walCheckpointTimeout ?? 0,
          wal_buffer_sz: walBufferSize ?? 0,
          checkpoint_buffer_sz: walCheckpointBufferSize ?? 0
        )
      ),
      http: EJDB_HTTP(
        enabled: httpEnabled ?? false,
        port: httpPort ?? 0,
        bind: httpBind,
        access_token: httpAccessToken,
        access_token_len: 0,
        blocking: httpBlocking ?? false,
        read_anon: httpReadAnon ?? false,
        max_body_size: Int(httpMaxBodySize ?? 0)
      ),
      no_wal: !(walEnabled ?? true),
      sort_buffer_sz: sortBufferSize ?? 0,
      document_buffer_sz: documentBufferSize ?? 0
    )
  }
}

/// EJDB2
public class EJDB2 {

  internal private(set) var handle: OpaquePointer?

  internal init(_ builder: EJDB2Builder) throws {
    var opts = builder.opts
    let rc = ejdb_open(&opts, &handle)
    guard rc == 0 else { throw EJDB2Error(rc) }
  }

  func close() {

  }

  deinit {
  }
}