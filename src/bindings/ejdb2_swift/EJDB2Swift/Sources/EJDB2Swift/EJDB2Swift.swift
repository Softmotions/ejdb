import EJDB2
import Foundation

public enum EJDB2Swift {

  /// EJDB2 engine version string
  public static var versionFull: String {
    return String(cString: ejdb_version_full())
  }

  /// EJDB2 engine version tuple: `(major, minor, patch)`
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

func toJsonCString(_ data: Any) throws -> CString {
  return try CString(toJsonString(data))
}

/// EJDB2 error code
public struct EJDB2Error: CustomStringConvertible, Error {

  init(_ code: UInt64) {
    self.code = code
    if let ptr = iwlog_ecode_explained(code) {
      self.message = String(cString: ptr)
    } else {
      self.message = nil
    }
  }

  /// `iwrc` error code.
  public let code: UInt64

  /// String describes associted `iwrc` error code.
  public let message: String?

  /// `True` If document not found error.
  public var isNotFound: Bool {
    return code == UInt64(IWKV_ERROR_NOTFOUND.rawValue)
  }

  /// `True` If query parsing error.
  public var isInvalidQuery: Bool {
    return code == UInt64(JQL_ERROR_QUERY_PARSE.rawValue)
  }

  public var description: String {
    return "EJDB2Error: \(code) \(message ?? "")"
  }
}

/// EJDB2 document item.
public final class JBDOC: CustomStringConvertible {

  /// Construct EJDB2 instance from given `id` and `json` string.
  public init(_ id: Int64, json: String) {
    self.id = id
    self.json = json
  }

  init(_ id: Int64, jbl: OpaquePointer) throws {
    guard let xstr = iwxstr_new() else {
      throw EJDB2Error(UInt64(IW_ERROR_ALLOC.rawValue))
    }
    defer {
      iwxstr_destroy(xstr)
    }
    try SWRC(jbl_as_json(jbl, jbl_xstr_json_printer, UnsafeMutableRawPointer(xstr), 0))
    self.id = id
    self.json = String(cString: iwxstr_ptr(xstr))
  }

  init(_ id: Int64, jbn: UnsafeMutablePointer<_JBL_NODE>) throws {
    guard let xstr = iwxstr_new() else {
      throw EJDB2Error(UInt64(IW_ERROR_ALLOC.rawValue))
    }
    defer {
      iwxstr_destroy(xstr)
    }
    try SWRC(jbl_node_as_json(jbn, jbl_xstr_json_printer, UnsafeMutableRawPointer(xstr), 0))
    self.id = id
    self.json = String(cString: iwxstr_ptr(xstr))
  }

  convenience init(_ id: Int64, swjbl: SWJBL) throws {
    try self.init(id, jbl: swjbl.handle!)
  }

  convenience init(_ doc: UnsafeMutablePointer<_EJDB_DOC>) throws {
    let id = doc.pointee.id
    if doc.pointee.node != nil {
      try self.init(id, jbn: doc.pointee.node!)
    } else {
      try self.init(id, jbl: doc.pointee.raw!)
    }
  }

  /// Document ID.
  public let id: Int64

  /// String representation of this document.
  public var description: String {
    return "JBDOC: \(id) \(json ?? object)"
  }

  public var object: Any {
    if _object != nil {
      return _object!
    }
    let data = json!.data(using: .utf8)!
    do {
      self._object = try JSONSerialization.jsonObject(with: data)
      // Release memory alloacted for JSON string
      json = nil
    } catch {
      // ignored since we can't declare props as throwable
    }
    return _object!
  }

  /// Gets subset of document using RFC 6901 JSON `pointer`.
  public func at(pointer: String) throws -> Any? {
    return try jsonAt(object, pointer)
  }

  /// Document body as JSON string.
  private var json: String?

  /// Parsed document body as json object.
  private var _object: Any? = nil
}

/// EJDB2 instance builder.
///
/// **Note:** HTTP API not enabled for Android/iOS
public final class EJDB2Builder {

  /// Database path.
  public let path: String

  /// Open database in readonly mode.
  public var readonly: Bool?

  /// Truncate database on open.
  public var truncate: Bool?

  /// WAL disabled.
  public var walDisabled: Bool?

  /// Check CRC32 sum of WAL data blocks during checkpoint.
  /// Default: false
  public var walCheckCRCOnCheckpoint: Bool?

  /// WAL checkpoint buffer size in bytes.
  /// Default: 64Mb for Android/iOS, 1Gb for others
  /// See https://iowow.io/wal
  public var walCheckpointBufferSize: UInt64?

  /// WAL checkpoint timeout seconds.
  /// Default: 60 (1 min) for Android/iOS, 300 (5 min) for others.
  /// Minimal allowed: 10
  /// See https://iowow.io/wal
  public var walCheckpointTimeout: UInt32?

  /// WAL savepoint timeout seconds.
  /// Default: 10 secs
  /// See https://iowow.io/wal
  public var walSavepointTimeout: UInt32?

  /// WAL file intermediate buffer size.
  /// Default: 2Mb Android/iOS, 8Mb for others
  /// https://iowow.io/wal
  public var walBufferSize: Int?

  /// **Note:** HTTP API is not available for Android/iOS/Windows.
  public var httpEnabled: Bool?

  /// Listen port number. Default: 9191.
  public var httpPort: Int32?

  /// Listen IP/host. Default: `localhost`.
  public var httpBind: String?

  /// Server access token passed in `X-Access-Token` header.
  public var httpAccessToken: String?

  /// Allow anonymous read-only database access over HTTP.
  public var httpReadAnon: Bool?

  /// Maximum WS/HTTP API body size in bytes.
  /// Default: 64Mb, Min: 512K
  public var httpMaxBodySize: UInt32?

  /// Initial size of buffer in bytes used to process/store document during query execution.
  /// Default 64Kb, Min: 16Kb
  public var documentBufferSize: UInt32?

  /// Max sorting buffer size. If exceeded an overflow temp file for sorted data will created.
  /// Default 16Mb, Min: 1Mb
  public var sortBufferSize: UInt32?

  /// Random seed used for database random generator.
  public var randomSeed: UInt32?

  /// Do not wait and raise error if database is locked by another process.
  public var fileLockFailFast: Bool?

  /// Create database instance on specified `path`.
  public init(_ path: String) {
    self.path = path
  }

  /// Set database readonly mode.
  public func withReadonly(_ v: Bool = true) -> EJDB2Builder {
    readonly = v
    return self
  }

  /// Truncate database data on open.
  public func withTruncate(_ v: Bool = true) -> EJDB2Builder {
    truncate = v
    return self
  }

  /// Disable WAL for database.
  public func withWalDisabled(_ v: Bool = true) -> EJDB2Builder {
    walDisabled = v
    return self
  }

  /// Check CRC32 sum of WAL data blocks during checkpoint.
  public func withWalCheckCRCOnCheckpoint(_ v: Bool = true) -> EJDB2Builder {
    walCheckCRCOnCheckpoint = v
    return self
  }

  /// WAL checkpoint buffer size in bytes.
  /// Default: 64Mb for Android/iOS, 1Gb for others
  /// https://iowow.io/wal
  public func withWalCheckpointBufferSize(_ v: UInt64) -> EJDB2Builder {
    walCheckpointBufferSize = v
    return self
  }

  /// WAL checkpoint timeout seconds.
  /// Default: 60 (1 min) for Android/iOS, 300 (5 min) for others.
  /// https://iowow.io/wal
  public func withWalCheckpointTimeout(_ v: UInt32) -> EJDB2Builder {
    walCheckpointTimeout = v
    return self
  }

  /// WAL file intermediate buffer size.
  /// Default: 2Mb Android/iOS, 8Mb for others
  /// https://iowow.io/wal
  public func withWalBufferSize(_ v: Int) -> EJDB2Builder {
    walBufferSize = v
    return self
  }

  /// **Note:** HTTP API is not available for Android/iOS/Windows.
  /// Enable HTTP/WS database endpoint
  public func withHttpEnabled(_ v: Bool = true) -> EJDB2Builder {
    httpEnabled = v
    return self
  }

  /// Listen port number.
  /// Default: 9191
  public func withHttpPort(_ v: Int32) -> EJDB2Builder {
    httpPort = v
    return self
  }

  /// Listen IP/host.
  /// Default: `localhost`
  public func withHttpBind(_ v: String) -> EJDB2Builder {
    httpBind = v
    return self
  }

  /// Server access token passed in `X-Access-Token` header
  public func withHttpAccessToken(_ v: String) -> EJDB2Builder {
    httpAccessToken = v
    return self
  }

  /// Allow anonymous read-only database access over HTTP
  public func withHttpReadAnon(_ v: Bool = true) -> EJDB2Builder {
    httpReadAnon = v
    return self
  }

  /// Maximum WS/HTTP API body size in bytes.
  /// Default: 64Mb, Min: 512K
  public func withHttpMaxBodySize(_ v: UInt32) -> EJDB2Builder {
    httpMaxBodySize = v
    return self
  }

  /// Initial size of buffer in bytes used to process/store document during query execution.
  /// Default 64Kb, min: 16Kb
  public func withDocumentBufferSize(_ v: UInt32) -> EJDB2Builder {
    documentBufferSize = v
    return self
  }

  /// Max sorting buffer size. If exceeded an overflow temp file for sorted data will created.
  /// Default 16Mb, Min: 1Mb
  public func withSortBufferSize(_ v: UInt32) -> EJDB2Builder {
    sortBufferSize = v
    return self
  }

  /// Random seed used for database random generator.
  public func withRandomSeed(_ v: UInt32) -> EJDB2Builder {
    randomSeed = v
    return self
  }

  /// Do not wait and raise error if database is locked by another process.
  public func withFileLockFailFast(_ v: Bool = true) -> EJDB2Builder {
    fileLockFailFast = v
    return self
  }

  /// Opens database instance.
  public func open() throws -> EJDB2 {
    var flags: iwkv_openflags = 0
    if self.readonly ?? false {
      flags |= IWKV_RDONLY
    }
    if self.truncate ?? false {
      flags |= IWKV_TRUNC
    }

    let cPath = CString(path)
    let cHttpBind = CString(httpBind)
    let cHttpAccessToken = CString(httpAccessToken)

    var opts = EJDB_OPTS(
      kv: IWKV_OPTS(
        path: cPath.buffer,
        random_seed: randomSeed ?? 0,
        oflags: flags,
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
        port: httpPort ?? 9191,
        bind: cHttpBind.bufferOrNil,
        access_token: cHttpAccessToken.bufferOrNil,
        access_token_len: cHttpAccessToken.count,
        blocking: false,
        read_anon: httpReadAnon ?? false,
        max_body_size: Int(httpMaxBodySize ?? 0)
      ),
      no_wal: walDisabled ?? false,
      sort_buffer_sz: sortBufferSize ?? 0,
      document_buffer_sz: documentBufferSize ?? 0
    )
    let ejdb2 = EJDB2()
    try SWRC(ejdb_open(&opts, &ejdb2.handle))
    return ejdb2
  }
}

/// EJDB2 Database info
public struct EJDB2Info: Codable {
  /// EJDB engine version
  var version: String

  /// Path to database storage file
  var file: String

  /// Database storage file size in bytes
  var size: Int64

  /// List of collections stored in db
  var collections: [CollectionInfo]

  public struct CollectionInfo: Codable {
    /// Collection name
    var name: String

    /// Collection database id
    var dbid: Int

    /// Number of documents in collection
    var rnum: Int64

    /// List of collections indexes
    var indexes: [IndexInfo]
  }

  public struct IndexInfo: Codable {
    /// rfc6901 JSON pointer to indexed field
    var ptr: String

    /// Index mode
    var mode: Int

    /// Index flags. See iwdb_flags_t
    var idbf: Int

    /// Index database ID
    var dbid: Int

    /// Number records stored in index database
    var rnum: Int64
  }
}

/// EJDB2 JBL Swift wrapper.
/// See ejdb2/jbl.h
final class SWJBL {

  init() {
  }

  init(_ data: Any) throws {
    let json = try toJsonString(data)
    try json.withCString {
      try SWRC(jbl_from_json(&handle, $0))
    }
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
    guard let xstr = iwxstr_new() else {
      throw EJDB2Error(UInt64(IW_ERROR_ALLOC.rawValue))
    }
    defer {
      iwxstr_destroy(xstr)
    }
    try SWRC(jbl_as_json(handle, jbl_xstr_json_printer, UnsafeMutableRawPointer(xstr), 0))
    return String(cString: iwxstr_ptr(xstr))
  }

  func toData() throws -> Data {
    guard let xstr = iwxstr_new() else {
      throw EJDB2Error(UInt64(IW_ERROR_ALLOC.rawValue))
    }
    var done = false
    defer {
      if !done {
        iwxstr_destroy(xstr)
      }
    }
    try SWRC(jbl_as_json(handle, jbl_xstr_json_printer, UnsafeMutableRawPointer(xstr), 0))
    let data = Data(
      bytesNoCopy: UnsafeMutableRawPointer(iwxstr_ptr(xstr))!,
      count: iwxstr_size(xstr),
      deallocator: .custom({ (_, _) in
        iwxstr_destroy(xstr)
      }))
    done = true
    return data
  }
}

/// EJDB2 JBL_NODE Swift wrapper.
/// See ejdb2/jbl.h
final class SWJBLN {

  init(_ data: Any, keep: Bool = false) throws {
    var done = false
    self.keep = keep
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
    try json.withCString {
      try SWRC(jbl_node_from_json($0, &handle, pool))
    }
    done = true
  }

  deinit {
    if pool != nil && !keep {
      iwpool_destroy(pool)
    }
  }

  var handle: UnsafeMutablePointer<_JBL_NODE>?
  private(set) var pool: OpaquePointer?
  private let keep: Bool

}

public typealias JBDOCVisitor = (_: JBDOC) -> Bool

/// EJDB2 Query builder/executor.
public final class SWJQL {

  init(_ db: EJDB2, _ query: String, _ collection: String?) throws {
    let cCollection = collection != nil ? CString(collection) : nil
    self.db = db
    try query.withCString {
      try SWRC(jql_create(&handle, cCollection?.buffer, $0))
    }
  }

  deinit {
    if handle != nil {
      jql_destroy(&handle)
    }
  }

  private(set) var handle: OpaquePointer?

  private var _skip: Int64?

  private var _limit: Int64?

  public let db: EJDB2

  /// Number of documents to skip.
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

  /// Max number of documents to fetch.
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

  public func setSkip(_ val: Int64) -> SWJQL {
    _skip = val
    return self
  }

  public func setLimit(_ val: Int64) -> SWJQL {
    _limit = val
    return self
  }

  /// Set in-query `JSON` object at the specified `placeholder`.
  public func setJson(_ placeholder: String, _ val: Any) throws -> SWJQL {
    try placeholder.withCString {
      let jbln = try SWJBLN(val, keep: true)
      try SWRC(
        jql_set_json2(
          handle, $0, 0, jbln.handle, swjb_free_json_node, UnsafeMutableRawPointer(jbln.pool)))
    }
    return self
  }

  /// Set in-query `JSON` object at the specified `index`.
  public func setJson(_ index: Int32, _ val: Any) throws -> SWJQL {
    let jbln = try SWJBLN(val, keep: true)
    try SWRC(
      jql_set_json2(
        handle, nil, index, jbln.handle, swjb_free_json_node, UnsafeMutableRawPointer(jbln.pool)))
    return self
  }

  /// Set in-query `String` object at the specified `placeholder`.
  public func setString(_ placeholder: String, _ val: String) throws -> SWJQL {
    let cPlaceholder = CString(placeholder)
    let cVal = CString(val, keep: true)
    try SWRC(jql_set_str2(handle, cPlaceholder.buffer, 0, cVal.buffer, swjb_free_str, nil))
    return self
  }

  /// Set in-query `String` object at the specified `index`.
  public func setString(_ index: Int32, _ val: String) throws -> SWJQL {
    let cVal = CString(val, keep: true)
    try SWRC(jql_set_str2(handle, nil, index, cVal.buffer, swjb_free_str, nil))
    return self
  }

  /// Set in-query `Int64` object at the specified `placeholder`.
  public func setInt64(_ placeholder: String, _ val: Int64) throws -> SWJQL {
    try placeholder.withCString {
      try SWRC(jql_set_i64(handle, $0, 0, val))
    }
    return self
  }

  /// Set in-query `Int64` object at the specified `index`.
  public func setInt64(_ index: Int32, _ val: Int64) throws -> SWJQL {
    try SWRC(jql_set_i64(handle, nil, index, val))
    return self
  }

  /// Set in-query `Double` object at the specified `placeholder`.
  public func setDouble(_ placeholder: String, _ val: Double) throws -> SWJQL {
    try placeholder.withCString {
      try SWRC(jql_set_f64(handle, $0, 0, val))
    }
    return self
  }

  /// Set in-query `Double` object at the specified `index`.
  public func setDouble(_ index: Int32, _ val: Double) throws -> SWJQL {
    try SWRC(jql_set_f64(handle, nil, index, val))
    return self
  }

  /// Set in-query `Bool` object at the specified `placeholder`.
  public func setBool(_ placeholder: String, _ val: Bool) throws -> SWJQL {
    try placeholder.withCString {
      try SWRC(jql_set_bool(handle, $0, 0, val))
    }
    return self
  }

  /// Set in-query `Bool` object at the specified `index`.
  public func setBool(_ index: Int32, _ val: Bool) throws -> SWJQL {
    try SWRC(jql_set_bool(handle, nil, index, val))
    return self
  }

  /// Set in-query regular expression string at the specified `placeholder`.
  public func setRegexp(_ placeholder: String, _ val: String) throws -> SWJQL {
    let cPlaceholder = CString(placeholder)
    let cVal = CString(val, keep: true)
    try SWRC(jql_set_regexp2(handle, cPlaceholder.buffer, 0, cVal.buffer, swjb_free_str, nil))
    return self
  }

  /// Set in-query regular expression string at the specified `index`.
  public func setRegexp(_ index: Int32, _ val: String) throws -> SWJQL {
    let cVal = CString(val, keep: true)
    try SWRC(jql_set_regexp2(handle, nil, 0, cVal.buffer, swjb_free_str, nil))
    return self
  }

  /// Set in-query `null` at the specified `placeholder`.
  public func setNull(_ placeholder: String) throws -> SWJQL {
    try placeholder.withCString {
      try SWRC(jql_set_null(handle, $0, 0))
    }
    return self
  }

  /// Set in-query `null` at the specified `index`.
  public func setNull(_ index: Int32) throws -> SWJQL {
    try SWRC(jql_set_null(handle, nil, index))
    return self
  }

  /// Reset all query parameters set previously.
  public func reset() -> SWJQL {
    jql_reset(handle, true, true)
    return self
  }

  /// Execute this query.
  ///
  /// Usage:
  ///
  ///     try q.execute { doc in
  ///       print("Doc: \(doc)")
  ///       return true
  ///     }
  ///
  /// - Parameter limit: Will owerride `limit` encoded in query
  /// - Parameter skip: Will owerride `skip` encoded in query
  /// - Parameter log: If `true` this method will return query execution log string
  /// - Parameter visitor: Visitor function.
  ///                      Returns `true` if you wish continue iterating over matching documents.
  ///
  @discardableResult
  public func execute(
    limit: Int64? = nil,
    skip: Int64? = nil,
    log: Bool = false,
    _ visitor: JBDOCVisitor? = nil
  ) throws -> (count: Int64, log: String?) {
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
    let cnt = try SWJQLExecutor(visitor).execute(&ux)
    return (cnt, logbuf != nil ? String(cString: iwxstr_ptr(logbuf)) : nil)
  }

  /// Returns first matched document.
  public func first() throws -> JBDOC? {
    var doc: JBDOC?
    try execute(limit: 1) { v in
      doc = v
      return false
    }
    return doc
  }

  /// Returns first matched document.
  /// Throw `EJDB2Error` if no matched documents found.
  public func firstRequired() throws -> JBDOC {
    guard let doc = try first() else {
      throw EJDB2Error(UInt64(IWKV_ERROR_NOTFOUND.rawValue))
    }
    return doc
  }

  /// Return scalar integer value as result of query execution.
  /// For example execution of count query: `/... | count`
  public func executeScalarInt() throws -> Int64 {
    let (cnt, _) = try execute()
    return cnt
  }

  /// Returns a list of matched documents.
  /// Use it with care to avoid wasting of memory.
  public func list(limit: Int64? = nil, skip: Int64? = nil) throws -> [JBDOC] {
    var ret = [JBDOC]()
    try execute(limit: limit, skip: skip) { doc in
      ret.append(doc)
      return true
    }
    return ret
  }
}

final class SWJQLExecutor {

  init(_ visitor: JBDOCVisitor?) {
    self.visitor = visitor
  }

  let visitor: JBDOCVisitor?

  func execute(_ uxp: UnsafeMutablePointer<_EJDB_EXEC>) throws -> Int64 {
    if visitor != nil {
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
            step.pointee = swe.visitor!(try JBDOC(doc)) ? 1 : 0
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
    }
    uxp.pointee.opaque = Unmanaged.passUnretained(self).toOpaque()
    try SWRC(ejdb_exec(uxp))
    return uxp.pointee.cnt
  }
}

/// EJDB2 database instances.
public final class EJDB2 {

  init() {
  }

  deinit {
    try? close()
  }

  var handle: OpaquePointer?

  /// Create instance of `SWJQL` query specified for `collection`
  ///
  /// If `collection` is not specified a `query` text
  /// spec must contain collection name,
  /// eg: `@mycollection/[foo=bar]`
  public func createQuery(_ query: String, _ collection: String? = nil) throws -> SWJQL {
    return try SWJQL(self, query, collection)
  }

  /// - Save `json` document under specified `id`
  /// - Create a new document with autogenerated `id` when `merge` is `false` and `id` is not specified
  /// - Apply JSON merge patch (rfc7396) to the document identified by `id` if `merge` is `true`
  ///   or insert it if document is not stored before
  @discardableResult
  public func put(
    _ collection: String, _ json: Any, _ id: Int64 = 0, merge: Bool = false
  ) throws -> Int64 {
    let cCollection = CString(collection)
    if merge {
      let jsonPatch = try toJsonString(patch)
      try jsonPatch.withCString {
        try SWRC(ejdb_merge_or_put(handle, cCollection.buffer, $0, id))
      }
      return id
    }
    let jbl = try SWJBL(json)
    if id == 0 {
      var oid: Int64 = 0
      try SWRC(ejdb_put_new(handle, cCollection.buffer, jbl.handle, &oid))
      return oid
    } else {
      try SWRC(ejdb_put(handle, cCollection.buffer, jbl.handle, id))
      return id
    }
  }

  /// Get document under specified `id` as optional value.
  public func getOptional(_ collection: String, _ id: Int64) throws -> JBDOC? {
    let jbl = SWJBL()
    let cCollection = CString(collection)
    let rc = ejdb_get(handle, cCollection.buffer, id, &jbl.handle)
    if rc == IWKV_ERROR_NOTFOUND.rawValue {
      return nil
    }
    try SWRC(rc)
    return try JBDOC(id, swjbl: jbl)
  }

  /// Get document under specified `id`
  /// Throws `EJDB2Error` with `IWKV_ERROR_NOTFOUND` error code
  /// if document is not found.
  public func get(_ collection: String, _ id: Int64) throws -> JBDOC {
    let jbl = SWJBL()
    try collection.withCString {
      try SWRC(ejdb_get(handle, $0, id, &jbl.handle))
    }
    return try JBDOC(id, swjbl: jbl)
  }

  /// Apply rfc6902/rfc6901 JSON patch to the document identified by `id`.
  public func patch(_ collection: String, _ patch: Any, _ id: Int64) throws {
    let cCollection = CString(collection)
    let jsonPatch = try toJsonString(patch)
    try jsonPatch.withCString {
      try SWRC(ejdb_patch(handle, cCollection.buffer, $0, id))
    }
  }

  /// Removes document identified by `id` from specified `collection`.
  /// Throws `EJDB2Error` with `IWKV_ERROR_NOTFOUND` error code
  /// if document is not found and `ignoreNotFound` is false.
  public func del(_ collection: String, _ id: Int64, ignoreNotFound: Bool = false) throws {
    let cCollection = CString(collection)
    let rc = ejdb_del(handle, cCollection.buffer, id)
    if ignoreNotFound && rc == IWKV_ERROR_NOTFOUND.rawValue {
      return
    }
    guard rc == 0 else { throw EJDB2Error(rc) }
  }

  /// Get database metadata
  public func info() throws -> EJDB2Info {
    let jbl = SWJBL()
    try SWRC(ejdb_get_meta(handle, &jbl.handle))
    return try JSONDecoder().decode(EJDB2Info.self, from: jbl.toData())
  }

  /// Ensures a `collection` is exists
  public func ensureCollection(_ collection: String) throws {
    try collection.withCString {
      try SWRC(ejdb_ensure_collection(handle, $0))
    }
  }

  /// Removes `collection`
  public func removeCollection(_ collection: String) throws {
    try collection.withCString {
      try SWRC(ejdb_remove_collection(handle, $0))
    }
  }

  /// Renames collection specified by `collection` to new `name`
  public func renameCollection(_ collection: String, _ name: String) throws {
    let cCollection = CString(collection)
    let cName = CString(name)
    try SWRC(ejdb_rename_collection(handle, cCollection.buffer, cName.buffer))
  }

  /// Creates an online database backup image and copies it into the specified `filePath`.
  /// During online backup phase read/write database operations are allowed and not
  /// blocked for significant amount of time. Returns backup finish time
  /// as number of milliseconds since epoch.
  public func onlineBackup(_ filePath: String) throws -> UInt64 {
    var ts: UInt64 = 0
    try filePath.withCString {
      try SWRC(ejdb_online_backup(handle, &ts, $0))
    }
    return ts
  }

  public func ensureStringIndex(_ collection: String, _ path: String, unique: Bool = false) throws {
    let cCollection = CString(collection)
    let cPath = CString(path)
    try SWRC(
      ejdb_ensure_index(
        handle, cCollection.buffer, cPath.buffer,
        EJDB_IDX_STR | (unique ? EJDB_IDX_UNIQUE : 0)))
  }

  public func ensureIntIndex(_ collection: String, _ path: String, unique: Bool = false) throws {
    let cCollection = CString(collection)
    let cPath = CString(path)
    try SWRC(
      ejdb_ensure_index(
        handle, cCollection.buffer, cPath.buffer,
        EJDB_IDX_I64 | (unique ? EJDB_IDX_UNIQUE : 0)))
  }

  public func ensureFloatIndex(_ collection: String, _ path: String, unique: Bool = false) throws {
    let cCollection = CString(collection)
    let cPath = CString(path)
    try SWRC(
      ejdb_ensure_index(
        handle, cCollection.buffer, cPath.buffer,
        EJDB_IDX_F64 | (unique ? EJDB_IDX_UNIQUE : 0)))
  }

  public func removeStringIndex(_ collection: String, _ path: String, unique: Bool = false) throws {
    let cCollection = CString(collection)
    let cPath = CString(path)
    try SWRC(
      ejdb_remove_index(
        handle, cCollection.buffer, cPath.buffer,
        EJDB_IDX_STR | (unique ? EJDB_IDX_UNIQUE : 0)))
  }

  public func removeIntIndex(_ collection: String, _ path: String, unique: Bool = false) throws {
    let cCollection = CString(collection)
    let cPath = CString(path)
    try SWRC(
      ejdb_remove_index(
        handle, cCollection.buffer, cPath.buffer,
        EJDB_IDX_I64 | (unique ? EJDB_IDX_UNIQUE : 0)))
  }

  public func removeFloatIndex(_ collection: String, _ path: String, unique: Bool = false) throws {
    let cCollection = CString(collection)
    let cPath = CString(path)
    try SWRC(
      ejdb_remove_index(
        handle, cCollection.buffer, cPath.buffer,
        EJDB_IDX_F64 | (unique ? EJDB_IDX_UNIQUE : 0)))
  }

  /// Closes a database instance.
  public func close() throws {
    var h = handle
    if h != nil {
      handle = nil
      try SWRC(ejdb_close(&h))
    }
  }
}