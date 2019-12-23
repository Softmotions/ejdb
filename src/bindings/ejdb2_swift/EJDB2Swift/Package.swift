// swift-tools-version:5.1
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
  name: "EJDB2Swift",
  products: [
    .library(
      name: "EJDB2Swift",
      targets: ["EJDB2Swift"]),
  ],
  dependencies: [
  ],
  targets: [
    .systemLibrary(name: "EJDB2", pkgConfig: "libejdb2"),
    .target(
      name: "EJDB2Swift",
      dependencies: [
        "EJDB2",
      ]),
    .testTarget(name: "EJDB2SwiftTests", dependencies: ["EJDB2Swift"]),
  ]
)