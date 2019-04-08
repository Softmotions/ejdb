library ejdb2_dart;

import 'dart:async';
import 'dart:isolate';
import 'dart:nativewrappers' show NativeFieldWrapperClass2;

import 'dart-ext:ejdb2_dart';

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

  @override
  String toString() => '$runtimeType: $code $message';
}

/// EJDB document item
class JBDOC {
  final int id;
  final String json;
  JBDOC(this.id, this.json);
  JBDOC.fromList(List list) : this(list[0] as int, list[1] as String);
  @override
  String toString() => '$runtimeType: $id $json';
}

/// Query
class JQL extends NativeFieldWrapperClass2 {
  final String query;
  final String collection;
  final EJDB2 db;

  StreamController<JBDOC> _controller;
  RawReceivePort _replyPort;

  JQL._(this.db, this.query, this.collection);

  Stream<JBDOC> execute() {
    abort();
    _controller = StreamController<JBDOC>();
    _replyPort = RawReceivePort();
    _replyPort.handler = (dynamic reply) {
      if (reply is int) {
        _replyPort.close();
        _controller.addError(EJDB2Error.fromCode(reply));
        return;
      } else if (reply is List) {
        _controller.add(JBDOC.fromList(reply));
      } else if (reply == null) {
        abort();
      }
    };
    _exec(_replyPort.sendPort);
    return _controller.stream;
  }

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

  Future<int> scalarInt() {
    return execute().map((d) => d.id).first;
  }

  void _exec(SendPort sendPort) native 'exec';
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
      try {
        _set_handle(null);
      } catch (e) {
        completer.completeError(e);
        return;
      }
      completer.complete();
    };
    _port().send([replyPort.sendPort, 'close', hdb]);
    return completer.future;
  }

  Future<int> put(String collection, String json, [int id]) {
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
    _port().send([replyPort.sendPort, 'put', hdb, collection, json, id]);
    return completer.future;
  }

  Future<void> patch(String collection, String patch, int id) {
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
    _port().send([replyPort.sendPort, 'patch', hdb, collection, patch, id]);
    return completer.future;
  }

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

  Future<void> ensureStringIndex(String collection, String path, {bool unique = false}) {
    return _idx(collection, path, 0x04 | (unique ? 0x01 : 0));
  }

  Future<void> removeStringIndex(String collection, String path, {bool unique = false}) {
    return _rmi(collection, path, 0x04 | (unique ? 0x01 : 0));
  }

  Future<void> ensureIntIndex(String collection, String path, {bool unique = false}) {
    return _idx(collection, path, 0x08 | (unique ? 0x01 : 0));
  }

  Future<void> removeIntIndex(String collection, String path, {bool unique = false}) {
    return _rmi(collection, path, 0x08 | (unique ? 0x01 : 0));
  }

  Future<void> ensureFloatIndex(String collection, String path, {bool unique = false}) {
    return _idx(collection, path, 0x10 | (unique ? 0x01 : 0));
  }

  Future<void> removeFloatIndex(String collection, String path, {bool unique = false}) {
    return _rmi(collection, path, 0x10 | (unique ? 0x01 : 0));
  }

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
