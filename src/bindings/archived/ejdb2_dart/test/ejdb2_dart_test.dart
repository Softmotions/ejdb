import 'package:ejdb2_dart/ejdb2_dart.dart';

void main() async {
  final db = await EJDB2.open('hello.db', truncate: true);

  var q = db.createQuery('@mycoll/*');
  assert(q.collection == 'mycoll');

  var id = await db.put('mycoll', {'foo': 'bar'});
  assert(id == 1);

  dynamic error;
  try {
    await db.put('mycoll', '{"');
  } on EJDB2Error catch (e) {
    error = e;
    assert(e.code == 76005);
    assert(e.message == 'Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING)');
  }
  assert(error != null);

  var json = await db.get('mycoll', id);
  assert(json == '{"foo":"bar"}');

  await db.put('mycoll', {'foo': 'baz'});

  final list = await db.createQuery('@mycoll/*').execute(limit: 1).toList();
  assert(list.length == 1);

  var first = await db.createQuery('@mycoll/*').first();
  assert(first.isPresent);
  assert(first.value.json == '{"foo":"baz"}');

  first = await db.createQuery('@mycoll/[zzz=bbb]').first();
  assert(first.isEmpty);

  var firstN = await db.createQuery('@mycoll/*').firstN(5);
  assert(firstN.length == 2);
  assert(firstN[0].json == '{"foo":"baz"}');
  assert(firstN[1].json == '{"foo":"bar"}');

  firstN = await db.createQuery('@mycoll/*').firstN(1);
  assert(firstN.length == 1);

  // Query 1
  final rbuf = <String>[];
  await for (final doc in q.execute()) {
    rbuf..add(doc.id.toString())..add(doc.json);
  }
  assert(rbuf.toString() == '[2, {"foo":"baz"}, 1, {"foo":"bar"}]');
  rbuf.clear();

  // Query 2
  await for (final doc in db.createQuery('@mycoll/[foo=zaz]').execute()) {
    rbuf..add(doc.id.toString())..add(doc.json);
  }
  assert(rbuf.isEmpty);

  // Query 3
  await for (final doc in db.createQuery('/[foo=bar]', 'mycoll').execute()) {
    rbuf..add(doc.id.toString())..add(doc.json);
  }
  assert(rbuf.toString() == '[1, {"foo":"bar"}]');

  error = null;
  try {
    await db.createQuery('@mycoll/[').execute();
  } on EJDB2Error catch (e) {
    error = e;
    assert(e.invalidQuery);
    assert(e.message.contains('@mycoll/[ <---'));
  }
  assert(error != null);

  var count = await db.createQuery('@mycoll/* | count').scalarInt();
  assert(count == 2);

  count = await db.createQuery('@mycoll/* | count').scalarInt(explainCallback: (log) {
    log.contains('[INDEX] NO [COLLECTOR] PLAIN');
  });
  assert(count == 2);

  // Del
  await db.del('mycoll', 1);
  error = null;
  try {
    await db.get('mycoll', 1);
  } on EJDB2Error catch (e) {
    error = e;
    assert(e.notFound);
    assert(e.message.contains('IWKV_ERROR_NOTFOUND'));
  }
  assert(error != null);

  count = await db.createQuery('@mycoll/* | count').scalarInt();
  assert(count == 1);

  // Patch
  await db.patch('mycoll', '[{"op":"add", "path":"/baz", "value":"qux"}]', 2);
  json = await db.get('mycoll', 2);
  assert(json == '{"foo":"baz","baz":"qux"}');

  // DB Info
  json = await db.info();
  assert(json.contains('"collections":[{"name":"mycoll","dbid":3,"rnum":1,"indexes":[]}]'));

  // Indexes
  await db.ensureStringIndex('mycoll', '/foo', unique: true);
  json = await db.info();
  assert(json.contains('"indexes":[{"ptr":"/foo","mode":5,"idbf":0,"dbid":4,"rnum":1}]'));

  // Test JQL set
  JBDOC? doc = await db.createQuery('@mycoll/[foo=:?]').setString(0, 'baz').execute().first;
  assert(doc.json == '{"foo":"baz","baz":"qux"}');

  // Test explain log
  await db.createQuery('@mycoll/[foo=:?]').setString(0, 'baz').execute(explainCallback: (log) {
    assert(
        log.contains("[INDEX] SELECTED UNIQUE|STR|1 /foo EXPR1: 'foo = :?' INIT: IWKV_CURSOR_EQ"));
  });

  doc = await db
      .createQuery('@mycoll/[foo=:?] and /[baz=:?]')
      .setString(0, 'baz')
      .setString(1, 'qux')
      .execute()
      .first;
  assert(doc.json == '{"foo":"baz","baz":"qux"}');

  doc = await db
      .createQuery('@mycoll/[foo=:foo] and /[baz=:baz]')
      .setString('foo', 'baz')
      .setString('baz', 'qux')
      .execute()
      .first;

  doc = await db.createQuery('@mycoll/[foo re :?]').setRegExp(0, RegExp('.*')).execute().first;
  assert(doc.json == '{"foo":"baz","baz":"qux"}');

  doc = await db.createQuery('@mycoll/* | /foo').execute().first;
  assert(doc.json == '{"foo":"baz"}');

  await db.removeStringIndex('mycoll', '/foo', unique: true);
  json = await db.info();
  assert(json.contains('"collections":[{"name":"mycoll","dbid":3,"rnum":1,"indexes":[]}]'));

  // Remove collection
  await db.removeCollection('mycoll');
  json = await db.info();
  assert(json.contains('"collections":[]'));

  // Check apply
  await db.put(
      'apply1', {'tx_hash': 'ed36cfd14a4fe29b16c511d68a8be89e42dcc6e4ced69d04f318448a2b8fafa0'});
  doc = await db
      .createQuery('/[tx_hash = :?] | apply :?', 'apply1')
      .setString(0, 'ed36cfd14a4fe29b16c511d68a8be89e42dcc6e4ced69d04f318448a2b8fafa0')
      .setJson(1, {'status': 'completed'}).firstRequired();
  assert(doc.object['status'] == 'completed');

  /// Test get limit
  q = db.createQuery('@c1/* | limit 1');
  assert(q.limit == 1);

  q = db.createQuery('@c1/*');
  assert(q.limit == 0);

  id = await db.put('cc1', {'foo': 1});
  await db.renameCollection('cc1', 'cc2');
  json = await db.get('cc2', id);
  assert(json == '{"foo":1}');

  for (var i = 0; i < 10000; ++i) {
    await db.put('cc1', {'name': 'v${i}'});
  }
  var cnt = 0;
  await for (final _ in db.createQuery('@cc1/*').execute()) {
    cnt++;
  }
  assert(cnt == 10000);

  final ts0 = DateTime.now().millisecondsSinceEpoch;
  final ts = await db.onlineBackup('hello-bkp.db');
  assert(ts > ts0);
  await db.close();

  // Reopen backup image
  final db2 = await EJDB2.open('hello-bkp.db', truncate: false);
  doc = (await db2.createQuery('@cc1/*').first()).orNull;
  assert(doc != null);
  await db2.close();
}
