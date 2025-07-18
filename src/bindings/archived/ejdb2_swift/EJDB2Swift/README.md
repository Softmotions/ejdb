# EJDB2Swift

EJDB2 Swift language binding

https://github.com/Softmotions/EJDB2Swift

* OSX
* iOS
* Linux

```swift
import EJDB2

let db = try EJDB2Builder("example.db").withTruncate().open()

var id = try db.put("parrots", ["name": "Bianca", "age": 4])
print("Bianca record: \(id)")

id = try db.put("parrots", ["name": "Darko", "age": 8])
print("Bianca record: \(id)")

try db.createQuery("@parrots/[age > :?]").setInt64(0, 3).list().forEach({
  print("Found \($0)")
})

try? db.close()
```

Code examples:
* [iOS Example app](https://github.com/Softmotions/EJDB2IOSExample)
* [Swift test cases](https://github.com/Softmotions/EJDB2Swift/blob/master/Tests/EJDB2Tests/EJDB2Tests.swift)

## OSX / Linux

### Prerequisites

* [Swift SDK](https://swift.org/download/)

### Setup

On OSX/Linux EJDB2 available as package for [Swift package manager](https://github.com/apple/swift-package-manager/blob/master/Documentation/Usage.md)

In order to use swift binding you should install system wide `libejdb2` library with pkgconfig supplied.

On linux it can be accomplished by installing Debian `ejdb2` package from [ppa:adamansky/ejdb2](https://launchpad.net/~adamansky/+archive/ubuntu/ejdb2)

On OSX

```
brew install ejdb
```

In another case you can build it manually with `cmake`

```sh
git clone https://github.com/Softmotions/ejdb
cd ./ejdb
git submodule update --init

mkdir ./build && cd ./build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_HTTP=ON
make
sudo make install
```

Check all is ok:

```sh
git clone https://github.com/Softmotions/EJDB2Swift
cd ./EJDB2Swift

swift test
```

Now you are able to use swift binding on OSX/Linux.

## iOS

### iOS Prerequisites

* cmake
* XCode

Checkout example todo-list app https://github.com/Softmotions/EJDB2IOSExample

### iOS Carthage

1. Create `Cartfile` with the following content
    ```
    github "Softmotions/EJDB2Swift"
    ```
2. Open your project XCode settings, navigate to: `Build settings` of your target then set
   * **Header search paths** to  `$(PROJECT_DIR)/Carthage/Checkouts/EJDB2Swift/include`
   * **Framework search paths** to `$(PROJECT_DIR)/Carthage/Build/iOS`
3. Run `carthage update --verbose`
4. Then follow usual carthage [project setup instructions](https://github.com/Carthage/Carthage#if-youre-building-for-ios-tvos-or-watchos).

[Sample EJDB2 iOS app with Carthage](https://github.com/Softmotions/EJDB2IOSExample)

### iOS Cocoapods

Setup `Podfile` like this:

```ruby
platform :ios, '9.0'

target 'EJDB2ExampleApp' do
  use_frameworks!
  pod "EJDB2"
end
```

```sh
pod install --verbose
```

Initial build takes some time - be patient

[Sample EJDB2 iOS app with Cocoapods](https://github.com/Softmotions/EJDB2IOSExample/tree/cocoapods)