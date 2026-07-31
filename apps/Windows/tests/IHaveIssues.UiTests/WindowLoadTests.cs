using System.Windows;
using IHaveIssues.ViewModels;
using IHaveIssues.Views;
using IssuesKit;

namespace IHaveIssues.UiTests;

/// <summary>
/// Constructs every window so its XAML is parsed and its resource references are resolved.
/// </summary>
/// <remarks>
/// A successful build proves nothing about XAML: resource keys, converter references, and property
/// values are resolved when a window is constructed, not when it is compiled. The three dialogs are
/// only constructed when the user opens them, so without this test a mistake in one of them would
/// first surface as a crash in front of the user. This caught two real defects — an invalid
/// <c>FontFamily</c> value and a globalization setting incompatible with WPF.
/// <para>
/// <b>Not covered:</b> expanding the item and detail <c>DataTemplate</c>s, which needs a laid-out
/// window. This test host has no interactive desktop, so a window never lays itself out and a
/// running <see cref="Application"/> never returns from <c>Run</c>. Template expansion is verified
/// by launching the app against a real document instead.
/// </para>
/// <para>
/// Everything happens in one test on one thread on purpose: WPF allows a single
/// <see cref="Application"/> per process, and its objects have thread affinity.
/// </para>
/// </remarks>
public class WindowLoadTests
{
    [Fact]
    public void EveryWindowConstructsWithoutXamlErrors() => RunOnStaThread(() =>
    {
        // Application.Current must exist first: the windows resolve their styles out of the
        // application-level resource dictionary, and ThemeMode pulls in the Fluent theme.
        var app = new App();
        app.InitializeComponent();
        Assert.NotNull(Application.Current);

        var model = SampleModel();

        Close(new MainWindow());
        Close(new IssueEditWindow(model.Issues[1]));
        Close(new ProjectSettingsWindow(model));
        Close(new GitHubSyncWindow(model));
    });

    /// <summary>
    /// The detail projection has no XAML in it, so it is verified directly: every optional row is
    /// present when populated, and the description and steps stay out of the section list so the
    /// view can order them the way the Apple build does.
    /// </summary>
    [Fact]
    public void DetailViewModelProjectsEveryPopulatedField()
    {
        var model = SampleModel();
        var detail = new IssueDetailViewModel(model.Issues[1], model);

        Assert.Equal("#002", detail.DisplayNumber);
        Assert.Equal("Rich", detail.Title);
        Assert.True(detail.HasResolutionKind);
        Assert.Equal("Fixed", detail.ResolutionKindText);

        Assert.Equal(
            ["Reported by", "Area", "Labels", "Assignees", "Milestone", "Estimate"],
            detail.OptionalMetadata.Select(row => row.Label));
        Assert.Equal("3.5", detail.OptionalMetadata.Single(row => row.Label == "Estimate").Value);

        // Description and steps are exposed separately; the section list is the remainder.
        Assert.Equal("Body", detail.Description);
        Assert.Equal(["1.", "2."], detail.Steps.Select(step => step.Ordinal));
        Assert.Equal(["Environment", "Notes / Investigation", "Resolution"], detail.Sections.Select(s => s.Title));

        Assert.Equal("sam", detail.Comments.Single().Author);
        Assert.Equal("Blocks", detail.Relations.Single().Kind);
        Assert.Equal("#001 Target", detail.Relations.Single().Target);
        Assert.Equal("GitHub 412", detail.RemoteLinks.Single().Label);
        Assert.NotNull(detail.RemoteLinks.Single().Url);
    }

    /// <summary>An unpopulated issue shows no empty rows at all.</summary>
    [Fact]
    public void DetailViewModelOmitsEmptyFields()
    {
        var model = new IssuesDocumentModel { Issues = [new Issue { Number = 1 }] };
        var detail = new IssueDetailViewModel(model.Issues[0], model);

        Assert.Equal("Untitled", detail.Title);
        Assert.False(detail.HasResolutionKind);
        Assert.Empty(detail.OptionalMetadata);
        Assert.Empty(detail.Sections);
        Assert.Empty(detail.Steps);
        Assert.Empty(detail.Comments);
        Assert.Empty(detail.Relations);
        Assert.Empty(detail.RemoteLinks);
    }

    /// <summary>A relation pointing at a deleted issue reads as missing rather than crashing.</summary>
    [Fact]
    public void DetailViewModelReportsADanglingRelation()
    {
        var issue = new Issue { Number = 1, Relations = [new Relation(RelationKind.Blocks, Guid.NewGuid())] };
        var model = new IssuesDocumentModel { Issues = [issue] };

        Assert.Equal("Missing issue", new IssueDetailViewModel(issue, model).Relations.Single().Target);
    }

    /// <summary>
    /// A model exercising the branches the views template over: both integrations configured, and
    /// an issue carrying comments, relations, and a remote link.
    /// </summary>
    private static IssuesDocumentModel SampleModel()
    {
        var target = Guid.NewGuid();
        return new IssuesDocumentModel
        {
            Project = new ProjectInfo { Name = "Smoke", Summary = "Fixture" },
            Integrations = new IntegrationSettings
            {
                GitHub = new GitHubIntegration { Owner = "openbcm", Repository = "i-have-issues" },
                AzureDevOps = new AzureDevOpsIntegration { Organization = "openbcm", Project = "IHaveIssues" }
            },
            Issues =
            [
                new Issue { Uuid = target, Number = 1, Title = "Target" },
                new Issue
                {
                    Number = 2,
                    Title = "Rich",
                    Type = IssueType.Bug,
                    Priority = IssuePriority.Critical,
                    Status = IssueStatus.Resolved,
                    ResolutionKind = ResolutionKind.Fixed,
                    Labels = ["ui"],
                    Assignees = ["dru"],
                    Milestone = "v1.0",
                    Area = "Views",
                    Estimate = 3.5,
                    ReportedBy = "dru",
                    Description = "Body",
                    StepsToReproduce = ["One", "Two"],
                    Environment = "Windows 11",
                    Notes = "Investigating",
                    Resolution = "Fixed it",
                    Comments = [new Comment { Author = "sam", Body = "Seen it" }],
                    Relations = [new Relation(RelationKind.Blocks, target)],
                    RemoteLinks =
                    [
                        new RemoteLink
                        {
                            Provider = RemoteProvider.GitHub,
                            Identifier = "412",
                            Url = new Uri("https://github.com/openbcm/i-have-issues/issues/412")
                        }
                    ]
                }
            ]
        };
    }

    private static void Close(Window window) => window.Close();

    /// <summary>Runs <paramref name="body"/> on an STA thread, rethrowing whatever it threw.</summary>
    private static void RunOnStaThread(Action body)
    {
        Exception? failure = null;
        var thread = new Thread(() =>
        {
            try
            {
                body();
            }
            catch (Exception error)
            {
                failure = error;
            }
        });

        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join();

        if (failure is not null)
        {
            throw new InvalidOperationException($"A window failed to load: {failure.Message}", failure);
        }
    }
}
