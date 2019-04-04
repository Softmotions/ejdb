import 'package:ejdb2_dart/ejdb2_dart.dart';

void main() async {
  final db = await EJDB2.open('hello.db');
  await db.close();
}
