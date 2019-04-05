import 'package:ejdb2_dart/ejdb2_dart.dart';

void main() async {
  final db = await EJDB2.open('hello.db');

  final q = db.createQuery('@mycoll/*');
  assert(q != null);
  assert(q.collection == 'mycoll');
  assert(q.db != null);


  await db.close();
}
