import 'package:ejdb2_dart/ejdb2_dart.dart';

void main() async {
  final db = await EJDB2.open('hello.db', truncate: true);

  var q = db.createQuery('@mycoll/*');
  assert(q != null);
  assert(q.collection == 'mycoll');
  assert(q.db != null);

  final id = await db.put('mycoll', {'foo':'bar'});
  assert(id == 1);

  dynamic error;
  try {
    await db.put('mycoll', '{"');
  } on EJDB2Error catch (e) {
    error = e;
    assert(e.code == 86005);
    assert(e.message == 'Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING)');
  }
  assert(error != null);

  var json = await db.get('mycoll', id);
  assert(json == '{"foo":"bar"}');

  await db.put('mycoll', {'foo':'baz'});

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
    assert(e.code == 87001);
    assert(e.message.contains('@mycoll/[ <---'));
  }
  assert(error != null);

  var count = await db.createQuery('@mycoll/* | count').scalarInt();
  assert(count == 2);

  // Del
  await db.del('mycoll', 1);
  error = null;
  try {
    await db.get('mycoll', 1);
  } on EJDB2Error catch (e) {
    error = e;
    assert(e.code == 75001);
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
  var doc = await db.createQuery('@mycoll/[foo=:?]').setString(0, 'baz').execute().first;
  assert(doc.json == '{"foo":"baz","baz":"qux"}');

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

  await db.close();
}
