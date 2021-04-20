///
/// EJDB2 Dart VM native API binding.
///
/// See https://github.com/Softmotions/ejdb/blob/master/README.md
///
/// For API usage examples look into `/example` and `/test` folders.
///

library ejdb2_dart;

import 'dart:async';
import 'dart:convert';
import 'dart:convert' as convert_lib;
import 'dart:io';
import 'dart:isolate';
import 'dart:nativewrappers' show NativeFieldWrapperClass2;

import 'package:path/path.dart' as path_lib;
import 'package:quiver/core.dart';
import 'package:json_at/json_at.dart';

import 'dart-ext:ejdb2dart';

String ejdb2ExplainRC(int rc) native 'explain_rc';

/// EJDB specific exception
class EJDB2Error implements Exception {
  EJDB2Error(this.code, this.message);

  EJDB2Error.fromCode(int code) : this(code, ejdb2ExplainRC(code));

  EJDB2Error.invalidState() : this.fromCode(EJD_ERROR_INVALID_STATE);

  EJDB2Error.notFound() : this.fromCode(IWKV_ERROR_NOTFOUND);

  static int EJD_ERROR_CREATE_PORT = 89001;
  static int EJD_ERROR_POST_PORT = 89002;
  static int EJD_ERROR_INVALID_NATIVE_CALL_ARGS = 89003;
  static int EJD_ERROR_INVALID_STATE = 89004;
  static int IWKV_ERROR_NOTFOUND = 75001;

  final int code;

  final String message;

  bool get notFound => code == IWKV_ERROR_NOTFOUND;

  bool get invalidQuery => code == 87001;

  @override
  String toString() => '$runtimeType: $code $message';
}

/// EJDB document item
class JBDOC {
  JBDOC(this.id, this._json);
  JBDOC._fromList(List list) : this(list[0] as int, list[1] as String?);

  /// Document identifier
  final int id;

  /// Document body as JSON string
  String get json => _json ?? convert_lib.jsonEncode(_object);

  /// Document body as parsed JSON object.
  dynamic get object {
    if (_json == null) {
      return _object;
    } else {
      _object = convert_lib.jsonDecode(_json!);
      _json = null; // Release memory used to store JSON string data
      return _object;
    }
  }

  /// Gets subset of document using RFC 6901 JSON [pointer].
  Optional<dynamic> at(String pointer) => jsonAt(object, pointer);

  /// Gets subset of document using RFC 6901 JSON [pointer].
  Optional<dynamic> operator [](String pointer) => at(pointer);

  String? _json;

  dynamic _object;

  @override
  String toString() => '$runtimeType: $id $json';
}

/// Represents query on ejdb collection.
/// Instance can be reused for multiple queries reusing
/// placeholder parameters.
class JQL extends NativeFieldWrapperClass2 {
  JQL._(this.db, this.query, this.collection);

  final String query;
  final String collection;
  final EJDB2 db;

  StreamController<JBDOC>? _controller;
  RawReceivePort? _replyPort;

  /// Execute query and returns a stream of matched documents.
  ///
  /// [explainCallback] Used to get query execution log.
  /// [limit] Overrides `limit` set by query text for this execution session.
  ///
  Stream<JBDOC> execute({void explainCallback(String log)?, int limit = 0}) {
    abort();
    var execHandle = 0;
    _controller = StreamController<JBDOC>();
    _replyPort = RawReceivePort();
    _replyPort!.handler = (dynamic reply) {
      if (reply is int) {
        _exec_check(execHandle, true);
        _replyPort!.close();
        _controller!.addError(EJDB2Error.fromCode(reply));
        return;
      } else if (reply is List) {
        _exec_check(execHandle, false);
        if (reply[2] != null && explainCallback != null) {
          explainCallback(reply[2] as String);
        }
        _controller!.add(JBDOC._fromList(reply));
      } else {
        _exec_check(execHandle, true);
        if (reply != null && explainCallback != null) {
          explainCallback(reply as String);
        }
        abort();
      }
    };
    execHandle = _exec(_replyPort!.sendPort, explainCallback != null, limit);
    return _controller!.stream;
  }

  /// Returns optional element for first record in result set.
  Future<Optional<JBDOC>> first({void explainCallback(String log)?}) async {
    await for (final doc in execute(explainCallback: explainCallback, limit: 1)) {
      return Optional.of(doc);
    }
    return const Optional.absent();
  }

  /// Return first record in result set or throw not found [EJDB2Error] error.
  Future<JBDOC> firstRequired({void explainCallback(String log)?}) async {
    await for (final doc in execute(explainCallback: explainCallback, limit: 1)) {
      return doc;
    }
    throw EJDB2Error.notFound();
  }

  /// Collects up to [n] elements from result set into array.
  Future<List<JBDOC>> firstN(int n, {void explainCallback(String log)?}) async {
    final ret = <JBDOC>[];
    await for (final doc in execute(explainCallback: explainCallback, limit: n)) {
      if (n-- <= 0) break;
      ret.add(doc);
    }
    return ret;
  }

  /// Abort query execution.
  void abort() {
    _replyPort?.close();
    _replyPort = null;
    _controller?.close();
    _controller = null;
  }

  /// Return scalar integer value as result of query execution.
  /// For example execution of count query: `/... | count`
  Future<int> scalarInt({void explainCallback(String log)?}) {
    return execute(explainCallback: explainCallback).map((d) => d.id).first;
  }

  /// Set [json] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setJson(dynamic placeholder, dynamic json) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(json);
    _set(placeholder, _asJsonString(json), 1);
    return this;
  }

  /// Set [regexp] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setRegExp(dynamic placeholder, RegExp regexp) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(regexp);
    _set(placeholder, regexp.pattern, 2);
    return this;
  }

  /// Set integer [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setInt(dynamic placeholder, int val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _set(placeholder, val);
    return this;
  }

  /// Set double [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setDouble(dynamic placeholder, double val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _set(placeholder, val);
    return this;
  }

  /// Set boolean [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setBoolean(dynamic placeholder, bool val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _set(placeholder, val);
    return this;
  }

  /// Set string [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setString(dynamic placeholder, String val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _set(placeholder, val);
    return this;
  }

  /// Set `null` at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setNull(dynamic placeholder) {
    _checkPlaceholder(placeholder);
    _set(placeholder, null);
    return this;
  }

  /// Get current `limit` encoded in query.
  int get limit native 'jql_get_limit';

  void _checkPlaceholder(dynamic placeholder) {
    if (!(placeholder is String) && !(placeholder is int)) {
      ArgumentError.value(placeholder, 'placeholder');
    }
  }

  void _set(dynamic placeholder, dynamic value, [int type]) native 'jql_set';

  int _exec(SendPort sendPort, bool explain, int limit) native 'exec';

  void _exec_check(int execHandle, bool terminate) native 'check_exec';
}

/// Database wrapper
class EJDB2 extends NativeFieldWrapperClass2 {
  EJDB2._();

  static bool _checkCompleterPortError(Completer<dynamic> completer, dynamic reply) {
    if (reply is int) {
      completer.completeError(EJDB2Error(reply, ejdb2ExplainRC(reply)));
      return true;
    } else if (reply is! List) {
      completer.completeError(EJDB2Error(0, 'Invalid port response'));
      return true;
    }
    return false;
  }

  /// Open EJDB2 database
  /// See https://github.com/Softmotions/ejdb/blob/master/src/ejdb2.h#L104
  /// for description of options.
  static Future<EJDB2> open(String path,
      {bool truncate = false,
      bool readonly = false,
      bool http_enabled = false,
      bool http_read_anon = false,
      bool wal_enabled = true,
      bool wal_check_crc_on_checkpoint = false,
      int? wal_checkpoint_buffer_sz,
      int? wal_checkpoint_timeout_sec,
      int? wal_savepoint_timeout_sec,
      int? wal_wal_buffer_sz,
      int? document_buffer_sz,
      int? sort_buffer_sz,
      String? http_access_token,
      String? http_bind,
      int? http_max_body_size,
      int? http_port}) {
    final completer = Completer<EJDB2>();
    final replyPort = RawReceivePort();
    final jb = EJDB2._();

    path = path_lib.canonicalize(File(path).absolute.path);

    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      try {
        jb._set_handle((reply as List).first as int);
      } catch (e) {
        completer.completeError(e);
        return;
      }
      completer.complete(jb);
    };

    // Open
    var oflags = 0;
    if (readonly) {
      oflags |= 0x02;
    } else if (truncate) {
      oflags |= 0x04;
    }

    jb._port().send([
      replyPort.sendPort,
      'open',
      path,
      oflags,
      wal_enabled,
      wal_check_crc_on_checkpoint,
      wal_checkpoint_buffer_sz as dynamic,
      wal_checkpoint_timeout_sec as dynamic,
      wal_savepoint_timeout_sec as dynamic,
      wal_wal_buffer_sz as dynamic,
      document_buffer_sz as dynamic,
      sort_buffer_sz as dynamic,
      http_enabled,
      http_access_token as dynamic,
      http_bind as dynamic,
      http_max_body_size as dynamic,
      http_port as dynamic,
      http_read_anon
    ]);
    return completer.future;
  }

  /// Closes database instance.
  Future<void> close() {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.value();
    }
    final completer = Completer<void>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete();
    };
    _set_handle(null);
    _port().send([replyPort.sendPort, 'close', hdb as dynamic]);
    return completer.future;
  }

  /// Save [json] document under specified [id] or create a document
  /// with new generated `id`. Returns future holding actual document `id`.
  Future<int> put(String collection, dynamic json, [int? id]) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<int>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete((reply as List).first as int);
    };
    _port().send([
      replyPort.sendPort,
      'put',
      hdb as dynamic,
      collection,
      _asJsonString(json),
      id as dynamic
    ]);
    return completer.future;
  }

  /// Apply rfc6902/rfc7386 JSON [patch] to the document identified by [id].
  Future<void> patch(String collection, dynamic patchObj, int id, [bool upsert = false]) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<void>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete();
    };
    _port().send([
      replyPort.sendPort,
      'patch',
      hdb as dynamic,
      collection,
      _asJsonString(patchObj),
      id as dynamic,
      upsert
    ]);
    return completer.future;
  }

  /// Apply JSON merge patch (rfc7396) to the document identified by `id` or
  /// insert new document under specified `id`.
  Future<void> patchOrPut(String collection, dynamic patchObj, int id) =>
      patch(collection, patch, id, true);

  /// Get json body of document identified by [id] and stored in [collection].
  /// Throws [EJDB2Error] not found exception if document is not found.
  Future<String> get(String collection, int id) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<String>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete((reply as List).first as String);
    };
    _port().send([replyPort.sendPort, 'get', hdb as dynamic, collection, id]);
    return completer.future;
  }

  /// Get json body of database metadata.
  Future<String> info() {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<String>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete((reply as List).first as String);
    };
    _port().send([replyPort.sendPort, 'info', hdb as dynamic]);
    return completer.future;
  }

  /// Remove document idenfied by [id] from [collection].
  Future<void> del(String collection, int id) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<void>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete();
    };
    _port().send([replyPort.sendPort, 'del', hdb as dynamic, collection, id]);
    return completer.future;
  }

  /// Remove document idenfied by [id] from [collection].
  /// Doesn't raise error if document is not found.
  Future<void> delIgnoreNotFound(String collection, int id) =>
      del(collection, id).catchError((err) {
        if (err is EJDB2Error && err.notFound) {
          return Future.value();
        } else {
          return Future.error(err as Object);
        }
      });

  Future<void> renameCollection(String oldCollection, String newCollectionName) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<void>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete();
    };
    _port().send([replyPort.sendPort, 'rename', hdb as dynamic, oldCollection, newCollectionName]);
    return completer.future;
  }

  /// Ensures json document database index specified by [path] json pointer to string data type.
  Future<void> ensureStringIndex(String collection, String path, {bool unique = false}) {
    return _idx(collection, path, 0x04 | (unique ? 0x01 : 0));
  }

  /// Removes specified database index.
  Future<void> removeStringIndex(String collection, String path, {bool unique = false}) {
    return _rmi(collection, path, 0x04 | (unique ? 0x01 : 0));
  }

  /// Ensures json document database index specified by [path] json pointer to integer data type.
  Future<void> ensureIntIndex(String collection, String path, {bool unique = false}) {
    return _idx(collection, path, 0x08 | (unique ? 0x01 : 0));
  }

  /// Removes specified database index.
  Future<void> removeIntIndex(String collection, String path, {bool unique = false}) {
    return _rmi(collection, path, 0x08 | (unique ? 0x01 : 0));
  }

  /// Ensures json document database index specified by [path] json pointer to floating point data type.
  Future<void> ensureFloatIndex(String collection, String path, {bool unique = false}) {
    return _idx(collection, path, 0x10 | (unique ? 0x01 : 0));
  }

  /// Removes specified database index.
  Future<void> removeFloatIndex(String collection, String path, {bool unique = false}) {
    return _rmi(collection, path, 0x10 | (unique ? 0x01 : 0));
  }

  /// Removes database [collection].
  Future<void> removeCollection(String collection) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<void>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete();
    };
    _port().send([replyPort.sendPort, 'rmc', hdb as dynamic, collection]);
    return completer.future;
  }

  /// Creates an online database backup image and copies it into the specified [fileName].
  /// During online backup phase read/write database operations are allowed and not
  /// blocked for significant amount of time. Returns future with backup
  /// finish time as number of milliseconds since epoch.
  Future<int> onlineBackup(String fileName) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<int>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete((reply as List).first as int);
    };
    _port().send([replyPort.sendPort, 'bkp', hdb as dynamic, fileName]);
    return completer.future;
  }

  /// Create instance of [query] specified for [collection].
  /// If [collection] is not specified a [query] spec must contain collection name,
  /// eg: `@mycollection/[foo=bar]`
  JQL createQuery(String query, [String collection]) native 'create_query';

  Future<void> _idx(String collection, String path, int mode) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<void>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete();
    };
    _port().send([replyPort.sendPort, 'idx', hdb as dynamic, collection, path, mode]);
    return completer.future;
  }

  Future<void> _rmi(String collection, String path, int mode) {
    final hdb = _get_handle();
    if (hdb == null) {
      return Future.error(EJDB2Error.invalidState());
    }
    final completer = Completer<void>();
    final replyPort = RawReceivePort();
    replyPort.handler = (dynamic reply) {
      replyPort.close();
      if (_checkCompleterPortError(completer, reply)) {
        return;
      }
      completer.complete();
    };
    _port().send([replyPort.sendPort, 'rmi', hdb as dynamic, collection, path, mode]);
    return completer.future;
  }

  SendPort _port() native 'port';

  void _set_handle(int? handle) native 'set_handle';

  int? _get_handle() native 'get_handle';
}

String _asJsonString(dynamic val) {
  if (val is String) {
    return val;
  } else {
    return jsonEncode(val);
  }
}
