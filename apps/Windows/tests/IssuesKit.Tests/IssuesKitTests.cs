using System.Text;
using IssuesKit.Json;

namespace IssuesKit.Tests;

/// <summary>
/// A port of the Apple project's <c>IssuesKitTests</c>, section for section, plus the
/// cross-platform byte-identity tests the Windows encoder needs to earn.
/// </summary>
public class IssuesKitTests
{
    // MARK: - Fixtures

    /// <summary>A whole-second date, so it survives the ISO-8601 encoding used by the coder.</summary>
    private static readonly DateTimeOffset ReferenceDate = DateTimeOffset.FromUnixTimeSeconds(1_767_225_600);

    private static readonly Guid IssueA = Guid.Parse("0F5B9C7E-0000-4000-8000-000000000001");
    private static readonly Guid IssueB = Guid.Parse("0F5B9C7E-0000-4000-8000-000000000002");

    private static DateTimeOffset Date(string text)
    {
        var value = IssueDate.Date(text);
        Assert.NotNull(value);
        return value.Value;
    }

    /// <summary>A model with every field populated, used to prove the JSON round trip is lossless.</summary>
    private static IssuesDocumentModel MakeFullModel()
    {
        var issue = new Issue
        {
            Uuid = IssueA,
            Number = 7,
            Title = "Login button does nothing",
            Type = IssueType.Bug,
            Priority = IssuePriority.High,
            Status = IssueStatus.Resolved,
            ResolutionKind = ResolutionKind.Fixed,
            Labels = ["ui", "regression"],
            Assignees = ["dru", "sam"],
            Milestone = "v1.0",
            Area = "Views",
            Estimate = 3.5,
            ReportedBy = "Dru",
            Reported = Date("2026-05-01"),
            CreatedAt = ReferenceDate,
            UpdatedAt = ReferenceDate.AddSeconds(3600),
            ClosedAt = ReferenceDate.AddSeconds(7200),
            Description = "The login button is inert.\nIt should authenticate.",
            StepsToReproduce = ["Open the app", "Tap Login"],
            Environment = "macOS 27.0, build 1234",
            Notes = "Possibly a missing action binding.",
            Resolution = "Rebound the action.",
            Comments =
            [
                new Comment
                {
                    Id = Guid.Parse("0F5B9C7E-0000-4000-8000-00000000000A"),
                    Author = "Sam",
                    CreatedAt = Date("2026-05-02"),
                    Body = "Reproduced on my machine.\nSame stack trace."
                }
            ],
            Relations = [new Relation(RelationKind.BlockedBy, IssueB)],
            RemoteLinks =
            [
                new RemoteLink
                {
                    Provider = RemoteProvider.GitHub,
                    Identifier = "412",
                    Url = new Uri("https://github.com/openbcm/i-have-issues/issues/412"),
                    LastSyncedAt = ReferenceDate,
                    RemoteUpdatedAt = ReferenceDate.AddSeconds(60)
                }
            ]
        };

        var other = new Issue
        {
            Uuid = IssueB,
            Number = 8,
            Title = "Blocked by nothing",
            Reported = ReferenceDate,
            CreatedAt = ReferenceDate,
            UpdatedAt = ReferenceDate
        };

        return new IssuesDocumentModel
        {
            Project = new ProjectInfo
            {
                Id = Guid.Parse("0F5B9C7E-0000-4000-8000-0000000000FF"),
                Name = "I Have Issues",
                Summary = "A document-based issue tracker."
            },
            Integrations = new IntegrationSettings
            {
                GitHub = new GitHubIntegration
                {
                    Owner = "openbcm",
                    Repository = "i-have-issues",
                    DefaultLabels = ["triage"],
                    DefaultAssignees = ["dru"],
                    DefaultMilestone = "v1.0"
                },
                AzureDevOps = new AzureDevOpsIntegration
                {
                    Organization = "openbcm",
                    Project = "IHaveIssues",
                    Team = "Core",
                    AreaPath = @"IHaveIssues\Client",
                    IterationPath = @"IHaveIssues\Sprint 1",
                    DefaultWorkItemType = "Bug"
                }
            },
            Labels = [new LabelDefinition { Name = "ui", ColorHex = "#FF00AA", Description = "User interface" }],
            Milestones = [new Milestone { Name = "v1.0", DueOn = Date("2026-06-30"), IsClosed = false }],
            People = [new Person { Handle = "dru", DisplayName = "Dru", Email = "dru@openbcm.com" }],
            Export = new ExportSettings { PreambleMarkdown = "# Issues\n\n" },
            Issues = [issue, other]
        };
    }

    // MARK: - 1. JSON round trip

    [Fact]
    public void FullModelRoundTripsThroughJson()
    {
        var model = MakeFullModel();
        var decoded = IssuesJsonCoder.Decode(IssuesJsonCoder.Encode(model));
        Assert.Equal(model, decoded);
    }

    [Fact]
    public void EncodedJsonEndsWithNewline()
    {
        var data = IssuesJsonCoder.Encode(MakeFullModel());
        Assert.Equal((byte)'\n', data[^1]);
    }

    // MARK: - 2. Minimal document

    [Fact]
    public void MinimalDocumentDecodesWithDefaults()
    {
        const string Json = """
            {
              "schemaVersion": 1,
              "project": { "id": "0F5B9C7E-0000-4000-8000-0000000000FF" },
              "issues": []
            }
            """;
        var model = IssuesJsonCoder.Decode(Json);

        Assert.Equal(1, model.SchemaVersion);
        Assert.Empty(model.Project.Name);
        Assert.Empty(model.Project.Summary);
        Assert.Equal(new IntegrationSettings(), model.Integrations);
        Assert.Empty(model.Labels);
        Assert.Empty(model.Milestones);
        Assert.Empty(model.People);
        Assert.Equal(ExportSettings.DefaultPreambleMarkdown, model.Export.PreambleMarkdown);
        Assert.Empty(model.Issues);
    }

    [Fact]
    public void MinimalIssueDecodesWithDefaults()
    {
        var model = IssuesJsonCoder.Decode("""{ "schemaVersion": 1, "issues": [ { "title": "Bare" } ] }""");
        var issue = Assert.Single(model.Issues);

        Assert.Equal("Bare", issue.Title);
        Assert.Equal(0, issue.Number);
        Assert.Equal(IssueType.Task, issue.Type);
        Assert.Equal(IssuePriority.Medium, issue.Priority);
        Assert.Equal(IssueStatus.Open, issue.Status);
        Assert.Null(issue.ResolutionKind);
        Assert.Empty(issue.Labels);
        Assert.Empty(issue.Assignees);
        Assert.Null(issue.Milestone);
        Assert.Null(issue.Estimate);
        Assert.Null(issue.ClosedAt);
        Assert.Empty(issue.StepsToReproduce);
        Assert.Empty(issue.Environment);
        Assert.Empty(issue.Comments);
        Assert.Empty(issue.Relations);
        Assert.Empty(issue.RemoteLinks);
    }

    [Fact]
    public void DocumentWithoutSchemaVersionThrows() =>
        Assert.Throws<MissingSchemaVersionException>(() => IssuesJsonCoder.Decode("""{ "issues": [] }"""));

    // MARK: - 3. Unknown keys

    [Fact]
    public void UnknownJsonKeysAreIgnored()
    {
        const string Json = """
            {
              "schemaVersion": 1,
              "futureTopLevelField": { "anything": [1, 2, 3] },
              "project": { "name": "Demo", "futureProjectField": true },
              "issues": [ { "title": "Kept", "futureIssueField": "ignored" } ]
            }
            """;
        var model = IssuesJsonCoder.Decode(Json);

        Assert.Equal("Demo", model.Project.Name);
        Assert.Equal("Kept", Assert.Single(model.Issues).Title);
    }

    // MARK: - 4. Unknown enum raw values

    [Fact]
    public void UnknownEnumRawValuesFallBackToDefaults()
    {
        const string Json = """
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
            """;
        var issue = Assert.Single(IssuesJsonCoder.Decode(Json).Issues);

        Assert.Equal(IssueType.Task, issue.Type);
        Assert.Equal(IssuePriority.Medium, issue.Priority);
        Assert.Equal(IssueStatus.Open, issue.Status);
        Assert.Null(issue.ResolutionKind);
        Assert.Equal(RelationKind.RelatedTo, Assert.Single(issue.Relations).Kind);
        // A provider is sync identity, so it is preserved verbatim rather than defaulted.
        Assert.Equal(new RemoteProvider("jira"), Assert.Single(issue.RemoteLinks).Provider);
    }

    [Fact]
    public void UnknownRemoteProviderIsPreservedNotCoercedToGitHub()
    {
        const string Json = """
            {
              "schemaVersion": 1,
              "issues": [
                {
                  "title": "Synced elsewhere",
                  "remoteLinks": [ { "provider": "gitlab", "identifier": "77" } ]
                }
              ]
            }
            """;
        var link = Assert.Single(Assert.Single(IssuesJsonCoder.Decode(Json).Issues).RemoteLinks);

        Assert.Equal(new RemoteProvider("gitlab"), link.Provider);
        Assert.NotEqual(RemoteProvider.GitHub, link.Provider);
        Assert.NotEqual(RemoteProvider.AzureDevOps, link.Provider);
        Assert.False(link.Provider.IsGitHub);
        Assert.False(link.Provider.IsAzureDevOps);
        Assert.Equal("gitlab", link.Provider.DisplayName);
        Assert.Equal("gitlab", link.Provider.RawValue);
    }

    [Fact]
    public void UnknownRemoteProviderReEncodesVerbatim()
    {
        var link = new RemoteLink(new RemoteProvider("gitlab"), "77");
        var model = new IssuesDocumentModel { Issues = [new Issue { Number = 1, RemoteLinks = [link] }] };

        var json = IssuesJsonCoder.EncodeToString(model);
        Assert.Contains("\"provider\" : \"gitlab\"", json, StringComparison.Ordinal);
        Assert.DoesNotContain("\"provider\" : \"github\"", json, StringComparison.Ordinal);

        var reencoded = IssuesJsonCoder.Encode(IssuesJsonCoder.Decode(IssuesJsonCoder.Encode(model)));
        Assert.Equal(IssuesJsonCoder.Encode(model), reencoded);
    }

    [Fact]
    public void KnownRemoteProvidersRoundTripAndAreSelectable()
    {
        Assert.Equal(RemoteProvider.GitHub, new RemoteProvider("github"));
        Assert.Equal(RemoteProvider.AzureDevOps, new RemoteProvider("azureDevOps"));
        Assert.True(RemoteProvider.GitHub.IsGitHub);
        Assert.True(RemoteProvider.AzureDevOps.IsAzureDevOps);
        Assert.Equal([RemoteProvider.GitHub, RemoteProvider.AzureDevOps], RemoteProvider.SelectableCases);
        Assert.Equal(["GitHub", "Azure DevOps"], RemoteProvider.SelectableCases.Select(p => p.DisplayName));
    }

    [Fact]
    public void AbsentRemoteProviderFallsBackToGitHub()
    {
        const string Json = """{ "schemaVersion": 1, "issues": [ { "remoteLinks": [ { "identifier": "9" } ] } ] }""";
        var link = Assert.Single(Assert.Single(IssuesJsonCoder.Decode(Json).Issues).RemoteLinks);
        Assert.Equal(RemoteProvider.GitHub, link.Provider);
    }

    // MARK: - 5. Schema version gate

    [Fact]
    public void NewerSchemaVersionThrows()
    {
        var error = Assert.Throws<UnsupportedSchemaVersionException>(
            () => IssuesJsonCoder.Decode("""{ "schemaVersion": 99, "issues": [] }"""));

        Assert.Equal(99, error.Found);
        Assert.Equal(1, error.Supported);
    }

    [Fact]
    public void SupportedSchemaVersionDecodes() =>
        Assert.Equal(1, IssuesJsonCoder.Decode("""{ "schemaVersion": 1, "issues": [] }""").SchemaVersion);

    [Fact]
    public void OlderSchemaVersionIsAccepted() =>
        Assert.Equal(0, IssuesJsonCoder.Decode("""{ "schemaVersion": 0, "issues": [] }""").SchemaVersion);

    // MARK: - 6. Legacy import of the sample

    [Fact]
    public void LegacySampleImportsWithPreambleCaptured()
    {
        var original = Fixtures.Text("Issues.md");
        var model = new LegacyMarkdownImporter().ImportDocument(original);

        Assert.Empty(model.Issues);
        Assert.Empty(model.OpenIssues);
        Assert.Empty(model.ResolvedIssues);
        Assert.StartsWith(model.Export.PreambleMarkdown, original, StringComparison.Ordinal);
        Assert.EndsWith("---\n\n", model.Export.PreambleMarkdown, StringComparison.Ordinal);
        Assert.StartsWith("# Issues\n", model.Export.PreambleMarkdown, StringComparison.Ordinal);
        Assert.Equal(IssuesDocumentModel.SupportedSchemaVersion, model.SchemaVersion);
        // The sample carries no entries, so exporting it reproduces the file byte for byte.
        Assert.Equal(original, IssuesMarkdownSerializer.Export(model));
    }

    [Fact]
    public void LegacyImportWithoutAnySectionThrows() =>
        Assert.Throws<MissingOpenSectionException>(
            () => new LegacyMarkdownImporter().ImportDocument("# Issues\n\nNo sections here at all.\n"));

    // MARK: - 7. Legacy import of a fully-populated entry

    private const string FullLegacyEntry = """
        # Issues

        Preamble text that must survive verbatim.

        ## Open

        ### #007 — Login button does nothing

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
        """;

    [Fact]
    public void LegacyFullEntryMapsEveryField()
    {
        var model = new LegacyMarkdownImporter().ImportDocument(FullLegacyEntry);
        var issue = Assert.Single(model.Issues);

        Assert.Equal(7, issue.Number);
        Assert.Equal("Login button does nothing", issue.Title);
        Assert.Equal(IssueType.Bug, issue.Type);
        Assert.Equal(IssuePriority.High, issue.Priority);
        Assert.Equal(IssueStatus.InProgress, issue.Status);
        Assert.Equal("2026-05-01", IssueDate.String(issue.Reported));
        Assert.Equal("Dru", issue.ReportedBy);
        Assert.Equal("Views", issue.Area);
        Assert.Equal(["ui", "regression"], issue.Labels);
        Assert.Equal(["dru", "sam"], issue.Assignees);
        Assert.Equal("v1.0", issue.Milestone);
        Assert.Equal(3.5, issue.Estimate);
        Assert.Equal("The login button is inert.\nIt should authenticate.", issue.Description);
        Assert.Equal(["Open the app", "Tap Login"], issue.StepsToReproduce);
        Assert.Equal("macOS 27.0, build 1234", issue.Environment);
        Assert.Equal("Possibly a missing action binding.", issue.Notes);
        Assert.Equal("Not yet fixed.", issue.Resolution);
        var comment = Assert.Single(issue.Comments);
        Assert.Equal("Sam", comment.Author);
        Assert.Equal("Reproduced on my machine.\nSame stack trace.", comment.Body);
        Assert.Equal("2026-05-02", IssueDate.String(comment.CreatedAt));
        Assert.Equal([new RemoteLink(RemoteProvider.GitHub, "412")], issue.RemoteLinks);
        Assert.StartsWith("# Issues\n", model.Export.PreambleMarkdown, StringComparison.Ordinal);
    }

    [Fact]
    public void LegacyImportAssignsFreshIdentityAndTimestamps()
    {
        var before = DateTimeOffset.UtcNow;
        var issue = Assert.Single(new LegacyMarkdownImporter().ImportDocument(FullLegacyEntry).Issues);
        var twin = Assert.Single(new LegacyMarkdownImporter().ImportDocument(FullLegacyEntry).Issues);

        Assert.NotEqual(issue.Uuid, twin.Uuid);
        Assert.True(issue.CreatedAt >= before);
        Assert.True(issue.UpdatedAt >= before);
    }

    [Fact]
    public void LegacyImportFallsBackToDefaultsOnUnknownDisplayStrings()
    {
        const string Markdown = """
            # Issues

            ## Open

            ### #013 — Weird values

            - **Type:** Epic
            - **Priority:** Whenever
            - **Status:** Vibing
            - **Reported:** not-a-date

            ---

            ## Resolved

            _No resolved issues._
            """;
        var issue = Assert.Single(new LegacyMarkdownImporter().ImportDocument(Markdown).Issues);

        Assert.Equal(IssueType.Task, issue.Type);
        Assert.Equal(IssuePriority.Medium, issue.Priority);
        Assert.Equal(IssueStatus.Open, issue.Status);
    }

    [Fact]
    public void LegacyMalformedEntryDoesNotAbortTheImport()
    {
        const string Markdown = """
            # Issues

            ## Open

            ### not-an-issue heading with no number

            random text
            - **Type:** Bug

            ### #020 — A real one

            - **Type:** Feature

            ## Resolved

            _No resolved issues._
            """;
        var issue = Assert.Single(new LegacyMarkdownImporter().ImportDocument(Markdown).Issues);

        Assert.Equal(20, issue.Number);
        Assert.Equal(IssueType.Feature, issue.Type);
    }

    [Fact]
    public void LegacyUnknownBodySectionLandsInNotes()
    {
        const string Markdown = """
            # Issues

            ## Open

            ### #030 — Has extra section

            - **Type:** Task

            **Design Considerations**

            Some extra prose that is not a known section.

            ---

            ## Resolved

            _No resolved issues._
            """;
        var model = new LegacyMarkdownImporter().ImportDocument(Markdown);
        var issue = Assert.Single(model.Issues);

        Assert.Equal(
            "**Design Considerations**\n\nSome extra prose that is not a known section.",
            issue.Notes);
        Assert.Contains("Some extra prose", IssuesMarkdownSerializer.Export(model), StringComparison.Ordinal);
    }

    // MARK: - 8. Markdown export

    [Fact]
    public void ExportRoutesIssuesToTheirSections()
    {
        var model = new IssuesDocumentModel
        {
            Export = new ExportSettings { PreambleMarkdown = "# Issues\n\n" },
            Issues =
            [
                new Issue { Number = 1, Title = "Open one", Status = IssueStatus.Open },
                new Issue { Number = 2, Title = "Done one", Status = IssueStatus.Resolved }
            ]
        };
        var output = IssuesMarkdownSerializer.Export(model);

        var openHeading = output.IndexOf("## Open", StringComparison.Ordinal);
        var resolvedHeading = output.IndexOf("## Resolved", StringComparison.Ordinal);
        var openEntry = output.IndexOf("Open one", StringComparison.Ordinal);
        var doneEntry = output.IndexOf("Done one", StringComparison.Ordinal);

        Assert.True(openEntry > openHeading);
        Assert.True(openEntry < resolvedHeading);
        Assert.True(doneEntry > resolvedHeading);
    }

    [Fact]
    public void ExportUsesDisplayNamesNotRawValues()
    {
        var issue = new Issue
        {
            Number = 1,
            Title = "Busy",
            Type = IssueType.Feature,
            Priority = IssuePriority.Critical,
            Status = IssueStatus.InProgress
        };
        var output = IssuesMarkdownSerializer.Export(new IssuesDocumentModel { Issues = [issue] });

        Assert.Contains("- **Status:** In Progress", output, StringComparison.Ordinal);
        Assert.Contains("- **Type:** Feature", output, StringComparison.Ordinal);
        Assert.Contains("- **Priority:** Critical", output, StringComparison.Ordinal);
        Assert.DoesNotContain("inProgress", output, StringComparison.Ordinal);
    }

    [Fact]
    public void ExportEmitsNewFieldsWhenPopulated()
    {
        var issue = new Issue
        {
            Number = 9,
            Title = "Rich entry",
            Labels = ["ui", "regression"],
            Assignees = ["dru"],
            Milestone = "v1.0",
            Estimate = 3,
            Environment = "macOS 27.0",
            Comments = [new Comment { Author = "Sam", CreatedAt = Date("2026-05-02"), Body = "Looks right." }]
        };
        var output = IssuesMarkdownSerializer.Export(new IssuesDocumentModel { Issues = [issue] });

        Assert.Contains("- **Labels:** ui, regression", output, StringComparison.Ordinal);
        Assert.Contains("- **Assignees:** dru", output, StringComparison.Ordinal);
        Assert.Contains("- **Milestone:** v1.0", output, StringComparison.Ordinal);
        Assert.Contains("- **Estimate:** 3", output, StringComparison.Ordinal);
        Assert.Contains("**Environment**", output, StringComparison.Ordinal);
        Assert.Contains("macOS 27.0", output, StringComparison.Ordinal);
        Assert.Contains("**Comments**", output, StringComparison.Ordinal);
        Assert.Contains("- **Sam** (2026-05-02): Looks right.", output, StringComparison.Ordinal);
    }

    [Fact]
    public void ExportOmitsNewFieldsWhenEmpty()
    {
        var issue = new Issue { Number = 9, Title = "Bare entry" };
        var output = IssuesMarkdownSerializer.Export(new IssuesDocumentModel { Issues = [issue] });

        Assert.DoesNotContain("**Labels:**", output, StringComparison.Ordinal);
        Assert.DoesNotContain("**Assignees:**", output, StringComparison.Ordinal);
        Assert.DoesNotContain("**Milestone:**", output, StringComparison.Ordinal);
        Assert.DoesNotContain("**Estimate:**", output, StringComparison.Ordinal);
        Assert.DoesNotContain("**Environment**", output, StringComparison.Ordinal);
        Assert.DoesNotContain("**Comments**", output, StringComparison.Ordinal);
    }

    // MARK: - 9. Export / import stability

    [Fact]
    public void ExportImportExportIsStable()
    {
        var resolved = new Issue
        {
            Number = 3,
            Title = "Crash on launch",
            Type = IssueType.Bug,
            Priority = IssuePriority.Critical,
            Status = IssueStatus.Resolved,
            Labels = ["startup"],
            Assignees = ["dru"],
            Milestone = "v0.9",
            Area = "App",
            Estimate = 2,
            ReportedBy = "Sam",
            Reported = Date("2026-04-01"),
            Description = "It crashes.",
            StepsToReproduce = ["Launch", "Watch it die"],
            Environment = "iOS 27.0",
            Notes = "Stack trace attached.",
            Resolution = "Fixed the nil unwrap.",
            Comments = [new Comment { Author = "Dru", CreatedAt = Date("2026-04-02"), Body = "First line.\nSecond line." }]
        };
        var open = new Issue
        {
            Number = 4,
            Title = "Add dark mode",
            Type = IssueType.Feature,
            Priority = IssuePriority.Low,
            Area = "Views",
            ReportedBy = "Dru",
            Reported = Date("2026-04-05"),
            Description = "Please."
        };
        var model = new IssuesDocumentModel { Issues = [open, resolved] };

        var first = IssuesMarkdownSerializer.Export(model);
        var reimported = new LegacyMarkdownImporter().ImportDocument(first);
        var second = IssuesMarkdownSerializer.Export(reimported);

        Assert.Equal(first, second);
        Assert.Single(reimported.OpenIssues);
        Assert.Single(reimported.ResolvedIssues);
    }

    // MARK: - 10. Model helpers

    [Fact]
    public void NextNumberIsOnePastHighest()
    {
        var model = new IssuesDocumentModel
        {
            Issues = [new Issue { Number = 3 }, new Issue { Number = 7 }, new Issue { Number = 5 }]
        };
        Assert.Equal(8, model.NextNumber);
    }

    [Fact]
    public void NextNumberStartsAtOneWhenEmpty() => Assert.Equal(1, new IssuesDocumentModel().NextNumber);

    [Fact]
    public void IssueLookupByIdentity()
    {
        var model = new IssuesDocumentModel
        {
            Issues = [new Issue { Uuid = IssueA, Number = 1, Title = "Wanted" }, new Issue { Number = 2 }]
        };
        Assert.Equal("Wanted", model.Issue(IssueA)?.Title);
        Assert.Null(model.Issue(Guid.NewGuid()));
    }

    [Fact]
    public void MakeEmptyCarriesTheTemplatePreamble()
    {
        var model = IssuesDocumentModel.MakeEmpty();

        Assert.Empty(model.Issues);
        Assert.Equal(IssuesDocumentModel.SupportedSchemaVersion, model.SchemaVersion);
        Assert.Equal(ExportSettings.DefaultPreambleMarkdown, model.Export.PreambleMarkdown);

        var exported = IssuesMarkdownSerializer.Export(model);
        Assert.Contains("## Open", exported, StringComparison.Ordinal);
        Assert.Contains("## Resolved", exported, StringComparison.Ordinal);
    }

    // MARK: - 11. Deterministic encoding

    [Fact]
    public void EncodingIsDeterministic()
    {
        var model = MakeFullModel();
        Assert.Equal(IssuesJsonCoder.Encode(model), IssuesJsonCoder.Encode(model));
    }

    [Fact]
    public void EncodedKeysAreSorted()
    {
        var json = IssuesJsonCoder.EncodeToString(MakeFullModel());
        // Pretty-printed output indents top-level keys by exactly two spaces.
        var topLevelKeys = json.Split('\n')
            .Where(line => line.StartsWith("  \"", StringComparison.Ordinal))
            .Select(line => line[3..line.IndexOf('"', 3)])
            .ToList();

        Assert.Equal(
            ["export", "integrations", "issues", "labels", "milestones", "people", "project", "schemaVersion"],
            topLevelKeys);
        Assert.Equal(topLevelKeys.Order(StringComparer.Ordinal), topLevelKeys);
    }

    [Fact]
    public void EncodedJsonDoesNotEscapeSlashes()
    {
        var json = IssuesJsonCoder.EncodeToString(MakeFullModel());
        Assert.Contains("https://github.com/openbcm/i-have-issues/issues/412", json, StringComparison.Ordinal);
        Assert.DoesNotContain("\\/", json, StringComparison.Ordinal);
    }

    // MARK: - Free-form legacy format (Claude.ai-generated issues file)

    [Fact]
    public void FreeFormSampleImportsAllIssues()
    {
        var model = new LegacyMarkdownImporter().ImportDocument(Fixtures.Text("MLM-issues.md"));
        Assert.Equal(5, model.Issues.Count);
        Assert.Equal(5, model.OpenIssues.Count());
    }

    [Fact]
    public void FreeFormSampleFirstIssueFields()
    {
        var model = new LegacyMarkdownImporter().ImportDocument(Fixtures.Text("MLM-issues.md"));
        var issue = model.Issues[0];

        Assert.Equal(1, issue.Number);
        Assert.Equal("Login endpoint never triggers the configured account lockout", issue.Title);
        Assert.Equal(IssuePriority.High, issue.Priority);
        Assert.Equal(IssueStatus.Open, issue.Status);
        Assert.Equal("API (Auth)", issue.Area);
        Assert.NotEmpty(issue.Description);
        Assert.Equal(3, issue.StepsToReproduce.Count);
        Assert.NotEmpty(issue.Resolution);
    }

    [Fact]
    public void FreeFormSampleSeverityMapsToPriority()
    {
        var model = new LegacyMarkdownImporter().ImportDocument(Fixtures.Text("MLM-issues.md"));
        Assert.Equal(
            [IssuePriority.High, IssuePriority.Medium, IssuePriority.Medium, IssuePriority.Medium, IssuePriority.Low],
            model.Issues.Select(issue => issue.Priority));
    }

    [Fact]
    public void FreeFormSampleComponentMapsToArea()
    {
        var model = new LegacyMarkdownImporter().ImportDocument(Fixtures.Text("MLM-issues.md"));
        Assert.Equal("API (Auth)", model.Issues[0].Area);
        Assert.Equal("Web", model.Issues[4].Area);
    }

    // MARK: - 12. Cross-platform byte compatibility
    //
    // These have no counterpart in the Swift suite: they exist because a second implementation of
    // the format now writes the same files. A `.issues` document is committed to a repository and
    // edited from both apps, so a difference in whitespace, key order, or number formatting would
    // show up as a whole-file diff every time the document changed hands.

    [Fact]
    public void ReEncodingAnAppleWrittenDocumentReproducesItByteForByte()
    {
        var original = Fixtures.Bytes("Example.issues");
        var reencoded = IssuesJsonCoder.Encode(IssuesJsonCoder.Decode(original));

        // Compared as text so a failure reports the differing line rather than a byte offset.
        Assert.Equal(Encoding.UTF8.GetString(original), Encoding.UTF8.GetString(reencoded));
        Assert.Equal(original, reencoded);
    }

    /// <summary>
    /// The project's own issue file is edited by hand and by coding agents as well as by the app,
    /// so its key order drifts — the last entry carries <c>closedAt</c> and <c>resolutionKind</c>
    /// appended after <c>type</c>. Saving it must repair the ordering without altering meaning,
    /// and must then be stable.
    /// </summary>
    [Fact]
    public void ReEncodingAHandEditedDocumentNormalizesItWithoutChangingMeaning()
    {
        var original = Fixtures.Text("IHaveIssues-Issues.issues");
        var model = IssuesJsonCoder.Decode(original);

        var normalized = IssuesJsonCoder.EncodeToString(model);
        Assert.NotEqual(original, normalized);
        Assert.Equal(model, IssuesJsonCoder.Decode(normalized));

        // Saving again changes nothing: the first save is the only one that moves lines.
        Assert.Equal(normalized, IssuesJsonCoder.EncodeToString(IssuesJsonCoder.Decode(normalized)));

        var resolved = Assert.Single(model.Issues, issue => issue.IsResolved);
        Assert.Equal(IssuesKit.ResolutionKind.Fixed, resolved.ResolutionKind);
        Assert.NotNull(resolved.ClosedAt);
    }

    [Fact]
    public void EmptyContainersMatchTheReferenceEncoderLayout()
    {
        var json = IssuesJsonCoder.EncodeToString(new IssuesDocumentModel
        {
            Issues = [new Issue { Uuid = IssueA, Number = 1, Reported = ReferenceDate }]
        });

        // An empty object or array spans three lines, with a blank line between the brackets.
        Assert.Contains("\"integrations\" : {\n\n  },\n", json, StringComparison.Ordinal);
        Assert.Contains("\"labels\" : [\n\n      ],\n", json, StringComparison.Ordinal);
    }

    [Fact]
    public void EncodedScalarsUseTheReferenceFormats()
    {
        var json = IssuesJsonCoder.EncodeToString(new IssuesDocumentModel
        {
            Issues =
            [
                new Issue
                {
                    Uuid = IssueA,
                    Number = 1,
                    Estimate = 5,
                    Reported = ReferenceDate,
                    CreatedAt = ReferenceDate,
                    UpdatedAt = ReferenceDate
                }
            ]
        });

        Assert.Contains("\"uuid\" : \"0F5B9C7E-0000-4000-8000-000000000001\"", json, StringComparison.Ordinal);
        Assert.Contains("\"reported\" : \"2026-01-01T00:00:00Z\"", json, StringComparison.Ordinal);
        // A whole estimate loses its fractional part, matching the markdown exporter.
        Assert.Contains("\"estimate\" : 5,", json, StringComparison.Ordinal);
    }

    [Fact]
    public void SubSecondPrecisionIsTruncatedOnEncode()
    {
        var precise = new DateTimeOffset(2026, 5, 1, 12, 30, 45, 678, TimeSpan.Zero);
        var model = new IssuesDocumentModel { Issues = [new Issue { Number = 1, Reported = precise }] };

        var decoded = IssuesJsonCoder.Decode(IssuesJsonCoder.Encode(model));
        Assert.Equal(precise.AddMilliseconds(-678), decoded.Issues[0].Reported);
    }

    // MARK: - 13. Windows line endings
    //
    // Also absent from the Swift suite: a markdown file authored on Windows arrives with CRLF,
    // which the reference importer would leave as trailing carriage returns inside every value.

    [Fact]
    public void LegacyImportTreatsCrlfAndLfIdentically()
    {
        var importer = new LegacyMarkdownImporter();
        var fromLf = importer.ImportDocument(FullLegacyEntry);
        var fromCrlf = importer.ImportDocument(FullLegacyEntry.Replace("\n", "\r\n", StringComparison.Ordinal));

        Assert.Equal(fromLf.Export.PreambleMarkdown.Replace("\r", string.Empty, StringComparison.Ordinal),
            fromCrlf.Export.PreambleMarkdown);

        var expected = fromLf.Issues[0];
        var actual = fromCrlf.Issues[0];
        Assert.Equal(expected.Title, actual.Title);
        Assert.Equal(expected.Description, actual.Description);
        Assert.Equal(expected.Labels, actual.Labels);
        Assert.Equal(expected.StepsToReproduce, actual.StepsToReproduce);
        Assert.Equal(expected.Comments[0].Body, actual.Comments[0].Body);
        Assert.Equal(expected.Estimate, actual.Estimate);
        Assert.Equal(expected.Priority, actual.Priority);
    }

    [Fact]
    public void ExportAlwaysUsesLineFeeds()
    {
        var model = new IssuesDocumentModel { Issues = [new Issue { Number = 1, Title = "Only LF" }] };
        Assert.DoesNotContain('\r', IssuesMarkdownSerializer.Export(model));
    }
}
