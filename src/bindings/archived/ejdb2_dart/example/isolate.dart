import 'dart:async';
import 'dart:isolate';

import 'package:ejdb2_dart/ejdb2_dart.dart';

/// Database access from multiple isolates.
/// Based on isolate example from https://github.com/adamlofts/leveldb_dart

void main() async {
  final runners = Iterable<int>.generate(5).map((int index) {
    return Runner.spawn(index);
  }).toList();
  await Future.wait(runners.map((Runner r) => r.finish));
}

Future<void> run(int index) async {
  print('Thread ${index} write');
  final db = await EJDB2.open('isolate.db', truncate: true);
  await db.put('c1', {'index': index});
  // Sleep 1 second
  await Future<void>.delayed(const Duration(seconds: 1));
  final nextKey = (index + 1) % 5;
  final doc = await db.createQuery('/[index=:?]', 'c1').setInt(0, nextKey).execute().first;
  print('Thread ${index} read: ${doc}');
}

class Runner {
  final Completer<void> _finish = Completer<void>();
  final RawReceivePort _finishPort = RawReceivePort();
  Runner.spawn(int index) {
    _finishPort.handler = (dynamic _) {
      _finish.complete();
      _finishPort.close();
    };
    Isolate.spawn(run, index, onExit: _finishPort.sendPort);
  }
  Future<void> get finish => _finish.future;
}
