using System.Windows;
using IHaveIssues.ViewModels;
using IssuesKit;

namespace IHaveIssues.Views;

/// <summary>
/// The GitHub sync sheet. The typed token lives only in the <see cref="System.Windows.Controls.PasswordBox"/>
/// and is handed to the view model for the length of a call — it is never mirrored into bindable
/// state, and a stored token is never read back into the window.
/// </summary>
public partial class GitHubSyncWindow : Window
{
    private readonly GitHubSyncViewModel _viewModel;

    public GitHubSyncWindow(IssuesDocumentModel model)
    {
        _viewModel = new GitHubSyncViewModel(model);
        DataContext = _viewModel;
        InitializeComponent();
        UpdateSyncButton();
    }

    /// <summary>The issues a completed sync produced, or <c>null</c> when nothing was synced.</summary>
    public List<Issue>? UpdatedIssues => _viewModel.UpdatedIssues;

    private async void OnSync(object sender, RoutedEventArgs e)
    {
        UpdateSyncButton();
        await _viewModel.SyncAsync(TokenBox.Password).ConfigureAwait(true);
        TokenBox.Clear();
        UpdateSyncButton();
    }

    /// <summary>
    /// Saving a typed token can fail — with no repository configured there is nowhere to put it —
    /// so Done stays put and shows the reason rather than dropping the token silently.
    /// </summary>
    private void OnDone(object sender, RoutedEventArgs e)
    {
        if (_viewModel.SaveToken(TokenBox.Password))
        {
            DialogResult = true;
        }
    }

    private void OnRemoveToken(object sender, RoutedEventArgs e)
    {
        _viewModel.RemoveStoredToken();
        TokenBox.Clear();
        UpdateSyncButton();
    }

    private void OnTokenChanged(object sender, RoutedEventArgs e) => UpdateSyncButton();

    private void UpdateSyncButton() => SyncButton.IsEnabled = _viewModel.CanSync(TokenBox.Password);
}
