#define DART_SHARED_LIB
#include "dart_api.h"
#include "dart_native_api.h"

DART_EXPORT Dart_Handle dart_sqlite_Init(Dart_Handle parent_library) {
  return parent_library;
}

