import Foundation

// MARK: - Issue

/// A single issue-tracker entry stored in an `.issues` document.
///
/// Identity is split in two on purpose. ``uuid`` is the stable sync identity used by
/// relations and remote links and never changes; ``number`` is the human display number
/// (`#007`) and may be renumbered without breaking anything.
///
/// Every field carries a default, so a fresh entry is one line:
/// `Issue(number: model.nextNumber, title: "Login button does nothing")`.
public struct Issue: Identifiable, Codable, Equatable, Sendable {
    /// The stable sync identity, independent of ``number``.
    public var uuid: UUID
    /// The human display number rendered as `#NNN`.
    public var number: Int

    public var title: String
    public var type: IssueType
    public var priority: IssuePriority
    public var status: IssueStatus
    /// Why the issue was closed, or `nil` while it is still open.
    public var resolutionKind: ResolutionKind?

    public var labels: [String]
    public var assignees: [String]
    /// The name of a ``Milestone`` in the document catalog, or `nil`.
    public var milestone: String?
    public var area: String
    /// The effort estimate in whatever unit the project uses (points, hours), or `nil`.
    public var estimate: Double?

    public var reportedBy: String
    /// The reported date, normalized to the start of its day in the document's fixed calendar.
    public var reported: Date

    public var createdAt: Date
    public var updatedAt: Date
    /// When the issue was closed, or `nil` while it is still open.
    public var closedAt: Date?

    public var description: String
    public var stepsToReproduce: [String]
    public var environment: String
    public var notes: String
    public var resolution: String

    public var comments: [Comment]
    public var relations: [Relation]
    public var remoteLinks: [RemoteLink]

    public var id: UUID { uuid }

    /// Whether the entry belongs in the `## Resolved` section on export.
    public var isResolved: Bool { status == .resolved }

    public init(
        uuid: UUID = UUID(),
        number: Int = 0,
        title: String = "",
        type: IssueType = .default,
        priority: IssuePriority = .default,
        status: IssueStatus = .default,
        resolutionKind: ResolutionKind? = nil,
        labels: [String] = [],
        assignees: [String] = [],
        milestone: String? = nil,
        area: String = "",
        estimate: Double? = nil,
        reportedBy: String = "",
        reported: Date = Date(),
        createdAt: Date = Date(),
        updatedAt: Date = Date(),
        closedAt: Date? = nil,
        description: String = "",
        stepsToReproduce: [String] = [],
        environment: String = "",
        notes: String = "",
        resolution: String = "",
        comments: [Comment] = [],
        relations: [Relation] = [],
        remoteLinks: [RemoteLink] = []
    ) {
        self.uuid = uuid
        self.number = number
        self.title = title
        self.type = type
        self.priority = priority
        self.status = status
        self.resolutionKind = resolutionKind
        self.labels = labels
        self.assignees = assignees
        self.milestone = milestone
        self.area = area
        self.estimate = estimate
        self.reportedBy = reportedBy
        self.reported = reported
        self.createdAt = createdAt
        self.updatedAt = updatedAt
        self.closedAt = closedAt
        self.description = description
        self.stepsToReproduce = stepsToReproduce
        self.environment = environment
        self.notes = notes
        self.resolution = resolution
        self.comments = comments
        self.relations = relations
        self.remoteLinks = remoteLinks
    }
}

extension Issue {
    private enum CodingKeys: String, CodingKey {
        case uuid, number, title, type, priority, status, resolutionKind
        case labels, assignees, milestone, area, estimate
        case reportedBy, reported, createdAt, updatedAt, closedAt
        case description, stepsToReproduce, environment, notes, resolution
        case comments, relations, remoteLinks
    }

    /// Decodes tolerantly: absent keys fall back to the initializer defaults, unknown keys are
    /// ignored, and an unrecognized ``resolutionKind`` decodes to `nil` rather than throwing.
    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let now = Date()
        self.init(
            uuid: try container.decodeIfPresent(UUID.self, forKey: .uuid) ?? UUID(),
            number: try container.decodeIfPresent(Int.self, forKey: .number) ?? 0,
            title: try container.decodeIfPresent(String.self, forKey: .title) ?? "",
            type: try container.decodeIfPresent(IssueType.self, forKey: .type) ?? .default,
            priority: try container.decodeIfPresent(IssuePriority.self, forKey: .priority) ?? .default,
            status: try container.decodeIfPresent(IssueStatus.self, forKey: .status) ?? .default,
            resolutionKind: try container.decodeIfPresent(String.self, forKey: .resolutionKind)
                .flatMap(ResolutionKind.init(rawValue:)),
            labels: try container.decodeIfPresent([String].self, forKey: .labels) ?? [],
            assignees: try container.decodeIfPresent([String].self, forKey: .assignees) ?? [],
            milestone: try container.decodeIfPresent(String.self, forKey: .milestone),
            area: try container.decodeIfPresent(String.self, forKey: .area) ?? "",
            estimate: try container.decodeIfPresent(Double.self, forKey: .estimate),
            reportedBy: try container.decodeIfPresent(String.self, forKey: .reportedBy) ?? "",
            reported: try container.decodeIfPresent(Date.self, forKey: .reported) ?? now,
            createdAt: try container.decodeIfPresent(Date.self, forKey: .createdAt) ?? now,
            updatedAt: try container.decodeIfPresent(Date.self, forKey: .updatedAt) ?? now,
            closedAt: try container.decodeIfPresent(Date.self, forKey: .closedAt),
            description: try container.decodeIfPresent(String.self, forKey: .description) ?? "",
            stepsToReproduce: try container.decodeIfPresent([String].self, forKey: .stepsToReproduce) ?? [],
            environment: try container.decodeIfPresent(String.self, forKey: .environment) ?? "",
            notes: try container.decodeIfPresent(String.self, forKey: .notes) ?? "",
            resolution: try container.decodeIfPresent(String.self, forKey: .resolution) ?? "",
            comments: try container.decodeIfPresent([Comment].self, forKey: .comments) ?? [],
            relations: try container.decodeIfPresent([Relation].self, forKey: .relations) ?? [],
            remoteLinks: try container.decodeIfPresent([RemoteLink].self, forKey: .remoteLinks) ?? []
        )
    }
}

// MARK: - Comment

/// A discussion entry attached to an ``Issue``.
public struct Comment: Identifiable, Codable, Equatable, Sendable {
    public var id: UUID
    public var author: String
    public var createdAt: Date
    public var body: String

    public init(id: UUID = UUID(), author: String = "", createdAt: Date = Date(), body: String = "") {
        self.id = id
        self.author = author
        self.createdAt = createdAt
        self.body = body
    }

    private enum CodingKeys: String, CodingKey {
        case id, author, createdAt, body
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            id: try container.decodeIfPresent(UUID.self, forKey: .id) ?? UUID(),
            author: try container.decodeIfPresent(String.self, forKey: .author) ?? "",
            createdAt: try container.decodeIfPresent(Date.self, forKey: .createdAt) ?? Date(),
            body: try container.decodeIfPresent(String.self, forKey: .body) ?? ""
        )
    }
}

// MARK: - Relation

/// A typed link from one issue to another within the same document.
public struct Relation: Codable, Equatable, Sendable {
    public var kind: RelationKind
    /// The ``Issue/uuid`` of the other issue.
    public var issueID: UUID

    public init(kind: RelationKind = .default, issueID: UUID) {
        self.kind = kind
        self.issueID = issueID
    }

    private enum CodingKeys: String, CodingKey {
        case kind, issueID
    }

    /// `issueID` is required — a relation that points nowhere is not a relation.
    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            kind: try container.decodeIfPresent(RelationKind.self, forKey: .kind) ?? .default,
            issueID: try container.decode(UUID.self, forKey: .issueID)
        )
    }
}

// MARK: - Remote link

/// A pointer from a local issue to its counterpart in an external tracker.
public struct RemoteLink: Codable, Equatable, Sendable {
    public var provider: RemoteProvider
    /// The provider's identifier — a GitHub issue number, or an Azure DevOps work item id.
    public var identifier: String
    public var url: URL?
    /// When this app last pushed or pulled the remote item.
    public var lastSyncedAt: Date?
    /// The remote item's own last-modified timestamp, for conflict detection.
    public var remoteUpdatedAt: Date?

    public init(
        provider: RemoteProvider = .github,
        identifier: String = "",
        url: URL? = nil,
        lastSyncedAt: Date? = nil,
        remoteUpdatedAt: Date? = nil
    ) {
        self.provider = provider
        self.identifier = identifier
        self.url = url
        self.lastSyncedAt = lastSyncedAt
        self.remoteUpdatedAt = remoteUpdatedAt
    }

    private enum CodingKeys: String, CodingKey {
        case provider, identifier, url, lastSyncedAt, remoteUpdatedAt
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            provider: try container.decodeIfPresent(RemoteProvider.self, forKey: .provider) ?? .github,
            identifier: try container.decodeIfPresent(String.self, forKey: .identifier) ?? "",
            url: try container.decodeIfPresent(URL.self, forKey: .url),
            lastSyncedAt: try container.decodeIfPresent(Date.self, forKey: .lastSyncedAt),
            remoteUpdatedAt: try container.decodeIfPresent(Date.self, forKey: .remoteUpdatedAt)
        )
    }
}
