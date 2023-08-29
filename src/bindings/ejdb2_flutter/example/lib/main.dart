import 'dart:async';

import 'package:json_at/json_at.dart';
import 'package:ejdb2_example/utils/assertions.dart';
import 'package:ejdb2_flutter/ejdb2_flutter.dart';
import 'package:flutter/material.dart';

void main() => runApp(MyApp());

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String _result = '';

  @override
  void initState() {
    super.initState();
    _initTesting();
  }

  Future<void> _doTest() async {
    String dbpath;

    var db = await (EJDB2Builder('test.db')
          ..truncate = true
          ..walEnabled = true)
        .open();
    try {
      var id = await db.put('mycoll', {'foo': 'bar'});
      assertEqual(id, 1);

      JBDOC doc = await db.get('mycoll', id);
      assertDeepEqual(doc.object, {'foo': 'bar'});

      id = await db.put('mycoll', {'foo': 'baz'}, id);
      assertEqual(id, 1);

      doc = await db.get('mycoll', id);
      assertDeepEqual(doc.object, {'foo': 'baz'});

      dynamic error;
      try {
        await db.put('mycoll', '{"');
      } catch (e) {
        error = e;
      }
      assertNotNull(error);
      assertTrue(error is EJDB2Error);
      assertEqual(error.code, '@ejdb IWRC:76005');

      assertTrue('${error.message}'.contains('Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING)'));

      id = await db.put('mycoll', {'foo': 'bar'});
      assertEqual(id, 2);

      var list = await db.createQuery('@mycoll/*').list();
      assertEqual(list.toString(), '[JBDOC: 2 {"foo":"bar"}, JBDOC: 1 {"foo":"baz"}]');

      JBDOC first = await db.createQuery('@mycoll/*').execute().first;
      assertEqual('$first', 'JBDOC: 2 {"foo":"bar"}');

      first = (await db.createQuery('@mycoll/*').first()).orNull;
      assertEqual('$first', 'JBDOC: 2 {"foo":"bar"}');

      first = (await db.createQuery('@mycoll/[zzz=bbb]').first()).orNull;
      assertNull(first);

      list = await db.createQuery('@mycoll/*').firstN(5);
      assertEqual('$list', '[JBDOC: 2 {"foo":"bar"}, JBDOC: 1 {"foo":"baz"}]');

      doc = await db
          .createQuery('@mycoll/[foo=:? and foo=:bar]')
          .setString(0, 'baz')
          .setString('bar', 'baz')
          .execute()
          .first;
      assertEqual('$doc', 'JBDOC: 1 {"foo":"baz"}');

      list = await (db.createQuery('@mycoll/[foo != :?]').setString(0, 'zaz')..skip = 1)
          .execute()
          .toList();
      assertEqual('$list', '[JBDOC: 1 {"foo":"baz"}]');

      error = null;
      try {
        await db['@mycoll/['].execute();
      } catch (e) {
        error = e;
      }
      assertNull(error); // Note: null error

      error = null;
      try {
        await db.createQuery('@mycoll/[').executeTouch();
      } catch (e) {
        error = e;
      }
      assertNotNull(error); // Note: non null error
      assertTrue(error is EJDB2Error);
      assertTrue((error as EJDB2Error).isInvalidQuery());

      final count = await db['@mycoll/* | count'].executeScalarInt();
      assertEqual(count, 2);

      await db.patch('mycoll', '[{"op":"add", "path":"/baz", "value":"qux"}]', 1);
      doc = await db.get('mycoll', 1);
      assertDeepEqual(doc.object, {'foo': 'baz', 'baz': 'qux'});

      var info = await db.info();
      assertEqual(jsonAt(info, '/size').orNull, 8192);
      assertEqual(jsonAt(info, '/collections/0/rnum').orNull, 2);
      assertNull(jsonAt(info, '/collections/0/indexes/0').orNull);

      await db.ensureStringIndex('mycoll', '/foo', true);
      info = await db.info();
      assertEqual(jsonAt(info, '/collections/0/indexes/0/ptr').orNull, '/foo');
      assertEqual(jsonAt(info, '/collections/0/indexes/0/mode').orNull, 5);
      assertEqual(jsonAt(info, '/collections/0/indexes/0/rnum').orNull, 2);

      await db.removeStringIndex('mycoll', '/foo', true);
      info = await db.info();
      assertNull(jsonAt(info, '/collections/0/indexes/0').orNull);

      doc = await db['@mycoll/[foo re :?]'].setRegexp(0, RegExp('.*')).firstRequired();
      assertEqual('$doc', 'JBDOC: 2 {"foo":"bar"}');

      await db.removeCollection('mycoll');
      info = await db.info();
      assertNull(jsonAt(info, '/collections/0').orNull);

      error = null;
      try {
        await db['@mycoll/*'].firstRequired();
      } catch (e) {
        error = e;
      }
      assertNotNull(error); // Note: non null error
      assertTrue(error is EJDB2Error);
      assertTrue((error as EJDB2Error).isNotFound());

      id = await db.put('cc1', {'foo': 1});
      doc = await db.get('cc1', id);
      assertEqual('$doc', 'JBDOC: 1 {"foo":1}');

      await db.renameCollection('cc1', 'cc2');
      doc = await db.get('cc2', id);
      assertEqual('$doc', 'JBDOC: 1 {"foo":1}');

      var opt = await db.getOptional('cc2', id);
      assertTrue(opt.isPresent);

      opt = await db.getOptional('cc2', 122);
      assertTrue(opt.isNotPresent);


      var i = 0;
      for (i = 0; i < 1023; ++i) {
        await db.put('load', {'name': 'v${i}'});
      }

      i = 0;
      await for (final doc in db.createQuery('@load/* | inverse').execute()) {
        final v = doc.object;
        assertEqual(v['name'], 'v${i}');
        ++i;
      }

      dbpath = jsonAt(info, '/file').orNull as String;
      assertNotNull(dbpath);

      final ts0 = DateTime.now().millisecondsSinceEpoch;
      final ts = await db.onlineBackup('${dbpath}.bkp');
      assertTrue(ts0 < ts);

    } catch(e, st) {
      print('$e\n$st');
      rethrow;
    } finally {
      await db.close();
    }

    db = await EJDB2Builder('${dbpath}.bkp').open();
    final doc = await db.get('cc2', 1);
    assertEqual('${doc}', 'JBDOC: 1 {"foo":1}');
    await db.close();
  }

  Future<void> _initTesting() async {
    if (!mounted) return;
    try {
      await _doTest();
      setState(() {
        _result = 'Success';
      });
    } catch (e, s) {
      print('Error $e, $s');
      setState(() {
        _result = 'Fail: ${e}';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: _result == 'Success'
              ? const Text(
                  'Success',
                  key: Key('success'),
                )
              : Text(
                  _result == '' ? '' : 'Fail: ${_result}',
                  key: const Key('fail'),
                ),
        ),
      ),
    );
  }
}
