// swift-tools-version:6.2
import PackageDescription

let package = Package(
    name: "Candy",
    platforms: [
        .macOS(.v26),
        .iOS(.v26),
    ],
    products: [
        .library(
            name: "Candy",
            targets: ["Candy"]
        )
    ],
    dependencies: [],
    targets: [
        .target(
            name: "Candy",
            dependencies: ["CSQLite"],
            path: ".",
            exclude: ["lib/Candy/CMakeLists.txt", "template", "test"],
            sources: ["lib/Candy"],
            publicHeadersPath: "include",
            cxxSettings: [
                .define("CANDY_BUILD_CORE_ONLY", to: "0"),
                .define("CANDY_SWIFT_BUILD", to: "1"),
                .headerSearchPath("include")
            ],
            swiftSettings: [
                .swiftLanguageMode(.v6),
                .interoperabilityMode(.Cxx),
                .unsafeFlags(["-cxx-interoperability-mode=default"])
                //.unsafeFlags(["-Xcc", "-fmodule-map-file=include/module.modulemap"])
            ]
        ),
        .systemLibrary(
            name: "CSQLite",
            path: "utils/CSQLite",
            pkgConfig: "sqlite3",
            providers: [
                .brew(["sqlite3"]),
                .apt(["libsqlite3-dev"])
            ]
        ),
        .executableTarget(
            name: "CandyTests",
            dependencies: ["Candy", "CSQLite"],
            path: "test/CandySwift",
            sources: ["TestImport.swift"],
            cxxSettings: [ .headerSearchPath("../include") ],
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ]
        )
    ],
    swiftLanguageModes: [.v6],
    cLanguageStandard: .c18,
    cxxLanguageStandard: .cxx20
)
