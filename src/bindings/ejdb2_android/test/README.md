# Android binding test project

Gradle project of the ejdb2 Android binding. The `ejdb2` module packages the
JNI binding:

* the Java API is compiled from
  [`../../ejdb2_jni/src/main/java`](../../ejdb2_jni/src/main/java)
* `libejdb2jni.so` is cross compiled by the Autark build from the repository
  root and staged into `ejdb2/src/main/jniLibs/<abi>/`
* the instrumentation tests live in `ejdb2/src/androidTest/`

See [../README.md](../README.md) for the binding documentation.

## Build and test

Build the native libraries first (from the repository root):

```sh
./build.sh -DBUILD_BINDING_ANDROID=1
```

Then assemble the library or run the instrumentation tests against a connected
device/emulator:

```sh
# ... or let the ejdb2 build drive the tests:
./build.sh -DBUILD_BINDING_ANDROID=1 -DANDROID_RUN_TESTS=1

cd src/bindings/ejdb2_android/test

./gradlew :ejdb2:assembleRelease      # AAR: ejdb2/build/outputs/aar/
./gradlew connectedAndroidTest        # runs Ejdb2AndroidTest
```

The project uses the Gradle wrapper (`gradle/wrapper/gradle-wrapper.properties`,
Gradle 9.8.2) and the Android Gradle Plugin declared in `build.gradle`
(9.4.1). Both need JDK 17+.

The Android SDK location is taken from `ANDROID_HOME` (or `ANDROID_SDK_ROOT`).
A machine specific `local.properties` is not required and not tracked; create
one only if the SDK is not exported in the environment:

```properties
sdk.dir=/path/to/Android/Sdk
```

### Emulator

The tests run on the ABI of the device/emulator, so at least one of the built
ABIs must match. For an `x86_64` emulator:

```sh
# Android CLI
android sdk install emulator system-images/android-36/default/x86_64
android emulator create medium_phone
android emulator start --headless medium_phone

# ... or the classic tools
sdkmanager --install "emulator" "system-images;android-36;default;x86_64"
avdmanager create avd -n medium_phone -k "system-images;android-36;default;x86_64"
emulator -avd medium_phone &

./gradlew connectedAndroidTest
```

Test reports:

* `ejdb2/build/reports/androidTests/connected/`
* `ejdb2/build/outputs/androidTest-results/connected/`
