// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "IssuesKit",
    platforms: [
        .iOS(.v26),
        .macOS(.v26)
    ],
    products: [
        .library(name: "IssuesKit", targets: ["IssuesKit"])
    ],
    targets: [
        .target(
            name: "IssuesKit",
            swiftSettings: [.swiftLanguageMode(.v6)]
        ),
        .testTarget(
            name: "IssuesKitTests",
            dependencies: ["IssuesKit"],
            resources: [.copy("Resources/Issues.md"), .copy("Resources/MLM-issues.md")],
            swiftSettings: [.swiftLanguageMode(.v6)]
        )
    ]
)
