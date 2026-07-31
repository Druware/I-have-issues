using System.Globalization;

namespace IssuesKit;

/// <summary>
/// A single issue-tracker entry stored in an <c>.issues</c> document.
/// </summary>
/// <remarks>
/// Identity is split in two on purpose. <see cref="Uuid"/> is the stable sync identity used by
/// relations and remote links and never changes; <see cref="Number"/> is the human display
/// number (<c>#007</c>) and may be renumbered without breaking anything.
/// <para>
/// Every property carries a default, so a fresh entry is one line:
/// <c>new Issue { Number = model.NextNumber, Title = "Login button does nothing" }</c>.
/// </para>
/// </remarks>
public sealed class Issue : IEquatable<Issue>
{
    /// <summary>The stable sync identity, independent of <see cref="Number"/>.</summary>
    public Guid Uuid { get; set; } = Guid.NewGuid();

    /// <summary>The human display number rendered as <c>#NNN</c>.</summary>
    public int Number { get; set; }

    public string Title { get; set; } = string.Empty;
    public IssueType Type { get; set; } = IssueEnums.DefaultType;
    public IssuePriority Priority { get; set; } = IssueEnums.DefaultPriority;
    public IssueStatus Status { get; set; } = IssueEnums.DefaultStatus;

    /// <summary>Why the issue was closed, or <c>null</c> while it is still open.</summary>
    public ResolutionKind? ResolutionKind { get; set; }

    public List<string> Labels { get; set; } = [];
    public List<string> Assignees { get; set; } = [];

    /// <summary>The name of a <see cref="Milestone"/> in the document catalog, or <c>null</c>.</summary>
    public string? Milestone { get; set; }

    public string Area { get; set; } = string.Empty;

    /// <summary>The effort estimate in whatever unit the project uses (points, hours), or <c>null</c>.</summary>
    public double? Estimate { get; set; }

    public string ReportedBy { get; set; } = string.Empty;

    /// <summary>The reported date, normalized to the start of its day in the document's fixed calendar.</summary>
    public DateTimeOffset Reported { get; set; } = DateTimeOffset.UtcNow;

    public DateTimeOffset CreatedAt { get; set; } = DateTimeOffset.UtcNow;
    public DateTimeOffset UpdatedAt { get; set; } = DateTimeOffset.UtcNow;

    /// <summary>When the issue was closed, or <c>null</c> while it is still open.</summary>
    public DateTimeOffset? ClosedAt { get; set; }

    public string Description { get; set; } = string.Empty;
    public List<string> StepsToReproduce { get; set; } = [];
    public string Environment { get; set; } = string.Empty;

    /// <summary>Investigation notes. Legacy import parks unrecognized sections here.</summary>
    public string Notes { get; set; } = string.Empty;

    public string Resolution { get; set; } = string.Empty;

    public List<Comment> Comments { get; set; } = [];
    public List<Relation> Relations { get; set; } = [];
    public List<RemoteLink> RemoteLinks { get; set; } = [];

    /// <summary>Whether the entry belongs in the <c>## Resolved</c> section on export.</summary>
    public bool IsResolved => Status == IssueStatus.Resolved;

    /// <summary>
    /// The <c>#NNN</c> display form of <see cref="Number"/>, zero-padded to three digits.
    /// This formats the display number, never <see cref="Uuid"/> — the UUID is sync identity,
    /// not something a person reads.
    /// </summary>
    public string DisplayNumber => "#" + Number.ToString("D3", CultureInfo.InvariantCulture);

    /// <summary>
    /// A deep copy, so an editor can mutate a draft and discard it without touching the document.
    /// </summary>
    public Issue Copy() => new()
    {
        Uuid = Uuid,
        Number = Number,
        Title = Title,
        Type = Type,
        Priority = Priority,
        Status = Status,
        ResolutionKind = ResolutionKind,
        Labels = [.. Labels],
        Assignees = [.. Assignees],
        Milestone = Milestone,
        Area = Area,
        Estimate = Estimate,
        ReportedBy = ReportedBy,
        Reported = Reported,
        CreatedAt = CreatedAt,
        UpdatedAt = UpdatedAt,
        ClosedAt = ClosedAt,
        Description = Description,
        StepsToReproduce = [.. StepsToReproduce],
        Environment = Environment,
        Notes = Notes,
        Resolution = Resolution,
        Comments = ModelEquality.CopyOf(Comments, c => c.Copy()),
        Relations = [.. Relations],
        RemoteLinks = [.. RemoteLinks]
    };

    public bool Equals(Issue? other) =>
        other is not null
        && Uuid == other.Uuid
        && Number == other.Number
        && Title == other.Title
        && Type == other.Type
        && Priority == other.Priority
        && Status == other.Status
        && ResolutionKind == other.ResolutionKind
        && ModelEquality.ListEquals(Labels, other.Labels)
        && ModelEquality.ListEquals(Assignees, other.Assignees)
        && Milestone == other.Milestone
        && Area == other.Area
        && Nullable.Equals(Estimate, other.Estimate)
        && ReportedBy == other.ReportedBy
        && Reported == other.Reported
        && CreatedAt == other.CreatedAt
        && UpdatedAt == other.UpdatedAt
        && Nullable.Equals(ClosedAt, other.ClosedAt)
        && Description == other.Description
        && ModelEquality.ListEquals(StepsToReproduce, other.StepsToReproduce)
        && Environment == other.Environment
        && Notes == other.Notes
        && Resolution == other.Resolution
        && ModelEquality.ListEquals(Comments, other.Comments)
        && ModelEquality.ListEquals(Relations, other.Relations)
        && ModelEquality.ListEquals(RemoteLinks, other.RemoteLinks);

    public override bool Equals(object? obj) => Equals(obj as Issue);

    public override int GetHashCode() => HashCode.Combine(Uuid, Number, Title);
}

/// <summary>A discussion entry attached to an <see cref="Issue"/>.</summary>
public sealed class Comment : IEquatable<Comment>
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public string Author { get; set; } = string.Empty;
    public DateTimeOffset CreatedAt { get; set; } = DateTimeOffset.UtcNow;
    public string Body { get; set; } = string.Empty;

    public Comment Copy() => new()
    {
        Id = Id,
        Author = Author,
        CreatedAt = CreatedAt,
        Body = Body
    };

    public bool Equals(Comment? other) =>
        other is not null
        && Id == other.Id
        && Author == other.Author
        && CreatedAt == other.CreatedAt
        && Body == other.Body;

    public override bool Equals(object? obj) => Equals(obj as Comment);

    public override int GetHashCode() => HashCode.Combine(Id, Author, CreatedAt, Body);
}

/// <summary>A typed link from one issue to another within the same document.</summary>
/// <param name="Kind">How the two issues relate.</param>
/// <param name="IssueId">The <see cref="Issue.Uuid"/> of the other issue.</param>
public readonly record struct Relation(RelationKind Kind, Guid IssueId)
{
    public Relation(Guid issueId) : this(IssueEnums.DefaultRelationKind, issueId)
    {
    }
}

/// <summary>A pointer from a local issue to its counterpart in an external tracker.</summary>
public readonly record struct RemoteLink
{
    public RemoteProvider Provider { get; init; }

    /// <summary>The provider's identifier — a GitHub issue number, or an Azure DevOps work item id.</summary>
    public string Identifier { get; init; }

    public Uri? Url { get; init; }

    /// <summary>When this app last pushed or pulled the remote item.</summary>
    public DateTimeOffset? LastSyncedAt { get; init; }

    /// <summary>The remote item's own last-modified timestamp, for conflict detection.</summary>
    public DateTimeOffset? RemoteUpdatedAt { get; init; }

    public RemoteLink()
    {
        Provider = RemoteProvider.GitHub;
        Identifier = string.Empty;
    }

    public RemoteLink(RemoteProvider provider, string identifier) : this()
    {
        Provider = provider;
        Identifier = identifier;
    }
}
