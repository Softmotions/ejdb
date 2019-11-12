import 'package:flutter_driver/flutter_driver.dart';
import 'package:test/test.dart';

void main() {
  group('EJDB2 tests', () {
    FlutterDriver driver;

    setUpAll(() async {
      driver = await FlutterDriver.connect();
    });

    tearDownAll(() async {
      if (driver != null) {
        await driver.close();
      }
    });

    test('main test', () async {
      await driver.waitFor(find.byValueKey('success'), timeout: const Duration(seconds: 10));
    });
  });
}
