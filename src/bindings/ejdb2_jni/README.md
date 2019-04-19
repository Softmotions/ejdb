# EJDB2 Java JNI binding

Embeddable JSON Database engine http://ejdb.org Java binding.

See https://github.com/Softmotions/ejdb/blob/master/README.md

For API usage examples take a look into [EJDB2Example.java](https://github.com/Softmotions/ejdb/tree/master/src/bindings/ejdb2_jni/src/main/java/com/softmotions/ejdb2/example/EJDB2Example.java) and [TestEJDB2.java](https://github.com/Softmotions/ejdb/blob/master/src/bindings/ejdb2_jni/src/test/java/com/softmotions/ejdb2/TestEJDB2.java) classes.

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

* Linux x64
* OSX

## How to build it manually

``` sh
git clone https://github.com/Softmotions/ejdb.git
cd ./ejdb
mkdir ./build && cd build
cmake .. -DBUILD_JNI_BINDING=ON -DCMAKE_BUILD_TYPE=Release
make
```

## Run example

``` sh
cd build/src/bindings/ejdb2_jni/src

java -Djava.library.path=. \
     -cp ejdb2jni.jar \
     com.softmotions.ejdb2.example.EJDB2Example
```
