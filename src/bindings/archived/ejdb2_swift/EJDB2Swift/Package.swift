// swift-tools-version:5.1
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
  name: "EJDB2",
  products: [
    .library(
      name: "EJDB2",
      targets: ["EJDB2"]),
  ],
  dependencies: [
  ],
  targets: [
    .systemLibrary(name: "CEJDB2", pkgConfig: "libejdb2"),
    .target(
      name: "EJDB2",
      dependencies: [
        "CEJDB2",
      ]),
    .testTarget(name: "EJDB2Tests", dependencies: ["EJDB2"]),
  ]
)