import Foundation

// MARK: - Protocols

/// An enum persisted by its stable raw value and presented by a separate ``displayName``.
///
/// Raw values are part of the on-disk `.issues` format and must never change. Display
/// strings are presentation only — reword them freely without touching stored documents.
public protocol DisplayNamedEnum: RawRepresentable, CaseIterable, Hashable, Sendable where RawValue == String {
    /// The human-facing name used by the UI and by the markdown export.
    var displayName: String { get }
}

public extension DisplayNamedEnum {
    /// Creates a case from its human-facing display string, matched case-insensitively.
    ///
    /// Used by ``LegacyMarkdownImporter`` to read documents that persisted display strings.
    /// - Parameter displayName: A display string such as `"In Progress"`.
    init?(displayName: String) {
        let needle = displayName.trimmingCharacters(in: .whitespaces).lowercased()
        guard let match = Self.allCases.first(where: { $0.displayName.lowercased() == needle }) else {
            return nil
        }
        self = match
    }
}

/// A ``DisplayNamedEnum`` that decodes an unrecognized raw value to a default instead of
/// failing, so a document written by a newer build still opens in an older one.
public protocol DefaultingEnum: DisplayNamedEnum, Codable {
    /// The value substituted for a missing or unrecognized raw value.
    static var `default`: Self { get }
}

public extension DefaultingEnum {
    /// Decodes a raw value, substituting ``DefaultingEnum/default`` when it is unrecognized.
    static func decoded(from decoder: any Decoder) throws -> Self {
        let raw = try decoder.singleValueContainer().decode(String.self)
        return Self(rawValue: raw) ?? .default
    }
}

// MARK: - Issue classification

/// The kind of an issue entry.
public enum IssueType: String, DefaultingEnum {
    case bug
    case feature
    case task
    case question

    /// The default applied when an entry omits or carries an unrecognized type.
    public static let `default`: IssueType = .task

    public var displayName: String {
        switch self {
        case .bug: "Bug"
        case .feature: "Feature"
        case .task: "Task"
        case .question: "Question"
        }
    }

    public init(from decoder: any Decoder) throws {
        self = try Self.decoded(from: decoder)
    }
}

/// The urgency of an issue entry.
public enum IssuePriority: String, DefaultingEnum {
    case low
    case medium
    case high
    case critical

    /// The default applied when an entry omits or carries an unrecognized priority.
    public static let `default`: IssuePriority = .medium

    public var displayName: String {
        switch self {
        case .low: "Low"
        case .medium: "Medium"
        case .high: "High"
        case .critical: "Critical"
        }
    }

    public init(from decoder: any Decoder) throws {
        self = try Self.decoded(from: decoder)
    }
}

/// The lifecycle state of an issue entry.
public enum IssueStatus: String, DefaultingEnum {
    case open
    case inProgress
    case blocked
    case resolved

    /// The default applied when an entry omits or carries an unrecognized status.
    public static let `default`: IssueStatus = .open

    public var displayName: String {
        switch self {
        case .open: "Open"
        case .inProgress: "In Progress"
        case .blocked: "Blocked"
        case .resolved: "Resolved"
        }
    }

    public init(from decoder: any Decoder) throws {
        self = try Self.decoded(from: decoder)
    }
}

/// Why an issue was closed.
///
/// Maps onto GitHub's `state_reason` and Azure DevOps' *Resolved Reason*. Unlike the other
/// issue enums this one has no default: an unrecognized raw value decodes to `nil`, because
/// inventing a close reason would misreport why work stopped.
public enum ResolutionKind: String, DisplayNamedEnum, Codable {
    case fixed
    case wontFix
    case duplicate
    case cannotReproduce
    case byDesign

    public var displayName: String {
        switch self {
        case .fixed: "Fixed"
        case .wontFix: "Won't Fix"
        case .duplicate: "Duplicate"
        case .cannotReproduce: "Cannot Reproduce"
        case .byDesign: "By Design"
        }
    }
}

// MARK: - Links

/// How one issue relates to another within the same document.
public enum RelationKind: String, DefaultingEnum {
    case blocks
    case blockedBy
    case duplicateOf
    case relatedTo
    case parent
    case child

    /// The default applied to an unrecognized relation, chosen because it is the weakest
    /// claim the format can make about two issues.
    public static let `default`: RelationKind = .relatedTo

    public var displayName: String {
        switch self {
        case .blocks: "Blocks"
        case .blockedBy: "Blocked By"
        case .duplicateOf: "Duplicate Of"
        case .relatedTo: "Related To"
        case .parent: "Parent"
        case .child: "Child"
        }
    }

    public init(from decoder: any Decoder) throws {
        self = try Self.decoded(from: decoder)
    }
}

/// The external issue tracker a ``RemoteLink`` points at.
///
/// Unlike the other issue enums this one does **not** fall back to a default. A provider is
/// sync identity: coercing an unrecognized provider to ``github`` would point a GitHub sync
/// at whatever issue happens to carry the same identifier in the user's repository, and
/// re-saving would make the mislabelling permanent. An unknown value is therefore preserved
/// verbatim in ``other(_:)`` and re-encodes byte-identically.
///
/// > Note: Build ``other(_:)`` only by decoding. Refer to known providers by their own case
/// > so that `.github` and `.other("github")` never both exist in a document.
public enum RemoteProvider: RawRepresentable, Hashable, Sendable, Codable {
    case github
    case azureDevOps
    /// A provider this build does not know about, carrying its raw value unchanged.
    case other(String)

    public init(rawValue: String) {
        switch rawValue {
        case "github": self = .github
        case "azureDevOps": self = .azureDevOps
        default: self = .other(rawValue)
        }
    }

    public var rawValue: String {
        switch self {
        case .github: "github"
        case .azureDevOps: "azureDevOps"
        case let .other(raw): raw
        }
    }

    /// The providers this build can actually sync with — what UI pickers should offer.
    /// Deliberately not `CaseIterable`: ``other(_:)`` has no finite set of values.
    public static let selectableCases: [RemoteProvider] = [.github, .azureDevOps]

    /// The human-facing name. An unknown provider shows its raw value rather than borrowing
    /// a known provider's name.
    public var displayName: String {
        switch self {
        case .github: "GitHub"
        case .azureDevOps: "Azure DevOps"
        case let .other(raw): raw
        }
    }

    /// Whether this link is specifically GitHub. Sync code must gate on this — an unknown
    /// provider is never GitHub, however similar its raw value looks.
    public var isGitHub: Bool { self == .github }

    /// Whether this link is specifically Azure DevOps.
    public var isAzureDevOps: Bool { self == .azureDevOps }

    public init(from decoder: any Decoder) throws {
        self.init(rawValue: try decoder.singleValueContainer().decode(String.self))
    }

    public func encode(to encoder: any Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(rawValue)
    }
}
