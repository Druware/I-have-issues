using IHaveIssues.Mvvm;
using IssuesKit;

namespace IHaveIssues.ViewModels;

/// <summary>
/// Per-document project identity and issue-tracker coordinates.
/// </summary>
/// <remarks>
/// Like <see cref="IssueEditViewModel"/> this edits a local draft and only writes back on save, so
/// cancelling discards everything.
/// <para>
/// <b>No credentials are edited or stored here.</b> <c>.issues</c> documents are committed to
/// project repositories, so tokens live in Windows Credential Manager (see
/// <see cref="Services.GitHubCredentialStore"/>) and only the non-secret coordinates below are
/// written to the file.
/// </para>
/// </remarks>
public sealed class ProjectSettingsViewModel : ObservableObject
{
    private string _name;
    private string _summary;

    private bool _isGitHubConfigured;
    private string _gitHubOwner;
    private string _gitHubRepository;
    private string _gitHubLabels;
    private string _gitHubAssignees;
    private string _gitHubMilestone;

    private bool _isAzureConfigured;
    private string _azureOrganization;
    private string _azureProject;
    private string _azureTeam;
    private string _azureAreaPath;
    private string _azureIterationPath;
    private string _azureWorkItemType;

    public ProjectSettingsViewModel(IssuesDocumentModel model)
    {
        ArgumentNullException.ThrowIfNull(model);

        _name = model.Project.Name;
        _summary = model.Project.Summary;

        var github = model.Integrations.GitHub;
        _isGitHubConfigured = github is not null;
        _gitHubOwner = github?.Owner ?? string.Empty;
        _gitHubRepository = github?.Repository ?? string.Empty;
        _gitHubLabels = string.Join(", ", github?.DefaultLabels ?? []);
        _gitHubAssignees = string.Join(", ", github?.DefaultAssignees ?? []);
        _gitHubMilestone = github?.DefaultMilestone ?? string.Empty;

        var azure = model.Integrations.AzureDevOps;
        _isAzureConfigured = azure is not null;
        _azureOrganization = azure?.Organization ?? string.Empty;
        _azureProject = azure?.Project ?? string.Empty;
        _azureTeam = azure?.Team ?? string.Empty;
        _azureAreaPath = azure?.AreaPath ?? string.Empty;
        _azureIterationPath = azure?.IterationPath ?? string.Empty;
        _azureWorkItemType = azure?.DefaultWorkItemType ?? "Issue";
    }

    public string Name { get => _name; set => SetProperty(ref _name, value); }

    public string Summary { get => _summary; set => SetProperty(ref _summary, value); }

    public bool IsGitHubConfigured
    {
        get => _isGitHubConfigured;
        set => SetProperty(ref _isGitHubConfigured, value);
    }

    public string GitHubOwner { get => _gitHubOwner; set => SetProperty(ref _gitHubOwner, value); }

    public string GitHubRepository
    {
        get => _gitHubRepository;
        set => SetProperty(ref _gitHubRepository, value);
    }

    public string GitHubLabels { get => _gitHubLabels; set => SetProperty(ref _gitHubLabels, value); }

    public string GitHubAssignees
    {
        get => _gitHubAssignees;
        set => SetProperty(ref _gitHubAssignees, value);
    }

    public string GitHubMilestone
    {
        get => _gitHubMilestone;
        set => SetProperty(ref _gitHubMilestone, value);
    }

    public bool IsAzureConfigured
    {
        get => _isAzureConfigured;
        set => SetProperty(ref _isAzureConfigured, value);
    }

    public string AzureOrganization
    {
        get => _azureOrganization;
        set => SetProperty(ref _azureOrganization, value);
    }

    public string AzureProject { get => _azureProject; set => SetProperty(ref _azureProject, value); }

    public string AzureTeam { get => _azureTeam; set => SetProperty(ref _azureTeam, value); }

    public string AzureAreaPath { get => _azureAreaPath; set => SetProperty(ref _azureAreaPath, value); }

    public string AzureIterationPath
    {
        get => _azureIterationPath;
        set => SetProperty(ref _azureIterationPath, value);
    }

    public string AzureWorkItemType
    {
        get => _azureWorkItemType;
        set => SetProperty(ref _azureWorkItemType, value);
    }

    /// <summary>Writes the edited settings into the document.</summary>
    public void Save(IssuesDocumentModel model)
    {
        ArgumentNullException.ThrowIfNull(model);

        model.Project.Name = Name.Trim();
        model.Project.Summary = Summary;
        model.Integrations.GitHub = IsGitHubConfigured ? BuildGitHub() : null;
        model.Integrations.AzureDevOps = IsAzureConfigured ? BuildAzure() : null;
    }

    private GitHubIntegration BuildGitHub() => new()
    {
        Owner = GitHubOwner.Trim(),
        Repository = GitHubRepository.Trim(),
        DefaultLabels = SplitEntries(GitHubLabels),
        DefaultAssignees = SplitEntries(GitHubAssignees),
        DefaultMilestone = Optional(GitHubMilestone)
    };

    private AzureDevOpsIntegration BuildAzure() => new()
    {
        Organization = AzureOrganization.Trim(),
        Project = AzureProject.Trim(),
        Team = Optional(AzureTeam),
        AreaPath = Optional(AzureAreaPath),
        IterationPath = Optional(AzureIterationPath),
        DefaultWorkItemType = Optional(AzureWorkItemType) ?? "Issue"
    };

    /// <summary>Splits a comma-separated field into trimmed, non-empty entries.</summary>
    private static List<string> SplitEntries(string text) =>
        [.. text.Split(',').Select(entry => entry.Trim()).Where(entry => entry.Length > 0)];

    /// <summary>An optional field is <c>null</c> when blank, never an empty string.</summary>
    private static string? Optional(string text)
    {
        var trimmed = text.Trim();
        return trimmed.Length == 0 ? null : trimmed;
    }
}
