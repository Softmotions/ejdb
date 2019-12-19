// swift-tools-version:5.1
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
  name: "EJDB2Swift",
  products: [
    .library(
      name: "EJDB2Swift",
      targets: ["EJDB2Swift"]),
    .executable(
      name: "EJDB2SwiftRun",
      targets: ["EJDB2SwiftRun"]
    ),
  ],
  dependencies: [
    //.package(url: "https://github.com/Softmotions/SwiftCoroutine", from: "1.0.1"),
  ],
  targets: [
    .systemLibrary(name: "EJDB2", pkgConfig: "libejdb2"),
    .target(
      name: "EJDB2Swift",
      dependencies: [
        "EJDB2",
        //"SwiftCoroutine"
      ]),
    .target(name: "EJDB2SwiftRun", dependencies: ["EJDB2Swift"]),
    .testTarget(name: "EJDB2SwiftTests", dependencies: ["EJDB2Swift"]),
  ]
)