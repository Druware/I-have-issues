using System.Globalization;

namespace IssuesKit;

/// <summary>
/// Exports an <see cref="IssuesDocumentModel"/> as markdown.
/// </summary>
/// <remarks>
/// Markdown is an <b>export</b> format, not the source of truth — the <c>.issues</c> JSON is (see
/// <see cref="Json.IssuesJsonCoder"/>). The export keeps the shape of the original hand-authored
/// template so generated files still read naturally in a repository:
/// <see cref="ExportSettings.PreambleMarkdown"/> verbatim, then <c>## Open</c> and
/// <c>## Resolved</c> sections built from <see cref="Issue.IsResolved"/>.
/// <para>
/// Fields the template has no slot for (stable UUIDs, timestamps, relations, remote links) are
/// omitted. <see cref="LegacyMarkdownImporter"/> can read back everything this emits.
/// </para>
/// <para>
/// Line endings are LF, matching the reference implementation, so an exported file does not churn
/// in git when it moves between platforms.
/// </para>
/// </remarks>
public static class IssuesMarkdownSerializer
{
    private const string EmDash = "—";

    /// <summary>Renders a model as a complete markdown document.</summary>
    public static string Export(IssuesDocumentModel model)
    {
        ArgumentNullException.ThrowIfNull(model);

        var openBody = SectionBody(model.OpenIssues, "_No open issues._");
        var resolvedBody = SectionBody(model.ResolvedIssues, "_No resolved issues._");

        return model.Export.PreambleMarkdown
            + "## Open\n\n"
            + openBody
            + "\n\n---\n\n## Resolved\n\n"
            + resolvedBody
            + "\n";
    }

    // MARK: - Sections

    private static string SectionBody(IEnumerable<Issue> issues, string emptyText)
    {
        var entries = issues.Select(ExportIssue).ToList();
        return entries.Count == 0 ? emptyText : string.Join("\n\n", entries);
    }

    private static string ExportIssue(Issue issue)
    {
        var lines = new List<string>
        {
            $"### #{FormatNumber(issue.Number)} {EmDash} {issue.Title}",
            string.Empty,
            $"- **Type:** {issue.Type.DisplayName()}",
            $"- **Priority:** {issue.Priority.DisplayName()}",
            $"- **Status:** {issue.Status.DisplayName()}",
            $"- **Reported:** {IssueDate.String(issue.Reported)}",
            $"- **Reported by:** {issue.ReportedBy}",
            $"- **Area:** {issue.Area}"
        };

        AppendMetadata(lines, "Labels", string.Join(", ", issue.Labels));
        AppendMetadata(lines, "Assignees", string.Join(", ", issue.Assignees));
        AppendMetadata(lines, "Milestone", issue.Milestone ?? string.Empty);
        if (issue.Estimate is { } estimate)
        {
            lines.Add($"- **Estimate:** {FormatEstimate(estimate)}");
        }

        AppendTextSection(lines, "Description", issue.Description);
        AppendSteps(lines, issue.StepsToReproduce);
        AppendTextSection(lines, "Environment", issue.Environment);
        AppendTextSection(lines, "Notes / Investigation", issue.Notes);
        AppendTextSection(lines, "Resolution", issue.Resolution);
        AppendComments(lines, issue.Comments);

        return string.Join("\n", lines);
    }

    // MARK: - Pieces

    private static void AppendMetadata(List<string> lines, string key, string value)
    {
        if (value.Length == 0)
        {
            return;
        }

        lines.Add($"- **{key}:** {value}");
    }

    private static void AppendTextSection(List<string> lines, string header, string content)
    {
        if (content.Length == 0)
        {
            return;
        }

        lines.Add(string.Empty);
        lines.Add($"**{header}**");
        lines.Add(string.Empty);
        lines.AddRange(content.Split('\n'));
    }

    private static void AppendSteps(List<string> lines, List<string> steps)
    {
        if (steps.Count == 0)
        {
            return;
        }

        lines.Add(string.Empty);
        lines.Add("**Steps to reproduce**");
        lines.Add(string.Empty);
        for (var index = 0; index < steps.Count; index++)
        {
            lines.Add($"{index + 1}. {steps[index]}");
        }
    }

    /// <summary>
    /// Emits one bullet per comment. Continuation lines are indented two spaces, which is how
    /// <see cref="LegacyMarkdownImporter"/> recognizes them as belonging to the preceding comment.
    /// </summary>
    private static void AppendComments(List<string> lines, List<Comment> comments)
    {
        if (comments.Count == 0)
        {
            return;
        }

        lines.Add(string.Empty);
        lines.Add("**Comments**");
        lines.Add(string.Empty);
        foreach (var comment in comments)
        {
            var bodyLines = comment.Body.Split('\n');
            var date = IssueDate.String(comment.CreatedAt);
            lines.Add($"- **{comment.Author}** ({date}): {bodyLines[0]}");
            for (var index = 1; index < bodyLines.Length; index++)
            {
                lines.Add("  " + bodyLines[index]);
            }
        }
    }

    // MARK: - Formatting

    private static string FormatNumber(int number) => number.ToString("D3", CultureInfo.InvariantCulture);

    /// <summary>Renders whole estimates without a decimal point so <c>3</c> survives an export/import cycle.</summary>
    private static string FormatEstimate(double estimate) =>
        estimate.ToString("R", CultureInfo.InvariantCulture);
}
