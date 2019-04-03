import 'dart:nativewrappers' show NativeFieldWrapperClass2;

class EJDB2Error implements Exception {
  final int code;

  final String message;

  EJDB2Error(int rc, String message) : this._internal(rc, message);

  EJDB2Error._internal(this.code, this.message);

  @override
  String toString() {
    return '$runtimeType: $code $message';
  }
}

class EJDB2 extends NativeFieldWrapperClass2 {}
