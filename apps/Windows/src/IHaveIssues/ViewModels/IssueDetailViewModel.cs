using System.Globalization;
using IssuesKit;

namespace IHaveIssues.ViewModels;

/// <summary>One label/value line in the detail header's metadata grid.</summary>
public sealed record MetadataRow(string Label, string Value);

/// <summary>One titled block of markdown body text.</summary>
public sealed record BodySection(string Title, string Text);

/// <summary>One entry of the steps-to-reproduce list, pre-numbered for display.</summary>
public sealed record NumberedStep(string Ordinal, string Text);

/// <summary>One comment, with its date already formatted as the document's day.</summary>
public sealed record CommentRow(string Author, string Date, string Body);

/// <summary>One relation, resolved to the issue it points at.</summary>
public sealed record RelationRow(string Kind, string Target);

/// <summary>One remote link. <see cref="Url"/> is <c>null</c> when the link carries no address.</summary>
public sealed record RemoteLinkRow(string Label, Uri? Url);

/// <summary>
/// A read-only presentation of a single issue, shaped so the detail view is plain data binding.
/// </summary>
/// <remarks>
/// The document model comes along so relations can be resolved to the issues they point at;
/// nothing here mutates it. Optional rows are omitted rather than shown blank, matching the Apple
/// build's detail view.
/// </remarks>
public sealed class IssueDetailViewModel
{
    public IssueDetailViewModel(Issue issue, IssuesDocumentModel model)
    {
        ArgumentNullException.ThrowIfNull(issue);
        ArgumentNullException.ThrowIfNull(model);

        Issue = issue;
        Title = string.IsNullOrWhiteSpace(issue.Title) ? "Untitled" : issue.Title;
        Reported = IssueDate.String(issue.Reported);

        OptionalMetadata = BuildMetadata(issue);
        Sections = BuildSections(issue);

        Steps =
        [
            .. issue.StepsToReproduce.Select((step, index) =>
                new NumberedStep($"{(index + 1).ToString(CultureInfo.InvariantCulture)}.", step))
        ];

        Comments =
        [
            .. issue.Comments.Select(comment => new CommentRow(
                string.IsNullOrWhiteSpace(comment.Author) ? "Unknown" : comment.Author,
                IssueDate.String(comment.CreatedAt),
                comment.Body))
        ];

        Relations =
        [
            .. issue.Relations.Select(relation => new RelationRow(
                relation.Kind.DisplayName(),
                DescribeTarget(model, relation)))
        ];

        RemoteLinks =
        [
            .. issue.RemoteLinks.Select(link =>
                new RemoteLinkRow($"{link.Provider.DisplayName} {link.Identifier}".Trim(), link.Url))
        ];
    }

    /// <summary>The issue itself, for the rows whose icon and colour come from an enum.</summary>
    public Issue Issue { get; }

    public string DisplayNumber => Issue.DisplayNumber;

    public string Title { get; }

    public string Reported { get; }

    public bool HasResolutionKind => Issue.ResolutionKind is not null;

    public string ResolutionKindText => Issue.ResolutionKind?.DisplayName() ?? string.Empty;

    public IReadOnlyList<MetadataRow> OptionalMetadata { get; }

    /// <summary>
    /// Environment, notes, and resolution. Description and steps are separate so the view can keep
    /// the Apple build's order: description, steps to reproduce, then the rest.
    /// </summary>
    public IReadOnlyList<BodySection> Sections { get; }

    public string Description => Issue.Description;

    public IReadOnlyList<NumberedStep> Steps { get; }

    public IReadOnlyList<CommentRow> Comments { get; }

    public IReadOnlyList<RelationRow> Relations { get; }

    public IReadOnlyList<RemoteLinkRow> RemoteLinks { get; }

    private static List<MetadataRow> BuildMetadata(Issue issue)
    {
        var rows = new List<MetadataRow>();

        Add("Reported by", issue.ReportedBy);
        Add("Area", issue.Area);
        Add("Labels", string.Join(", ", issue.Labels));
        Add("Assignees", string.Join(", ", issue.Assignees));
        Add("Milestone", issue.Milestone ?? string.Empty);
        if (issue.Estimate is { } estimate)
        {
            Add("Estimate", estimate.ToString("R", CultureInfo.InvariantCulture));
        }

        return rows;

        void Add(string label, string value)
        {
            if (value.Length > 0)
            {
                rows.Add(new MetadataRow(label, value));
            }
        }
    }

    private static List<BodySection> BuildSections(Issue issue)
    {
        var sections = new List<BodySection>();

        Add("Environment", issue.Environment);
        Add("Notes / Investigation", issue.Notes);
        Add("Resolution", issue.Resolution);

        return sections;

        void Add(string title, string text)
        {
            if (text.Length > 0)
            {
                sections.Add(new BodySection(title, text));
            }
        }
    }

    private static string DescribeTarget(IssuesDocumentModel model, Relation relation)
    {
        var related = model.Issue(relation.IssueId);
        if (related is null)
        {
            return "Missing issue";
        }

        var title = string.IsNullOrWhiteSpace(related.Title) ? "Untitled" : related.Title;
        return $"{related.DisplayNumber} {title}";
    }
}
