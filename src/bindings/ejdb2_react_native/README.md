# EJDB2 React Native binding

Embeddable JSON Database engine http://ejdb.org Node.js binding.

See https://github.com/Softmotions/ejdb/blob/master/README.md

## Status

- Android
- OSX not yet implemented, work in progress.

## Prerequisites

- React native `0.61+`

## Getting started

```
yarn add react-native-ejdb2

react-native link react-native-ejdb2
```

### Usage

```js
import { EJDB2, JBE } from 'ejdb2_react_native';

// Simple query
const db = await EJDB2.open('hello.db');
await db.createQuery('@mycoll/*').useExecute(doc => {
  const doc = doc.json;
  console.log(`Document: `, doc);
});

db.close();
```

[See API docs](./binding/index.d.ts) and [Tests](./tests/App.js);

## How build it manually

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_REACT_NATIVE_BINDING=ON \
         -DANDROID_NDK_HOME=<path to Android NDK>
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
