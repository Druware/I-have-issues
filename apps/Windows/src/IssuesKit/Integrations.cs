namespace IssuesKit;

/// <summary>
/// Per-project issue-tracker configuration carried inside the <c>.issues</c> document.
/// </summary>
/// <remarks>
/// <b>No credentials belong in this type, ever.</b> <c>.issues</c> files are committed to project
/// repositories, so a token stored here would be published to everyone with read access and to
/// every fork and CI log. Personal access tokens, OAuth tokens, and passwords live in Windows
/// Credential Manager, keyed by the values below (owner/repository, organization/project). This
/// type holds only the non-secret coordinates needed to find the remote tracker.
/// </remarks>
public sealed record IntegrationSettings
{
    public GitHubIntegration? GitHub { get; set; }

    public AzureDevOpsIntegration? AzureDevOps { get; set; }

    public IntegrationSettings Copy() => new()
    {
        GitHub = GitHub?.Copy(),
        AzureDevOps = AzureDevOps is null ? null : AzureDevOps with { }
    };
}

/// <summary>
/// Where a project's issues live on GitHub, plus the defaults applied to newly pushed issues.
/// Contains no token — see <see cref="IntegrationSettings"/> for why.
/// </summary>
public sealed class GitHubIntegration : IEquatable<GitHubIntegration>
{
    public string Owner { get; set; } = string.Empty;
    public string Repository { get; set; } = string.Empty;
    public List<string> DefaultLabels { get; set; } = [];
    public List<string> DefaultAssignees { get; set; } = [];
    public string? DefaultMilestone { get; set; }

    public GitHubIntegration Copy() => new()
    {
        Owner = Owner,
        Repository = Repository,
        DefaultLabels = [.. DefaultLabels],
        DefaultAssignees = [.. DefaultAssignees],
        DefaultMilestone = DefaultMilestone
    };

    public bool Equals(GitHubIntegration? other) =>
        other is not null
        && Owner == other.Owner
        && Repository == other.Repository
        && ModelEquality.ListEquals(DefaultLabels, other.DefaultLabels)
        && ModelEquality.ListEquals(DefaultAssignees, other.DefaultAssignees)
        && DefaultMilestone == other.DefaultMilestone;

    public override bool Equals(object? obj) => Equals(obj as GitHubIntegration);

    public override int GetHashCode() => HashCode.Combine(Owner, Repository, DefaultMilestone);
}

/// <summary>
/// Where a project's work items live in Azure DevOps, plus the defaults applied to new items.
/// Contains no token — see <see cref="IntegrationSettings"/> for why.
/// </summary>
public sealed record AzureDevOpsIntegration
{
    public string Organization { get; set; } = string.Empty;
    public string Project { get; set; } = string.Empty;
    public string? Team { get; set; }
    public string? AreaPath { get; set; }
    public string? IterationPath { get; set; }

    /// <summary>e.g. <c>Bug</c>, <c>Task</c>, <c>User Story</c>.</summary>
    public string DefaultWorkItemType { get; set; } = "Issue";
}
