// swift-tools-version:5.3

import Foundation
import PackageDescription

var sources = ["src/parser.c"]
if FileManager.default.fileExists(atPath: "src/scanner.c") {
    sources.append("src/scanner.c")
}

let package = Package(
    name: "TreeSitterBrgen",
    products: [
        .library(name: "TreeSitterBrgen", targets: ["TreeSitterBrgen"]),
    ],
    dependencies: [
        .package(name: "SwiftTreeSitter", url: "https://github.com/tree-sitter/swift-tree-sitter", from: "0.25.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterBrgen",
            dependencies: [],
            path: ".",
            sources: sources,
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterBrgenTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterBrgen",
            ],
            path: "bindings/swift/TreeSitterBrgenTests"
        )
    ],
    cLanguageStandard: .c11
)
