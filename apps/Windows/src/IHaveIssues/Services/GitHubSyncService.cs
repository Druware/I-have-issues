using System.Globalization;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using IssuesKit;

namespace IHaveIssues.Services;

/// <summary>The tally of one sync run.</summary>
public sealed class SyncResult
{
    public int Created { get; set; }
    public int Updated { get; set; }
    public int Failed { get; set; }
    public List<string> Errors { get; } = [];
}

/// <summary>A failure worth showing the user during a GitHub sync.</summary>
public class GitHubSyncException(string message) : Exception(message);

/// <summary>
/// The token was rejected. Unlike a per-issue failure this aborts the whole run: every remaining
/// request would fail the same way, and hammering the API with a bad token invites rate limiting.
/// </summary>
public sealed class GitHubAuthenticationException()
    : GitHubSyncException("Invalid or expired GitHub token. Check your personal access token.");

/// <summary>
/// Pushes the document's issues to GitHub.
/// </summary>
/// <remarks>
/// Carries no credentials of its own beyond the token handed to it for the length of one run; the
/// token lives in Windows Credential Manager (see <see cref="GitHubCredentialStore"/>) and never
/// touches the <c>.issues</c> file.
/// </remarks>
public sealed class GitHubSyncService(string token, GitHubIntegration integration) : IDisposable
{
    private static readonly JsonSerializerOptions CompactJson = new() { WriteIndented = false };

    private readonly HttpClient _client = CreateClient(token);

    /// <summary>A GitHub issue as this app reads it back from a create or update response.</summary>
    private readonly record struct RemoteIssue(int Number, Uri? HtmlUrl, DateTimeOffset? UpdatedAt);

    public void Dispose() => _client.Dispose();

    private static HttpClient CreateClient(string token)
    {
        var client = new HttpClient { BaseAddress = new Uri("https://api.github.com/") };
        client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);
        client.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
        client.DefaultRequestHeaders.Add("X-GitHub-Api-Version", "2022-11-28");
        client.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("IHaveIssues", "1.0"));
        return client;
    }

    /// <summary>
    /// Syncs every issue to GitHub, returning the updated issues — a <c>github</c> remote link
    /// filled in for newly created ones, refreshed timestamps for existing ones — and a summary.
    /// </summary>
    /// <exception cref="GitHubSyncException">
    /// Authentication failed, or the repository coordinates cannot form a URL. Per-issue failures
    /// are accumulated in the result instead of thrown.
    /// </exception>
    public async Task<(List<Issue> Issues, SyncResult Result)> SyncAsync(
        IEnumerable<Issue> issues,
        CancellationToken cancellationToken = default)
    {
        var updated = issues.Select(issue => issue.Copy()).ToList();
        var result = new SyncResult();
        var milestones = await MilestoneNumbersIfNeededAsync(updated, cancellationToken).ConfigureAwait(false);

        foreach (var issue in updated)
        {
            try
            {
                var linkIndex = issue.RemoteLinks.FindIndex(link => link.Provider.IsGitHub);
                if (linkIndex >= 0)
                {
                    var identifier = issue.RemoteLinks[linkIndex].Identifier;
                    if (!int.TryParse(identifier, NumberStyles.Integer, CultureInfo.InvariantCulture, out var number))
                    {
                        throw new GitHubSyncException(
                            $"The existing GitHub link \"{identifier}\" is not an issue number.");
                    }

                    var remote = await UpdateIssueAsync(issue, number, milestones, cancellationToken)
                        .ConfigureAwait(false);
                    issue.RemoteLinks[linkIndex] = Record(remote, issue.RemoteLinks[linkIndex]);
                    result.Updated++;
                }
                else
                {
                    var remote = await CreateIssueAsync(issue, milestones, cancellationToken).ConfigureAwait(false);
                    if (issue.IsResolved)
                    {
                        // GitHub cannot create an issue in the closed state, so a resolved entry is
                        // created open and closed by an immediate follow-up patch.
                        remote = await UpdateIssueAsync(issue, remote.Number, milestones, cancellationToken)
                            .ConfigureAwait(false) ?? remote;
                    }

                    issue.RemoteLinks.Add(new RemoteLink
                    {
                        Provider = RemoteProvider.GitHub,
                        Identifier = remote.Number.ToString(CultureInfo.InvariantCulture),
                        Url = remote.HtmlUrl,
                        LastSyncedAt = DateTimeOffset.UtcNow,
                        RemoteUpdatedAt = remote.UpdatedAt
                    });
                    result.Created++;
                }
            }
            catch (Exception error) when (error is not GitHubAuthenticationException and not OperationCanceledException)
            {
                result.Failed++;
                result.Errors.Add($"{issue.DisplayNumber}: {error.Message}");
            }
        }

        return (updated, result);
    }

    /// <summary>
    /// Refreshes an existing link. The remote fields are only overwritten when the response
    /// actually carried them, so a terse reply never erases what we already knew.
    /// </summary>
    private static RemoteLink Record(RemoteIssue? remote, RemoteLink link)
    {
        var refreshed = link with { LastSyncedAt = DateTimeOffset.UtcNow };
        if (remote is not { } value)
        {
            return refreshed;
        }

        if (value.HtmlUrl is not null)
        {
            refreshed = refreshed with { Url = value.HtmlUrl };
        }

        return value.UpdatedAt is null ? refreshed : refreshed with { RemoteUpdatedAt = value.UpdatedAt };
    }

    private async Task<RemoteIssue> CreateIssueAsync(
        Issue issue,
        IReadOnlyDictionary<string, int> milestones,
        CancellationToken cancellationToken)
    {
        var body = Payload(issue, milestones, includeState: false);
        var json = await SendAsync(HttpMethod.Post, ["issues"], body, cancellationToken).ConfigureAwait(false);
        return ReadRemoteIssue(json) ?? throw new GitHubSyncException("Unexpected response from the GitHub API.");
    }

    private async Task<RemoteIssue?> UpdateIssueAsync(
        Issue issue,
        int number,
        IReadOnlyDictionary<string, int> milestones,
        CancellationToken cancellationToken)
    {
        var body = Payload(issue, milestones, includeState: true);
        var path = new[] { "issues", number.ToString(CultureInfo.InvariantCulture) };
        var json = await SendAsync(HttpMethod.Patch, path, body, cancellationToken).ConfigureAwait(false);
        return ReadRemoteIssue(json);
    }

    private static RemoteIssue? ReadRemoteIssue(string json)
    {
        if (JsonNode.Parse(json) is not JsonObject root || root["number"] is not { } numberNode)
        {
            return null;
        }

        Uri? htmlUrl = null;
        if (root["html_url"]?.GetValue<string>() is { } url && Uri.TryCreate(url, UriKind.Absolute, out var parsed))
        {
            htmlUrl = parsed;
        }

        DateTimeOffset? updatedAt = null;
        if (root["updated_at"]?.GetValue<string>() is { } stamp
            && DateTimeOffset.TryParse(stamp, CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind, out var value))
        {
            updatedAt = value;
        }

        return new RemoteIssue(numberNode.GetValue<int>(), htmlUrl, updatedAt);
    }

    // MARK: - Payload

    private JsonObject Payload(Issue issue, IReadOnlyDictionary<string, int> milestones, bool includeState)
    {
        var body = new JsonObject
        {
            ["title"] = issue.Title.Length == 0 ? "Untitled Issue" : issue.Title,
            ["body"] = IssueBody(issue)
        };

        var labels = Merged(issue.Labels, integration.DefaultLabels);
        if (labels.Count > 0)
        {
            body["labels"] = new JsonArray([.. labels.Select(label => JsonValue.Create(label))]);
        }

        var assignees = Merged(issue.Assignees, integration.DefaultAssignees);
        if (assignees.Count > 0)
        {
            body["assignees"] = new JsonArray([.. assignees.Select(name => JsonValue.Create(name))]);
        }

        // GitHub identifies a milestone by its number, not its title, so a name with no match in
        // the repository is omitted — sending the title would fail the whole request.
        var milestoneName = issue.Milestone ?? integration.DefaultMilestone;
        if (milestoneName is not null && milestones.TryGetValue(milestoneName, out var milestoneNumber))
        {
            body["milestone"] = milestoneNumber;
        }

        if (includeState)
        {
            body["state"] = issue.IsResolved ? "closed" : "open";
        }

        return body;
    }

    /// <summary>The issue's own values first, then any document defaults it does not already carry.</summary>
    private static List<string> Merged(List<string> own, List<string> defaults) =>
        [.. own, .. defaults.Where(value => !own.Contains(value))];

    /// <summary>
    /// Maps milestone titles to GitHub milestone numbers, fetched once per sync and only when some
    /// issue (or the document default) actually names one.
    /// </summary>
    /// <remarks>
    /// Pages to the end of the list. The Apple build reads only the first hundred, which silently
    /// drops the milestone from any issue whose milestone sorts later than that.
    /// </remarks>
    private async Task<IReadOnlyDictionary<string, int>> MilestoneNumbersIfNeededAsync(
        List<Issue> issues,
        CancellationToken cancellationToken)
    {
        var isNamed = issues.Any(issue => !string.IsNullOrEmpty(issue.Milestone))
            || !string.IsNullOrEmpty(integration.DefaultMilestone);
        if (!isNamed)
        {
            return new Dictionary<string, int>(StringComparer.Ordinal);
        }

        var numbers = new Dictionary<string, int>(StringComparer.Ordinal);
        for (var page = 1; ; page++)
        {
            var query = $"state=all&per_page=100&page={page.ToString(CultureInfo.InvariantCulture)}";
            var json = await SendAsync(HttpMethod.Get, ["milestones"], body: null, cancellationToken, query)
                .ConfigureAwait(false);

            if (JsonNode.Parse(json) is not JsonArray items || items.Count == 0)
            {
                return numbers;
            }

            foreach (var item in items)
            {
                if (item is JsonObject milestone
                    && milestone["title"]?.GetValue<string>() is { } title
                    && milestone["number"] is { } number)
                {
                    numbers[title] = number.GetValue<int>();
                }
            }

            if (items.Count < 100)
            {
                return numbers;
            }
        }
    }

    // MARK: - Requests

    /// <summary>
    /// Characters safe to leave unescaped in a single path segment. Deliberately excludes <c>/</c>
    /// so a stray slash in an owner or repository cannot invent extra path components.
    /// </summary>
    private static readonly System.Buffers.SearchValues<char> PathSegmentAllowed =
        System.Buffers.SearchValues.Create(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~");

    private async Task<string> SendAsync(
        HttpMethod method,
        string[] path,
        JsonObject? body,
        CancellationToken cancellationToken,
        string? query = null)
    {
        var segments = new[] { "repos", integration.Owner, integration.Repository }.Concat(path);
        var encoded = new List<string>();
        foreach (var segment in segments)
        {
            if (segment.Length == 0)
            {
                throw new GitHubSyncException(
                    "The GitHub owner and repository in Project Settings do not form a valid URL.");
            }

            encoded.Add(EscapeSegment(segment));
        }

        var uri = string.Join('/', encoded) + (query is null ? string.Empty : "?" + query);
        using var request = new HttpRequestMessage(method, uri);
        if (body is not null)
        {
            request.Content = new StringContent(
                body.ToJsonString(CompactJson),
                Encoding.UTF8,
                "application/json");
        }

        using var response = await _client.SendAsync(request, cancellationToken).ConfigureAwait(false);
        var payload = await response.Content.ReadAsStringAsync(cancellationToken).ConfigureAwait(false);

        if (response.IsSuccessStatusCode)
        {
            return payload;
        }

        if (response.StatusCode == HttpStatusCode.Unauthorized)
        {
            throw new GitHubAuthenticationException();
        }

        var message = TryReadMessage(payload) ?? $"HTTP {(int)response.StatusCode}";
        throw new GitHubSyncException(message);
    }

    private static string? TryReadMessage(string payload)
    {
        try
        {
            return JsonNode.Parse(payload) is JsonObject root ? root["message"]?.GetValue<string>() : null;
        }
        catch (JsonException)
        {
            return null;
        }
    }

    private static string EscapeSegment(string segment)
    {
        var builder = new StringBuilder(segment.Length);
        foreach (var character in segment)
        {
            if (PathSegmentAllowed.Contains(character))
            {
                builder.Append(character);
            }
            else
            {
                foreach (var b in Encoding.UTF8.GetBytes([character]))
                {
                    builder.Append(CultureInfo.InvariantCulture, $"%{b:X2}");
                }
            }
        }

        return builder.ToString();
    }

    private static string IssueBody(Issue issue)
    {
        var parts = new List<string>
        {
            $"**Type:** {issue.Type.DisplayName()} | **Priority:** {issue.Priority.DisplayName()}"
                + $" | **Status:** {issue.Status.DisplayName()}"
        };

        var dateLine = $"**Reported:** {IssueDate.String(issue.Reported)}";
        if (issue.ReportedBy.Length > 0)
        {
            dateLine += $" · {issue.ReportedBy}";
        }

        if (issue.Area.Length > 0)
        {
            dateLine += $" | **Area:** {issue.Area}";
        }

        parts.Add(dateLine);

        if (issue.Description.Length > 0)
        {
            parts.Add($"---\n**Description**\n\n{issue.Description}");
        }

        if (issue.StepsToReproduce.Count > 0)
        {
            var numbered = string.Join(
                "\n",
                issue.StepsToReproduce.Select((step, index) =>
                    $"{(index + 1).ToString(CultureInfo.InvariantCulture)}. {step}"));
            parts.Add($"**Steps to Reproduce**\n\n{numbered}");
        }

        if (issue.Notes.Length > 0)
        {
            parts.Add($"**Notes / Investigation**\n\n{issue.Notes}");
        }

        if (issue.Resolution.Length > 0)
        {
            parts.Add($"**Resolution**\n\n{issue.Resolution}");
        }

        parts.Add($"---\n*Synced from local issue tracker · {issue.DisplayNumber}*");
        return string.Join("\n\n", parts);
    }
}
