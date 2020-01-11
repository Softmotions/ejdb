import Flutter
import EJDB2
import PathKit

typealias MethodInvocation = (_: DbEntry) -> Void

extension Dictionary {
  func containsKey(_ key: Key) -> Bool {
    return self[key] != nil
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

func asNumber(_ v: Any?) -> NSNumber {
  if let c = v as? NSNumber {
    return c
  }
  return 0
}

class DbEntry {
  let db: EJDB2DB
  let handle: UInt32
  let path: String
  var counter: UInt32 = 0

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
    if counter == 0 {
      try db.close()
    }
    return counter == 0
  }
}

struct DbMethodCall {
  let dbe: DbEntry?
  let args: [Any]
  let result: FlutterResult

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
    NSLog("\nCall method \(call.method)")
    let args = call.arguments as? [Any] ?? []
    var dbe: DbEntry?

    if let dbid = args.first as? NSNumber {
      dbe = dbmap[dbid.uint32Value]
      if dbe == nil {
        result(FlutterError(code: "@ejdb", message: "Database handle already disposed", details: nil))
        return
      }
    }
    let mc = DbMethodCall(dbe: dbe, args: args, result: result)
    //result(FlutterMethodNotImplemented)

    switch call.method {
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

  func open(_ mc: DbMethodCall) throws {
    var path = Path(mc.args[1] as! String)
    if !path.isAbsolute {
      let documentsPath = NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true)[0]
      path = Path(documentsPath) + path
    }
    path = path.normalize()
    NSLog("\nOPEN: \(path)")
    var dbe = dbmap.values.first(where: { $0.path == path.description })
    if dbe != nil {
      dbe!.countOpen()
      mc.successOnMainThread(dbe!.handle)
      return
    }
    let cfg = mc.args[2] as? [String: Any] ?? [:]
    var b = EJDB2Builder(path.description)
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
      b.withWalCheckpointBufferSize(asNumber(cfg["wal_checkpoint_buffer_sz"]).uint64Value)
    }
    if cfg.containsKey("wal_checkpoint_timeout_sec") {
      b.withWalCheckpointTimeout(asNumber(cfg["wal_checkpoint_timeout_sec"]).uint32Value)
    }
    if cfg.containsKey("wal_savepoint_timeout_sec") {
      b.withWalSavepointTimeout(asNumber(cfg["wal_checkpoint_timeout_sec"]).uint32Value) 
    }
    if cfg.containsKey("wal_wal_buffer_sz") {
      b.withWalBufferSize(asNumber(cfg["wal_wal_buffer_sz"]).intValue)
    }
    if cfg.containsKey("document_buffer_sz") {
      b.withDocumentBufferSize(asNumber(cfg["document_buffer_sz"]).uint32Value)
    }
    if cfg.containsKey("sort_buffer_sz") {
      b.withSortBufferSize(asNumber(cfg["sort_buffer_sz"]).uint32Value)
    }
    dbkeys += 1
    let key = dbkeys
    dbmap[key] = try DbEntry(b.open(), key, path.description)
    mc.successOnMainThread(key as NSNumber)
  }

  func close(_ mc: DbMethodCall) throws {
    if try mc.dbe?.close() ?? false {
      dbmap.removeValue(forKey: mc.dbe?.handle ?? 0)
    }
    mc.successOnMainThread()
  }

  public func onListen(withArguments arguments: Any?, eventSink events: FlutterEventSink) -> FlutterError? {
    //fatalError("onListen(withArguments:eventSink:) has not been implemented")
    return nil
  }

  public func onCancel(withArguments arguments: Any?) -> FlutterError? {
    //fatalError("onCancel(withArguments:) has not been implemented")
    return nil
  }
}
