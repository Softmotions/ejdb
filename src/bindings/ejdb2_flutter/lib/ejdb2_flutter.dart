// @dart=2.5

import 'dart:async';
import 'dart:convert' as convert_lib;

import 'package:flutter/services.dart';
import 'package:json_at/json_at.dart';
import 'package:pedantic/pedantic.dart';
import 'package:quiver/core.dart';

const MethodChannel _mc = MethodChannel('ejdb2');

const EventChannel _qc = EventChannel('ejdb2/query');

Map<String, StreamController<JBDOC>> _ecm;

void _execute(String hook, dynamic args, StreamController<JBDOC> sctl) {
  if (_ecm == null) {
    _ecm = {};
    _qc.receiveBroadcastStream().listen(_onQueryData);
  }
  unawaited(_mc.invokeMethod('executeQuery', args).catchError((err, StackTrace s) {
    _ecm.remove(hook);
    dynamic streamError = err;
    if (err is PlatformException && err.code.startsWith('@ejdb')) {
      streamError = EJDB2Error(err.code, err.message);
    }
    sctl.addError(streamError, s);
  }));
  _ecm[hook] = sctl;
}

void _onQueryData(dynamic dataArg) {
  final data = dataArg as List;
  if (data.isEmpty) {
    return;
  }
  final qhook = data[0] as String;
  final sctl = _ecm[qhook];
  if (sctl == null) {
    return;
  }
  final last = data.last == true;
  final l = last ? data.length - 1 : data.length;
  for (int i = 1; i < l; i += 2) {
    sctl.add(JBDOC(data[i] as int, data[i + 1] as String));
  }
  if (last) {
    _ecm.remove(qhook);
    sctl.close();
  }
}

/// EJDB2 Instance builder.
///
class EJDB2Builder {
  EJDB2Builder(this.path);

  final String path;

  bool readonly;

  bool truncate;

  bool walEnabled;

  bool walCheckCRCOnCheckpoint;

  int walCheckpointBufferSize;

  int walCheckpointTimeout;

  int walSavepointTimeout;

  int walBufferSize;

  int documentBufferSize;

  int sortBufferSize;

  /// Open EJDB2 database
  /// See https://github.com/Softmotions/ejdb/blob/master/src/ejdb2.h#L104
  /// for description of options.
  Future<EJDB2> open() => EJDB2._open(this);

  Map<String, dynamic> _asOpts() => <String, dynamic>{
        'readonly': readonly ?? false,
        'truncate': truncate ?? false,
        'wal_enabled': walEnabled ?? true,
        'wal_check_crc_on_checkpoint': walCheckCRCOnCheckpoint ?? false,
        ...walCheckpointBufferSize != null
            ? {'wal_checkpoint_buffer_sz': walCheckpointBufferSize}
            : {},
        ...walCheckpointTimeout != null ? {'wal_checkpoint_timeout_sec': walCheckpointTimeout} : {},
        ...walSavepointTimeout != null ? {'wal_savepoint_timeout_sec': walSavepointTimeout} : {},
        ...walBufferSize != null ? {'wal_wal_buffer_sz': walBufferSize} : {},
        ...documentBufferSize != null ? {'document_buffer_sz': documentBufferSize} : {},
        ...sortBufferSize != null ? {'sort_buffer_sz': sortBufferSize} : {}
      };
}

FutureOr<T> Function(Object, StackTrace) _errorHandler<T>() =>
  (Object err, StackTrace s) {
    if (err is PlatformException && err.code.startsWith('@ejdb')) {
    return Future<T>.error(EJDB2Error(err.code, err.message), s);
    }
    return Future<T>.error(err, s);
  };

class EJDB2Error implements Exception {
  EJDB2Error(this.code, this.message);
  EJDB2Error.notFound() : this('@ejdb IWRC:75001', 'Not found');

  final String code;
  final String message;

  bool isNotFound() => (code ?? '').startsWith('@ejdb IWRC:75001');

  bool isInvalidQuery() => (code ?? '').startsWith('@ejdb IWRC:87001');

  @override
  String toString() => '$runtimeType: $code $message';
}

/// EJDB document item.
///
class JBDOC {
  JBDOC(this.id, this._json);
  JBDOC._fromList(List list) : this(list[0] as int, list[1] as String);

  /// Document identifier
  final int id;

  /// Document body as JSON string
  String get json => _json ?? convert_lib.jsonEncode(_object);

  /// Document body as parsed JSON object.
  dynamic get object {
    if (_json == null) {
      return _object;
    } else {
      _object = convert_lib.jsonDecode(_json);
      _json = null; // Release memory used to store JSON string data
      return _object;
    }
  }

  /// Gets subset of document using RFC 6901 JSON [pointer].
  Optional<dynamic> at(String pointer) => jsonAt(object, pointer);

  /// Gets subset of document using RFC 6901 JSON [pointer].
  Optional<dynamic> operator [](String pointer) => at(pointer);

  String _json;

  dynamic _object;

  @override
  String toString() => '$runtimeType: $id $json';
}

/// EJDB2 query builder/executor.
///
class JQL {
  JQL._(this._jb, this.collection, this.qtext);

  static int _qhandle = 0;

  final EJDB2 _jb;

  final _qspec = <String, dynamic>{};

  final _params = <dynamic>[];

  final String collection;

  final String qtext;

  int get limit => _qspec['l'] as int ?? 0;

  set limit(int limit) => _qspec['l'] = limit;

  int get skip => _qspec['s'] as int ?? 0;

  set skip(int skip) => _qspec['s'] = skip;

  int get _handle => _jb._handle;

  /// Set string [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setString(dynamic placeholder, String val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _params.add([1, placeholder, val]);
    return this;
  }

  /// Set integer [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setInt(dynamic placeholder, int val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _params.add([2, placeholder, val]);
    return this;
  }

  /// Set double [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setDouble(dynamic placeholder, double val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _params.add([3, placeholder, val]);
    return this;
  }

  /// Set bool [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setBoolean(dynamic placeholder, bool val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _params.add([4, placeholder, val]);
    return this;
  }

  /// Set RegExp [val] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setRegexp(dynamic placeholder, RegExp val) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(val);
    _params.add([5, placeholder, val.pattern]);
    return this;
  }

  /// Set [json] at the specified [placeholder].
  /// [placeholder] can be either `string` or `int`
  JQL setJson(dynamic placeholder, Object json) {
    _checkPlaceholder(placeholder);
    ArgumentError.checkNotNull(json);
    if (json is! String) {
      json = convert_lib.jsonEncode(json);
    }
    _params.add([6, placeholder, json]);
    return this;
  }

  /// Execute query and returns a stream of matched documents.
  Stream<JBDOC> execute() {
    final qh = '${++JQL._qhandle}';
    final sctl = StreamController<JBDOC>(onCancel: () {
      _ecm.remove(qh);
    });
    final args = [_handle, qh, collection, qtext, _qspec, _params];
    _execute(qh, args, sctl);
    return sctl.stream;
  }

  /// Executes query then listen event stream for first event/error
  /// to eagerly fetch pending error if available.
  Future<void> executeTouch() {
    StreamSubscription subscription;
    final Completer completer = Completer();
    final stream = execute();
    subscription = stream.listen((val) {
      if (!completer.isCompleted) {
        completer.complete();
      }
      final sf = subscription.cancel();
      if (sf != null) {
        unawaited(sf.catchError(() {}));
      }
    }, onError: (e, StackTrace s) {
      if (!completer.isCompleted) {
        completer.completeError(e, s);
      }
    }, onDone: () {
      if (!completer.isCompleted) {
        completer.complete();
      }
    }, cancelOnError: true);
    return completer.future;
  }

  /// Return scalar integer value as result of query execution.
  /// For example execution of count query: `/... | count`
  Future<int> executeScalarInt() => _mc.invokeMethod('executeScalarInt',
      [_handle, null, collection, qtext, _qspec, _params]).then((v) => v as int);

  /// Returns list of matched documents.
  /// Use it with care to avoid wasting of memory.
  Future<List<JBDOC>> list([int limitArg]) async {
    var qspec = _qspec;
    if (limitArg != null) {
      // Copy on write to be safe
      qspec = {
        ..._qspec,
        ...{'l': limitArg}
      };
    }
    final list = await _mc
        .invokeListMethod('executeList', [_handle, null, collection, qtext, qspec, _params]);
    final res = <JBDOC>[];
    for (int i = 0; i < list.length; i += 2) {
      res.add(JBDOC(list[i] as int, list[i + 1] as String));
    }
    return res;
  }

  /// Returns optional element for first record in result set.
  Future<Optional<JBDOC>> first() async {
    final l = await list(limit = 1);
    return l.isNotEmpty ? Optional.of(l.first) : const Optional.absent();
  }

  /// Return first record in result set or resolve to notfound [EJDB2Error] error.
  Future<JBDOC> firstRequired() async {
    final f = await first();
    if (f.isPresent) {
      return f.value;
    }
    return Future.error(EJDB2Error.notFound());
  }

  /// Returns list of first [n] matched documents.
  /// Use it with care to avoid wasting of memory.
  Future<List<JBDOC>> firstN(int n) => list(n);

  void _checkPlaceholder(dynamic placeholder) {
    if (!(placeholder is String) && !(placeholder is int)) {
      ArgumentError.value(placeholder, 'placeholder');
    }
  }
}

class EJDB2 {
  EJDB2._(this._handle);

  final int _handle;

  static Future<EJDB2> _open(EJDB2Builder b) async {
    final hdl =
        await _mc.invokeMethod<int>('open', [null, b.path, b._asOpts()]).catchError(_errorHandler<int>());
    return EJDB2._(hdl);
  }

  /// Closes database instance.
  Future<void> close() {
    if (_handle == null) {
      throw StateError('Closed');
    }
    return _mc.invokeMethod('close', [_handle]).catchError(_errorHandler());
  }

  /// Create instance of [query] specified for [collection].
  /// If [collection] is not specified a [query] spec must contain collection name,
  /// eg: `@mycollection/[foo=bar]`
  JQL createQuery(String query, [String collection]) => JQL._(this, collection, query);

  /// Create instance of [query].
  JQL operator [](String query) => createQuery(query);

  /// Save [json] document under specified [id] or create a new document
  /// with new generated `id`. Returns future holding actual document `id`.
  Future<int> put(String collection, dynamic json, [int id]) => Future.sync(() => _mc
      .invokeMethod('put', [
        _handle,
        collection,
        (json is! String) ? convert_lib.jsonEncode(json) : json as String,
        id
      ])
      .catchError(_errorHandler<int>())
      .then((v) => v as int));

  /// Apply rfc6902/rfc6901 JSON [patch] to the document identified by [id].
  Future<void> patch(String collection, dynamic json, int id, [bool upsert = false]) =>
      Future.sync(() => _mc.invokeMethod('patch', [
            _handle,
            collection,
            (json is! String) ? convert_lib.jsonEncode(json) : json as String,
            id,
            upsert
          ]).catchError(_errorHandler()));

  /// Apply JSON merge patch (rfc7396) to the document identified by `id` or
  /// insert new document under specified `id`.
  Future<void> patchOrPut(String collection, dynamic json, int id) =>
      patch(collection, json, id, true);

  /// Get json body of document identified by [id] and stored in [collection].
  /// Throws [EJDB2Error] not found exception if document is not found.
  Future<JBDOC> get(String collection, int id) => _mc
      .invokeMethod('get', [_handle, collection, id])
      .catchError(_errorHandler<JBDOC>())
      .then((v) => JBDOC(id, v as String));

  /// Get json body of document identified by [id] and stored in [collection].
  Future<Optional<JBDOC>> getOptional(String collection, int id) {
    return get(collection, id).then((doc) => Optional.of(doc)).catchError((err) {
      if (err is EJDB2Error && err.isNotFound()) {
        return Future.value(const Optional<JBDOC>.absent());
      }
      return Future.error(err);
    });
  }

  /// Remove document identified by [id] from specified [collection].
  Future<void> del(String collection, int id) =>
      _mc.invokeMethod('del', [_handle, collection, id]).catchError(_errorHandler());

  /// Remove document identified by [id] from specified [collection].
  /// Doesn't raise error if document is not found.
  Future<void> delIgnoreNotFound(String collection, int id) =>
      del(collection, id).catchError((err) {
        if (err is EJDB2Error && err.isNotFound()) {
          return Future.value();
        } else {
          return Future.error(err);
        }
      });

  /// Get json body of database metadata.
  Future<dynamic> info() => _mc
      .invokeMethod('info', [_handle])
      .catchError(_errorHandler())
      .then((v) => convert_lib.jsonDecode(v as String));

  /// Removes database [collection].
  Future<void> removeCollection(String collection) =>
      _mc.invokeMethod('removeCollection', [_handle, collection]).catchError(_errorHandler());

  /// Renames database collection.
  Future<void> renameCollection(String oldName, String newName) {
    return _mc
        .invokeMethod('renameCollection', [_handle, oldName, newName]).catchError(_errorHandler());
  }

  Future<void> ensureStringIndex(String coll, String path, [bool unique]) =>
      _mc.invokeMethod('ensureStringIndex', [_handle, coll, path, unique ?? false]);

  Future<void> removeStringIndex(String coll, String path, [bool unique]) =>
      _mc.invokeMethod('removeStringIndex', [_handle, coll, path, unique ?? false]);

  Future<void> ensureIntIndex(String coll, String path, [bool unique]) =>
      _mc.invokeMethod('ensureIntIndex', [_handle, coll, path, unique ?? false]);

  Future<void> removeIntIndex(String coll, String path, [bool unique]) =>
      _mc.invokeMethod('removeIntIndex', [_handle, coll, path, unique ?? false]);

  Future<void> ensureFloatIndex(String coll, String path, [bool unique]) =>
      _mc.invokeMethod('ensureFloatIndex', [_handle, coll, path, unique ?? false]);

  Future<void> removeFloatIndex(String coll, String path, [bool unique]) =>
      _mc.invokeMethod('removeFloatIndex', [_handle, coll, path, unique ?? false]);

  /// Creates an online database backup image and copies it into the specified [fileName].
  /// During online backup phase read/write database operations are allowed and not
  /// blocked for significant amount of time. Returns future with backup
  /// finish time as number of milliseconds since epoch.
  Future<int> onlineBackup(String fileName) =>
      _mc.invokeMethod('onlineBackup', [_handle, fileName]).then((v) => v as int);
}
