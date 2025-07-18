import Flutter
import EJDB2
import PathKit

typealias MethodInvocation = (_: DbEntry) -> Void

typealias DbQueryHandler = (_: SWJQL, _: String?) throws -> Void

extension Dictionary {
  func containsKey(_ key: Key) -> Bool {
    self[key] != nil
  }
}

func asBoolean(_ v: Any?, _ def: Bool = false) -> Bool {
  if let c = v as? NSNumber {
    return c.intValue > 0
  }
  if let c = v as? NSString {
    return c.isEqual(to: "true")
  }
  if let c = v as? Bool {
    return c
  }
  return def
}

func asNumber(_ v: Any?) -> NSNumber? {
  if let c = v as? NSNumber {
    return c
  }
  if let c = v as? String {
    return NSNumber(value: Int64(c) ?? 0)
  }
  return nil
}

func asDouble(_ v: Any?) -> Double? {
  if let c = v as? NSNumber {
    return c.doubleValue
  }
  if let c = v as? String {
    return Double(c)
  }
  return nil
}

func asString(_ v: Any?, _ def: String? = nil) -> String? {
  if let c = v as? String {
    return c
  }
  if v != nil {
    return String(describing: v!)
  }
  return def
}

class DbEntry {
  let db: EJDB2DB
  let handle: UInt32
  let path: String
  var counter: Int = 0

  init(_ db: EJDB2DB, _ handle: UInt32, _ path: String) {
    self.db = db
    self.handle = handle
    self.path = path
  }

  func countOpen() {
    counter += 1
  }

  func close() throws -> Bool {
    counter -= 1
    if counter <= 0 {
      try db.close()
    }
    return counter <= 0
  }
}

struct DbMethodCall {
  let dbe: DbEntry?
  let args: [Any?]
  let result: FlutterResult

  var db: EJDB2DB {
    dbe!.db
  }

  func successOnMainThread(_ val: Any? = nil) {
    DispatchQueue.main.async {
      self.result(val)
    }
  }

  func errorOnMainThread(_ code: String, _ message: String?, _ details: Any? = nil) {
    DispatchQueue.main.async {
      self.result(FlutterError(code: code, message: message, details: details))
    }
  }
}

public class SwiftEjdb2FlutterPlugin: NSObject, FlutterPlugin, FlutterStreamHandler {

  public static func register(with registrar: FlutterPluginRegistrar) {
    let _ = SwiftEjdb2FlutterPlugin(registrar)
  }

  let methodChannel: FlutterMethodChannel
  let eventChannel: FlutterEventChannel
  var dbmap: [UInt32: DbEntry] = [:]
  var dbkeys: UInt32 = 0
  var events: FlutterEventSink?

  init(_ registrar: FlutterPluginRegistrar) {
    self.methodChannel = FlutterMethodChannel(name: "ejdb2", binaryMessenger: registrar.messenger())
    self.eventChannel = FlutterEventChannel(name: "ejdb2/query", binaryMessenger: registrar.messenger())
    super.init()
    registrar.addMethodCallDelegate(self, channel: methodChannel)
    eventChannel.setStreamHandler(self)
  }

  func execute(_ mc: DbMethodCall, _ block: @escaping (_: DbMethodCall) throws -> Void) {
    DispatchQueue.global().async {
      do {
        try block(mc)
      } catch let error as EJDB2Error {
        mc.errorOnMainThread("@ejdb IWRC:\(error.code)", error.message)
      } catch {
        mc.errorOnMainThread("@ejdb", "\(error)")
      }
    }
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    // NSLog("\nCall method \(call.method)")
    let args = call.arguments as? [Any?] ?? []
    var dbe: DbEntry?
    if let dbid = args.first as? NSNumber {
      dbe = dbmap[dbid.uint32Value]
      if dbe == nil {
        result(FlutterError(code: "@ejdb", message: "Database handle already disposed", details: nil))
        return
      }
    }

    let mc = DbMethodCall(dbe: dbe, args: args, result: result)

    switch call.method {
    case "get":
      execute(mc, get)
      break
    case "put":
      execute(mc, put)
      break;
    case "patch":
      execute(mc, patch)
      break
    case "del":
      execute(mc, del)
      break
    case "info":
      execute(mc, info)
      break
    case "executeList":
      execute(mc, executeList)
      break
    case "executeFirst":
      execute(mc, executeFirst)
      break
    case "executeScalarInt":
      execute(mc, executeScalarInt)
      break
    case "executeQuery":
      execute(mc, executeQuery)
      break
    case "renameCollection":
      execute(mc, renameCollection)
      break
    case "removeCollection":
      execute(mc, removeCollection)
      break
    case "ensureFloatIndex":
      execute(mc, ensureFloatIndex)
      break
    case "ensureStringIndex":
      execute(mc, ensureStringIndex)
      break
    case "ensureIntIndex":
      execute(mc, ensureIntIndex)
      break
    case "removeFloatIndex":
      execute(mc, removeFloatIndex)
      break
    case "removeStringIndex":
      execute(mc, removeStringIndex)
      break
    case "removeIntIndex":
      execute(mc, removeIntIndex)
      break
    case "onlineBackup":
      execute(mc, onlineBackup)
      break
    case "open":
      execute(mc, open)
      break
    case "close":
      execute(mc, close)
      break
    default:
      NSLog("\nMethod call: \(call.method) not implemented")
      result(FlutterMethodNotImplemented)
      break
    }
  }

  func executeFirst(_ mc: DbMethodCall) throws {
    return try executeListImpl(mc, 1)
  }

  func executeList(_ mc: DbMethodCall) throws {
    return try executeListImpl(mc)
  }

  func executeListImpl(_ mc: DbMethodCall, _ limit: Int64? = nil) throws {
    try prepareQuery(mc) { q, _ in
      if limit != nil {
        q.setLimit(limit!)
      }
      var res: [Any?] = []
      try q.execute() { doc in
        res.append(doc.id)
        res.append(doc.json)
        return true
      }
      mc.successOnMainThread(res)
    }
  }

  func executeScalarInt(_ mc: DbMethodCall) throws {
    try prepareQuery(mc) { q, _ in
      let res = try q.executeScalarInt()
      mc.successOnMainThread(res)
    }
  }

  func executeQuery(_ mc: DbMethodCall) throws {
    try prepareQuery(mc) { q, hook in
      var batch: [Any?] = []
      try q.execute() { doc in
        if batch.count >= 128 && batch.count % 2 == 0 {
          batch.insert(hook, at: 0)
          let res = batch
          DispatchQueue.main.sync {
            self.events?(res)
          }
          batch = []
        }
        batch.append(doc.id)
        batch.append(doc.json)
        return true
      }
      batch.insert(hook, at: 0)
      batch.append(true)
      DispatchQueue.main.sync {
        self.events?(batch)
      }
    }
  }

  func prepareQuery(_ mc: DbMethodCall, _ qh: DbQueryHandler) throws {
    let db = mc.db
    let hook = asString(mc.args[1])
    let coll = asString(mc.args[2])
    let qtext = asString(mc.args[3], "")!
    let qspec = mc.args[4] as! [String: Any?]
    let params = mc.args[5] as! [[Any?]]

    let q = try db.createQuery(qtext, coll)
    qspec.forEach { k, v in
      switch (k) {
      case "l": // limit
        q.setLimit(asNumber(v)?.int64Value ?? 0)
        break
      case "s": // skip
        q.setSkip(asNumber(v)?.int64Value ?? 0)
        break
      default:
        break
      }
    }
    for pslot in params {
      let type = asNumber(pslot[0])?.intValue
      let plh = pslot[1]
      let val = pslot[2]
      if type == 0 || val == nil {
        if let v = plh as? NSNumber {
          try q.setNull(v.int32Value)
        } else {
          try q.setNull(plh as! String)
        }
      } else {
        switch (type) {
        case 1: // String
          if let v = plh as? NSNumber {
            try q.setString(v.int32Value, asString(val))
          } else {
            try q.setString(plh as! String, asString(val))
          }
          break
        case 2: // Long
          if let v = plh as? NSNumber {
            try q.setInt64(v.int32Value, asNumber(val)?.int64Value)
          } else {
            try q.setInt64(plh as! String, asNumber(val)?.int64Value)
          }
          break
        case 3: // Double
          if let v = plh as? NSNumber {
            try q.setDouble(v.int32Value, asDouble(val))
          } else {
            try q.setDouble(plh as! String, asDouble(val))
          }
          break
        case 4: // Boolean
          if let v = plh as? NSNumber {
            try q.setBool(v.int32Value, asBoolean(val))
          } else {
            try q.setBool(plh as! String, asBoolean(val))
          }
          break
        case 5: // Regexp
          if let v = plh as? NSNumber {
            try q.setRegexp(v.int32Value, asString(val))
          } else {
            try q.setRegexp(plh as! String, asString(val))
          }
          break
        case 6: // JSON
          if let v = plh as? NSNumber {
            try q.setJson(v.int32Value, val!)
          } else {
            try q.setJson(plh as! String, val!)
          }
          break
        default:
          break
        }
      }
    }
    try qh(q, hook)
  }

  func onlineBackup(_ mc: DbMethodCall) throws {
    var path = Path(asString(mc.args[1])!)
    if !path.isAbsolute {
      let documentsPath = NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true)[0]
      path = Path(documentsPath) + path
    }
    NSLog("\nOnline backup into: \(path)")
    mc.successOnMainThread(try mc.db.onlineBackup(path.string))
  }

  func removeFloatIndex(_ mc: DbMethodCall) throws {
    let coll = asString(mc.args[1])!
    let path = asString(mc.args[2])!
    let unique = asBoolean(mc.args[3])
    try mc.db.removeFloatIndex(coll, path, unique: unique)
    mc.successOnMainThread()
  }

  func ensureFloatIndex(_ mc: DbMethodCall) throws {
    let coll = asString(mc.args[1])!
    let path = asString(mc.args[2])!
    let unique = asBoolean(mc.args[3])
    try mc.db.ensureFloatIndex(coll, path, unique: unique)
    mc.successOnMainThread()
  }

  func removeIntIndex(_ mc: DbMethodCall) throws {
    let coll = asString(mc.args[1])!
    let path = asString(mc.args[2])!
    let unique = asBoolean(mc.args[3])
    try mc.db.removeIntIndex(coll, path, unique: unique)
    mc.successOnMainThread()
  }

  func ensureIntIndex(_ mc: DbMethodCall) throws {
    let coll = asString(mc.args[1])!
    let path = asString(mc.args[2])!
    let unique = asBoolean(mc.args[3])
    try mc.db.ensureIntIndex(coll, path, unique: unique)
    mc.successOnMainThread()
  }

  func removeStringIndex(_ mc: DbMethodCall) throws {
    let coll = asString(mc.args[1])!
    let path = asString(mc.args[2])!
    let unique = asBoolean(mc.args[3])
    try mc.db.removeStringIndex(coll, path, unique: unique)
    mc.successOnMainThread()
  }

  func ensureStringIndex(_ mc: DbMethodCall) throws {
    let coll = asString(mc.args[1])!
    let path = asString(mc.args[2])!
    let unique = asBoolean(mc.args[3])
    try mc.db.ensureStringIndex(coll, path, unique: unique)
    mc.successOnMainThread()
  }

  func removeCollection(_ mc: DbMethodCall) throws {
    let coll = asString(mc.args[1])!
    try mc.db.removeCollection(coll)
    mc.successOnMainThread()
  }

  func renameCollection(_ mc: DbMethodCall) throws {
    let oldName = asString(mc.args[1])!
    let newName = asString(mc.args[2])!
    try mc.db.renameCollection(oldName, newName)
    mc.successOnMainThread()
  }

  func del(_ mc: DbMethodCall) throws {
    let coll = asString(mc.args[1])!
    let id = asNumber(mc.args[2])!.int64Value
    try mc.db.del(coll, id)
    mc.successOnMainThread()
  }

  func patch(_ mc: DbMethodCall) throws {
    let db = mc.db
    let coll = asString(mc.args[1])!
    let json = asString(mc.args[2])!
    let id = asNumber(mc.args[3])!.int64Value
    let upsert = asBoolean(mc.args[4])
    if upsert {
      try db.put(coll, json, id, merge: true)
    } else {
      try db.patch(coll, json, id)
    }
    mc.successOnMainThread()
  }

  func get(_ mc: DbMethodCall) throws {
    let db = mc.db
    let coll = asString(mc.args[1])!
    let id = asNumber(mc.args[2])!.int64Value
    let doc = try db.get(coll, id)
    mc.successOnMainThread(doc.json)
  }

  func put(_ mc: DbMethodCall) throws {
    let db = mc.db
    let coll = asString(mc.args[1])!
    let json = asString(mc.args[2])!
    let id = asNumber(mc.args[3])?.int64Value ?? 0
    let res = try db.put(coll, json, id)
    mc.successOnMainThread(res)
  }

  func info(_ mc: DbMethodCall) throws {
    let info = try mc.db.info()
    let data = String(data: try JSONEncoder().encode(info), encoding: .utf8)
    mc.successOnMainThread(data)
  }

  func open(_ mc: DbMethodCall) throws {
    var path = Path(mc.args[1] as! String)
    if !path.isAbsolute {
      let documentsPath = NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true)[0]
      path = Path(documentsPath) + path
    }
    path = path.normalize()
    let dbe = dbmap.values.first(where: { $0.path == path.string })
    if dbe != nil {
      dbe!.countOpen()
      mc.successOnMainThread(dbe!.handle)
      return
    }
    let cfg = mc.args[2] as? [String: Any] ?? [:]
    var b = EJDB2Builder(path.string)
    if cfg.containsKey("truncate") {
      b = b.withTruncate(asBoolean(cfg["truncate"]))
    }
    if cfg.containsKey("readonly") {
      b = b.withReadonly(asBoolean(cfg["readonly"]))
    }
    if cfg.containsKey("wal_enabled") {
      b.withWalDisabled(!asBoolean(cfg["wal_enabled"], true))
    }
    if cfg.containsKey("wal_check_crc_on_checkpoint") {
      b.withWalCheckCRCOnCheckpoint(asBoolean(cfg["wal_check_crc_on_checkpoint"]))
    }
    if cfg.containsKey("wal_checkpoint_buffer_sz") {
      b.withWalCheckpointBufferSize(asNumber(cfg["wal_checkpoint_buffer_sz"])?.uint64Value ?? 0)
    }
    if cfg.containsKey("wal_checkpoint_timeout_sec") {
      b.withWalCheckpointTimeout(asNumber(cfg["wal_checkpoint_timeout_sec"])?.uint32Value ?? 0)
    }
    if cfg.containsKey("wal_savepoint_timeout_sec") {
      b.withWalSavepointTimeout(asNumber(cfg["wal_checkpoint_timeout_sec"])?.uint32Value ?? 0)
    }
    if cfg.containsKey("wal_wal_buffer_sz") {
      b.withWalBufferSize(asNumber(cfg["wal_wal_buffer_sz"])?.intValue ?? 0)
    }
    if cfg.containsKey("document_buffer_sz") {
      b.withDocumentBufferSize(asNumber(cfg["document_buffer_sz"])?.uint32Value ?? 0)
    }
    if cfg.containsKey("sort_buffer_sz") {
      b.withSortBufferSize(asNumber(cfg["sort_buffer_sz"])?.uint32Value ?? 0)
    }
    dbkeys += 1
    let key = dbkeys
    dbmap[key] = try DbEntry(b.open(), key, path.string)
    mc.successOnMainThread(key as NSNumber)
  }

  func close(_ mc: DbMethodCall) throws {
    if try mc.dbe?.close() ?? false {
      dbmap.removeValue(forKey: mc.dbe?.handle ?? 0)
    }
    mc.successOnMainThread()
  }

  public func onListen(withArguments arguments: Any?, eventSink events: @escaping FlutterEventSink) -> FlutterError? {
    self.events = events
    return nil
  }

  public func onCancel(withArguments arguments: Any?) -> FlutterError? {
    self.events = nil
    return nil
  }
}
