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
import 'dart:io';
import 'dart:isolate';
import 'dart:nativewrappers' show NativeFieldWrapperClass2;

import 'package:path/path.dart' as path_lib;

import 'dart-ext:ejdb2dart';

String ejdb2ExplainRC(int rc) native 'explain_rc';

/// EJDB specific exception
class EJDB2Error implements Exception {
  static int EJD_ERROR_CREATE_PORT = 89001;
  static int EJD_ERROR_POST_PORT = 89002;
  static int EJD_ERROR_INVALID_NATIVE_CALL_ARGS = 89003;
  static int EJD_ERROR_INVALID_STATE = 89004;

  final int code;

  final String message;

  EJDB2Error(this.code, this.message);

  EJDB2Error.fromCode(int code) : this(code, ejdb2ExplainRC(code));

  EJDB2Error.invalidState() : this.fromCode(EJD_ERROR_INVALID_STATE);

  bool get notFound => code == 75001;

  bool get invalidQuery => code == 87001;

  @override
  String toString() => '$runtimeType: $code $message';
}

/// EJDB document item
class JBDOC {
  /// Document identifier
  final int id;

  /// Document JSON body as string
  final String json;

  JBDOC(this.id, this.json);
  JBDOC.fromList(List list) : this(list[0] as int, list[1] as String);
  @override
  String toString() => '$runtimeType: $id $json';
}

/// Represents query on ejdb collection.
/// Instance can be reused for multiple queries reusing
/// placeholder parameters.
class JQL extends NativeFieldWrapperClass2 {
  final String query;
  final String collection;
  final EJDB2 db;

  StreamController<JBDOC> _controller;
  RawReceivePort _replyPort;

  JQL._(this.db, this.query, this.collection);

  /// Execute query and returns a stream of documents in result set.
  Stream<JBDOC> execute({void explainCallback(String log)}) {
    abort();
    _controller = StreamController<JBDOC>();
    _replyPort = RawReceivePort();
    _replyPort.handler = (dynamic reply) {
      if (reply is int) {
        _replyPort.close();
        _controller.addError(EJDB2Error.fromCode(reply));
        return;
      } else if (reply is List) {
        if (reply[2] != null && explainCallback != null) {
          explainCallback(reply[2] as String);
        }
        _controller.add(JBDOC.fromList(reply));
      } else {
        if (reply != null && explainCallback != null) {
          explainCallback(reply as String);
        }
        abort();
      }
    };
    _exec(_replyPort.sendPort, explainCallback != null);
    return _controller.stream;
  }

  /// Abort query execution.
  void abort() {
    if (_replyPort != null) {
      _replyPort.close();
      _replyPort = null;
    }
    if (_controller != null) {
      _controller.close();
      _controller = null;
    }
  }

  /// Return scalar integer value as result of query execution.
  /// For example execution of count query: `/... | count`
  Future<int> scalarInt({void explainCallback(String log)}) {
    return execute(explainCallback: explainCallback).map((d) => d.id).first;
  }

  /// Set [json] at specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setJson(dynamic placeholder, Object json) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(json);
    _set(placeholder, _asJsonString(json), 1);
    return this;
  }

  /// Set [regexp] at specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setRegExp(dynamic placeholder, RegExp regexp) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(regexp);
    _set(placeholder, regexp.pattern, 2);
    return this;
  }

  /// Set integer [val] at specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setInt(dynamic placeholder, int val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _set(placeholder, val);
    return this;
  }

  /// Set double [val] at specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setDouble(dynamic placeholder, double val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _set(placeholder, val);
    return this;
  }

  /// Set boolean [val] at specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setBoolean(dynamic placeholder, bool val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _set(placeholder, val);
    return this;
  }

  /// Set string [val] at specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setString(dynamic placeholder, String val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _set(placeholder, val);
    return this;
  }

  /// Set `null` at specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setNull(dynamic placeholder) {
    _checkPlaceholder(placeholder);
    _set(placeholder, null);
    return this;
  }

  void _checkPlaceholder(dynamic placeholder) {
    if (!(placeholder is String) && !(placeholder is int)) {
      ArgumentError.value(placeholder, 'placeholder');
    }
  }

  void _set(dynamic placeholder, dynamic value, [int type]) native 'jql_set';

  void _exec(SendPort sendPort, bool explain) native 'exec';
}

/// Database wrapper
class EJDB2 extends NativeFieldWrapperClass2 {
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
  /// for options description.
  static Future<EJDB2> open(String path,
      {bool truncate = false,
      bool readonly = false,
      bool http_enabled = false,
      bool http_read_anon = false,
      bool wal_enabled = true,
      bool wal_check_crc_on_checkpoint = false,
      int wal_checkpoint_buffer_sz,
      int wal_checkpoint_timeout_sec,
      int wal_savepoint_timeout_sec,
      int wal_wal_buffer_sz,
      int document_buffer_sz,
      int sort_buffer_sz,
      String http_access_token,
      String http_bind,
      int http_max_body_size,
      int http_port}) {
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
    final args = <dynamic>[]
      ..add(replyPort.sendPort)
      ..add('open')
      ..add(path)
      ..add(oflags)
      ..add(wal_enabled)
      ..add(wal_check_crc_on_checkpoint)
      ..add(wal_checkpoint_buffer_sz)
      ..add(wal_checkpoint_timeout_sec)
      ..add(wal_savepoint_timeout_sec)
      ..add(wal_wal_buffer_sz)
      ..add(document_buffer_sz)
      ..add(sort_buffer_sz)
      ..add(http_enabled)
      ..add(http_access_token)
      ..add(http_bind)
      ..add(http_max_body_size)
      ..add(http_port)
      ..add(http_read_anon);

    jb._port().send(args);
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
    _port().send([replyPort.sendPort, 'close', hdb]);
    return completer.future;
  }

  /// Save [json] document under specified [id] or create a document
  /// with new generated `id`. Returns future holding actual document `id`.
  Future<int> put(String collection, Object json, [int id]) {
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
    _port().send([replyPort.sendPort, 'put', hdb, collection, _asJsonString(json), id]);
    return completer.future;
  }

  /// Apply rfc6902/rfc6901 JSON [patch] to the document identified by [id].
  Future<void> patch(String collection, Object patch, int id) {
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
    _port().send([replyPort.sendPort, 'patch', hdb, collection, _asJsonString(patch), id]);
    return completer.future;
  }

  /// Get json body of document identified by [id] and stored in [collection].
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
    _port().send([replyPort.sendPort, 'get', hdb, collection, id]);
    return completer.future;
  }

  /// Get json body with database metadata.
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
    _port().send([replyPort.sendPort, 'info', hdb]);
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
    _port().send([replyPort.sendPort, 'del', hdb, collection, id]);
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
    _port().send([replyPort.sendPort, 'rmc', hdb, collection]);
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
    _port().send([replyPort.sendPort, 'idx', hdb, collection, path, mode]);
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
    _port().send([replyPort.sendPort, 'rmi', hdb, collection, path, mode]);
    return completer.future;
  }

  SendPort _port() native 'port';

  void _set_handle(int handle) native 'set_handle';

  int _get_handle() native 'get_handle';

  EJDB2._();
}

String _asJsonString(Object val) {
  if (val is String) {
    return val;
  } else {
    return jsonEncode(val);
  }
}
