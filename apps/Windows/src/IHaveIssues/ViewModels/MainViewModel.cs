using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Windows.Data;
using System.Windows.Input;
using IHaveIssues.Mvvm;
using IHaveIssues.Services;
using IssuesKit;

namespace IHaveIssues.ViewModels;

/// <summary>
/// The window's state and commands: the open document, the sidebar list, and the current selection.
/// </summary>
/// <remarks>
/// Every dialog and file picker goes through <see cref="IMainInteraction"/>, so this type owns no
/// windows. It keeps <c>Document.Model.Issues</c> in step with the bound collection, which is what
/// keeps the dirty indicator accurate.
/// </remarks>
public sealed class MainViewModel : ObservableObject
{
    private const string IssuesFilter = "Issues document (*.issues)|*.issues|All files (*.*)|*.*";
    private const string MarkdownFilter = "Markdown (*.md)|*.md;*.markdown|All files (*.*)|*.*";

    private readonly IMainInteraction _interaction;

    private Issue? _selectedIssue;
    private IssueDetailViewModel? _detail;

    public MainViewModel(IMainInteraction interaction)
    {
        _interaction = interaction ?? throw new ArgumentNullException(nameof(interaction));

        Document = IssuesDocument.New();
        Document.PropertyChanged += OnDocumentChanged;
        Issues = [];
        IssuesView = BuildView(Issues);

        NewDocumentCommand = new RelayCommand(NewDocument);
        OpenCommand = new RelayCommand(Open);
        SaveCommand = new RelayCommand(() => Save());
        SaveAsCommand = new RelayCommand(() => SaveAs());
        ImportMarkdownCommand = new RelayCommand(ImportMarkdown);
        ExportMarkdownCommand = new RelayCommand(ExportMarkdown);
        ExitCommand = new RelayCommand(() => _interaction.Shutdown());
        AddIssueCommand = new RelayCommand(AddIssue);
        EditIssueCommand = new RelayCommand(EditSelected, () => HasSelection);
        DeleteIssueCommand = new RelayCommand(DeleteSelected, () => HasSelection);
        ProjectSettingsCommand = new RelayCommand(ShowProjectSettings);
        GitHubSyncCommand = new RelayCommand(SyncToGitHub);
    }

    // MARK: - State

    /// <summary>The open document.</summary>
    public IssuesDocument Document { get; private set; }

    /// <summary>The sidebar's backing collection, kept in step with <c>Document.Model.Issues</c>.</summary>
    public ObservableCollection<Issue> Issues { get; }

    /// <summary>The sidebar's grouped, sorted view: open entries first, then resolved.</summary>
    public ICollectionView IssuesView { get; }

    public Issue? SelectedIssue
    {
        get => _selectedIssue;
        set
        {
            if (SetProperty(ref _selectedIssue, value))
            {
                Detail = value is null ? null : new IssueDetailViewModel(value, Document.Model);
                OnPropertyChanged(nameof(HasSelection));
            }
        }
    }

    /// <summary>The selected issue rendered for the detail pane, or <c>null</c> when nothing is selected.</summary>
    public IssueDetailViewModel? Detail
    {
        get => _detail;
        private set => SetProperty(ref _detail, value);
    }

    public bool HasSelection => _selectedIssue is not null;

    /// <summary>Whether the document holds no issues at all, so the sidebar can explain itself.</summary>
    public bool IsEmpty => Issues.Count == 0;

    public string WindowTitle
    {
        get
        {
            var project = Document.Model.Project.Name.Trim();
            var name = project.Length == 0 ? Document.DisplayName : project;
            return $"{(Document.IsDirty ? "• " : string.Empty)}{name} — I Have Issues";
        }
    }

    // MARK: - Commands

    public ICommand NewDocumentCommand { get; }
    public ICommand OpenCommand { get; }
    public ICommand SaveCommand { get; }
    public ICommand SaveAsCommand { get; }
    public ICommand ImportMarkdownCommand { get; }
    public ICommand ExportMarkdownCommand { get; }
    public ICommand ExitCommand { get; }
    public ICommand AddIssueCommand { get; }
    public ICommand EditIssueCommand { get; }
    public ICommand DeleteIssueCommand { get; }
    public ICommand ProjectSettingsCommand { get; }
    public ICommand GitHubSyncCommand { get; }

    // MARK: - Document lifecycle

    /// <summary>
    /// Offers to save unsaved work before the document is replaced or the window closes.
    /// </summary>
    /// <returns><c>true</c> when it is safe to carry on.</returns>
    public bool ConfirmClose()
    {
        if (!Document.IsDirty)
        {
            return true;
        }

        return _interaction.ConfirmDiscardChanges() switch
        {
            true => Save(),
            false => true,
            null => false
        };
    }

    private void NewDocument()
    {
        if (!ConfirmClose())
        {
            return;
        }

        LoadDocument(IssuesDocument.New());
    }

    private void Open()
    {
        if (!ConfirmClose())
        {
            return;
        }

        if (_interaction.PickOpenFile("Open Issues Document", IssuesFilter) is { } path)
        {
            OpenFile(path);
        }
    }

    /// <summary>
    /// Opens a document from a known path, reporting a failure rather than throwing. Used by the
    /// Open command and by a path passed on the command line.
    /// </summary>
    public void OpenFile(string path)
    {
        try
        {
            LoadDocument(IssuesDocument.Open(path));
        }
        catch (Exception error) when (error is IssuesException or IOException or UnauthorizedAccessException)
        {
            _interaction.ShowError("Open Failed", error.Message);
        }
    }

    /// <summary>Saves to the document's own path, falling back to Save As for an untitled document.</summary>
    /// <returns><c>true</c> when the document was written.</returns>
    public bool Save() => Document.FilePath is { } path ? Write(path) : SaveAs();

    /// <returns><c>true</c> when the document was written.</returns>
    public bool SaveAs()
    {
        var suggested = Document.FilePath is { } path ? Path.GetFileName(path) : ExportFileName + ".issues";
        return _interaction.PickSaveFile("Save Issues Document", IssuesFilter, suggested) is { } target
            && Write(target);
    }

    private bool Write(string path)
    {
        try
        {
            Document.Save(path);
            OnPropertyChanged(nameof(WindowTitle));
            return true;
        }
        catch (Exception error) when (error is IssuesException or IOException or UnauthorizedAccessException)
        {
            _interaction.ShowError("Save Failed", error.Message);
            return false;
        }
    }

    private void LoadDocument(IssuesDocument document)
    {
        Document.PropertyChanged -= OnDocumentChanged;
        Document = document;
        Document.PropertyChanged += OnDocumentChanged;
        OnPropertyChanged(nameof(Document));
        Reload();
    }

    /// <summary>Rebuilds the sidebar from the model, clearing the selection.</summary>
    private void Reload()
    {
        SelectedIssue = null;
        Issues.Clear();
        foreach (var issue in Document.Model.Issues)
        {
            Issues.Add(issue);
        }

        OnPropertyChanged(nameof(IsEmpty));
        OnPropertyChanged(nameof(WindowTitle));
    }

    // MARK: - Markdown

    /// <summary>The file name suggested by "Export as Markdown…", from the project name.</summary>
    private string ExportFileName
    {
        get
        {
            var project = Document.Model.Project.Name.Trim();
            return project.Length == 0 ? "Issues" : project;
        }
    }

    private void ExportMarkdown()
    {
        if (_interaction.PickSaveFile("Export as Markdown", MarkdownFilter, ExportFileName + ".md") is not { } path)
        {
            return;
        }

        try
        {
            Document.ExportMarkdown(path);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            _interaction.ShowError("Export Failed", error.Message);
        }
    }

    private void ImportMarkdown()
    {
        if (_interaction.PickOpenFile("Import Legacy Markdown", MarkdownFilter) is not { } path)
        {
            return;
        }

        if (!_interaction.Confirm(
                "Replace this document with the imported markdown?",
                "Every issue and setting in this document is discarded and replaced by the markdown file."))
        {
            return;
        }

        try
        {
            Document.Replace(IssuesDocument.ReadLegacyMarkdown(path));
            Reload();
        }
        catch (Exception error) when (error is IssuesException or IOException or UnauthorizedAccessException)
        {
            _interaction.ShowError("Import Failed", error.Message);
        }
    }

    // MARK: - Issue mutations

    private void AddIssue()
    {
        var draft = new Issue { Number = Document.Model.NextNumber, Reported = IssueDate.Today() };
        if (_interaction.EditIssue(draft) is { } saved)
        {
            Apply(saved);
        }
    }

    /// <summary>Opens the editor on the selection. Also reachable by double-clicking a row.</summary>
    public void EditSelected()
    {
        if (_selectedIssue is not { } issue)
        {
            return;
        }

        if (_interaction.EditIssue(issue) is { } saved)
        {
            Apply(saved);
        }
    }

    private void DeleteSelected()
    {
        if (_selectedIssue is not { } issue)
        {
            return;
        }

        if (!_interaction.Confirm(
                $"Delete {issue.DisplayNumber}?",
                "This removes the issue from the document. This cannot be undone."))
        {
            return;
        }

        Document.Model.Issues.RemoveAll(existing => existing.Uuid == issue.Uuid);

        var index = IndexInSidebar(issue.Uuid);
        if (index >= 0)
        {
            Issues.RemoveAt(index);
        }

        SelectedIssue = null;
        OnPropertyChanged(nameof(IsEmpty));
        Document.MarkChanged();
    }

    /// <summary>Inserts or replaces an edited issue and selects it, in both the model and the sidebar.</summary>
    private void Apply(Issue issue)
    {
        var modelIndex = Document.Model.Issues.FindIndex(existing => existing.Uuid == issue.Uuid);
        if (modelIndex >= 0)
        {
            Document.Model.Issues[modelIndex] = issue;
        }
        else
        {
            Document.Model.Issues.Add(issue);
        }

        var viewIndex = IndexInSidebar(issue.Uuid);
        if (viewIndex >= 0)
        {
            Issues[viewIndex] = issue;
        }
        else
        {
            Issues.Add(issue);
        }

        OnPropertyChanged(nameof(IsEmpty));
        Document.MarkChanged();
        SelectedIssue = issue;
    }

    // MARK: - Project settings and sync

    private void ShowProjectSettings()
    {
        if (_interaction.ShowProjectSettings(Document.Model))
        {
            Document.MarkChanged();
            OnPropertyChanged(nameof(WindowTitle));
        }
    }

    private void SyncToGitHub()
    {
        if (_interaction.ShowGitHubSync(Document.Model) is not { } updated)
        {
            return;
        }

        var selectedId = _selectedIssue?.Uuid;
        Document.Model.Issues = updated;
        Reload();

        if (selectedId is { } id)
        {
            SelectedIssue = Issues.FirstOrDefault(issue => issue.Uuid == id);
        }

        Document.MarkChanged();
    }

    // MARK: - Plumbing

    private int IndexInSidebar(Guid uuid)
    {
        for (var index = 0; index < Issues.Count; index++)
        {
            if (Issues[index].Uuid == uuid)
            {
                return index;
            }
        }

        return -1;
    }

    private void OnDocumentChanged(object? sender, PropertyChangedEventArgs e) =>
        OnPropertyChanged(nameof(WindowTitle));

    /// <summary>
    /// Open entries above resolved ones, each group in display-number order — the order the markdown
    /// export writes them in.
    /// </summary>
    private static ICollectionView BuildView(ObservableCollection<Issue> issues)
    {
        var view = CollectionViewSource.GetDefaultView(issues);
        view.GroupDescriptions.Add(new PropertyGroupDescription(nameof(Issue.IsResolved)));
        view.SortDescriptions.Add(new SortDescription(nameof(Issue.IsResolved), ListSortDirection.Ascending));
        view.SortDescriptions.Add(new SortDescription(nameof(Issue.Number), ListSortDirection.Ascending));
        return view;
    }
}
