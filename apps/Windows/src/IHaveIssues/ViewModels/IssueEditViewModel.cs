using System.Globalization;
using IHaveIssues.Mvvm;
using IssuesKit;

namespace IHaveIssues.ViewModels;

/// <summary>One entry in the resolution picker; <see cref="Value"/> is <c>null</c> for "None".</summary>
public sealed record ResolutionOption(string Text, ResolutionKind? Value);

/// <summary>
/// The editor for a single issue.
/// </summary>
/// <remarks>
/// Edits a copy and only produces an <see cref="Issue"/> from <see cref="Build"/>, so cancelling
/// discards everything. List-valued fields are edited as free text and split on save, matching the
/// Apple build's form.
/// </remarks>
public sealed class IssueEditViewModel : ObservableObject
{
    private readonly Issue _draft;

    private string _title;
    private IssueType _type;
    private IssuePriority _priority;
    private IssueStatus _status;
    private ResolutionKind? _resolutionKind;
    private DateTime _reported;
    private string _reportedBy;
    private string _area;
    private string _milestone;
    private string _estimate;
    private string _labels;
    private string _assignees;
    private string _description;
    private string _steps;
    private string _environment;
    private string _notes;
    private string _resolution;

    public IssueEditViewModel(Issue issue)
    {
        ArgumentNullException.ThrowIfNull(issue);

        _draft = issue.Copy();

        _title = _draft.Title;
        _type = _draft.Type;
        _priority = _draft.Priority;
        _status = _draft.Status;
        _resolutionKind = _draft.ResolutionKind;

        // The document stores a reported date as a calendar day at UTC midnight (see IssueDate), so
        // the picker reads and writes that same day with no zone attached — interpreting it locally
        // would shift the displayed day, and writing back a local midnight would save the wrong day.
        _reported = DateTime.SpecifyKind(_draft.Reported.UtcDateTime.Date, DateTimeKind.Unspecified);

        _reportedBy = _draft.ReportedBy;
        _area = _draft.Area;
        _milestone = _draft.Milestone ?? string.Empty;
        _estimate = _draft.Estimate is { } estimate ? FormatEstimate(estimate) : string.Empty;
        _labels = string.Join(", ", _draft.Labels);
        _assignees = string.Join(", ", _draft.Assignees);
        _description = _draft.Description;
        _steps = string.Join(Environment.NewLine, _draft.StepsToReproduce);
        _environment = _draft.Environment;
        _notes = _draft.Notes;
        _resolution = _draft.Resolution;
    }

    /// <summary>The window title, e.g. <c>#007</c>.</summary>
    public string DisplayNumber => _draft.DisplayNumber;

    public string Title { get => _title; set => SetProperty(ref _title, value); }

    public IssueType Type { get => _type; set => SetProperty(ref _type, value); }

    public IssuePriority Priority { get => _priority; set => SetProperty(ref _priority, value); }

    public IssueStatus Status { get => _status; set => SetProperty(ref _status, value); }

    public ResolutionKind? ResolutionKind
    {
        get => _resolutionKind;
        set => SetProperty(ref _resolutionKind, value);
    }

    public DateTime Reported { get => _reported; set => SetProperty(ref _reported, value); }

    public string ReportedBy { get => _reportedBy; set => SetProperty(ref _reportedBy, value); }

    public string Area { get => _area; set => SetProperty(ref _area, value); }

    public string Milestone { get => _milestone; set => SetProperty(ref _milestone, value); }

    public string Estimate { get => _estimate; set => SetProperty(ref _estimate, value); }

    public string Labels { get => _labels; set => SetProperty(ref _labels, value); }

    public string Assignees { get => _assignees; set => SetProperty(ref _assignees, value); }

    public string Description { get => _description; set => SetProperty(ref _description, value); }

    public string Steps { get => _steps; set => SetProperty(ref _steps, value); }

    public string EnvironmentText { get => _environment; set => SetProperty(ref _environment, value); }

    public string Notes { get => _notes; set => SetProperty(ref _notes, value); }

    public string Resolution { get => _resolution; set => SetProperty(ref _resolution, value); }

    // MARK: - Picker sources

    public IReadOnlyList<IssueType> Types { get; } = Enum.GetValues<IssueType>();

    public IReadOnlyList<IssuePriority> Priorities { get; } = Enum.GetValues<IssuePriority>();

    public IReadOnlyList<IssueStatus> Statuses { get; } = Enum.GetValues<IssueStatus>();

    /// <summary>Resolution has no default — an issue that is still open has none.</summary>
    public IReadOnlyList<ResolutionOption> ResolutionKinds { get; } =
    [
        new ResolutionOption("None", null),
        .. Enum.GetValues<IssuesKit.ResolutionKind>()
            .Select(kind => new ResolutionOption(kind.DisplayName(), kind))
    ];

    // MARK: - Result

    /// <summary>Produces the edited issue. The caller decides whether to keep it.</summary>
    public Issue Build()
    {
        var result = _draft.Copy();

        result.Title = Title;
        result.Type = Type;
        result.Priority = Priority;
        result.Status = Status;
        result.ResolutionKind = ResolutionKind;
        result.Reported = new DateTimeOffset(Reported.Date, TimeSpan.Zero);
        result.ReportedBy = ReportedBy;
        result.Area = Area;
        result.Description = Description;
        result.Environment = EnvironmentText;
        result.Notes = Notes;
        result.Resolution = Resolution;

        result.StepsToReproduce = SplitEntries(Steps, '\n');
        result.Labels = SplitEntries(Labels, ',');
        result.Assignees = SplitEntries(Assignees, ',');

        var milestone = Milestone.Trim();
        result.Milestone = milestone.Length == 0 ? null : milestone;

        result.Estimate = double.TryParse(
            Estimate.Trim(),
            NumberStyles.Float,
            CultureInfo.InvariantCulture,
            out var estimate)
            ? estimate
            : null;

        result.UpdatedAt = DateTimeOffset.UtcNow;
        return result;
    }

    /// <summary>Splits a free-text field into trimmed, non-empty entries.</summary>
    private static List<string> SplitEntries(string text, char separator) =>
        [.. text.Split(separator).Select(entry => entry.Trim()).Where(entry => entry.Length > 0)];

    /// <summary>
    /// Renders an estimate the way it is parsed back, so editing round-trips regardless of locale —
    /// a culture-formatted string would not.
    /// </summary>
    private static string FormatEstimate(double value) => value.ToString("R", CultureInfo.InvariantCulture);
}
