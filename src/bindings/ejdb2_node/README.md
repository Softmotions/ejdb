# EJDB2 Node.js native binding

Embeddable JSON Database engine Node.js binding.

See https://github.com/Softmotions/ejdb/blob/master/README.md

For API usage examples take a look into [/example](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_node/example) and [test.js](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_node/test.js)

## Sample code

```ts
import { EJDB2 } from "ejdb2_node";

async function run() {
  const db = await EJDB2.open("example.db", { truncate: true });

  var id = await db.put("parrots", { name: "Bianca", age: 4 });
  console.log(`Bianca record: ${id}`);

  id = await db.put("parrots", { name: "Darko", age: 8 });
  console.log(`Darko record: ${id}`);

  const q = db.createQuery("/[age > :age]", "parrots");

  for await (const doc of q.setNumber("age", 3).stream()) {
    console.log(`Found ${doc}`);
  }

  await db.close();
}

run();
```

## Supported platforms

- Linux x64
- OSX

## Prerequisites

- Node.js >= v16.0.0 (the package is ESM-only)
- npm
- C compiler (gcc or clang), make, pkgconf
- git (used by Autark to fetch dependencies on the first build)

The project is built by [Autark](https://github.com/Softmotions/autark), which is
vendored as `./build.sh`; no separate build system installation is required.

## Building from source

```sh
git clone https://github.com/Softmotions/ejdb.git
cd ./ejdb

# Build the native addon (Release build by default)
./build.sh -DBUILD_BINDING_NODEJS=1

# Debug build
CC=clang BUILD_TYPE=Debug ./build.sh -DBUILD_BINDING_NODEJS=1

# Build and run the Node.js test suite (also runs the C test suites)
./build.sh -DBUILD_BINDING_NODEJS=1 -DEJDB_RUN_TESTS=1
```

The build artifacts are placed into
`autark-cache/src/bindings/ejdb2_node/ejdb2_node/`. The native addon is located at
`autark-cache/src/bindings/ejdb2_node/ejdb2_node/<platform>-<arch>/ejdb2_node.node`.

To create a redistributable npm package:

```sh
cd autark-cache/src/bindings/ejdb2_node/ejdb2_node
npm pack
```

Use `./build.sh -c` for a clean build and `./build.sh --help` to list all options.