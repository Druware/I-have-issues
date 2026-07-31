import Foundation

/// One-way importer for the legacy markdown issues format.
///
/// Markdown was once the source of truth; it is now an export target (see
/// ``IssuesMarkdownSerializer``) and this type is the migration path for files written before
/// the `.issues` JSON format existed. It also reads back everything the exporter emits.
///
/// Importing is deliberately tolerant: missing metadata falls back to the enum defaults, a
/// malformed entry never aborts the whole import, and body content under an unrecognized
/// `**Header**` is appended to ``Issue/notes`` rather than dropped. Imported issues get fresh
/// ``Issue/uuid`` values and `createdAt`/`updatedAt` set to the import time, because the
/// markdown format records neither.
///
/// Two layouts are accepted:
/// - **Standard**: has a `## Open` or `## Known Issues` section; issues use `### #NNN — Title`.
/// - **Free-form**: uses category headings like `## Bugs` / `## Enhancements`; issues use plain
///   `### Title` or `### ~~Title~~ ✓ Fixed` headings (no number required).
public struct LegacyMarkdownImporter {
    public init() {}

    /// Imports a legacy markdown document.
    /// - Parameter markdown: The document text.
    /// - Returns: A model whose ``ExportSettings/preambleMarkdown`` is the file's preamble.
    /// - Throws: ``IssuesError/missingOpenSection`` when the document has no level-2 headings
    ///   at all and therefore cannot be interpreted as either layout.
    public func importDocument(from markdown: String) throws(IssuesError) -> IssuesDocumentModel {
        let lines = markdown.components(separatedBy: "\n")
        let now = Date()

        if let openIndex = lines.firstIndex(where: {
            Self.isLevel2Heading($0, named: "Open") || Self.isLevel2Heading($0, named: "Known Issues")
        }) {
            let preamble = lines[0..<openIndex].joined(separator: "\n") + (openIndex > 0 ? "\n" : "")
            return IssuesDocumentModel(
                export: ExportSettings(preambleMarkdown: preamble),
                issues: Self.collectIssues(from: lines, start: openIndex + 1, freeForm: false, now: now)
            )
        }

        // Fall back to free-form parsing when the file uses its own category headings
        // (e.g. ## Bugs / ## Enhancements) rather than the canonical ## Open section.
        guard lines.contains(where: { Self.isLevel2Heading($0) }) else {
            throw IssuesError.missingOpenSection
        }
        return IssuesDocumentModel(
            export: ExportSettings(preambleMarkdown: ""),
            issues: Self.collectIssues(from: lines, start: 0, freeForm: true, now: now)
        )
    }

    // MARK: - Line classification

    /// Whether the line is any level-2 (`## `) heading, used to terminate an issue block.
    private static func isLevel2Heading(_ line: String) -> Bool {
        line.trimmingCharacters(in: .whitespaces).hasPrefix("## ")
    }

    /// Whether the line is the specific `## <name>` heading.
    private static func isLevel2Heading(_ line: String, named name: String) -> Bool {
        line.trimmingCharacters(in: .whitespaces) == "## \(name)"
    }

    /// Whether the line is a thematic break (`---` or longer), which separates document sections.
    private static func isHorizontalRule(_ line: String) -> Bool {
        let trimmed = line.trimmingCharacters(in: .whitespaces)
        return trimmed.count >= 3 && trimmed.allSatisfy { $0 == "-" }
    }

    /// Parses a `### #NNN — Title` heading, tolerating a missing `#`, varied dashes, and no title.
    /// Returns `nil` when the line is not a level-3 heading; returns a non-nil tuple with a `nil`
    /// number when the heading carries no numeric identifier (free-form layout).
    /// Also detects the `~~strikethrough~~ ✓ Fixed` syntax used by free-form resolved entries.
    private static func parseHeading(_ line: String) -> (number: Int?, title: String, isResolved: Bool)? {
        guard line.hasPrefix("### ") else { return nil }
        let body = String(line.dropFirst(4))

        var cursor = body.startIndex
        while cursor < body.endIndex, body[cursor] == " " { cursor = body.index(after: cursor) }
        if cursor < body.endIndex, body[cursor] == "#" { cursor = body.index(after: cursor) }

        // Skip an optional letter prefix such as "MLM-" before the issue number.
        let prefixStart = cursor
        while cursor < body.endIndex, body[cursor].isLetter { cursor = body.index(after: cursor) }
        if cursor < body.endIndex, body[cursor] == "-" {
            cursor = body.index(after: cursor)
        } else {
            cursor = prefixStart
        }

        let digitsStart = cursor
        while cursor < body.endIndex, body[cursor].isNumber { cursor = body.index(after: cursor) }

        let number: Int?
        if cursor > digitsStart {
            number = Int(body[digitsStart..<cursor])
        } else {
            number = nil
            cursor = body.startIndex // no numeric identifier; the title starts at the beginning
        }

        let separators: Set<Character> = [" ", "\t", "-", "\u{2014}", "\u{2013}"]
        var titleStart = cursor
        while titleStart < body.endIndex, separators.contains(body[titleStart]) {
            titleStart = body.index(after: titleStart)
        }
        var rawTitle = String(body[titleStart...]).trimmingCharacters(in: .whitespaces)

        // Detect ~~strikethrough~~ titles used in free-form resolved entries.
        var isResolved = false
        if rawTitle.hasPrefix("~~") {
            let searchStart = rawTitle.index(rawTitle.startIndex, offsetBy: 2)
            if let closeRange = rawTitle.range(of: "~~", range: searchStart..<rawTitle.endIndex) {
                let inner = String(rawTitle[searchStart..<closeRange.lowerBound])
                let suffix = String(rawTitle[closeRange.upperBound...]).trimmingCharacters(in: .whitespaces)
                isResolved = suffix.hasPrefix("\u{2713}") || suffix.lowercased().hasPrefix("fixed")
                    || suffix.lowercased().hasPrefix("closed") || suffix.isEmpty
                rawTitle = inner
            }
        }

        return (number, rawTitle, isResolved)
    }

    /// Parses a `- **Key:** value` metadata bullet.
    private static func parseMetadata(_ line: String) -> (key: String, value: String)? {
        let trimmed = line.trimmingCharacters(in: .whitespaces)
        guard trimmed.hasPrefix("- ") else { return nil }
        let afterDash = trimmed.dropFirst(2)
        guard afterDash.hasPrefix("**") else { return nil }
        let afterStars = afterDash.dropFirst(2)
        guard let close = afterStars.range(of: ":**") else { return nil }
        let key = String(afterStars[..<close.lowerBound])
        let value = String(afterStars[close.upperBound...]).trimmingCharacters(in: .whitespaces)
        return (key, value)
    }

    /// Parses a bold body-section header such as `**Description**` or `**Steps to reproduce** (bugs only)`.
    private static func parseSectionHeader(_ line: String) -> String? {
        let trimmed = line.trimmingCharacters(in: .whitespaces)
        guard trimmed.hasPrefix("**") else { return nil }
        let afterStars = trimmed.dropFirst(2)
        guard let close = afterStars.range(of: "**") else { return nil }
        return String(afterStars[..<close.lowerBound])
    }

    // MARK: - Body assembly

    private enum Section {
        case none
        case description
        case steps
        case environment
        case notes
        case resolution
        case comments
        case unknown(String)
    }

    private static func classify(_ header: String) -> Section {
        switch header.lowercased() {
        case "description": .description
        case "steps to reproduce": .steps
        case "environment": .environment
        case "notes / investigation", "notes/investigation", "notes": .notes
        case "resolution", "proposed fix": .resolution
        case "comments": .comments
        default: .unknown(header)
        }
    }

    // MARK: - Issue collection

    /// Collects issues from `lines` starting at `start`.
    /// In standard mode (`freeForm: false`) headings without a number are skipped.
    /// In free-form mode numbers are auto-assigned and `~~strikethrough~~ ✓ Fixed` sets `.resolved`.
    private static func collectIssues(from lines: [String], start: Int, freeForm: Bool, now: Date) -> [Issue] {
        var issues: [Issue] = []
        var nextAutoNumber = 1
        var currentSectionType: IssueType = .task
        var index = start

        while index < lines.count {
            if freeForm, isLevel2Heading(lines[index]) {
                currentSectionType = sectionType(for: lines[index])
                index += 1
                continue
            }
            guard let heading = parseHeading(lines[index]) else {
                index += 1
                continue
            }
            guard freeForm || heading.number != nil else {
                index += 1
                continue
            }
            let number = heading.number ?? nextAutoNumber
            nextAutoNumber = max(nextAutoNumber, number) + 1
            index += 1
            var body: [String] = []
            while index < lines.count,
                  parseHeading(lines[index]) == nil,
                  !isLevel2Heading(lines[index]),
                  !isHorizontalRule(lines[index]) {
                body.append(lines[index])
                index += 1
            }
            var issue = makeIssue(
                number: number,
                title: heading.title,
                body: body,
                defaultType: freeForm ? currentSectionType : .default,
                now: now
            )
            if freeForm, heading.isResolved { issue.status = .resolved }
            issues.append(issue)
        }
        return issues
    }

    /// Maps a `## Section Name` line to the corresponding issue type for free-form documents.
    private static func sectionType(for line: String) -> IssueType {
        let lower = line.trimmingCharacters(in: .whitespaces).lowercased()
        if lower.hasPrefix("## bug") { return .bug }
        if lower.hasPrefix("## enhancement") || lower.hasPrefix("## feature") { return .feature }
        if lower.hasPrefix("## question") { return .question }
        return .task
    }

    private static func makeIssue(
        number: Int,
        title: String,
        body: [String],
        defaultType: IssueType,
        now: Date
    ) -> Issue {
        var issue = Issue(
            number: number,
            title: title,
            type: defaultType,
            reported: IssueDate.today(),
            createdAt: now,
            updatedAt: now
        )

        var current: Section = .none
        var buffer: [String] = []
        var extraParts: [String] = []

        func flush() {
            switch current {
            case .none:
                let content = joinBody(buffer)
                if !content.isEmpty { extraParts.append(content) }
            case .description:
                issue.description = joinBody(buffer)
            case .steps:
                issue.stepsToReproduce = parseSteps(buffer)
            case .environment:
                issue.environment = joinBody(buffer)
            case .notes:
                issue.notes = joinBody(buffer)
            case .resolution:
                issue.resolution = joinBody(buffer)
            case .comments:
                issue.comments = parseComments(buffer)
            case let .unknown(header):
                let content = joinBody(buffer)
                extraParts.append(content.isEmpty ? "**\(header)**" : "**\(header)**\n\n\(content)")
            }
            buffer.removeAll(keepingCapacity: true)
        }

        for line in body {
            if let header = parseSectionHeader(line) {
                flush()
                current = classify(header)
            } else if case .none = current, let meta = parseMetadata(line) {
                apply(meta, to: &issue)
            } else {
                buffer.append(line)
            }
        }
        flush()

        // Content the markdown format has no dedicated slot for lands in Notes rather than
        // being dropped; the exporter re-emits it there, so the cycle is stable.
        let extras = extraParts.filter { !$0.isEmpty }
        if !extras.isEmpty {
            let joined = extras.joined(separator: "\n\n")
            issue.notes = issue.notes.isEmpty ? joined : issue.notes + "\n\n" + joined
        }
        return issue
    }

    private static func apply(_ meta: (key: String, value: String), to issue: inout Issue) {
        switch meta.key.lowercased() {
        case "type":
            issue.type = IssueType(displayName: meta.value) ?? .default
        case "priority", "severity":
            issue.priority = IssuePriority(displayName: meta.value) ?? .default
        case "status":
            issue.status = IssueStatus(displayName: meta.value) ?? .default
        case "reported":
            if let date = IssueDate.date(from: meta.value) { issue.reported = date }
        case "reported by":
            issue.reportedBy = meta.value
        case "area", "component":
            issue.area = meta.value
        case "labels":
            issue.labels = splitList(meta.value)
        case "assignees":
            issue.assignees = splitList(meta.value)
        case "milestone":
            issue.milestone = meta.value.isEmpty ? nil : meta.value
        case "estimate":
            if let estimate = Double(meta.value) { issue.estimate = estimate }
        case "github":
            issue.remoteLinks.append(RemoteLink(provider: .github, identifier: meta.value))
        default:
            break
        }
    }

    // MARK: - Value parsing

    private static func splitList(_ value: String) -> [String] {
        value.components(separatedBy: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    /// Drops leading and trailing blank lines while keeping interior blanks.
    private static func trimmingBlankEdges(_ lines: [String]) -> [String] {
        var lines = lines
        while let first = lines.first, first.trimmingCharacters(in: .whitespaces).isEmpty {
            lines.removeFirst()
        }
        while let last = lines.last, last.trimmingCharacters(in: .whitespaces).isEmpty {
            lines.removeLast()
        }
        return lines
    }

    private static func joinBody(_ lines: [String]) -> String {
        trimmingBlankEdges(lines).joined(separator: "\n")
    }

    /// Extracts the text of a numbered list, dropping empty template placeholders.
    private static func parseSteps(_ lines: [String]) -> [String] {
        var steps: [String] = []
        for raw in lines {
            let trimmed = raw.trimmingCharacters(in: .whitespaces)
            guard !trimmed.isEmpty else { continue }
            if let dot = trimmed.firstIndex(of: "."),
               dot > trimmed.startIndex,
               trimmed[trimmed.startIndex..<dot].allSatisfy(\.isNumber) {
                let text = String(trimmed[trimmed.index(after: dot)...]).trimmingCharacters(in: .whitespaces)
                if !text.isEmpty { steps.append(text) }
            } else if !steps.isEmpty {
                steps[steps.count - 1] += " " + trimmed
            }
        }
        return steps
    }

    /// Reads the `- **Author** (YYYY-MM-DD): body` bullets emitted by ``IssuesMarkdownSerializer``.
    /// Lines that are not a bullet continue the preceding comment's body.
    private static func parseComments(_ lines: [String]) -> [Comment] {
        var comments: [Comment] = []
        for raw in trimmingBlankEdges(lines) {
            if let comment = parseCommentBullet(raw) {
                comments.append(comment)
            } else if !comments.isEmpty {
                let continuation = raw.hasPrefix("  ") ? String(raw.dropFirst(2)) : raw
                comments[comments.count - 1].body += "\n" + continuation
            }
        }
        return comments
    }

    private static func parseCommentBullet(_ line: String) -> Comment? {
        guard line.hasPrefix("- **") else { return nil }
        let afterBullet = line.dropFirst(4)
        guard let closeStars = afterBullet.range(of: "**") else { return nil }
        let author = String(afterBullet[..<closeStars.lowerBound])

        var remainder = String(afterBullet[closeStars.upperBound...]).trimmingCharacters(in: .whitespaces)
        var createdAt = IssueDate.today()
        if remainder.hasPrefix("("), let closeParen = remainder.firstIndex(of: ")") {
            let text = String(remainder[remainder.index(after: remainder.startIndex)..<closeParen])
            if let date = IssueDate.date(from: text) { createdAt = date }
            remainder = String(remainder[remainder.index(after: closeParen)...])
        }
        if remainder.hasPrefix(":") { remainder = String(remainder.dropFirst()) }

        return Comment(author: author, createdAt: createdAt, body: remainder.trimmingCharacters(in: .whitespaces))
    }
}
