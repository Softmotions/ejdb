# EJDB2 Java JNI binding

Embeddable JSON Database engine http://ejdb.org Java binding.

See https://github.com/Softmotions/ejdb/blob/master/README.md

For API usage examples take a look into [EJDB2Example.java](https://github.com/Softmotions/ejdb/blob/master/src/bindings/ejdb2_jni/example/src/main/java/EJDB2Example.java) and [TestEJDB2.java](https://github.com/Softmotions/ejdb/blob/master/src/bindings/ejdb2_jni/src/test/java/com/softmotions/ejdb2/TestEJDB2.java) classes.

## Minimal example

```java
public static void main(String[] args) {
  try (EJDB2 db = new EJDB2Builder("example.db").truncate().open()) {
    long id = db.put("parrots", "{\"name\":\"Bianca\", \"age\": 4}");
    System.out.println("Bianca record: " + id);

    id = db.put("parrots", "{\"name\":\"Darko\", \"age\": 8}");
    System.out.println("Darko record: " + id);

    db.createQuery("@parrots/[age > :age]").setLong("age", 3).execute((docId, doc) -> {
      System.out.println(String.format("Found %d %s", docId, doc));
      return 1;
    });
  }
}
```

## Supported platforms

- Linux x64
- MacOS

## Prerequisites

- JDK 11 or newer (`JAVA_HOME`/`JDK_HOME` is auto-detected)
- C compiler (gcc or clang), make, pkgconf
- git (used by Autark to fetch dependencies on the first build)

The project is built by [Autark](https://github.com/Softmotions/autark), which is
vendored as `./build.sh`; no separate build system installation is required.

## How to build it manually

```sh
git clone https://github.com/Softmotions/ejdb.git
cd ./ejdb

# Build the JNI library and the Java jar (Release build by default)
./build.sh -DBUILD_BINDING_JNI=1

# Debug build
CC=clang BUILD_TYPE=Debug ./build.sh -DBUILD_BINDING_JNI=1

# Build and run the test suites (Java test plus the C test suites)
./build.sh -DBUILD_BINDING_JNI=1 -DEJDB_RUN_TESTS=1
```

If the JDK is not detected automatically, point Autark to it explicitly:

```sh
JAVA_HOME=/path/to/jdk ./build.sh -DBUILD_BINDING_JNI=1
```

The build artifacts are placed into `autark-cache/src/bindings/ejdb2_jni/`:
`ejdb2-<version>.jar`, `ejdb2-<version>-sources.jar` and the native
`libejdb2jni.so.<version>` (`.dylib` on macOS).

To run the JNI test manually:

```sh
cd autark-cache/src/bindings/ejdb2_jni
java -ea -Djava.library.path=. \
  -cp ejdb2-<version>.jar:./test-classes com.softmotions.ejdb2.TestEJDB2
```

Use `./build.sh -c` for a clean build and `./build.sh --help` to list all options.

[Sample EJDB2 java project](./example)
