import Foundation

/// Fixed date handling for the `YYYY-MM-DD` reported dates in the document.
///
/// A single POSIX/GMT calendar and formatter guarantees that parsing then formatting a
/// date is a stable identity, which is what keeps issue round trips lossless.
public enum IssueDate {
    static let calendar: Calendar = {
        var calendar = Calendar(identifier: .gregorian)
        // Force-unwrap is a genuine programmer error here: "GMT" is a guaranteed identifier.
        guard let zone = TimeZone(identifier: "GMT") else {
            preconditionFailure("GMT is always a valid time-zone identifier")
        }
        calendar.timeZone = zone
        return calendar
    }()

    static let formatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.calendar = calendar
        formatter.timeZone = calendar.timeZone
        formatter.dateFormat = "yyyy-MM-dd"
        return formatter
    }()

    /// Parses a `YYYY-MM-DD` string into a day-normalized date, or `nil` if malformed.
    public static func date(from text: String) -> Date? {
        formatter.date(from: text.trimmingCharacters(in: .whitespaces))
    }

    /// Formats a date as `YYYY-MM-DD`.
    public static func string(from date: Date) -> String {
        formatter.string(from: date)
    }

    /// The current date normalized to the start of its day in the fixed calendar.
    public static func today() -> Date {
        calendar.startOfDay(for: Date())
    }
}
