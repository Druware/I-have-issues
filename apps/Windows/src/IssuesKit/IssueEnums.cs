namespace IssuesKit;

// Raw values are part of the on-disk `.issues` format and must never change. Display strings
// are presentation only — reword them freely without touching stored documents.

/// <summary>The kind of an issue entry.</summary>
public enum IssueType
{
    Bug,
    Feature,
    Task,
    Question
}

/// <summary>The urgency of an issue entry.</summary>
public enum IssuePriority
{
    Low,
    Medium,
    High,
    Critical
}

/// <summary>The lifecycle state of an issue entry.</summary>
public enum IssueStatus
{
    Open,
    InProgress,
    Blocked,
    Resolved
}

/// <summary>
/// Why an issue was closed. Maps onto GitHub's <c>state_reason</c> and Azure DevOps'
/// <i>Resolved Reason</i>. Unlike the other issue enums this one has no default: an
/// unrecognized raw value decodes to <c>null</c>, because inventing a close reason would
/// misreport why work stopped.
/// </summary>
public enum ResolutionKind
{
    Fixed,
    WontFix,
    Duplicate,
    CannotReproduce,
    ByDesign
}

/// <summary>How one issue relates to another within the same document.</summary>
public enum RelationKind
{
    Blocks,
    BlockedBy,
    DuplicateOf,
    RelatedTo,
    Parent,
    Child
}

/// <summary>
/// The external issue tracker a <see cref="RemoteLink"/> points at.
/// </summary>
/// <remarks>
/// Unlike the other issue enums this one does <b>not</b> fall back to a default. A provider is
/// sync identity: coercing an unrecognized provider to <see cref="GitHub"/> would point a GitHub
/// sync at whatever issue happens to carry the same identifier in the user's repository, and
/// re-saving would make the mislabelling permanent. An unknown value is therefore preserved
/// verbatim and re-encodes byte-identically.
/// <para>
/// Modelled as a raw-value wrapper rather than an enum because the set of values is open. The
/// constructor canonicalizes known raw values, so <c>new RemoteProvider("github")</c> and
/// <see cref="GitHub"/> are the same value and can never both appear in one document.
/// </para>
/// </remarks>
public readonly record struct RemoteProvider
{
    /// <summary>The value written to and read from the document.</summary>
    public string RawValue { get; }

    public RemoteProvider(string rawValue) => RawValue = rawValue;

    public static RemoteProvider GitHub { get; } = new("github");

    public static RemoteProvider AzureDevOps { get; } = new("azureDevOps");

    /// <summary>
    /// The providers this build can actually sync with — what UI pickers should offer.
    /// Deliberately not every possible value: an unknown provider has no finite set.
    /// </summary>
    public static IReadOnlyList<RemoteProvider> SelectableCases { get; } = [GitHub, AzureDevOps];

    /// <summary>
    /// The human-facing name. An unknown provider shows its raw value rather than borrowing a
    /// known provider's name.
    /// </summary>
    public string DisplayName => RawValue switch
    {
        "github" => "GitHub",
        "azureDevOps" => "Azure DevOps",
        _ => RawValue
    };

    /// <summary>
    /// Whether this link is specifically GitHub. Sync code must gate on this — an unknown
    /// provider is never GitHub, however similar its raw value looks.
    /// </summary>
    public bool IsGitHub => RawValue == "github";

    /// <summary>Whether this link is specifically Azure DevOps.</summary>
    public bool IsAzureDevOps => RawValue == "azureDevOps";

    public override string ToString() => RawValue;
}

/// <summary>
/// Raw-value and display-name mapping for the issue enums.
/// </summary>
/// <remarks>
/// Every <c>From*</c> overload that takes a raw value substitutes the enum's documented default
/// when the value is absent or unrecognized, so a document written by a newer build still opens
/// in an older one. <see cref="ResolutionKindFromRaw"/> is the deliberate exception.
/// </remarks>
public static class IssueEnums
{
    // MARK: - Defaults

    public const IssueType DefaultType = IssueType.Task;
    public const IssuePriority DefaultPriority = IssuePriority.Medium;
    public const IssueStatus DefaultStatus = IssueStatus.Open;

    /// <summary>Chosen because it is the weakest claim the format can make about two issues.</summary>
    public const RelationKind DefaultRelationKind = RelationKind.RelatedTo;

    // MARK: - Raw values

    public static string RawValue(this IssueType value) => value switch
    {
        IssueType.Bug => "bug",
        IssueType.Feature => "feature",
        IssueType.Task => "task",
        IssueType.Question => "question",
        _ => DefaultType.RawValue()
    };

    public static string RawValue(this IssuePriority value) => value switch
    {
        IssuePriority.Low => "low",
        IssuePriority.Medium => "medium",
        IssuePriority.High => "high",
        IssuePriority.Critical => "critical",
        _ => DefaultPriority.RawValue()
    };

    public static string RawValue(this IssueStatus value) => value switch
    {
        IssueStatus.Open => "open",
        IssueStatus.InProgress => "inProgress",
        IssueStatus.Blocked => "blocked",
        IssueStatus.Resolved => "resolved",
        _ => DefaultStatus.RawValue()
    };

    public static string RawValue(this ResolutionKind value) => value switch
    {
        ResolutionKind.Fixed => "fixed",
        ResolutionKind.WontFix => "wontFix",
        ResolutionKind.Duplicate => "duplicate",
        ResolutionKind.CannotReproduce => "cannotReproduce",
        ResolutionKind.ByDesign => "byDesign",
        _ => "fixed"
    };

    public static string RawValue(this RelationKind value) => value switch
    {
        RelationKind.Blocks => "blocks",
        RelationKind.BlockedBy => "blockedBy",
        RelationKind.DuplicateOf => "duplicateOf",
        RelationKind.RelatedTo => "relatedTo",
        RelationKind.Parent => "parent",
        RelationKind.Child => "child",
        _ => DefaultRelationKind.RawValue()
    };

    // MARK: - Display names

    public static string DisplayName(this IssueType value) => value switch
    {
        IssueType.Bug => "Bug",
        IssueType.Feature => "Feature",
        IssueType.Task => "Task",
        IssueType.Question => "Question",
        _ => DefaultType.DisplayName()
    };

    public static string DisplayName(this IssuePriority value) => value switch
    {
        IssuePriority.Low => "Low",
        IssuePriority.Medium => "Medium",
        IssuePriority.High => "High",
        IssuePriority.Critical => "Critical",
        _ => DefaultPriority.DisplayName()
    };

    public static string DisplayName(this IssueStatus value) => value switch
    {
        IssueStatus.Open => "Open",
        IssueStatus.InProgress => "In Progress",
        IssueStatus.Blocked => "Blocked",
        IssueStatus.Resolved => "Resolved",
        _ => DefaultStatus.DisplayName()
    };

    public static string DisplayName(this ResolutionKind value) => value switch
    {
        ResolutionKind.Fixed => "Fixed",
        ResolutionKind.WontFix => "Won't Fix",
        ResolutionKind.Duplicate => "Duplicate",
        ResolutionKind.CannotReproduce => "Cannot Reproduce",
        ResolutionKind.ByDesign => "By Design",
        _ => "Fixed"
    };

    public static string DisplayName(this RelationKind value) => value switch
    {
        RelationKind.Blocks => "Blocks",
        RelationKind.BlockedBy => "Blocked By",
        RelationKind.DuplicateOf => "Duplicate Of",
        RelationKind.RelatedTo => "Related To",
        RelationKind.Parent => "Parent",
        RelationKind.Child => "Child",
        _ => DefaultRelationKind.DisplayName()
    };

    // MARK: - Raw value parsing

    /// <summary>Reads a persisted <see cref="IssueType"/>, substituting the default when unrecognized.</summary>
    public static IssueType IssueTypeFromRaw(string? raw) =>
        FromRaw(raw, DefaultType, RawValue);

    /// <summary>Reads a persisted <see cref="IssuePriority"/>, substituting the default when unrecognized.</summary>
    public static IssuePriority IssuePriorityFromRaw(string? raw) =>
        FromRaw(raw, DefaultPriority, RawValue);

    /// <summary>Reads a persisted <see cref="IssueStatus"/>, substituting the default when unrecognized.</summary>
    public static IssueStatus IssueStatusFromRaw(string? raw) =>
        FromRaw(raw, DefaultStatus, RawValue);

    /// <summary>Reads a persisted <see cref="RelationKind"/>, substituting the default when unrecognized.</summary>
    public static RelationKind RelationKindFromRaw(string? raw) =>
        FromRaw(raw, DefaultRelationKind, RawValue);

    /// <summary>
    /// Reads a persisted <see cref="ResolutionKind"/>. Returns <c>null</c> rather than a default
    /// when the value is absent or unrecognized — inventing a close reason would misreport why
    /// work stopped.
    /// </summary>
    public static ResolutionKind? ResolutionKindFromRaw(string? raw)
    {
        if (raw is null)
        {
            return null;
        }

        foreach (var candidate in Enum.GetValues<ResolutionKind>())
        {
            if (candidate.RawValue() == raw)
            {
                return candidate;
            }
        }

        return null;
    }

    // MARK: - Display name parsing

    /// <summary>
    /// Creates a case from its human-facing display string, matched case-insensitively.
    /// Used by <see cref="LegacyMarkdownImporter"/> to read documents that persisted display strings.
    /// </summary>
    public static IssueType? IssueTypeFromDisplayName(string? text) =>
        FromDisplayName<IssueType>(text, DisplayName);

    /// <inheritdoc cref="IssueTypeFromDisplayName"/>
    public static IssuePriority? IssuePriorityFromDisplayName(string? text) =>
        FromDisplayName<IssuePriority>(text, DisplayName);

    /// <inheritdoc cref="IssueTypeFromDisplayName"/>
    public static IssueStatus? IssueStatusFromDisplayName(string? text) =>
        FromDisplayName<IssueStatus>(text, DisplayName);

    /// <inheritdoc cref="IssueTypeFromDisplayName"/>
    public static ResolutionKind? ResolutionKindFromDisplayName(string? text) =>
        FromDisplayName<ResolutionKind>(text, DisplayName);

    /// <inheritdoc cref="IssueTypeFromDisplayName"/>
    public static RelationKind? RelationKindFromDisplayName(string? text) =>
        FromDisplayName<RelationKind>(text, DisplayName);

    // MARK: - Shared lookups

    private static T FromRaw<T>(string? raw, T fallback, Func<T, string> rawValue) where T : struct, Enum
    {
        if (raw is null)
        {
            return fallback;
        }

        foreach (var candidate in Enum.GetValues<T>())
        {
            if (rawValue(candidate) == raw)
            {
                return candidate;
            }
        }

        return fallback;
    }

    private static T? FromDisplayName<T>(string? text, Func<T, string> displayName) where T : struct, Enum
    {
        if (text is null)
        {
            return null;
        }

        var needle = text.Trim();
        foreach (var candidate in Enum.GetValues<T>())
        {
            if (string.Equals(displayName(candidate), needle, StringComparison.OrdinalIgnoreCase))
            {
                return candidate;
            }
        }

        return null;
    }
}
