import Foundation

// MARK: - Label

/// A label the project uses, cached in the document so pickers work without network access.
public struct LabelDefinition: Codable, Equatable, Sendable, Identifiable {
    public var name: String
    /// A `RRGGBB` or `#RRGGBB` colour, or `nil` when the label has no colour of its own.
    public var colorHex: String?
    public var description: String

    public var id: String { name }

    public init(name: String = "", colorHex: String? = nil, description: String = "") {
        self.name = name
        self.colorHex = colorHex
        self.description = description
    }

    private enum CodingKeys: String, CodingKey {
        case name, colorHex, description
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            name: try container.decodeIfPresent(String.self, forKey: .name) ?? "",
            colorHex: try container.decodeIfPresent(String.self, forKey: .colorHex),
            description: try container.decodeIfPresent(String.self, forKey: .description) ?? ""
        )
    }
}

// MARK: - Milestone

/// A milestone the project uses, cached in the document so pickers work without network access.
public struct Milestone: Codable, Equatable, Sendable, Identifiable {
    public var name: String
    public var dueOn: Date?
    public var isClosed: Bool

    public var id: String { name }

    public init(name: String = "", dueOn: Date? = nil, isClosed: Bool = false) {
        self.name = name
        self.dueOn = dueOn
        self.isClosed = isClosed
    }

    private enum CodingKeys: String, CodingKey {
        case name, dueOn, isClosed
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            name: try container.decodeIfPresent(String.self, forKey: .name) ?? "",
            dueOn: try container.decodeIfPresent(Date.self, forKey: .dueOn),
            isClosed: try container.decodeIfPresent(Bool.self, forKey: .isClosed) ?? false
        )
    }
}

// MARK: - Person

/// Someone who can report or be assigned an issue, cached in the document so pickers work
/// without network access.
public struct Person: Codable, Equatable, Sendable, Identifiable {
    /// The tracker handle used in ``Issue/assignees`` and ``Issue/reportedBy``.
    public var handle: String
    public var displayName: String
    public var email: String

    public var id: String { handle }

    public init(handle: String = "", displayName: String = "", email: String = "") {
        self.handle = handle
        self.displayName = displayName
        self.email = email
    }

    private enum CodingKeys: String, CodingKey {
        case handle, displayName, email
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            handle: try container.decodeIfPresent(String.self, forKey: .handle) ?? "",
            displayName: try container.decodeIfPresent(String.self, forKey: .displayName) ?? "",
            email: try container.decodeIfPresent(String.self, forKey: .email) ?? ""
        )
    }
}
