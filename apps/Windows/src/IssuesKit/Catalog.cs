namespace IssuesKit;

/// <summary>
/// A label the project uses, cached in the document so pickers work without network access.
/// </summary>
public sealed record LabelDefinition
{
    /// <summary>The value used in <see cref="Issue.Labels"/>.</summary>
    public string Name { get; set; } = string.Empty;

    /// <summary>A <c>RRGGBB</c> or <c>#RRGGBB</c> colour, or <c>null</c> when the label has none of its own.</summary>
    public string? ColorHex { get; set; }

    public string Description { get; set; } = string.Empty;
}

/// <summary>
/// A milestone the project uses, cached in the document so pickers work without network access.
/// </summary>
public sealed record Milestone
{
    /// <summary>The value used in <see cref="Issue.Milestone"/>.</summary>
    public string Name { get; set; } = string.Empty;

    public DateTimeOffset? DueOn { get; set; }

    public bool IsClosed { get; set; }
}

/// <summary>
/// Someone who can report or be assigned an issue, cached in the document so pickers work
/// without network access.
/// </summary>
public sealed record Person
{
    /// <summary>The tracker handle used in <see cref="Issue.Assignees"/> and <see cref="Issue.ReportedBy"/>.</summary>
    public string Handle { get; set; } = string.Empty;

    public string DisplayName { get; set; } = string.Empty;

    public string Email { get; set; } = string.Empty;
}
