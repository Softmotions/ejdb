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

func SWRC(_ rc: UInt64) throws {
  guard rc == 0 else { throw EJDB2Error(rc) }
}

func toJsonString(_ data: Any) throws -> String {
  switch data {
  case is String:
    return data as! String
  case is Data:
    return String(data: data as! Data, encoding: .utf8)!
  default:
    return String(data: try JSONSerialization.data(withJSONObject: data), encoding: .utf8)!
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
public final class JBDOC: CustomStringConvertible {

  public init(_ id: Int64, json: String) {
    self.id = id
    self.json = json
  }

  init(_ id: Int64, jbl: SWJBL) throws {
    self.id = id
    self.json = try jbl.toString()
  }

  public let id: Int64
  public let json: String
  var _object: Any? = nil

  public var description: String {
    return "JBDOC: \(id) \(json)"
  }

  public var object: Any? {
    if _object != nil {
      return _object
    }
    let data = json.data(using: .utf8)!
    do {
      self._object = try JSONSerialization.jsonObject(with: data)
    } catch {
      // ignored since we can't declare props as throwable
    }
    return _object
  }
}

/// EJDB2 instance builder
public final class EJDB2Builder {

  /// Database path
  public let path: String

  /// Open database in readonly mode
  public var readonly: Bool?

  /// Truncate database on open
  public var truncate: Bool?

  /// WAL enabled
  public var walEnabled: Bool?

  public var walCheckCRCOnCheckpoint: Bool?

  public var walCheckpointBufferSize: UInt64?

  public var walCheckpointTimeout: UInt32?

  public var walSavepointTimeout: UInt32?

  public var walBufferSize: Int?

  public var httpEnabled: Bool?

  public var httpPort: Int32?

  public var httpBind: String?

  public var httpAccessToken: String?

  public var httpBlocking: Bool?

  public var httpReadAnon: Bool?

  public var httpMaxBodySize: UInt32?

  public var documentBufferSize: UInt32?

  public var sortBufferSize: UInt32?

  public var randomSeed: UInt32?

  public var fileLockFailFast: Bool?

  public init(_ path: String) {
    self.path = path
  }

  public func withReadonly(_ v: Bool = true) -> EJDB2Builder {
    readonly = v
    return self
  }

  public func withTruncate(_ v: Bool = true) -> EJDB2Builder {
    truncate = v
    return self
  }

  public func withWalDisabled(_ v: Bool = true) -> EJDB2Builder {
    walEnabled = !v
    return self
  }

  public func withWalCheckCRCOnCheckpoint(_ v: Bool = true) -> EJDB2Builder {
    walCheckCRCOnCheckpoint = v
    return self
  }

  public func withWalCheckpointBufferSize(_ v: UInt64) -> EJDB2Builder {
    walCheckpointBufferSize = v
    return self
  }

  public func withWalCheckpointTimeout(_ v: UInt32) -> EJDB2Builder {
    walCheckpointTimeout = v
    return self
  }

  public func withWalBufferSize(_ v: Int) -> EJDB2Builder {
    walBufferSize = v
    return self
  }

  public func withHttpEnabled(_ v: Bool = true) -> EJDB2Builder {
    httpEnabled = v
    return self
  }

  public func withHttpPort(_ v: Int32) -> EJDB2Builder {
    httpPort = v
    return self
  }

  public func withHttpBind(_ v: String) -> EJDB2Builder {
    httpBind = v
    return self
  }

  public func withHttpAccessToken(_ v: String) -> EJDB2Builder {
    httpAccessToken = v
    return self
  }

  public func withHttpBlocking(_ v: Bool = true) -> EJDB2Builder {
    httpBlocking = v
    return self
  }

  public func withHttpReadAnon(_ v: Bool = true) -> EJDB2Builder {
    httpReadAnon = v
    return self
  }

  public func withHttpMaxBodySize(_ v: UInt32) -> EJDB2Builder {
    httpMaxBodySize = v
    return self
  }

  public func withSortBufferSize(_ v: UInt32) -> EJDB2Builder {
    sortBufferSize = v
    return self
  }

  public func withDocumentBufferSize(_ v: UInt32) -> EJDB2Builder {
    documentBufferSize = v
    return self
  }

  public func withRandomSeed(_ v: UInt32) -> EJDB2Builder {
    randomSeed = v
    return self
  }

  public func withFileLockFailFast(_ v: Bool = true) -> EJDB2Builder {
    fileLockFailFast = v
    return self
  }

  public func open() throws -> EJDB2 {
    return try EJDB2(self)
  }

  var iwkvOpenFlags: iwkv_openflags {
    var flags: iwkv_openflags = 0
    if self.readonly ?? false {
      flags |= IWKV_RDONLY
    }
    if self.truncate ?? false {
      flags |= IWKV_TRUNC
    }
    return flags
  }

  var opts: EJDB_OPTS {
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
        access_token_len: httpAccessToken?.lengthOfBytes(using: String.Encoding.utf8) ?? 0,
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

/// EJDB2 JBL Swift wrapper
final class SWJBL {

  init() {
  }

  init(_ data: Any) throws {
    try SWRC(jbl_from_json(&handle, toJsonString(data)))
  }

  init(_ raw: OpaquePointer?) {
    self.handle = raw
  }

  deinit {
    if handle != nil {
      jbl_destroy(&handle)
    }
  }

  var handle: OpaquePointer?

  func toString() throws -> String {
    let xstr = iwxstr_new()
    defer {
      iwxstr_destroy(xstr)
    }
    try SWRC(jbl_as_json(handle, jbl_xstr_json_printer, UnsafeMutableRawPointer(xstr), 0))
    return String(cString: iwxstr_ptr(xstr))
  }
}

/// EJDB2 JBL_NODE Swift wrapper
final class SWJBLN {

  init(_ data: Any) throws {
    var done = false
    pool = iwpool_create(255)
    if pool == nil {
      throw EJDB2Error(UInt64(IW_ERROR_ALLOC.rawValue))
    }
    defer {
      if !done {
        iwpool_destroy(pool)
      }
    }
    let json = try toJsonString(data)
    try SWRC(jbl_node_from_json(json, &handle, pool))
    done = true
  }

  deinit {
    if pool != nil {
      iwpool_destroy(pool)
    }
  }

  var handle: UnsafeMutablePointer<_JBL_NODE>?

  private var pool: OpaquePointer?

}

final class SWJQL {

  init(collection: String?, query: String) throws {
    try SWRC(jql_create(&handle, collection, query))
  }

  deinit {
    jql_destroy(&handle)
  }

  private(set) var handle: OpaquePointer?

  public func setJson(_ placeholder: String, json: String) throws -> SWJQL {
    let jbln = try SWJBLN(json)
    try SWRC(jql_set_json(handle, placeholder, 0, jbln.handle))
    return self
  }

  public func setJson(_ index: Int32, json: String) throws -> SWJQL {
    let jbln = try SWJBLN(json)
    try SWRC(jql_set_json(handle, nil, index, jbln.handle))
    return self
  }


}

/// EJDB2
public final class EJDB2 {

  init(_ builder: EJDB2Builder) throws {
    var opts = builder.opts
    try SWRC(ejdb_open(&opts, &handle))
  }

  deinit {
    try? close()
  }

  private(set) var handle: OpaquePointer?

  public func put(_ collection: String, _ json: Any, _ id: Int64 = 0) throws -> Int64 {
    let jbl = try SWJBL(json)
    if id == 0 {
      var oid: Int64 = 0
      try SWRC(ejdb_put_new(handle, collection, jbl.handle, &oid))
      return oid
    } else {
      try SWRC(ejdb_put(handle, collection, jbl.handle, id))
      return id
    }
  }

  public func get(_ collection: String, _ id: Int64) throws -> JBDOC {
    let jbl = SWJBL()
    try SWRC(ejdb_get(handle, collection, id, &jbl.handle))
    return try JBDOC(id, jbl: jbl)
  }

  // https://forums.swift.org/t/call-swift-function-by-pointer/21083
  // https://github.com/belozierov/SwiftCoroutine

  public func close() throws {
    if handle != nil {
      try SWRC(ejdb_close(&handle))
    }
  }
}