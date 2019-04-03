#define DART_SHARED_LIB
#include "dart_api.h"
#include "dart_native_api.h"
#include "ejdb2.h"
#include "ejdb2cfg.h"

Dart_NativeFunction ResolveName(Dart_Handle name,
                                int argc,
                                bool* auto_setup_scope);

DART_EXPORT Dart_Handle ejdb2_dart_Init(Dart_Handle parent_library) {
  return parent_library;
}

