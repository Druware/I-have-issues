import Foundation

// MARK: - Document

/// The full contents of an `.issues` document: project identity, tracker integration
/// coordinates, the label/milestone/people catalogs, export settings, and the issues.
///
/// This is the source of truth. Markdown is produced from it by ``IssuesMarkdownSerializer``
/// and read into it — once, from legacy files — by ``LegacyMarkdownImporter``.
public struct IssuesDocumentModel: Codable, Equatable, Sendable {
    /// The highest `schemaVersion` this build can read. Bump it only alongside a migration.
    public static let supportedSchemaVersion = 1

    /// The schema version the document was written with.
    public var schemaVersion: Int
    public var project: ProjectInfo
    public var integrations: IntegrationSettings
    /// The label catalog, so pickers work offline.
    public var labels: [LabelDefinition]
    /// The milestone catalog, so pickers work offline.
    public var milestones: [Milestone]
    /// The people catalog, so pickers work offline.
    public var people: [Person]
    public var export: ExportSettings
    public var issues: [Issue]

    public init(
        schemaVersion: Int = IssuesDocumentModel.supportedSchemaVersion,
        project: ProjectInfo = ProjectInfo(),
        integrations: IntegrationSettings = IntegrationSettings(),
        labels: [LabelDefinition] = [],
        milestones: [Milestone] = [],
        people: [Person] = [],
        export: ExportSettings = ExportSettings(),
        issues: [Issue] = []
    ) {
        self.schemaVersion = schemaVersion
        self.project = project
        self.integrations = integrations
        self.labels = labels
        self.milestones = milestones
        self.people = people
        self.export = export
        self.issues = issues
    }

    // MARK: Helpers

    /// The next display number: one past the highest existing number, or `1` when empty.
    public var nextNumber: Int {
        (issues.map(\.number).max() ?? 0) + 1
    }

    /// The entries that belong under `## Open`, in document order.
    public var openIssues: [Issue] { issues.filter { !$0.isResolved } }

    /// The entries that belong under `## Resolved`, in document order.
    public var resolvedIssues: [Issue] { issues.filter(\.isResolved) }

    /// The issue with the given stable identity, or `nil` when the document has no such issue.
    public func issue(withID id: UUID) -> Issue? {
        issues.first { $0.uuid == id }
    }

    /// An empty document at the current schema version, used for new documents.
    public static func makeEmpty() -> IssuesDocumentModel {
        IssuesDocumentModel()
    }
}

extension IssuesDocumentModel {
    private enum CodingKeys: String, CodingKey {
        case schemaVersion, project, integrations, labels, milestones, people, export, issues
    }

    /// Decodes a document, gating on ``supportedSchemaVersion``.
    ///
    /// - Throws: ``IssuesError/missingSchemaVersion`` when the key is absent, or
    ///   ``IssuesError/unsupportedSchemaVersion(found:supported:)`` when the file was written
    ///   by a newer build.
    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)

        guard let version = try container.decodeIfPresent(Int.self, forKey: .schemaVersion) else {
            throw IssuesError.missingSchemaVersion
        }
        guard version <= Self.supportedSchemaVersion else {
            throw IssuesError.unsupportedSchemaVersion(found: version, supported: Self.supportedSchemaVersion)
        }
        // Migration seam: only version 1 exists today, so an older version is read as-is.
        // When version 2 lands, branch here — decode the old shape, transform it, and set
        // `schemaVersion` to the migrated value so the next save writes the new shape.

        self.init(
            schemaVersion: version,
            project: try container.decodeIfPresent(ProjectInfo.self, forKey: .project) ?? ProjectInfo(),
            integrations: try container.decodeIfPresent(IntegrationSettings.self, forKey: .integrations)
                ?? IntegrationSettings(),
            labels: try container.decodeIfPresent([LabelDefinition].self, forKey: .labels) ?? [],
            milestones: try container.decodeIfPresent([Milestone].self, forKey: .milestones) ?? [],
            people: try container.decodeIfPresent([Person].self, forKey: .people) ?? [],
            export: try container.decodeIfPresent(ExportSettings.self, forKey: .export) ?? ExportSettings(),
            issues: try container.decodeIfPresent([Issue].self, forKey: .issues) ?? []
        )
    }
}

// MARK: - Project

/// The identity of the project the document tracks.
public struct ProjectInfo: Codable, Equatable, Sendable, Identifiable {
    public var id: UUID
    public var name: String
    public var summary: String

    public init(id: UUID = UUID(), name: String = "", summary: String = "") {
        self.id = id
        self.name = name
        self.summary = summary
    }

    private enum CodingKeys: String, CodingKey {
        case id, name, summary
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            id: try container.decodeIfPresent(UUID.self, forKey: .id) ?? UUID(),
            name: try container.decodeIfPresent(String.self, forKey: .name) ?? "",
            summary: try container.decodeIfPresent(String.self, forKey: .summary) ?? ""
        )
    }
}

// MARK: - Export settings

/// How the document is rendered when exported to markdown.
public struct ExportSettings: Codable, Equatable, Sendable {
    /// Everything emitted before the `## Open` heading — title, how-to text, and template.
    public var preambleMarkdown: String

    public init(preambleMarkdown: String = ExportSettings.defaultPreambleMarkdown) {
        self.preambleMarkdown = preambleMarkdown
    }

    private enum CodingKeys: String, CodingKey {
        case preambleMarkdown
    }

    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            preambleMarkdown: try container.decodeIfPresent(String.self, forKey: .preambleMarkdown)
                ?? Self.defaultPreambleMarkdown
        )
    }

    /// The preamble a new document exports with.
    public static let defaultPreambleMarkdown = """
    # Issues

    A running log of known issues, bugs, and feature requests.

    ## How to use this file

    - Add new issues under **Open**, most recent first.
    - Move issues to **Resolved** when closed, noting the fix and date.
    - Use the template below for each entry. Keep IDs sequential (e.g. `#001`).

    ### Template

    ```
    ### #000 — Short descriptive title

    - **Type:** Bug | Feature | Task | Question
    - **Priority:** Low | Medium | High | Critical
    - **Status:** Open | In Progress | Blocked | Resolved
    - **Reported:** YYYY-MM-DD
    - **Reported by:**
    - **Area:** (e.g. Networking, Views, Session)

    **Description**

    What is the problem or request? What is the expected vs. actual behavior?

    **Steps to reproduce** (bugs only)

    1.
    2.
    3.

    **Notes / Investigation**

    Findings, related files, or discussion.

    **Resolution** (when closed)

    What was changed, and where. Reference commit/PR if applicable.
    ```

    ---

    """
}
