library ejdb2_dart;

import 'dart:async';
import 'dart:isolate';
import 'dart:convert';
import 'dart:nativewrappers' show NativeFieldWrapperClass2;
import 'dart:typed_data';

import 'dart-ext:/home/adam/Projects/softmotions/ejdb/build/src/bindings/ejdb2_dart/ejdb2_dart';

String ejdb2ExplainRC(int rc) native 'explain_rc';

class EJDB2Error implements Exception {
  final int code;

  final String message;

  EJDB2Error(this.code, this.message);

  @override
  String toString() {
    return '$runtimeType: $code $message';
  }
}


/// Query
class JQL extends NativeFieldWrapperClass2 {
  final String query;
  final String collection;
  final EJDB2 db;

  StreamController<ByteBuffer> _controller;
  RawReceivePort _replyPort;

  JQL._(this.db, this.query, this.collection);

  Stream<ByteBuffer> execute() {
    abort();
    _controller = StreamController<ByteBuffer>();
    _replyPort = RawReceivePort();
    _replyPort.handler = (dynamic reply) {
      if (reply is int) {
        _replyPort.close();
        _controller.addError(EJDB2Error(reply, ejdb2ExplainRC(reply)));
        return;
      } else if (reply is ByteBuffer) {
        _controller.add(reply);
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

  void _exec(SendPort sendPort) native 'exec';
}

/// Database wrapper
class EJDB2 extends NativeFieldWrapperClass2 {
  static bool _checkCompleterPortError(Completer<dynamic> completer, dynamic reply) {
    // print('!!!! Reply: $reply');
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
    final replyPort = new RawReceivePort();
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
    int oflags = 0;
    if (readonly) {
      oflags |= 0x02;
    } else if (truncate) {
      oflags |= 0x04;
    }
    final args = List<dynamic>()
      ..add(replyPort.sendPort)
      ..add('open_wrapped')

      // opts.kv.path                         // non null
      // opts.kv.oflags                       // non null
      // opts.kv.wal.enabled                  // non null
      // opts.kv.wal.check_crc_on_checkpoint
      // opts.kv.wal.checkpoint_buffer_sz
      // opts.kv.wal.checkpoint_timeout_sec
      // opts.kv.wal.savepoint_timeout_sec
      // opts.kv.wal.wal_buffer_sz
      // opts.document_buffer_sz
      // opts.sort_buffer_sz
      // opts.http.enabled
      // opts.http.access_token
      // opts.http.bind
      // opts.http.max_body_size
      // opts.http.port
      // opts.http.read_anon
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
    final dbh = _get_handle();
    if (dbh == null) {
      return Future.value();
    }
    final completer = Completer<void>();
    final replyPort = new RawReceivePort();

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
    _port().send(List<dynamic>()..add(replyPort.sendPort)..add('close_wrapped')..add(dbh));
    return completer.future;
  }

  JQL createQuery(String query, [String collection]) native 'create_query';

  SendPort _port() native 'port';

  void _set_handle(int handle) native 'set_handle';

  int _get_handle() native 'get_handle';

  EJDB2._();
}
