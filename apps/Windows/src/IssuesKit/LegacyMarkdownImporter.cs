using System.Globalization;

namespace IssuesKit;

/// <summary>
/// One-way importer for the legacy markdown issues format.
/// </summary>
/// <remarks>
/// Markdown was once the source of truth; it is now an export target (see
/// <see cref="IssuesMarkdownSerializer"/>) and this type is the migration path for files written
/// before the <c>.issues</c> JSON format existed. It also reads back everything the exporter emits.
/// <para>
/// Importing is deliberately tolerant: missing metadata falls back to the enum defaults, a
/// malformed entry never aborts the whole import, and body content under an unrecognized
/// <c>**Header**</c> is appended to <see cref="Issue.Notes"/> rather than dropped. Imported issues
/// get fresh <see cref="Issue.Uuid"/> values and <c>CreatedAt</c>/<c>UpdatedAt</c> set to the
/// import time, because the markdown format records neither.
/// </para>
/// <para>
/// Two layouts are accepted:
/// <list type="bullet">
/// <item><b>Standard</b>: has a <c>## Open</c> or <c>## Known Issues</c> section; issues use
/// <c>### #NNN — Title</c>.</item>
/// <item><b>Free-form</b>: uses category headings like <c>## Bugs</c> / <c>## Enhancements</c>;
/// issues use plain <c>### Title</c> or <c>### ~~Title~~ ✓ Fixed</c> headings (no number
/// required).</item>
/// </list>
/// </para>
/// </remarks>
public sealed class LegacyMarkdownImporter
{
    /// <summary>Imports a legacy markdown document.</summary>
    /// <param name="markdown">The document text. CRLF and LF line endings are both accepted.</param>
    /// <returns>A model whose <see cref="ExportSettings.PreambleMarkdown"/> is the file's preamble.</returns>
    /// <exception cref="MissingOpenSectionException">
    /// The document has no level-2 headings at all and therefore cannot be interpreted as either layout.
    /// </exception>
    public IssuesDocumentModel ImportDocument(string markdown)
    {
        ArgumentNullException.ThrowIfNull(markdown);

        var lines = SplitLines(markdown);
        var now = DateTimeOffset.UtcNow;

        var openIndex = lines.FindIndex(line =>
            IsLevel2Heading(line, "Open") || IsLevel2Heading(line, "Known Issues"));

        if (openIndex >= 0)
        {
            var preamble = string.Join("\n", lines.Take(openIndex)) + (openIndex > 0 ? "\n" : string.Empty);
            return new IssuesDocumentModel
            {
                Export = new ExportSettings { PreambleMarkdown = preamble },
                Issues = CollectIssues(lines, openIndex + 1, freeForm: false, now)
            };
        }

        // Fall back to free-form parsing when the file uses its own category headings
        // (e.g. ## Bugs / ## Enhancements) rather than the canonical ## Open section.
        if (!lines.Any(IsLevel2Heading))
        {
            throw new MissingOpenSectionException();
        }

        return new IssuesDocumentModel
        {
            Export = new ExportSettings { PreambleMarkdown = string.Empty },
            Issues = CollectIssues(lines, 0, freeForm: true, now)
        };
    }

    /// <summary>
    /// Splits on LF and drops a trailing CR, so a file saved with Windows line endings parses the
    /// same as one saved with Unix endings.
    /// </summary>
    private static List<string> SplitLines(string markdown) =>
        [.. markdown.Split('\n').Select(line => line.EndsWith('\r') ? line[..^1] : line)];

    // MARK: - Line classification

    /// <summary>Whether the line is any level-2 (<c>## </c>) heading, used to terminate an issue block.</summary>
    private static bool IsLevel2Heading(string line) => line.Trim().StartsWith("## ", StringComparison.Ordinal);

    /// <summary>Whether the line is the specific <c>## &lt;name&gt;</c> heading.</summary>
    private static bool IsLevel2Heading(string line, string name) => line.Trim() == $"## {name}";

    /// <summary>
    /// Whether the line is a thematic break (<c>---</c> or longer), which separates document sections.
    /// </summary>
    private static bool IsHorizontalRule(string line)
    {
        var trimmed = line.Trim();
        return trimmed.Length >= 3 && trimmed.All(character => character == '-');
    }

    private readonly record struct Heading(int? Number, string Title, bool IsResolved);

    /// <summary>
    /// Parses a <c>### #NNN — Title</c> heading, tolerating a missing <c>#</c>, varied dashes, and
    /// no title. Returns <c>null</c> when the line is not a level-3 heading; returns a heading with
    /// a <c>null</c> number when it carries no numeric identifier (free-form layout). Also detects
    /// the <c>~~strikethrough~~ ✓ Fixed</c> syntax used by free-form resolved entries.
    /// </summary>
    private static Heading? ParseHeading(string line)
    {
        if (!line.StartsWith("### ", StringComparison.Ordinal))
        {
            return null;
        }

        var body = line[4..];

        var cursor = 0;
        while (cursor < body.Length && body[cursor] == ' ')
        {
            cursor++;
        }

        if (cursor < body.Length && body[cursor] == '#')
        {
            cursor++;
        }

        // Skip an optional letter prefix such as "MLM-" before the issue number.
        var prefixStart = cursor;
        while (cursor < body.Length && char.IsLetter(body[cursor]))
        {
            cursor++;
        }

        if (cursor < body.Length && body[cursor] == '-')
        {
            cursor++;
        }
        else
        {
            cursor = prefixStart;
        }

        var digitsStart = cursor;
        while (cursor < body.Length && char.IsDigit(body[cursor]))
        {
            cursor++;
        }

        int? number;
        if (cursor > digitsStart)
        {
            number = int.TryParse(
                body.AsSpan(digitsStart, cursor - digitsStart),
                NumberStyles.None,
                CultureInfo.InvariantCulture,
                out var parsed)
                ? parsed
                : null;
        }
        else
        {
            number = null;
            cursor = 0; // no numeric identifier; the title starts at the beginning
        }

        var titleStart = cursor;
        while (titleStart < body.Length && IsTitleSeparator(body[titleStart]))
        {
            titleStart++;
        }

        var rawTitle = body[titleStart..].Trim();

        // Detect ~~strikethrough~~ titles used in free-form resolved entries.
        var isResolved = false;
        if (rawTitle.StartsWith("~~", StringComparison.Ordinal))
        {
            var closeIndex = rawTitle.IndexOf("~~", 2, StringComparison.Ordinal);
            if (closeIndex >= 0)
            {
                var inner = rawTitle[2..closeIndex];
                var suffix = rawTitle[(closeIndex + 2)..].Trim();
                isResolved = suffix.StartsWith('✓')
                    || suffix.StartsWith("fixed", StringComparison.OrdinalIgnoreCase)
                    || suffix.StartsWith("closed", StringComparison.OrdinalIgnoreCase)
                    || suffix.Length == 0;
                rawTitle = inner;
            }
        }

        return new Heading(number, rawTitle, isResolved);
    }

    private static bool IsTitleSeparator(char character) =>
        character is ' ' or '\t' or '-' or '—' or '–';

    /// <summary>Parses a <c>- **Key:** value</c> metadata bullet.</summary>
    private static (string Key, string Value)? ParseMetadata(string line)
    {
        var trimmed = line.Trim();
        if (!trimmed.StartsWith("- **", StringComparison.Ordinal))
        {
            return null;
        }

        var afterStars = trimmed[4..];
        var close = afterStars.IndexOf(":**", StringComparison.Ordinal);
        if (close < 0)
        {
            return null;
        }

        return (afterStars[..close], afterStars[(close + 3)..].Trim());
    }

    /// <summary>
    /// Parses a bold body-section header such as <c>**Description**</c> or
    /// <c>**Steps to reproduce** (bugs only)</c>.
    /// </summary>
    private static string? ParseSectionHeader(string line)
    {
        var trimmed = line.Trim();
        if (!trimmed.StartsWith("**", StringComparison.Ordinal))
        {
            return null;
        }

        var afterStars = trimmed[2..];
        var close = afterStars.IndexOf("**", StringComparison.Ordinal);
        return close < 0 ? null : afterStars[..close];
    }

    // MARK: - Body assembly

    private enum SectionKind
    {
        None,
        Description,
        Steps,
        Environment,
        Notes,
        Resolution,
        Comments,
        Unknown
    }

    private static SectionKind Classify(string header) => header.ToLowerInvariant() switch
    {
        "description" => SectionKind.Description,
        "steps to reproduce" => SectionKind.Steps,
        "environment" => SectionKind.Environment,
        "notes / investigation" or "notes/investigation" or "notes" => SectionKind.Notes,
        "resolution" or "proposed fix" => SectionKind.Resolution,
        "comments" => SectionKind.Comments,
        _ => SectionKind.Unknown
    };

    // MARK: - Issue collection

    /// <summary>
    /// Collects issues from <paramref name="lines"/> starting at <paramref name="start"/>.
    /// In standard mode (<paramref name="freeForm"/> is <c>false</c>) headings without a number are
    /// skipped. In free-form mode numbers are auto-assigned and <c>~~strikethrough~~ ✓ Fixed</c>
    /// sets the status to resolved.
    /// </summary>
    private static List<Issue> CollectIssues(List<string> lines, int start, bool freeForm, DateTimeOffset now)
    {
        var issues = new List<Issue>();
        var nextAutoNumber = 1;
        var currentSectionType = IssueType.Task;
        var index = start;

        while (index < lines.Count)
        {
            if (freeForm && IsLevel2Heading(lines[index]))
            {
                currentSectionType = SectionType(lines[index]);
                index++;
                continue;
            }

            if (ParseHeading(lines[index]) is not { } heading)
            {
                index++;
                continue;
            }

            if (!freeForm && heading.Number is null)
            {
                index++;
                continue;
            }

            var number = heading.Number ?? nextAutoNumber;
            nextAutoNumber = Math.Max(nextAutoNumber, number) + 1;
            index++;

            var body = new List<string>();
            while (index < lines.Count
                   && ParseHeading(lines[index]) is null
                   && !IsLevel2Heading(lines[index])
                   && !IsHorizontalRule(lines[index]))
            {
                body.Add(lines[index]);
                index++;
            }

            var issue = MakeIssue(
                number,
                heading.Title,
                body,
                freeForm ? currentSectionType : IssueEnums.DefaultType,
                now);

            if (freeForm && heading.IsResolved)
            {
                issue.Status = IssueStatus.Resolved;
            }

            issues.Add(issue);
        }

        return issues;
    }

    /// <summary>Maps a <c>## Section Name</c> line to the corresponding issue type for free-form documents.</summary>
    private static IssueType SectionType(string line)
    {
        var lower = line.Trim().ToLowerInvariant();
        if (lower.StartsWith("## bug", StringComparison.Ordinal))
        {
            return IssueType.Bug;
        }

        if (lower.StartsWith("## enhancement", StringComparison.Ordinal)
            || lower.StartsWith("## feature", StringComparison.Ordinal))
        {
            return IssueType.Feature;
        }

        return lower.StartsWith("## question", StringComparison.Ordinal) ? IssueType.Question : IssueType.Task;
    }

    private static Issue MakeIssue(
        int number,
        string title,
        List<string> body,
        IssueType defaultType,
        DateTimeOffset now)
    {
        var issue = new Issue
        {
            Number = number,
            Title = title,
            Type = defaultType,
            Reported = IssueDate.Today(),
            CreatedAt = now,
            UpdatedAt = now
        };

        var current = SectionKind.None;
        var currentHeader = string.Empty;
        var buffer = new List<string>();
        var extraParts = new List<string>();

        void Flush()
        {
            switch (current)
            {
                case SectionKind.None:
                    var loose = JoinBody(buffer);
                    if (loose.Length > 0)
                    {
                        extraParts.Add(loose);
                    }

                    break;
                case SectionKind.Description:
                    issue.Description = JoinBody(buffer);
                    break;
                case SectionKind.Steps:
                    issue.StepsToReproduce = ParseSteps(buffer);
                    break;
                case SectionKind.Environment:
                    issue.Environment = JoinBody(buffer);
                    break;
                case SectionKind.Notes:
                    issue.Notes = JoinBody(buffer);
                    break;
                case SectionKind.Resolution:
                    issue.Resolution = JoinBody(buffer);
                    break;
                case SectionKind.Comments:
                    issue.Comments = ParseComments(buffer);
                    break;
                case SectionKind.Unknown:
                    var content = JoinBody(buffer);
                    extraParts.Add(content.Length == 0
                        ? $"**{currentHeader}**"
                        : $"**{currentHeader}**\n\n{content}");
                    break;
                default:
                    throw new InvalidOperationException($"Unhandled section {current}.");
            }

            buffer.Clear();
        }

        foreach (var line in body)
        {
            if (ParseSectionHeader(line) is { } header)
            {
                Flush();
                current = Classify(header);
                currentHeader = header;
            }
            else if (current == SectionKind.None && ParseMetadata(line) is { } meta)
            {
                Apply(meta, issue);
            }
            else
            {
                buffer.Add(line);
            }
        }

        Flush();

        // Content the markdown format has no dedicated slot for lands in Notes rather than being
        // dropped; the exporter re-emits it there, so the cycle is stable.
        var extras = extraParts.Where(part => part.Length > 0).ToList();
        if (extras.Count > 0)
        {
            var joined = string.Join("\n\n", extras);
            issue.Notes = issue.Notes.Length == 0 ? joined : issue.Notes + "\n\n" + joined;
        }

        return issue;
    }

    private static void Apply((string Key, string Value) meta, Issue issue)
    {
        switch (meta.Key.ToLowerInvariant())
        {
            case "type":
                issue.Type = IssueEnums.IssueTypeFromDisplayName(meta.Value) ?? IssueEnums.DefaultType;
                break;
            case "priority":
            case "severity":
                issue.Priority = IssueEnums.IssuePriorityFromDisplayName(meta.Value) ?? IssueEnums.DefaultPriority;
                break;
            case "status":
                issue.Status = IssueEnums.IssueStatusFromDisplayName(meta.Value) ?? IssueEnums.DefaultStatus;
                break;
            case "reported":
                if (IssueDate.Date(meta.Value) is { } reported)
                {
                    issue.Reported = reported;
                }

                break;
            case "reported by":
                issue.ReportedBy = meta.Value;
                break;
            case "area":
            case "component":
                issue.Area = meta.Value;
                break;
            case "labels":
                issue.Labels = SplitList(meta.Value);
                break;
            case "assignees":
                issue.Assignees = SplitList(meta.Value);
                break;
            case "milestone":
                issue.Milestone = meta.Value.Length == 0 ? null : meta.Value;
                break;
            case "estimate":
                if (double.TryParse(meta.Value, NumberStyles.Float, CultureInfo.InvariantCulture, out var estimate))
                {
                    issue.Estimate = estimate;
                }

                break;
            case "github":
                issue.RemoteLinks.Add(new RemoteLink(RemoteProvider.GitHub, meta.Value));
                break;
            default:
                break;
        }
    }

    // MARK: - Value parsing

    private static List<string> SplitList(string value) =>
        [.. value.Split(',').Select(entry => entry.Trim()).Where(entry => entry.Length > 0)];

    /// <summary>Drops leading and trailing blank lines while keeping interior blanks.</summary>
    private static List<string> TrimmingBlankEdges(List<string> lines)
    {
        var first = 0;
        var last = lines.Count - 1;
        while (first <= last && lines[first].Trim().Length == 0)
        {
            first++;
        }

        while (last >= first && lines[last].Trim().Length == 0)
        {
            last--;
        }

        return [.. lines.Skip(first).Take(last - first + 1)];
    }

    private static string JoinBody(List<string> lines) => string.Join("\n", TrimmingBlankEdges(lines));

    /// <summary>Extracts the text of a numbered list, dropping empty template placeholders.</summary>
    private static List<string> ParseSteps(List<string> lines)
    {
        var steps = new List<string>();
        foreach (var raw in lines)
        {
            var trimmed = raw.Trim();
            if (trimmed.Length == 0)
            {
                continue;
            }

            var dot = trimmed.IndexOf('.');
            if (dot > 0 && trimmed[..dot].All(char.IsDigit))
            {
                var text = trimmed[(dot + 1)..].Trim();
                if (text.Length > 0)
                {
                    steps.Add(text);
                }
            }
            else if (steps.Count > 0)
            {
                steps[^1] += " " + trimmed;
            }
        }

        return steps;
    }

    /// <summary>
    /// Reads the <c>- **Author** (YYYY-MM-DD): body</c> bullets emitted by
    /// <see cref="IssuesMarkdownSerializer"/>. Lines that are not a bullet continue the preceding
    /// comment's body.
    /// </summary>
    private static List<Comment> ParseComments(List<string> lines)
    {
        var comments = new List<Comment>();
        foreach (var raw in TrimmingBlankEdges(lines))
        {
            if (ParseCommentBullet(raw) is { } comment)
            {
                comments.Add(comment);
            }
            else if (comments.Count > 0)
            {
                var continuation = raw.StartsWith("  ", StringComparison.Ordinal) ? raw[2..] : raw;
                comments[^1].Body += "\n" + continuation;
            }
        }

        return comments;
    }

    private static Comment? ParseCommentBullet(string line)
    {
        if (!line.StartsWith("- **", StringComparison.Ordinal))
        {
            return null;
        }

        var afterBullet = line[4..];
        var closeStars = afterBullet.IndexOf("**", StringComparison.Ordinal);
        if (closeStars < 0)
        {
            return null;
        }

        var author = afterBullet[..closeStars];
        var remainder = afterBullet[(closeStars + 2)..].Trim();
        var createdAt = IssueDate.Today();

        var closeParen = remainder.IndexOf(')');
        if (remainder.StartsWith('(') && closeParen >= 0)
        {
            if (IssueDate.Date(remainder[1..closeParen]) is { } parsed)
            {
                createdAt = parsed;
            }

            remainder = remainder[(closeParen + 1)..];
        }

        if (remainder.StartsWith(':'))
        {
            remainder = remainder[1..];
        }

        return new Comment
        {
            Author = author,
            CreatedAt = createdAt,
            Body = remainder.Trim()
        };
    }
}
