# EJDB2 Android binding

Android (JNI) binding of EJDB2: the [ejdb2 JNI binding](../ejdb2_jni) packaged
as an Android library (`aar`) together with a Gradle test project.

The binding is **cross compiled with the Android NDK** as part of the regular
ejdb2 [Autark](https://github.com/Softmotions/autark) build, there is no
separate CMake/Gradle native build.

* Supported ABIs: `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`
* Minimum Android API level: **24** (Android 7.0)
* Java API: identical to the [ejdb2 JNI binding](../ejdb2_jni)
  (`com.softmotions.ejdb2`), compiled from the same sources

## Requirements

* Android SDK (`ANDROID_HOME`) with
  * the latest platform (`platforms;android-36`)
  * build tools (`build-tools;37.0.0` or compatible)
* Android NDK (`ANDROID_NDK_HOME`, or any NDK under `$ANDROID_HOME/ndk`)
* JDK 17+
* Gradle 9.8 (provided by the wrapper) with Android Gradle Plugin 9.4

The SDK packages are installed with `sdkmanager` (cmdline-tools), or with the
[`android` CLI](https://developer.android.com/tools):

```sh
sdkmanager --install "platforms;android-36" "build-tools;37.0.0" "ndk;30.0.16248370"
#   or
android sdk install platforms/android-36 build-tools/android-37.0.0 ndk/30.0.16248370
```

## Building

The native library is built from the repository root:

```sh
# Cross compile libejdb2jni.so for the default ABIs (arm64-v8a, x86_64)
./build.sh -DBUILD_BINDING_ANDROID=1

# Only for a specific set of ABIs
./build.sh -DBUILD_BINDING_ANDROID=1 -DANDROID_ABIS="arm64-v8a armeabi-v7a"

# Debug symbols instead of the default release build
./build.sh -DBUILD_BINDING_ANDROID=1 -DANDROID_BUILD_TYPE=Debug
```

Autark options used by the Android build:

| Option                | Default               | Description                                        |
|-----------------------|-----------------------|----------------------------------------------------|
| `BUILD_BINDING_ANDROID` | off                 | Build the Android native libraries                |
| `ANDROID_ABIS`        | `arm64-v8a x86_64`    | ABIs to build                                      |
| `ANDROID_API_LEVEL`   | `24`                  | Android API level of the native code               |
| `ANDROID_BUILD_TYPE`  | `Release`             | Build type of the native code                      |
| `ANDROID_RUN_TESTS`   | off                   | Run the instrumentation tests (needs a device)     |

The libraries are staged into the `jniLibs` directory of the test project:

```
test/ejdb2/src/main/jniLibs/<abi>/libejdb2jni.so
```

Each ABI is built in its own cache directory
(`autark-cache/android-<abi>`) because a cache holds the artifacts of exactly
one target. The cache directory must stay inside the repository (the nested
builds of the dependencies query `git` for their version).

## Running the tests

The tests are instrumentation tests, they run on a device or an emulator of a
built ABI:

```sh
# Build the native libraries and run the instrumentation tests
./build.sh -DBUILD_BINDING_ANDROID=1 -DANDROID_RUN_TESTS=1

# ... or drive Gradle directly
cd src/bindings/ejdb2_android/test
./gradlew connectedAndroidTest
```

To run them on an emulator, install one for one of the built ABIs:

```sh
# Android CLI
android sdk install emulator system-images/android-36/default/x86_64
android emulator create medium_phone      # creates an AVD for the x86_64 image
android emulator start --headless medium_phone

# ... or the classic tools
sdkmanager --install "emulator" "system-images;android-36;default;x86_64"
avdmanager create avd -n medium_phone -k "system-images;android-36;default;x86_64"
emulator -avd medium_phone &

./gradlew connectedAndroidTest
```

`test/ejdb2/build/outputs/androidTest-results/` and
`test/ejdb2/build/reports/androidTests/` contain the results.

Assembling the library without a device:

```sh
cd src/bindings/ejdb2_android/test
./gradlew :ejdb2:assembleRelease   # -> ejdb2/build/outputs/aar/ejdb2-release.aar
```

## How the cross compilation works

Autark has no notion of a host/target split, so the Android build uses the
regular ejdb2 build with a separate cache and the NDK toolchain, see
[`scripts/build-abi.sh`](scripts/build-abi.sh):

* `CC` is [`scripts/ndk-cc.sh`](scripts/ndk-cc.sh), a small front-end which
  * forwards compilation to the NDK compiler of the target ABI,
  * answers the `system.sh` sanity check of Autark for the target (that check
    compiles *and executes* a probe program, which is impossible for a target
    binary), and
  * builds host tools (the PEG `leg` generator) with the build host compiler.
* `-DEJDB_ENABLE_HTTP=0` disables the HTTP/WebSocket server (`src/jbr`, backed
  by `iwnet`): it is a server component and is not part of the Android
  binding. `JB_HTTP` is not defined in the generated `ejdb2cfg.h` in this case.
* The dependencies (`binnc`, `iowow`) are built by their own
  Autark rules with the same toolchain, so they use the same flags as on the
  build host.
* `-DPTHREAD_SPIN_LOCK_SHIM` selects bionic's native pthread spinlocks
  (available since API 24) instead of the iowow shim which conflicts with the
  `pthread_spinlock_t` declaration of modern bionic.

## Limitations

* The HTTP/WebSocket server endpoint is not available on Android. The Java
  `EJDB2Builder` HTTP options have no effect.
* API levels below 24 are not supported.

## Sample Android application

* https://github.com/Softmotions/ejdb_android_todo_app