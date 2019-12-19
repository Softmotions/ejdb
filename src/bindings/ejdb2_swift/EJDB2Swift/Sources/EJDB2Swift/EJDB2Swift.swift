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

  init(_ id: Int64, jbl: OpaquePointer) throws {
    let xstr = iwxstr_new()
    defer {
      iwxstr_destroy(xstr)
    }
    try SWRC(jbl_as_json(jbl, jbl_xstr_json_printer, UnsafeMutableRawPointer(xstr), 0))
    self.id = id
    self.json = String(cString: iwxstr_ptr(xstr))
  }

  init(_ id: Int64, jbn: UnsafeMutablePointer<_JBL_NODE>) throws {
    let xstr = iwxstr_new()
    defer {
      iwxstr_destroy(xstr)
    }
    try SWRC(jbl_node_as_json(jbn, jbl_xstr_json_printer, UnsafeMutableRawPointer(xstr), 0))
    self.id = id
    self.json = String(cString: iwxstr_ptr(xstr))
  }

  convenience init(_ doc: UnsafeMutablePointer<_EJDB_DOC>) throws {
    let id = doc.pointee.id
    if doc.pointee.node != nil {
      try self.init(id, jbn: doc.pointee.node!)
    } else {
      try self.init(id, jbl: doc.pointee.raw!)
    }
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

  /// WAL disabled
  public var walDisabled: Bool?

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
    walDisabled = v
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
          enabled: !(walDisabled ?? false),
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
      no_wal: walDisabled ?? false,
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

public final class SWJQL {

  init(_ db: EJDB2, _ query: String, _ collection: String?) throws {
    try SWRC(jql_create(&handle, collection, query))
    self.db = db
  }

  deinit {
    jql_destroy(&handle)
  }

  private(set) var handle: OpaquePointer?

  private var _skip: Int64?

  private var _limit: Int64?

  public let db: EJDB2

  public var skip: Int64 {
    get {
      if _skip != nil {
        return _skip!
      }
      var v: Int64 = 0
      jql_get_skip(handle, &v)
      return v
    }
    set(val) {
      _skip = val
    }
  }

  public var limit: Int64 {
    get {
      if _limit != nil {
        return _limit!
      }
      var v: Int64 = 0
      jql_get_skip(handle, &v)
      return v
    }
    set(val) {
      _limit = val
    }
  }

  public func setJson(_ placeholder: String, _ val: String) throws -> SWJQL {
    let jbln = try SWJBLN(val)
    try SWRC(jql_set_json(handle, placeholder, 0, jbln.handle))
    return self
  }

  public func setJson(_ index: Int32, _ val: String) throws -> SWJQL {
    let jbln = try SWJBLN(val)
    try SWRC(jql_set_json(handle, nil, index, jbln.handle))
    return self
  }

  public func setString(_ placeholder: String, _ val: String) throws -> SWJQL {
    try SWRC(jql_set_str(handle, placeholder, 0, val))
    return self
  }

  public func setString(_ index: Int32, _ val: String) throws -> SWJQL {
    try SWRC(jql_set_str(handle, nil, index, val))
    return self
  }

  public func setInt64(_ placeholder: String, _ val: Int64) throws -> SWJQL {
    try SWRC(jql_set_i64(handle, placeholder, 0, val))
    return self
  }

  public func setInt64(_ index: Int32, _ val: Int64) throws -> SWJQL {
    try SWRC(jql_set_i64(handle, nil, index, val))
    return self
  }

  public func setDouble(_ placeholder: String, _ val: Double) throws -> SWJQL {
    try SWRC(jql_set_f64(handle, placeholder, 0, val))
    return self
  }

  public func setDouble(_ index: Int32, _ val: Double) throws -> SWJQL {
    try SWRC(jql_set_f64(handle, nil, index, val))
    return self
  }

  public func setBool(_ placeholder: String, _ val: Bool) throws -> SWJQL {
    try SWRC(jql_set_bool(handle, placeholder, 0, val))
    return self
  }

  public func setBool(_ index: Int32, _ val: Bool) throws -> SWJQL {
    try SWRC(jql_set_bool(handle, nil, index, val))
    return self
  }

  public func setRegexp(_ placeholder: String, _ val: String) throws -> SWJQL {
    try SWRC(jql_set_regexp(handle, placeholder, 0, val))
    return self
  }

  public func setRegexp(_ index: Int32, _ val: String) throws -> SWJQL {
    try SWRC(jql_set_regexp(handle, nil, 0, val))
    return self
  }

  public func setNull(_ placeholder: String) throws -> SWJQL {
    try SWRC(jql_set_null(handle, placeholder, 0))
    return self
  }

  public func setNull(_ index: Int32) throws -> SWJQL {
    try SWRC(jql_set_null(handle, nil, index))
    return self
  }

  public func reset() -> SWJQL {
    jql_reset(handle, true, true)
    return self
  }

  @discardableResult
  public func execute(
    skip: Int64? = nil,
    limit: Int64? = nil,
    log: Bool = false,
    _ visitor: @escaping (_: JBDOC) -> Bool
  ) throws -> String? {
    let logbuf = log ? iwxstr_new() : nil
    defer {
      if logbuf != nil {
        iwxstr_destroy(logbuf)
      }
    }
    var ux = _EJDB_EXEC(
      db: self.db.handle,
      q: self.handle,
      visitor: nil,  // Will be filled on execute(&ux)
      opaque: nil,  // Will be filled on execute(&ux)
      skip: skip ?? _skip ?? 0,
      limit: limit ?? _limit ?? 0,
      cnt: 0,
      log: logbuf,
      pool: nil)
    try SWJQLExecutor(visitor).execute(&ux)
    return logbuf != nil ? String(cString: iwxstr_ptr(logbuf)) : nil
  }

  public func first() throws -> JBDOC? {
    var doc: JBDOC?
    try execute(limit: 1) { v in doc = v
      return false
    }
    return doc
  }

  public func firstRequired() throws -> JBDOC {
    guard let doc = try first() else {
      throw EJDB2Error(UInt64(IWKV_ERROR_NOTFOUND.rawValue))
    }
    return doc
  }

  public func executeScalarInt() throws -> Int64 {
    return try first()?.id ?? 0
  }
}

final class SWJQLExecutor {

  init(_ visitor: @escaping (_: JBDOC) -> Bool) {
    self.visitor = visitor
  }

  let visitor: (_: JBDOC) -> Bool

  func execute(_
  uxp: UnsafeMutablePointer<_EJDB_EXEC>) throws {
    let _visitor:
      @convention(c) (
        _: UnsafeMutablePointer<_EJDB_EXEC>?,
        _: UnsafeMutablePointer<_EJDB_DOC>?,
        _: UnsafeMutablePointer<Int64>?
      ) -> iwrc = {
        let ux = $0!.pointee
        let doc = $1!
        let step = $2!
        let swe = Unmanaged<SWJQLExecutor>.fromOpaque(ux.opaque).takeUnretainedValue()
        do {
          step.pointee = swe.visitor(try JBDOC(doc)) ? 1 : 0
        } catch {
          if let jbe = error as? EJDB2Error {
            return jbe.code
          } else {
            return UInt64(IW_ERROR_FAIL.rawValue)
          }
        }
        return 0
      }
    uxp.pointee.visitor = _visitor
    uxp.pointee.opaque = Unmanaged.passUnretained(self).toOpaque()
    try SWRC(ejdb_exec(uxp))
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

  public func createQuery(_ query: String, _ collection: String? = nil) throws -> SWJQL {
    return try SWJQL(self, query, collection)
  }

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
    var jbl: OpaquePointer?
    try SWRC(ejdb_get(handle, collection, id, &jbl))
    return try JBDOC(id, jbl: jbl!)
  }

  // https://github.com/belozierov/SwiftCoroutine

  public func close() throws {
    if handle != nil {
      try SWRC(ejdb_close(&handle))
    }
  }
}