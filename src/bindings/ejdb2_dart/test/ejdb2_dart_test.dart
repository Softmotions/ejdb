import 'package:ejdb2_dart/ejdb2_dart.dart';

void main() async {
  final db = await EJDB2.open('hello.db', truncate: true);

  final q = db.createQuery('@mycoll/*');
  assert(q != null);
  assert(q.collection == 'mycoll');
  assert(q.db != null);

  var id = await db.put('mycoll', '{"foo":"bar"}');
  assert(id == 1);

  dynamic error;
  try {
    await db.put('mycoll','{"');
  } on EJDB2Error catch(e) {
    error = e;
    assert(e.code == 86005);
    assert(e.message == 'Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING)');
  }
  assert(error != null);


  await db.close();
}
