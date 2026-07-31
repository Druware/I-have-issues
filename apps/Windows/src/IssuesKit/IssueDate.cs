using System.Globalization;

namespace IssuesKit;

/// <summary>
/// Fixed date handling for the <c>YYYY-MM-DD</c> reported dates in the document.
/// </summary>
/// <remarks>
/// A single invariant/UTC calendar and format guarantees that parsing then formatting a date is
/// a stable identity, which is what keeps issue round trips lossless.
/// </remarks>
public static class IssueDate
{
    private const string DayFormat = "yyyy-MM-dd";

    /// <summary>Parses a <c>YYYY-MM-DD</c> string into a day-normalized UTC date, or <c>null</c> if malformed.</summary>
    public static DateTimeOffset? Date(string? text)
    {
        if (text is null)
        {
            return null;
        }

        var parsed = DateTimeOffset.TryParseExact(
            text.Trim(),
            DayFormat,
            CultureInfo.InvariantCulture,
            DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal,
            out var value);

        return parsed ? value : null;
    }

    /// <summary>Formats a date as <c>YYYY-MM-DD</c> in UTC.</summary>
    public static string String(DateTimeOffset date) =>
        date.ToUniversalTime().ToString(DayFormat, CultureInfo.InvariantCulture);

    /// <summary>The current date normalized to the start of its day in UTC.</summary>
    public static DateTimeOffset Today()
    {
        var now = DateTimeOffset.UtcNow;
        return new DateTimeOffset(now.Year, now.Month, now.Day, 0, 0, 0, TimeSpan.Zero);
    }
}
