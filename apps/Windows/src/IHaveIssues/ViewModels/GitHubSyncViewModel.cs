using System.Globalization;
using System.IO;
using System.Net.Http;
using IHaveIssues.Mvvm;
using IHaveIssues.Services;
using IssuesKit;

namespace IHaveIssues.ViewModels;

/// <summary>
/// Pushes the document's issues to GitHub.
/// </summary>
/// <remarks>
/// Owner and repository belong to the document, not to this window — they are edited once, in
/// Project Settings, so they are never duplicated as editable fields here.
/// <para>
/// The entered token is only ever passed in for the length of a call. A stored token is never read
/// back into the window: it needs to know that one exists, not what it is.
/// </para>
/// </remarks>
public sealed class GitHubSyncViewModel : ObservableObject
{
    private readonly IssuesDocumentModel _model;

    private bool _hasStoredToken;
    private bool _isSyncing;
    private string? _errorMessage;
    private string? _summary;
    private List<string> _failures = [];

    public GitHubSyncViewModel(IssuesDocumentModel model)
    {
        ArgumentNullException.ThrowIfNull(model);
        _model = model;

        if (GitHubCredentialStore.Target(Integration) is { } target)
        {
            _hasStoredToken = GitHubCredentialStore.HasToken(target);
        }
    }

    private GitHubIntegration? Integration => _model.Integrations.GitHub;

    public bool HasIntegration => Integration is not null;

    public string Owner => Integration?.Owner ?? string.Empty;

    public string Repository => Integration?.Repository ?? string.Empty;

    public bool HasStoredToken
    {
        get => _hasStoredToken;
        private set => SetProperty(ref _hasStoredToken, value);
    }

    public bool IsSyncing
    {
        get => _isSyncing;
        private set
        {
            if (SetProperty(ref _isSyncing, value))
            {
                OnPropertyChanged(nameof(IsIdle));
            }
        }
    }

    public bool IsIdle => !_isSyncing;

    public string? ErrorMessage
    {
        get => _errorMessage;
        private set
        {
            if (SetProperty(ref _errorMessage, value))
            {
                OnPropertyChanged(nameof(HasError));
            }
        }
    }

    public bool HasError => !string.IsNullOrEmpty(_errorMessage);

    /// <summary>A one-line tally of the last run, or <c>null</c> when nothing has been synced.</summary>
    public string? Summary
    {
        get => _summary;
        private set
        {
            if (SetProperty(ref _summary, value))
            {
                OnPropertyChanged(nameof(HasSummary));
            }
        }
    }

    public bool HasSummary => !string.IsNullOrEmpty(_summary);

    /// <summary>The per-issue failures from the last run.</summary>
    public List<string> Failures
    {
        get => _failures;
        private set => SetProperty(ref _failures, value);
    }

    /// <summary>The issues a completed sync produced, or <c>null</c> when nothing was synced.</summary>
    public List<Issue>? UpdatedIssues { get; private set; }

    /// <summary>
    /// Stores a typed token against the document's repository, and reports whether it is now safe
    /// to leave the window.
    /// </summary>
    /// <remarks>
    /// With no repository configured there is no target to scope the item to, so a typed token
    /// cannot be stored anywhere — say so instead of dropping it on the floor.
    /// <para>
    /// A blank field means "keep what is stored", not "delete it": the field starts empty every
    /// time, so leaving it alone must not be destructive. Removal is the Remove button's job.
    /// </para>
    /// </remarks>
    public bool SaveToken(string enteredToken)
    {
        if (GitHubCredentialStore.Target(Integration) is not { } target)
        {
            if (enteredToken.Length == 0)
            {
                return true;
            }

            ErrorMessage = "Set the owner and repository in Project Settings before saving a token.";
            return false;
        }

        if (enteredToken.Length == 0)
        {
            return true;
        }

        try
        {
            GitHubCredentialStore.Save(enteredToken, target);
            HasStoredToken = true;
            return true;
        }
        catch (IOException error)
        {
            ErrorMessage = error.Message;
            return false;
        }
    }

    public void RemoveStoredToken()
    {
        if (GitHubCredentialStore.Target(Integration) is not { } target)
        {
            return;
        }

        GitHubCredentialStore.Delete(target);
        HasStoredToken = false;
    }

    /// <summary>Whether Sync can run right now.</summary>
    public bool CanSync(string enteredToken) =>
        Integration is { } integration
        && integration.Owner.Trim().Length > 0
        && integration.Repository.Trim().Length > 0
        && (HasStoredToken || enteredToken.Trim().Length > 0)
        && !IsSyncing;

    /// <summary>
    /// Runs the sync. The token is read back from Credential Manager rather than from window state,
    /// so the secret only exists for the length of this call.
    /// </summary>
    public async Task SyncAsync(string enteredToken)
    {
        if (Integration is not { } integration || !SaveToken(enteredToken))
        {
            return;
        }

        if (GitHubCredentialStore.Target(integration) is not { } target
            || GitHubCredentialStore.Load(target) is not { } token)
        {
            ErrorMessage = "No GitHub token saved for this repository. Enter one and try again.";
            return;
        }

        IsSyncing = true;
        ErrorMessage = null;
        Summary = null;
        Failures = [];

        try
        {
            using var service = new GitHubSyncService(token, integration);
            var (issues, result) = await service.SyncAsync(_model.Issues).ConfigureAwait(true);

            UpdatedIssues = issues;
            Summary = Describe(result);
            Failures = result.Errors;
        }
        catch (Exception error) when (error is GitHubSyncException or HttpRequestException or TaskCanceledException)
        {
            ErrorMessage = error.Message;
        }
        finally
        {
            IsSyncing = false;
        }
    }

    private static string Describe(SyncResult result)
    {
        var parts = new List<string>();
        if (result.Created > 0)
        {
            parts.Add($"{result.Created.ToString(CultureInfo.CurrentCulture)} created");
        }

        if (result.Updated > 0)
        {
            parts.Add($"{result.Updated.ToString(CultureInfo.CurrentCulture)} updated");
        }

        if (result.Failed > 0)
        {
            parts.Add($"{result.Failed.ToString(CultureInfo.CurrentCulture)} failed");
        }

        return parts.Count == 0 ? "Nothing to sync." : string.Join(", ", parts);
    }
}
