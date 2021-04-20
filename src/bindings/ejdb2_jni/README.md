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

## Install from Ubuntu PPA

```sh
sudo add-apt-repository ppa:adamansky/ejdb2
sudo apt-get update
sudo apt-get install ejdb2-java
```

Note: Yoy may need to specify `LD_LIBRARY_PATH` env for `java` in order to help JVM find where
the `libejdb2jni.so` library is located. For Linux systems `ejdb2-java` PPA debian package installs
shared library symlink to `/usr/java/packages/lib` folder listed as default library search
path for JVM so you can skip specifying `LD_LIBRARY_PATH` in that case.

## How to build it manually

```sh
git clone https://github.com/Softmotions/ejdb.git
cd ./ejdb
mkdir ./build && cd build
cmake .. -DBUILD_JNI_BINDING=ON -DCMAKE_BUILD_TYPE=Release
make
```

[Sample EJDB2 java project](./example)
