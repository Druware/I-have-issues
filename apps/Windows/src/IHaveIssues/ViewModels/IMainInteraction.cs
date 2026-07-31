using IssuesKit;

namespace IHaveIssues.ViewModels;

/// <summary>
/// The dialogs and file pickers <see cref="MainViewModel"/> needs, kept behind an interface so the
/// view model expresses <i>what</i> should be asked rather than owning any window.
/// </summary>
public interface IMainInteraction
{
    /// <summary>Reports a failure the user must acknowledge.</summary>
    void ShowError(string title, string message);

    /// <summary>Asks a yes/no question. Returns <c>true</c> when the user agrees.</summary>
    bool Confirm(string title, string message);

    /// <summary>
    /// Asks whether to save, discard, or cancel before the open document is replaced or closed.
    /// </summary>
    /// <returns>
    /// <c>true</c> to save first, <c>false</c> to discard the changes, <c>null</c> to abandon
    /// whatever prompted the question.
    /// </returns>
    bool? ConfirmDiscardChanges();

    /// <summary>Shows an open-file picker. Returns <c>null</c> when the user cancels.</summary>
    string? PickOpenFile(string title, string filter);

    /// <summary>Shows a save-file picker. Returns <c>null</c> when the user cancels.</summary>
    string? PickSaveFile(string title, string filter, string suggestedName);

    /// <summary>
    /// Edits a copy of an issue. Returns the edited issue, or <c>null</c> when the user cancels.
    /// </summary>
    Issue? EditIssue(Issue draft);

    /// <summary>
    /// Edits project identity and tracker coordinates, writing straight into <paramref name="model"/>
    /// on save. Returns <c>true</c> when anything was saved.
    /// </summary>
    bool ShowProjectSettings(IssuesDocumentModel model);

    /// <summary>
    /// Runs the GitHub sync sheet. Returns the updated issues when a sync completed, otherwise
    /// <c>null</c>.
    /// </summary>
    List<Issue>? ShowGitHubSync(IssuesDocumentModel model);

    /// <summary>Closes the application.</summary>
    void Shutdown();
}
