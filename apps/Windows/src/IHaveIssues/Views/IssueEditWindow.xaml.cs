using System.Windows;
using IHaveIssues.ViewModels;
using IssuesKit;

namespace IHaveIssues.Views;

/// <summary>
/// A modal editor for a single issue. It edits a local draft and only produces a result on Save,
/// so cancelling discards changes.
/// </summary>
public partial class IssueEditWindow : Window
{
    private readonly IssueEditViewModel _viewModel;

    public IssueEditWindow(Issue issue)
    {
        _viewModel = new IssueEditViewModel(issue);
        DataContext = _viewModel;
        InitializeComponent();
    }

    /// <summary>The edited issue, set only when the user saved.</summary>
    public Issue? Result { get; private set; }

    private void OnSave(object sender, RoutedEventArgs e)
    {
        Result = _viewModel.Build();
        DialogResult = true;
    }

    private void OnCancel(object sender, RoutedEventArgs e) => DialogResult = false;
}
