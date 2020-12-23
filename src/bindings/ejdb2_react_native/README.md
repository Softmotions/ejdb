# EJDB2 React Native binding

Embeddable JSON Database engine http://ejdb.org Node.js binding.

See https://github.com/Softmotions/ejdb/blob/master/README.md


## Note on versioning

Since `2020-12-22` a new version scheme is used and the major version
of package has been incremented. Now package version is the concatenation
of ejdb2 core engine version and version serial number of this binding.
For example: given ejdb2 version `2.0.52` and binding serial `2`
the actual package version will be `2.0.522`.

## Prerequisites

- React native `0.61+`
- [Yarn](https://yarnpkg.com) package manager

## Status

- Android
- OSX not yet implemented, work in progress.

## Getting started

```
yarn add ejdb2_react_native

react-native link ejdb2_react_native
```

### Usage

```js
import { EJDB2, JBE } from 'ejdb2_react_native';

// Simple query
const db = await EJDB2.open('hello.db');
await db.createQuery('@mycoll/[foo = :?]')
        .setString(0, 'bar')
        .useExecute(doc => {
  const doc = doc.json;
  console.log(`Document: `, doc);
});

db.close();
```

[See API docs](https://github.com/Softmotions/ejdb/blob/master/src/bindings/ejdb2_react_native/binding/index.d.ts) and [Tests](https://github.com/Softmotions/ejdb/blob/master/src/bindings/ejdb2_react_native/tests/App.js);

## How build it manually

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_REACT_NATIVE_BINDING=ON \
         -DANDROID_NDK_HOME=<path to Android NDK> \
         -DANDROID_ABIS="x86;x86_64;arm64-v8a;armeabi-v7a"
```

## Testing

```sh
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_REACT_NATIVE_BINDING=ON \
          -DANDROID_NDK_HOME=<path to Android NDK> \
          -DANDROID_ABIS="x86" \
          -DANDROID_AVD=TestingAVD \
          -DBUILD_TESTS=ON

ctest
```
