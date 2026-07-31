using System.Windows.Controls;
using System.Windows.Navigation;
using IHaveIssues.Presentation;

namespace IHaveIssues.Views;

public partial class IssueDetailView : UserControl
{
    public IssueDetailView() => InitializeComponent();

    /// <summary>
    /// Opens a remote link in the default browser. WPF hyperlinks do not navigate on their own, and
    /// the address comes from the document, so it goes through the same scheme check as a link in
    /// the markdown body.
    /// </summary>
    private void OnRequestNavigate(object sender, RequestNavigateEventArgs e)
    {
        ExternalLink.Open(e.Uri);
        e.Handled = true;
    }
}
