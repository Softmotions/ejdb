# EJDB2 Dart VM native binding

Embeddable JSON Database engine http://ejdb.org Dart binding.

See https://github.com/Softmotions/ejdb/blob/master/README.md

For API usage examples look into `/example` and `/test` folders.

## Example

```dart
import 'package:ejdb2_dart/ejdb2_dart.dart';

void main() async {
  final db = await EJDB2.open('example.db', truncate: true);

  var id = await db.put('parrots', {'name': 'Bianca', 'age': 4});
  print('Biana record: ${id}');

  id = await db.put('parrots', {'name': 'Darko', 'age': 8});
  print('Darko record: ${id}');

  final q = db.createQuery('/[age > :age]', 'parrots');
  await for (final doc in q.setInt('age', 3).execute()) {
    print('Found ${doc}');
  }
  await db.close();
}
```

# Supported platforms

* Linux x64

Contributors needed for OSX and Flutter.

