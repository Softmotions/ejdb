# Android

## Sample Android application

https://github.com/Softmotions/ejdb_android_todo_app

## Android binding showcase and unit tests

```bash
cd ./src/bindings/ejdb2_android/test
```

Set local android SDK/NDK path and target `arch` in `local.properties`

```properties
# Path to Android SDK dir
sdk.dir=/Android-sdk

# Path to Android NDK dir
ndk.dir=/Android-sdk/ndk-bundle

# Target abi name: armeabi-v7a, arm64-v8a, x86, x86_64
abi.name=arm64-v8a
```

Run Android emulator for the same abi version then:

```bash
./gradlew connectedAndroidTest
```