import 'package:ejdb2_dart/ejdb2_dart.dart';

void main() async {
  final db = await EJDB2.open('example.db', truncate: true);

  var id = await db.put('parrots', {'name': 'Bianca', 'age': 4});
  print('Bianca record: ${id}');

  id = await db.put('parrots', {'name': 'Darko', 'age': 8});
  print('Darko record: ${id}');

  final q = db.createQuery('/[age > :age]', 'parrots');
  await for (final doc in q.setInt('age', 3).execute()) {
    print('Found $doc');
  }
  await db.close();
}
