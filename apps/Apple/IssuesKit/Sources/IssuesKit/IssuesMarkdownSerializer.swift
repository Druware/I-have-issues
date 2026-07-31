import Foundation

/// Exports an ``IssuesDocumentModel`` as markdown.
///
/// Markdown is an **export** format, not the source of truth — the `.issues` JSON is (see
/// ``IssuesJSONCoder``). The export keeps the shape of the original hand-authored template so
/// generated files still read naturally in a repository: ``ExportSettings/preambleMarkdown``
/// verbatim, then `## Open` and `## Resolved` sections built from ``Issue/isResolved``.
///
/// Fields the template has no slot for (stable UUIDs, timestamps, relations, remote links)
/// are omitted. ``LegacyMarkdownImporter`` can read back everything this emits.
public enum IssuesMarkdownSerializer {
    /// Renders a model as a complete markdown document.
    public static func export(_ model: IssuesDocumentModel) -> String {
        let openBody = sectionBody(for: model.openIssues, emptyText: "_No open issues._")
        let resolvedBody = sectionBody(for: model.resolvedIssues, emptyText: "_No resolved issues._")

        return model.export.preambleMarkdown
            + "## Open\n\n"
            + openBody
            + "\n\n---\n\n## Resolved\n\n"
            + resolvedBody
            + "\n"
    }

    // MARK: - Sections

    private static func sectionBody(for issues: [Issue], emptyText: String) -> String {
        guard !issues.isEmpty else { return emptyText }
        return issues.map(export(issue:)).joined(separator: "\n\n")
    }

    private static func export(issue: Issue) -> String {
        var lines: [String] = []
        lines.append("### #\(formatNumber(issue.number)) \u{2014} \(issue.title)")
        lines.append("")
        lines.append("- **Type:** \(issue.type.displayName)")
        lines.append("- **Priority:** \(issue.priority.displayName)")
        lines.append("- **Status:** \(issue.status.displayName)")
        lines.append("- **Reported:** \(IssueDate.string(from: issue.reported))")
        lines.append("- **Reported by:** \(issue.reportedBy)")
        lines.append("- **Area:** \(issue.area)")
        appendMetadata(&lines, key: "Labels", value: issue.labels.joined(separator: ", "))
        appendMetadata(&lines, key: "Assignees", value: issue.assignees.joined(separator: ", "))
        appendMetadata(&lines, key: "Milestone", value: issue.milestone ?? "")
        if let estimate = issue.estimate {
            lines.append("- **Estimate:** \(format(estimate: estimate))")
        }

        appendTextSection(&lines, header: "Description", content: issue.description)
        appendSteps(&lines, steps: issue.stepsToReproduce)
        appendTextSection(&lines, header: "Environment", content: issue.environment)
        appendTextSection(&lines, header: "Notes / Investigation", content: issue.notes)
        appendTextSection(&lines, header: "Resolution", content: issue.resolution)
        appendComments(&lines, comments: issue.comments)

        return lines.joined(separator: "\n")
    }

    // MARK: - Pieces

    private static func appendMetadata(_ lines: inout [String], key: String, value: String) {
        guard !value.isEmpty else { return }
        lines.append("- **\(key):** \(value)")
    }

    private static func appendTextSection(_ lines: inout [String], header: String, content: String) {
        guard !content.isEmpty else { return }
        lines.append("")
        lines.append("**\(header)**")
        lines.append("")
        lines.append(contentsOf: content.components(separatedBy: "\n"))
    }

    private static func appendSteps(_ lines: inout [String], steps: [String]) {
        guard !steps.isEmpty else { return }
        lines.append("")
        lines.append("**Steps to reproduce**")
        lines.append("")
        for (offset, step) in steps.enumerated() {
            lines.append("\(offset + 1). \(step)")
        }
    }

    /// Emits one bullet per comment. Continuation lines are indented two spaces, which is how
    /// ``LegacyMarkdownImporter`` recognizes them as belonging to the preceding comment.
    private static func appendComments(_ lines: inout [String], comments: [Comment]) {
        guard !comments.isEmpty else { return }
        lines.append("")
        lines.append("**Comments**")
        lines.append("")
        for comment in comments {
            let bodyLines = comment.body.components(separatedBy: "\n")
            let date = IssueDate.string(from: comment.createdAt)
            lines.append("- **\(comment.author)** (\(date)): \(bodyLines.first ?? "")")
            for continuation in bodyLines.dropFirst() {
                lines.append("  \(continuation)")
            }
        }
    }

    // MARK: - Formatting

    private static func formatNumber(_ number: Int) -> String {
        String(format: "%03d", number)
    }

    /// Renders whole estimates without a decimal point so `3` survives an export/import cycle.
    private static func format(estimate: Double) -> String {
        guard estimate.rounded() == estimate, let whole = Int(exactly: estimate.rounded()) else {
            return String(estimate)
        }
        return String(whole)
    }
}
