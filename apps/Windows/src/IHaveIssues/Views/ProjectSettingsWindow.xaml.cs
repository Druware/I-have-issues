using System.Windows;
using IHaveIssues.ViewModels;
using IssuesKit;

namespace IHaveIssues.Views;

/// <summary>
/// Per-document project identity and tracker coordinates. Edits a draft and only writes into the
/// document on Save.
/// </summary>
public partial class ProjectSettingsWindow : Window
{
    private readonly IssuesDocumentModel _model;
    private readonly ProjectSettingsViewModel _viewModel;

    public ProjectSettingsWindow(IssuesDocumentModel model)
    {
        _model = model;
        _viewModel = new ProjectSettingsViewModel(model);
        DataContext = _viewModel;
        InitializeComponent();
    }

    private void OnSave(object sender, RoutedEventArgs e)
    {
        _viewModel.Save(_model);
        DialogResult = true;
    }

    private void OnCancel(object sender, RoutedEventArgs e) => DialogResult = false;
}
