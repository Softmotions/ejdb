#import "Ejdb2FlutterPlugin.h"
#if __has_include(<ejdb2_flutter/ejdb2_flutter-Swift.h>)
#import <ejdb2_flutter/ejdb2_flutter-Swift.h>
#else
// Support project import fallback if the generated compatibility header
// is not copied when this plugin is created as a library.
// https://forums.swift.org/t/swift-static-libraries-dont-copy-generated-objective-c-header/19816
#import "ejdb2_flutter-Swift.h"
#endif

@implementation Ejdb2FlutterPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  [SwiftEjdb2FlutterPlugin registerWithRegistrar:registrar];
}
@end
