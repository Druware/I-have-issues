namespace IssuesKit;

/// <summary>
/// The full contents of an <c>.issues</c> document: project identity, tracker integration
/// coordinates, the label/milestone/people catalogs, export settings, and the issues.
/// </summary>
/// <remarks>
/// This is the source of truth. Markdown is produced from it by
/// <see cref="IssuesMarkdownSerializer"/> and read into it — once, from legacy files — by
/// <see cref="LegacyMarkdownImporter"/>.
/// </remarks>
public sealed class IssuesDocumentModel : IEquatable<IssuesDocumentModel>
{
    /// <summary>The highest schema version this build can read. Bump it only alongside a migration.</summary>
    public const int SupportedSchemaVersion = 1;

    /// <summary>The schema version the document was written with.</summary>
    public int SchemaVersion { get; set; } = SupportedSchemaVersion;

    public ProjectInfo Project { get; set; } = new();

    public IntegrationSettings Integrations { get; set; } = new();

    /// <summary>The label catalog, so pickers work offline.</summary>
    public List<LabelDefinition> Labels { get; set; } = [];

    /// <summary>The milestone catalog, so pickers work offline.</summary>
    public List<Milestone> Milestones { get; set; } = [];

    /// <summary>The people catalog, so pickers work offline.</summary>
    public List<Person> People { get; set; } = [];

    public ExportSettings Export { get; set; } = new();

    public List<Issue> Issues { get; set; } = [];

    // MARK: - Helpers

    /// <summary>The next display number: one past the highest existing number, or <c>1</c> when empty.</summary>
    public int NextNumber => (Issues.Count == 0 ? 0 : Issues.Max(issue => issue.Number)) + 1;

    /// <summary>The entries that belong under <c>## Open</c>, in document order.</summary>
    public IEnumerable<Issue> OpenIssues => Issues.Where(issue => !issue.IsResolved);

    /// <summary>The entries that belong under <c>## Resolved</c>, in document order.</summary>
    public IEnumerable<Issue> ResolvedIssues => Issues.Where(issue => issue.IsResolved);

    /// <summary>The issue with the given stable identity, or <c>null</c> when there is no such issue.</summary>
    public Issue? Issue(Guid id) => Issues.FirstOrDefault(issue => issue.Uuid == id);

    /// <summary>An empty document at the current schema version, used for new documents.</summary>
    public static IssuesDocumentModel MakeEmpty() => new();

    /// <summary>A deep copy, so an editor can mutate a draft and discard it without touching the document.</summary>
    public IssuesDocumentModel Copy() => new()
    {
        SchemaVersion = SchemaVersion,
        Project = Project with { },
        Integrations = Integrations.Copy(),
        Labels = ModelEquality.CopyOf(Labels, label => label with { }),
        Milestones = ModelEquality.CopyOf(Milestones, milestone => milestone with { }),
        People = ModelEquality.CopyOf(People, person => person with { }),
        Export = Export with { },
        Issues = ModelEquality.CopyOf(Issues, issue => issue.Copy())
    };

    public bool Equals(IssuesDocumentModel? other) =>
        other is not null
        && SchemaVersion == other.SchemaVersion
        && Project == other.Project
        && Integrations == other.Integrations
        && ModelEquality.ListEquals(Labels, other.Labels)
        && ModelEquality.ListEquals(Milestones, other.Milestones)
        && ModelEquality.ListEquals(People, other.People)
        && Export == other.Export
        && ModelEquality.ListEquals(Issues, other.Issues);

    public override bool Equals(object? obj) => Equals(obj as IssuesDocumentModel);

    public override int GetHashCode() => HashCode.Combine(SchemaVersion, Project, Issues.Count);
}

/// <summary>The identity of the project the document tracks.</summary>
public sealed record ProjectInfo
{
    /// <summary>Stable project identity.</summary>
    public Guid Id { get; set; } = Guid.NewGuid();

    public string Name { get; set; } = string.Empty;

    public string Summary { get; set; } = string.Empty;
}

/// <summary>How the document is rendered when exported to markdown.</summary>
public sealed record ExportSettings
{
    /// <summary>Everything emitted before the <c>## Open</c> heading — title, how-to text, and template.</summary>
    public string PreambleMarkdown { get; set; } = DefaultPreambleMarkdown;

    /// <summary>The preamble a new document exports with.</summary>
    public const string DefaultPreambleMarkdown = """
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

        """;
}
