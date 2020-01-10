import Flutter
import UIKit
import EJDB2

public class SwiftEjdb2FlutterPlugin: NSObject, FlutterPlugin, FlutterStreamHandler {

  public static func register(with registrar: FlutterPluginRegistrar) {
    SwiftEjdb2FlutterPlugin(registrar)
  }

  let methodChannel: FlutterMethodChannel
  let eventChannel: FlutterEventChannel

  init(_ registrar: FlutterPluginRegistrar) {
    self.methodChannel = FlutterMethodChannel(name: "ejdb2", binaryMessenger: registrar.messenger())
    self.eventChannel = FlutterEventChannel(name: "ejdb2/query", binaryMessenger: registrar.messenger())
    super.init()
    registrar.addMethodCallDelegate(self, channel: methodChannel)
    eventChannel.setStreamHandler(self)
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    // result("iOS " + UIDevice.current.systemVersion)
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
