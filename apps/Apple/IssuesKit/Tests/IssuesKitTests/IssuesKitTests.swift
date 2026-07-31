import Foundation
import Testing
@testable import IssuesKit

// MARK: - Fixtures

private func loadResource(_ name: String) throws -> String {
    let url = try #require(Bundle.module.url(forResource: name, withExtension: "md"))
    return try String(contentsOf: url, encoding: .utf8)
}

/// A whole-second date, so it survives the ISO-8601 encoding used by ``IssuesJSONCoder``.
private let referenceDate = Date(timeIntervalSince1970: 1_767_225_600) // 2026-01-01T00:00:00Z

private func date(_ text: String) throws -> Date {
    try #require(IssueDate.date(from: text))
}

private let issueA = UUID(uuidString: "0F5B9C7E-0000-4000-8000-000000000001")
private let issueB = UUID(uuidString: "0F5B9C7E-0000-4000-8000-000000000002")

/// A model with every field populated, used to prove the JSON round trip is lossless.
private func makeFullModel() throws -> IssuesDocumentModel {
    let primary = try #require(issueA)
    let secondary = try #require(issueB)

    let issue = Issue(
        uuid: primary,
        number: 7,
        title: "Login button does nothing",
        type: .bug,
        priority: .high,
        status: .resolved,
        resolutionKind: .fixed,
        labels: ["ui", "regression"],
        assignees: ["dru", "sam"],
        milestone: "v1.0",
        area: "Views",
        estimate: 3.5,
        reportedBy: "Dru",
        reported: try date("2026-05-01"),
        createdAt: referenceDate,
        updatedAt: referenceDate.addingTimeInterval(3600),
        closedAt: referenceDate.addingTimeInterval(7200),
        description: "The login button is inert.\nIt should authenticate.",
        stepsToReproduce: ["Open the app", "Tap Login"],
        environment: "macOS 27.0, build 1234",
        notes: "Possibly a missing action binding.",
        resolution: "Rebound the action.",
        comments: [
            Comment(
                id: try #require(UUID(uuidString: "0F5B9C7E-0000-4000-8000-00000000000A")),
                author: "Sam",
                createdAt: try date("2026-05-02"),
                body: "Reproduced on my machine.\nSame stack trace."
            )
        ],
        relations: [Relation(kind: .blockedBy, issueID: secondary)],
        remoteLinks: [
            RemoteLink(
                provider: .github,
                identifier: "412",
                url: URL(string: "https://github.com/openbcm/i-have-issues/issues/412"),
                lastSyncedAt: referenceDate,
                remoteUpdatedAt: referenceDate.addingTimeInterval(60)
            )
        ]
    )

    let other = Issue(
        uuid: secondary,
        number: 8,
        title: "Blocked by nothing",
        reported: referenceDate,
        createdAt: referenceDate,
        updatedAt: referenceDate
    )

    return IssuesDocumentModel(
        project: ProjectInfo(
            id: try #require(UUID(uuidString: "0F5B9C7E-0000-4000-8000-0000000000FF")),
            name: "I Have Issues",
            summary: "A document-based issue tracker."
        ),
        integrations: IntegrationSettings(
            github: GitHubIntegration(
                owner: "openbcm",
                repository: "i-have-issues",
                defaultLabels: ["triage"],
                defaultAssignees: ["dru"],
                defaultMilestone: "v1.0"
            ),
            azureDevOps: AzureDevOpsIntegration(
                organization: "openbcm",
                project: "IHaveIssues",
                team: "Core",
                areaPath: "IHaveIssues\\Client",
                iterationPath: "IHaveIssues\\Sprint 1",
                defaultWorkItemType: "Bug"
            )
        ),
        labels: [LabelDefinition(name: "ui", colorHex: "#FF00AA", description: "User interface")],
        milestones: [Milestone(name: "v1.0", dueOn: try date("2026-06-30"), isClosed: false)],
        people: [Person(handle: "dru", displayName: "Dru", email: "dru@openbcm.com")],
        export: ExportSettings(preambleMarkdown: "# Issues\n\n"),
        issues: [issue, other]
    )
}

// MARK: - 1. JSON round trip

@Test func fullModelRoundTripsThroughJSON() throws {
    let model = try makeFullModel()
    let decoded = try IssuesJSONCoder.decode(try IssuesJSONCoder.encode(model))
    #expect(decoded == model)
}

@Test func encodedJSONEndsWithNewline() throws {
    let data = try IssuesJSONCoder.encode(try makeFullModel())
    #expect(data.last == UInt8(ascii: "\n"))
}

// MARK: - 2. Minimal document

@Test func minimalDocumentDecodesWithDefaults() throws {
    let json = """
    {
      "schemaVersion": 1,
      "project": { "id": "0F5B9C7E-0000-4000-8000-0000000000FF" },
      "issues": []
    }
    """
    let model = try IssuesJSONCoder.decode(Data(json.utf8))
    #expect(model.schemaVersion == 1)
    #expect(model.project.name.isEmpty)
    #expect(model.project.summary.isEmpty)
    #expect(model.integrations == IntegrationSettings())
    #expect(model.labels.isEmpty)
    #expect(model.milestones.isEmpty)
    #expect(model.people.isEmpty)
    #expect(model.export.preambleMarkdown == ExportSettings.defaultPreambleMarkdown)
    #expect(model.issues.isEmpty)
}

@Test func minimalIssueDecodesWithDefaults() throws {
    let json = """
    { "schemaVersion": 1, "issues": [ { "title": "Bare" } ] }
    """
    let model = try IssuesJSONCoder.decode(Data(json.utf8))
    let issue = try #require(model.issues.first)
    #expect(issue.title == "Bare")
    #expect(issue.number == 0)
    #expect(issue.type == .task)
    #expect(issue.priority == .medium)
    #expect(issue.status == .open)
    #expect(issue.resolutionKind == nil)
    #expect(issue.labels.isEmpty)
    #expect(issue.assignees.isEmpty)
    #expect(issue.milestone == nil)
    #expect(issue.estimate == nil)
    #expect(issue.closedAt == nil)
    #expect(issue.stepsToReproduce.isEmpty)
    #expect(issue.environment.isEmpty)
    #expect(issue.comments.isEmpty)
    #expect(issue.relations.isEmpty)
    #expect(issue.remoteLinks.isEmpty)
}

@Test func documentWithoutSchemaVersionThrows() {
    #expect(throws: IssuesError.missingSchemaVersion) {
        try IssuesJSONCoder.decode(Data(#"{ "issues": [] }"#.utf8))
    }
}

// MARK: - 3. Unknown keys

@Test func unknownJSONKeysAreIgnored() throws {
    let json = """
    {
      "schemaVersion": 1,
      "futureTopLevelField": { "anything": [1, 2, 3] },
      "project": { "name": "Demo", "futureProjectField": true },
      "issues": [ { "title": "Kept", "futureIssueField": "ignored" } ]
    }
    """
    let model = try IssuesJSONCoder.decode(Data(json.utf8))
    #expect(model.project.name == "Demo")
    #expect(model.issues.first?.title == "Kept")
}

// MARK: - 4. Unknown enum raw values

@Test func unknownEnumRawValuesFallBackToDefaults() throws {
    let json = """
    {
      "schemaVersion": 1,
      "issues": [
        {
          "title": "From the future",
          "type": "epic",
          "priority": "whenever",
          "status": "vibing",
          "resolutionKind": "ascended",
          "relations": [ { "kind": "supersedes", "issueID": "0F5B9C7E-0000-4000-8000-000000000002" } ],
          "remoteLinks": [ { "provider": "jira", "identifier": "X-1" } ]
        }
      ]
    }
    """
    let model = try IssuesJSONCoder.decode(Data(json.utf8))
    let issue = try #require(model.issues.first)
    #expect(issue.type == .task)
    #expect(issue.priority == .medium)
    #expect(issue.status == .open)
    #expect(issue.resolutionKind == nil)
    #expect(issue.relations.first?.kind == .relatedTo)
    // A provider is sync identity, so it is preserved verbatim rather than defaulted.
    #expect(issue.remoteLinks.first?.provider == .other("jira"))
}

@Test func unknownRemoteProviderIsPreservedNotCoercedToGitHub() throws {
    let json = """
    {
      "schemaVersion": 1,
      "issues": [
        {
          "title": "Synced elsewhere",
          "remoteLinks": [ { "provider": "gitlab", "identifier": "77" } ]
        }
      ]
    }
    """
    let model = try IssuesJSONCoder.decode(Data(json.utf8))
    let link = try #require(model.issues.first?.remoteLinks.first)

    #expect(link.provider == .other("gitlab"))
    #expect(link.provider != .github)
    #expect(link.provider != .azureDevOps)
    #expect(link.provider.isGitHub == false)
    #expect(link.provider.isAzureDevOps == false)
    #expect(link.provider.displayName == "gitlab")
    #expect(link.provider.rawValue == "gitlab")
}

@Test func unknownRemoteProviderReEncodesVerbatim() throws {
    let link = RemoteLink(provider: .other("gitlab"), identifier: "77")
    let model = IssuesDocumentModel(issues: [Issue(number: 1, remoteLinks: [link])])

    let json = String(decoding: try IssuesJSONCoder.encode(model), as: UTF8.self)
    #expect(json.contains("\"provider\" : \"gitlab\""))
    #expect(!json.contains("\"provider\" : \"github\""))

    let reencoded = try IssuesJSONCoder.encode(try IssuesJSONCoder.decode(try IssuesJSONCoder.encode(model)))
    #expect(reencoded == (try IssuesJSONCoder.encode(model)))
}

@Test func knownRemoteProvidersRoundTripAndAreSelectable() throws {
    #expect(RemoteProvider(rawValue: "github") == .github)
    #expect(RemoteProvider(rawValue: "azureDevOps") == .azureDevOps)
    #expect(RemoteProvider.github.isGitHub)
    #expect(RemoteProvider.azureDevOps.isAzureDevOps)
    #expect(RemoteProvider.selectableCases == [.github, .azureDevOps])
    #expect(RemoteProvider.selectableCases.map(\.displayName) == ["GitHub", "Azure DevOps"])
}

@Test func absentRemoteProviderFallsBackToGitHub() throws {
    let json = #"{ "schemaVersion": 1, "issues": [ { "remoteLinks": [ { "identifier": "9" } ] } ] }"#
    let model = try IssuesJSONCoder.decode(Data(json.utf8))
    #expect(model.issues.first?.remoteLinks.first?.provider == .github)
}

// MARK: - 5. Schema version gate

@Test func newerSchemaVersionThrows() {
    let json = #"{ "schemaVersion": 99, "issues": [] }"#
    #expect(throws: IssuesError.unsupportedSchemaVersion(found: 99, supported: 1)) {
        try IssuesJSONCoder.decode(Data(json.utf8))
    }
}

@Test func supportedSchemaVersionDecodes() throws {
    let model = try IssuesJSONCoder.decode(Data(#"{ "schemaVersion": 1, "issues": [] }"#.utf8))
    #expect(model.schemaVersion == 1)
}

@Test func olderSchemaVersionIsAccepted() throws {
    let model = try IssuesJSONCoder.decode(Data(#"{ "schemaVersion": 0, "issues": [] }"#.utf8))
    #expect(model.schemaVersion == 0)
}

// MARK: - 6. Legacy import of the sample

@Test func legacySampleImportsWithPreambleCaptured() throws {
    let original = try loadResource("Issues")
    let model = try LegacyMarkdownImporter().importDocument(from: original)

    #expect(model.issues.isEmpty)
    #expect(model.openIssues.isEmpty)
    #expect(model.resolvedIssues.isEmpty)
    #expect(original.hasPrefix(model.export.preambleMarkdown))
    #expect(model.export.preambleMarkdown.hasSuffix("---\n\n"))
    #expect(model.export.preambleMarkdown.hasPrefix("# Issues\n"))
    #expect(model.schemaVersion == IssuesDocumentModel.supportedSchemaVersion)
    // The sample carries no entries, so exporting it reproduces the file byte for byte.
    #expect(IssuesMarkdownSerializer.export(model) == original)
}

@Test func legacyImportWithoutAnySectionThrows() {
    #expect(throws: IssuesError.missingOpenSection) {
        try LegacyMarkdownImporter().importDocument(from: "# Issues\n\nNo sections here at all.\n")
    }
}

// MARK: - 7. Legacy import of a fully-populated entry

private let fullLegacyEntry = """
# Issues

Preamble text that must survive verbatim.

## Open

### #007 \u{2014} Login button does nothing

- **Type:** Bug
- **Priority:** High
- **Status:** In Progress
- **Reported:** 2026-05-01
- **Reported by:** Dru
- **Area:** Views
- **Labels:** ui, regression
- **Assignees:** dru, sam
- **Milestone:** v1.0
- **Estimate:** 3.5
- **GitHub:** 412

**Description**

The login button is inert.
It should authenticate.

**Steps to reproduce**

1. Open the app
2. Tap Login

**Environment**

macOS 27.0, build 1234

**Notes / Investigation**

Possibly a missing action binding.

**Resolution**

Not yet fixed.

**Comments**

- **Sam** (2026-05-02): Reproduced on my machine.
  Same stack trace.

---

## Resolved

_No resolved issues._
"""

@Test func legacyFullEntryMapsEveryField() throws {
    let model = try LegacyMarkdownImporter().importDocument(from: fullLegacyEntry)
    let issue = try #require(model.issues.first)

    #expect(issue.number == 7)
    #expect(issue.title == "Login button does nothing")
    #expect(issue.type == .bug)
    #expect(issue.priority == .high)
    #expect(issue.status == .inProgress)
    #expect(IssueDate.string(from: issue.reported) == "2026-05-01")
    #expect(issue.reportedBy == "Dru")
    #expect(issue.area == "Views")
    #expect(issue.labels == ["ui", "regression"])
    #expect(issue.assignees == ["dru", "sam"])
    #expect(issue.milestone == "v1.0")
    #expect(issue.estimate == 3.5)
    #expect(issue.description == "The login button is inert.\nIt should authenticate.")
    #expect(issue.stepsToReproduce == ["Open the app", "Tap Login"])
    #expect(issue.environment == "macOS 27.0, build 1234")
    #expect(issue.notes == "Possibly a missing action binding.")
    #expect(issue.resolution == "Not yet fixed.")
    #expect(issue.comments.count == 1)
    #expect(issue.comments.first?.author == "Sam")
    #expect(issue.comments.first?.body == "Reproduced on my machine.\nSame stack trace.")
    #expect(issue.comments.map { IssueDate.string(from: $0.createdAt) } == ["2026-05-02"])
    #expect(issue.remoteLinks == [RemoteLink(provider: .github, identifier: "412")])
    #expect(model.export.preambleMarkdown.hasPrefix("# Issues\n"))
}

@Test func legacyImportAssignsFreshIdentityAndTimestamps() throws {
    let before = Date()
    let model = try LegacyMarkdownImporter().importDocument(from: fullLegacyEntry)
    let other = try LegacyMarkdownImporter().importDocument(from: fullLegacyEntry)
    let issue = try #require(model.issues.first)
    let twin = try #require(other.issues.first)

    #expect(issue.uuid != twin.uuid)
    #expect(issue.createdAt >= before)
    #expect(issue.updatedAt >= before)
}

@Test func legacyImportFallsBackToDefaultsOnUnknownDisplayStrings() throws {
    let markdown = """
    # Issues

    ## Open

    ### #013 \u{2014} Weird values

    - **Type:** Epic
    - **Priority:** Whenever
    - **Status:** Vibing
    - **Reported:** not-a-date

    ---

    ## Resolved

    _No resolved issues._
    """
    let issue = try #require(try LegacyMarkdownImporter().importDocument(from: markdown).issues.first)
    #expect(issue.type == .task)
    #expect(issue.priority == .medium)
    #expect(issue.status == .open)
}

@Test func legacyMalformedEntryDoesNotAbortTheImport() throws {
    let markdown = """
    # Issues

    ## Open

    ### not-an-issue heading with no number

    random text
    - **Type:** Bug

    ### #020 \u{2014} A real one

    - **Type:** Feature

    ## Resolved

    _No resolved issues._
    """
    let model = try LegacyMarkdownImporter().importDocument(from: markdown)
    #expect(model.issues.count == 1)
    #expect(model.issues.first?.number == 20)
    #expect(model.issues.first?.type == .feature)
}

@Test func legacyUnknownBodySectionLandsInNotes() throws {
    let markdown = """
    # Issues

    ## Open

    ### #030 \u{2014} Has extra section

    - **Type:** Task

    **Design Considerations**

    Some extra prose that is not a known section.

    ---

    ## Resolved

    _No resolved issues._
    """
    let model = try LegacyMarkdownImporter().importDocument(from: markdown)
    let issue = try #require(model.issues.first)
    #expect(issue.notes == "**Design Considerations**\n\nSome extra prose that is not a known section.")
    #expect(IssuesMarkdownSerializer.export(model).contains("Some extra prose"))
}

// MARK: - 8. Markdown export

@Test func exportRoutesIssuesToTheirSections() throws {
    let open = Issue(number: 1, title: "Open one", status: .open)
    let done = Issue(number: 2, title: "Done one", status: .resolved)
    let model = IssuesDocumentModel(
        export: ExportSettings(preambleMarkdown: "# Issues\n\n"),
        issues: [open, done]
    )
    let output = IssuesMarkdownSerializer.export(model)

    let openHeading = try #require(output.range(of: "## Open"))
    let resolvedHeading = try #require(output.range(of: "## Resolved"))
    let openEntry = try #require(output.range(of: "Open one"))
    let doneEntry = try #require(output.range(of: "Done one"))

    #expect(openEntry.lowerBound > openHeading.upperBound)
    #expect(openEntry.lowerBound < resolvedHeading.lowerBound)
    #expect(doneEntry.lowerBound > resolvedHeading.lowerBound)
}

@Test func exportUsesDisplayNamesNotRawValues() {
    let issue = Issue(number: 1, title: "Busy", type: .feature, priority: .critical, status: .inProgress)
    let output = IssuesMarkdownSerializer.export(IssuesDocumentModel(issues: [issue]))
    #expect(output.contains("- **Status:** In Progress"))
    #expect(output.contains("- **Type:** Feature"))
    #expect(output.contains("- **Priority:** Critical"))
    #expect(!output.contains("inProgress"))
}

@Test func exportEmitsNewFieldsWhenPopulated() throws {
    let issue = Issue(
        number: 9,
        title: "Rich entry",
        labels: ["ui", "regression"],
        assignees: ["dru"],
        milestone: "v1.0",
        estimate: 3,
        environment: "macOS 27.0",
        comments: [Comment(author: "Sam", createdAt: try date("2026-05-02"), body: "Looks right.")]
    )
    let output = IssuesMarkdownSerializer.export(IssuesDocumentModel(issues: [issue]))

    #expect(output.contains("- **Labels:** ui, regression"))
    #expect(output.contains("- **Assignees:** dru"))
    #expect(output.contains("- **Milestone:** v1.0"))
    #expect(output.contains("- **Estimate:** 3"))
    #expect(output.contains("**Environment**"))
    #expect(output.contains("macOS 27.0"))
    #expect(output.contains("**Comments**"))
    #expect(output.contains("- **Sam** (2026-05-02): Looks right."))
}

@Test func exportOmitsNewFieldsWhenEmpty() {
    let issue = Issue(number: 9, title: "Bare entry")
    let output = IssuesMarkdownSerializer.export(IssuesDocumentModel(issues: [issue]))

    #expect(!output.contains("**Labels:**"))
    #expect(!output.contains("**Assignees:**"))
    #expect(!output.contains("**Milestone:**"))
    #expect(!output.contains("**Estimate:**"))
    #expect(!output.contains("**Environment**"))
    #expect(!output.contains("**Comments**"))
}

// MARK: - 9. Export / import stability

@Test func exportImportExportIsStable() throws {
    let resolved = Issue(
        number: 3,
        title: "Crash on launch",
        type: .bug,
        priority: .critical,
        status: .resolved,
        labels: ["startup"],
        assignees: ["dru"],
        milestone: "v0.9",
        area: "App",
        estimate: 2,
        reportedBy: "Sam",
        reported: try date("2026-04-01"),
        description: "It crashes.",
        stepsToReproduce: ["Launch", "Watch it die"],
        environment: "iOS 27.0",
        notes: "Stack trace attached.",
        resolution: "Fixed the nil unwrap.",
        comments: [
            Comment(author: "Dru", createdAt: try date("2026-04-02"), body: "First line.\nSecond line.")
        ]
    )
    let open = Issue(
        number: 4,
        title: "Add dark mode",
        type: .feature,
        priority: .low,
        area: "Views",
        reportedBy: "Dru",
        reported: try date("2026-04-05"),
        description: "Please."
    )
    let model = IssuesDocumentModel(issues: [open, resolved])

    let first = IssuesMarkdownSerializer.export(model)
    let reimported = try LegacyMarkdownImporter().importDocument(from: first)
    let second = IssuesMarkdownSerializer.export(reimported)

    #expect(second == first)
    #expect(reimported.openIssues.count == 1)
    #expect(reimported.resolvedIssues.count == 1)
}

// MARK: - 10. Model helpers

@Test func nextNumberIsOnePastHighest() {
    let model = IssuesDocumentModel(issues: [Issue(number: 3), Issue(number: 7), Issue(number: 5)])
    #expect(model.nextNumber == 8)
}

@Test func nextNumberStartsAtOneWhenEmpty() {
    #expect(IssuesDocumentModel().nextNumber == 1)
}

@Test func issueLookupByIdentity() throws {
    let wanted = try #require(issueA)
    let model = IssuesDocumentModel(issues: [Issue(uuid: wanted, number: 1, title: "Wanted"), Issue(number: 2)])
    #expect(model.issue(withID: wanted)?.title == "Wanted")
    #expect(model.issue(withID: UUID()) == nil)
}

@Test func makeEmptyCarriesTheTemplatePreamble() {
    let model = IssuesDocumentModel.makeEmpty()
    #expect(model.issues.isEmpty)
    #expect(model.schemaVersion == IssuesDocumentModel.supportedSchemaVersion)
    #expect(model.export.preambleMarkdown == ExportSettings.defaultPreambleMarkdown)

    let exported = IssuesMarkdownSerializer.export(model)
    #expect(exported.contains("## Open"))
    #expect(exported.contains("## Resolved"))
}

// MARK: - 11. Deterministic encoding

@Test func encodingIsDeterministic() throws {
    let model = try makeFullModel()
    #expect(try IssuesJSONCoder.encode(model) == (try IssuesJSONCoder.encode(model)))
}

@Test func encodedKeysAreSorted() throws {
    let json = String(decoding: try IssuesJSONCoder.encode(try makeFullModel()), as: UTF8.self)
    // Pretty-printed output indents top-level keys by exactly two spaces.
    let topLevelKeys = json.components(separatedBy: "\n").compactMap { line -> String? in
        guard line.hasPrefix("  \"") else { return nil }
        let afterIndent = line.dropFirst(3)
        guard let close = afterIndent.firstIndex(of: "\"") else { return nil }
        return String(afterIndent[..<close])
    }
    #expect(topLevelKeys == ["export", "integrations", "issues", "labels",
                             "milestones", "people", "project", "schemaVersion"])
    #expect(topLevelKeys == topLevelKeys.sorted())
}

@Test func encodedJSONDoesNotEscapeSlashes() throws {
    let json = String(decoding: try IssuesJSONCoder.encode(try makeFullModel()), as: UTF8.self)
    #expect(json.contains("https://github.com/openbcm/i-have-issues/issues/412"))
    #expect(!json.contains("\\/"))
}

// MARK: - Free-form legacy format (Claude.ai-generated issues file)

@Test func freeFormSampleImportsAllIssues() throws {
    let model = try LegacyMarkdownImporter().importDocument(from: try loadResource("MLM-issues"))
    #expect(model.issues.count == 5)
    #expect(model.openIssues.count == 5)
}

@Test func freeFormSampleFirstIssueFields() throws {
    let model = try LegacyMarkdownImporter().importDocument(from: try loadResource("MLM-issues"))
    let issue = try #require(model.issues.first)
    #expect(issue.number == 1)
    #expect(issue.title == "Login endpoint never triggers the configured account lockout")
    #expect(issue.priority == .high)
    #expect(issue.status == .open)
    #expect(issue.area == "API (Auth)")
    #expect(!issue.description.isEmpty)
    #expect(issue.stepsToReproduce.count == 3)
    #expect(!issue.resolution.isEmpty)
}

@Test func freeFormSampleSeverityMapsToPriority() throws {
    let model = try LegacyMarkdownImporter().importDocument(from: try loadResource("MLM-issues"))
    let expected: [IssuePriority] = [.high, .medium, .medium, .medium, .low]
    #expect(model.issues.map(\.priority) == expected)
}

@Test func freeFormSampleComponentMapsToArea() throws {
    let model = try LegacyMarkdownImporter().importDocument(from: try loadResource("MLM-issues"))
    #expect(model.issues[0].area == "API (Auth)")
    #expect(model.issues[4].area == "Web")
}
