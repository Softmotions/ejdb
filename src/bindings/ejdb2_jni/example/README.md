## EJDB2 Java sample app

First of all you need to get `libejdb2jni.so` native binding library for JVM.

You can build it yourself

```sh
git clone https://github.com/Softmotions/ejdb.git
cd ./ejdb
mkdir ./build && cd build
cmake .. -DBUILD_JNI_BINDING=ON -DCMAKE_BUILD_TYPE=Release
make
```

or install binary package from [our PPA repository](https://launchpad.net/~adamansky/+archive/ubuntu/ejdb2)

```sh
sudo add-apt-repository ppa:adamansky/ejdb2
sudo apt-get update
sudo apt-get install ejdb2-java
```

Then compile and run app (Linux x86-64)

```sh
cd ejdb2_jni/example

mvn clean compile
mvn exec:java -Dexec.cleanupDaemonThreads=false -Dexec.mainClass="EJDB2Example"
```

Note: Yoy may need to specify `LD_LIBRARY_PATH` env for `java` in order to help JVM find where
the `libejdb2jni.so` library is located. For Linux systems `ejdb2-java` PPA debian package installs
shared library symlink to `/usr/java/packages/lib` folder listed as default library search
path for JVM so you can skip specifying `LD_LIBRARY_PATH` in that case.

### Maven project configuration

```xml
<repositories>
  <repository>
    <id>repsy</id>
    <name>EJDB2 Maven Repositoty on Repsy</name>
    <url>https://repo.repsy.io/mvn/adamansky/softmotions</url>
  </repository>
</repositories>

<dependencies>
  <dependency>
    <groupId>softmotions</groupId>
    <artifactId>ejdb2</artifactId>
    <version>2.0.57.11</version>
  </dependency>
</dependencies>
```