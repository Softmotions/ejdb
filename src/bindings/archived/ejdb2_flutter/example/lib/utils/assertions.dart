import 'package:collection/collection.dart';

class AssertionException implements Exception {
  AssertionException(this.check, [this.message]);

  final String check;
  final String message;

  @override
  String toString() {
    if (message == null) return 'AssertionException.${check}';
    return 'AssertionException.${check}: $message';
  }
}

void assertTrue(bool val, [String message]) {
  if (!val) {
    throw AssertionException('assertTrue', message);
  }
}

void assertFalse(bool val, [String message]) {
  if (val) {
    throw AssertionException('assertFalse', message);
  }
}

void assertEqual(var v1, var v2, [String message]) {
  if (v1 != v2) {
    throw AssertionException('assertEqual', message);
  }
}

void assertIdentical(var v1, var v2, [String message]) {
  if (!identical(v1, v2)) {
    throw AssertionException('assertIdentical', message);
  }
}

void assertNotIdentical(var v1, var v2, [String message]) {
  if (identical(v1, v2)) {
    throw AssertionException('assertNotIdentical', message);
  }
}

void assertDeepEqual(var v1, var v2, [bool unordered = true, String message]) {
  final deepEq = unordered
      ? const DeepCollectionEquality.unordered().equals
      : const DeepCollectionEquality().equals;
  if (!deepEq(v1, v2)) {
    throw AssertionException('assertDeepEquals', message);
  }
}

void assertDeepNotEqual(var v1, var v2, [bool unordered = true, String message]) {
  final deepEq = unordered
      ? const DeepCollectionEquality.unordered().equals
      : const DeepCollectionEquality().equals;
  if (deepEq(v1, v2)) {
    throw AssertionException('assertNotDeepEquals', message);
  }
}

void assertNotEqual(var v1, var v2, [String message]) {
  if (v1 == v2) {
    throw AssertionException('assertNotEquals', message);
  }
}

void assertNotNull(var v1, [String message]) {
  if (v1 == null) {
    throw AssertionException('assertNotNull', message);
  }
}

void assertNull(var v1, [String message]) {
  if (v1 != null) {
    throw AssertionException('assertNull', message);
  }
}

