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
(`autark-cache/android-<abi>`) because an Autark cache holds the artifacts of
exactly one target: a shared cache is not supported between targets. The cache
directory must stay inside the repository (the nested builds of the
dependencies query `git` for their version).

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

Autark supports cross compilation natively (see `--toolchain` in the
[Autark documentation](https://github.com/Softmotions/autark#cross-compilation)),
so the Android libraries are produced by the regular ejdb2 build: one build per
ABI, each in its own cache directory, with the NDK toolchain descriptor
[`toolchains/android-<abi>.autark`](toolchains):

```sh
NDK_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/<host tag>/bin"

ANDROID_NDK_BIN="$NDK_BIN" \
ANDROID_NDK_SYSROOT="$NDK_BIN/../sysroot" \
ANDROID_NDK_API_LEVEL=24 \
  autark --toolchain toolchains/android-arm64-v8a.autark \
         -H autark-cache/android-arm64-v8a \
         -DBUILD_BINDING_JNI=1 -DEJDB_ENABLE_HTTP=0 <ejdb root>
```

[`scripts/build-abi.sh`](scripts/build-abi.sh) prepares that invocation:

* The toolchain descriptor declares the target facts (`SYSTEM_*`, `CC`, `AR`,
  `RANLIB`, sysroot) instead of having Autark probe the build host, so
  `system.sh` skips the runnable probe while all other checks
  (`test_pthread.sh`, `test_qsort_r.sh`, ...) are compiled for the target and
  report the target capabilities.  The NDK layout depends on the build host, so
  the script passes it to the descriptor in the environment:
  `ANDROID_NDK_BIN`, `ANDROID_NDK_SYSROOT` and `ANDROID_NDK_API_LEVEL`.
* Build tools - the PEG `leg` generator - are built by the build host toolchain
  (`run { host ... }` with `HOST_CC` from the descriptor) and can not be mixed
  with the target objects.
* `-DEJDB_ENABLE_HTTP=0` disables the HTTP/WebSocket server (`src/jbr`, backed by
  `iwnet`): it is a server component and is not part of the Android binding.
  `JB_HTTP` is not defined in the generated `ejdb2cfg.h` in this case.
* The dependencies (`binnc`, `iowow`) are cross built by their own Autark rules:
  the target context (`AUTARK_TOOLCHAIN`) is propagated to the nested builds, so
  they are built with the same toolchain as the rest of the build.  Their
  `system.sh` checks are cross compilation aware, so the target facts come from
  the descriptor and no runnable probe is required.
* `iowow` detects the bionic pthread spinlocks with its own `test_pthread.sh`
  check, so neither a shim nor an extra define is required as long as the target
  API level is at least 24.

### Expected diagnostics in the build log

The checks are feature probes compiled for the target, so a probe that reports a
failure only means that the feature is not available for this target.  The
following messages are normal and do not affect the produced library:

* `ld.lld: error: unable to find library -lBlocksRuntime` - the NDK ships no
  Clang Blocks runtime, so `IW_BLOCKS` is not set.  The same probe also fails on
  build hosts without `libBlocksRuntime`.
* `test_qsort_r.c: ... error: call to undeclared function 'qsort_r'` - bionic
  declares `qsort_r` only from API 36, so the code uses its own fallback.
* `+ ...` lines - the `set -x` trace of the `test_pthread.sh` check.

## Limitations

* The HTTP/WebSocket server endpoint is not available on Android. The Java
  `EJDB2Builder` HTTP options have no effect.
* API levels below 24 are not supported.

## Sample Android application

* https://github.com/Softmotions/ejdb_android_todo_app