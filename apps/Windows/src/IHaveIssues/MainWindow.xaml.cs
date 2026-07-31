using System.IO;
using System.Windows;
using System.Windows.Input;
using IHaveIssues.ViewModels;
using IHaveIssues.Views;
using IssuesKit;
using Microsoft.Win32;

namespace IHaveIssues;

/// <summary>
/// The document window: a sidebar list of issues beside a detail pane, with add, edit, and delete.
/// </summary>
/// <remarks>
/// Implements <see cref="IMainInteraction"/> so the view model can ask for a dialog without owning
/// one.
/// </remarks>
public partial class MainWindow : Window, IMainInteraction
{
    private readonly MainViewModel _viewModel;

    public MainWindow() : this(null)
    {
    }

    /// <param name="path">
    /// An <c>.issues</c> file to open on launch, as passed by Explorer for a double-clicked
    /// document, or <c>null</c> to start with an empty one.
    /// </param>
    public MainWindow(string? path)
    {
        _viewModel = new MainViewModel(this);
        DataContext = _viewModel;
        InitializeComponent();

        if (path is not null)
        {
            _viewModel.OpenFile(path);
        }
    }

    // MARK: - Window events

    private void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e)
    {
        if (!_viewModel.ConfirmClose())
        {
            e.Cancel = true;
        }
    }

    /// <summary>Double-clicking a row opens the editor, as double-clicking a document row usually does.</summary>
    private void OnIssueListDoubleClick(object sender, MouseButtonEventArgs e) => _viewModel.EditSelected();

    // MARK: - IMainInteraction

    public void ShowError(string title, string message) =>
        MessageBox.Show(this, message, title, MessageBoxButton.OK, MessageBoxImage.Error);

    public bool Confirm(string title, string message) =>
        MessageBox.Show(this, message, title, MessageBoxButton.OKCancel, MessageBoxImage.Warning)
            == MessageBoxResult.OK;

    public bool? ConfirmDiscardChanges()
    {
        var answer = MessageBox.Show(
            this,
            "This document has unsaved changes. Save them before continuing?",
            "Unsaved Changes",
            MessageBoxButton.YesNoCancel,
            MessageBoxImage.Warning);

        return answer switch
        {
            MessageBoxResult.Yes => true,
            MessageBoxResult.No => false,
            _ => null
        };
    }

    public string? PickOpenFile(string title, string filter)
    {
        var dialog = new OpenFileDialog { Title = title, Filter = filter, CheckFileExists = true };
        return dialog.ShowDialog(this) == true ? dialog.FileName : null;
    }

    public string? PickSaveFile(string title, string filter, string suggestedName)
    {
        var dialog = new SaveFileDialog
        {
            Title = title,
            Filter = filter,
            FileName = suggestedName,
            AddExtension = true,
            DefaultExt = Path.GetExtension(suggestedName).TrimStart('.'),
            OverwritePrompt = true
        };

        return dialog.ShowDialog(this) == true ? dialog.FileName : null;
    }

    public Issue? EditIssue(Issue draft)
    {
        var window = new IssueEditWindow(draft) { Owner = this };
        return window.ShowDialog() == true ? window.Result : null;
    }

    public bool ShowProjectSettings(IssuesDocumentModel model)
    {
        var window = new ProjectSettingsWindow(model) { Owner = this };
        return window.ShowDialog() == true;
    }

    public List<Issue>? ShowGitHubSync(IssuesDocumentModel model)
    {
        var window = new GitHubSyncWindow(model) { Owner = this };
        window.ShowDialog();
        return window.UpdatedIssues;
    }

    public void Shutdown() => Close();
}
