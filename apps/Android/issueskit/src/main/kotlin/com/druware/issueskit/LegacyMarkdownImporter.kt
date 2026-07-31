package com.druware.issueskit

import java.time.Instant

/**
 * One-way importer for the legacy markdown issues format.
 *
 * Markdown was once the source of truth; it is now an export target (see
 * [IssuesMarkdownSerializer]) and this type is the migration path for files written before the
 * `.issues` JSON format existed. It also reads back everything the exporter emits.
 *
 * Importing is deliberately tolerant: missing metadata falls back to the enum defaults, a
 * malformed entry never aborts the whole import, and body content under an unrecognized
 * `**Header**` is appended to [Issue.notes] rather than dropped. Imported issues get fresh
 * [Issue.uuid] values and `createdAt`/`updatedAt` set to the import time, because the markdown
 * format records neither.
 *
 * Two layouts are accepted:
 * - **Standard**: has a `## Open` or `## Known Issues` section; issues use `### #NNN — Title`.
 * - **Free-form**: uses category headings like `## Bugs` / `## Enhancements`; issues use plain
 *   `### Title` or `### ~~Title~~ ✓ Fixed` headings (no number required).
 */
object LegacyMarkdownImporter {

    /**
     * Imports a legacy markdown document.
     *
     * @return a model whose [ExportSettings.preambleMarkdown] is the file's preamble.
     * @throws IssuesError.MissingOpenSection when the document has no level-2 headings at all and
     *   therefore cannot be interpreted as either layout.
     */
    fun importDocument(markdown: String): IssuesDocumentModel {
        val lines = markdown.split("\n")
        val now = Instant.now()

        val openIndex = lines.indexOfFirst {
            isLevel2Heading(it, "Open") || isLevel2Heading(it, "Known Issues")
        }
        if (openIndex >= 0) {
            val preamble = lines.subList(0, openIndex).joinToString("\n") +
                if (openIndex > 0) "\n" else ""
            return IssuesDocumentModel(
                export = ExportSettings(preamble),
                issues = collectIssues(lines, start = openIndex + 1, freeForm = false, now = now),
            )
        }

        // Fall back to free-form parsing when the file uses its own category headings
        // (e.g. ## Bugs / ## Enhancements) rather than the canonical ## Open section.
        if (lines.none { isLevel2Heading(it) }) throw IssuesError.MissingOpenSection

        return IssuesDocumentModel(
            export = ExportSettings(""),
            issues = collectIssues(lines, start = 0, freeForm = true, now = now),
        )
    }

    // MARK: - Line classification

    /** Whether the line is any level-2 (`## `) heading, used to terminate an issue block. */
    private fun isLevel2Heading(line: String): Boolean = line.trimSpaces().startsWith("## ")

    /** Whether the line is the specific `## <name>` heading. */
    private fun isLevel2Heading(line: String, name: String): Boolean = line.trimSpaces() == "## $name"

    /** Whether the line is a thematic break (`---` or longer), which separates document sections. */
    private fun isHorizontalRule(line: String): Boolean {
        val trimmed = line.trimSpaces()
        return trimmed.length >= 3 && trimmed.all { it == '-' }
    }

    private data class Heading(val number: Int?, val title: String, val isResolved: Boolean)

    /**
     * Parses a `### #NNN — Title` heading, tolerating a missing `#`, varied dashes, an optional
     * letter prefix such as `MLM-`, and no title. Returns `null` when the line is not a level-3
     * heading; returns a heading with a `null` number when it carries no numeric identifier.
     * Also detects the `~~strikethrough~~ ✓ Fixed` syntax used by free-form resolved entries.
     */
    private fun parseHeading(line: String): Heading? {
        if (!line.startsWith("### ")) return null
        val body = line.substring(4)

        var cursor = 0
        while (cursor < body.length && body[cursor] == ' ') cursor++
        if (cursor < body.length && body[cursor] == '#') cursor++

        // Skip an optional letter prefix such as "MLM-" before the issue number.
        val prefixStart = cursor
        while (cursor < body.length && body[cursor].isLetter()) cursor++
        if (cursor < body.length && body[cursor] == '-') cursor++ else cursor = prefixStart

        val digitsStart = cursor
        while (cursor < body.length && body[cursor].isDigit()) cursor++

        val number: Int?
        if (cursor > digitsStart) {
            number = body.substring(digitsStart, cursor).toIntOrNull()
        } else {
            number = null
            cursor = 0 // no numeric identifier; the title starts at the beginning
        }

        var titleStart = cursor
        while (titleStart < body.length && body[titleStart] in HEADING_SEPARATORS) titleStart++
        var rawTitle = body.substring(titleStart).trimSpaces()

        // Detect ~~strikethrough~~ titles used in free-form resolved entries.
        var isResolved = false
        if (rawTitle.startsWith("~~")) {
            val close = rawTitle.indexOf("~~", startIndex = 2)
            if (close >= 0) {
                val inner = rawTitle.substring(2, close)
                val suffix = rawTitle.substring(close + 2).trimSpaces()
                val lowered = suffix.lowercase()
                isResolved = suffix.startsWith('✓') || lowered.startsWith("fixed") ||
                    lowered.startsWith("closed") || suffix.isEmpty()
                rawTitle = inner
            }
        }

        return Heading(number, rawTitle, isResolved)
    }

    private val HEADING_SEPARATORS = setOf(' ', '\t', '-', '—', '–')

    /** Parses a `- **Key:** value` metadata bullet. */
    private fun parseMetadata(line: String): Pair<String, String>? {
        val trimmed = line.trimSpaces()
        if (!trimmed.startsWith("- ")) return null
        val afterDash = trimmed.substring(2)
        if (!afterDash.startsWith("**")) return null
        val afterStars = afterDash.substring(2)
        val close = afterStars.indexOf(":**")
        if (close < 0) return null
        return afterStars.substring(0, close) to afterStars.substring(close + 3).trimSpaces()
    }

    /** Parses a bold body-section header such as `**Description**` or `**Steps to reproduce** (bugs only)`. */
    private fun parseSectionHeader(line: String): String? {
        val trimmed = line.trimSpaces()
        if (!trimmed.startsWith("**")) return null
        val afterStars = trimmed.substring(2)
        val close = afterStars.indexOf("**")
        if (close < 0) return null
        return afterStars.substring(0, close)
    }

    // MARK: - Body assembly

    private sealed interface Section {
        data object None : Section
        data object Description : Section
        data object Steps : Section
        data object Environment : Section
        data object Notes : Section
        data object Resolution : Section
        data object Comments : Section
        data class Unknown(val header: String) : Section
    }

    private fun classify(header: String): Section = when (header.lowercase()) {
        "description" -> Section.Description
        "steps to reproduce" -> Section.Steps
        "environment" -> Section.Environment
        "notes / investigation", "notes/investigation", "notes" -> Section.Notes
        "resolution", "proposed fix" -> Section.Resolution
        "comments" -> Section.Comments
        else -> Section.Unknown(header)
    }

    // MARK: - Issue collection

    /**
     * Collects issues from [lines] starting at [start].
     *
     * In standard mode (`freeForm = false`) headings without a number are skipped. In free-form
     * mode numbers are auto-assigned and `~~strikethrough~~ ✓ Fixed` sets the resolved status.
     */
    private fun collectIssues(
        lines: List<String>,
        start: Int,
        freeForm: Boolean,
        now: Instant,
    ): List<Issue> {
        val issues = mutableListOf<Issue>()
        var nextAutoNumber = 1
        var currentSectionType = IssueType.TASK
        var index = start

        while (index < lines.size) {
            if (freeForm && isLevel2Heading(lines[index])) {
                currentSectionType = sectionType(lines[index])
                index++
                continue
            }
            val heading = parseHeading(lines[index])
            if (heading == null || (!freeForm && heading.number == null)) {
                index++
                continue
            }
            val number = heading.number ?: nextAutoNumber
            nextAutoNumber = maxOf(nextAutoNumber, number) + 1
            index++

            val body = mutableListOf<String>()
            while (index < lines.size &&
                parseHeading(lines[index]) == null &&
                !isLevel2Heading(lines[index]) &&
                !isHorizontalRule(lines[index])
            ) {
                body += lines[index]
                index++
            }

            val issue = makeIssue(
                number = number,
                title = heading.title,
                body = body,
                defaultType = if (freeForm) currentSectionType else IssueType.DEFAULT,
                now = now,
            )
            // Markdown carries no close timestamp, so `closedAt` stays null here rather than
            // inventing the import time as the moment the issue was resolved.
            issues += if (freeForm && heading.isResolved) {
                issue.copy(status = IssueStatus.RESOLVED)
            } else {
                issue
            }
        }
        return issues
    }

    /** Maps a `## Section Name` line to the corresponding issue type for free-form documents. */
    private fun sectionType(line: String): IssueType {
        val lower = line.trimSpaces().lowercase()
        return when {
            lower.startsWith("## bug") -> IssueType.BUG
            lower.startsWith("## enhancement") || lower.startsWith("## feature") -> IssueType.FEATURE
            lower.startsWith("## question") -> IssueType.QUESTION
            else -> IssueType.TASK
        }
    }

    private fun makeIssue(
        number: Int,
        title: String,
        body: List<String>,
        defaultType: IssueType,
        now: Instant,
    ): Issue {
        var issue = Issue(
            number = number,
            title = title,
            type = defaultType,
            reported = IssueDate.today(),
            createdAt = now,
            updatedAt = now,
        )

        var current: Section = Section.None
        val buffer = mutableListOf<String>()
        val extraParts = mutableListOf<String>()

        fun flush() {
            when (val section = current) {
                Section.None -> joinBody(buffer).takeIf { it.isNotEmpty() }?.let { extraParts += it }
                Section.Description -> issue = issue.copy(description = joinBody(buffer))
                Section.Steps -> issue = issue.copy(stepsToReproduce = parseSteps(buffer))
                Section.Environment -> issue = issue.copy(environment = joinBody(buffer))
                Section.Notes -> issue = issue.copy(notes = joinBody(buffer))
                Section.Resolution -> issue = issue.copy(resolution = joinBody(buffer))
                Section.Comments -> issue = issue.copy(comments = parseComments(buffer))
                is Section.Unknown -> {
                    val content = joinBody(buffer)
                    extraParts += if (content.isEmpty()) {
                        "**${section.header}**"
                    } else {
                        "**${section.header}**\n\n$content"
                    }
                }
            }
            buffer.clear()
        }

        for (line in body) {
            val header = parseSectionHeader(line)
            if (header != null) {
                flush()
                current = classify(header)
                continue
            }
            val meta = if (current == Section.None) parseMetadata(line) else null
            if (meta != null) issue = apply(meta, issue) else buffer += line
        }
        flush()

        // Content the markdown format has no dedicated slot for lands in Notes rather than being
        // dropped; the exporter re-emits it there, so the cycle is stable.
        val extras = extraParts.filter { it.isNotEmpty() }
        if (extras.isNotEmpty()) {
            val joined = extras.joinToString("\n\n")
            issue = issue.copy(notes = if (issue.notes.isEmpty()) joined else issue.notes + "\n\n" + joined)
        }
        return issue
    }

    private fun apply(meta: Pair<String, String>, issue: Issue): Issue {
        val (key, value) = meta
        return when (key.lowercase()) {
            "type" -> issue.copy(type = IssueType.fromDisplayName(value) ?: IssueType.DEFAULT)
            "priority", "severity" ->
                issue.copy(priority = IssuePriority.fromDisplayName(value) ?: IssuePriority.DEFAULT)
            "status" -> issue.copy(status = IssueStatus.fromDisplayName(value) ?: IssueStatus.DEFAULT)
            "reported" -> IssueDate.dateFrom(value)?.let { issue.copy(reported = it) } ?: issue
            "reported by" -> issue.copy(reportedBy = value)
            "area", "component" -> issue.copy(area = value)
            "labels" -> issue.copy(labels = splitList(value))
            "assignees" -> issue.copy(assignees = splitList(value))
            "milestone" -> issue.copy(milestone = value.ifEmpty { null })
            "estimate" -> value.toDoubleOrNull()?.let { issue.copy(estimate = it) } ?: issue
            "github" -> issue.copy(
                remoteLinks = issue.remoteLinks + RemoteLink(RemoteProvider.Github, identifier = value),
            )
            else -> issue
        }
    }

    // MARK: - Value parsing

    private fun splitList(value: String): List<String> =
        value.split(",").map { it.trimSpaces() }.filter { it.isNotEmpty() }

    /** Drops leading and trailing blank lines while keeping interior blanks. */
    private fun trimmingBlankEdges(lines: List<String>): List<String> =
        lines.dropWhile { it.trimSpaces().isEmpty() }.dropLastWhile { it.trimSpaces().isEmpty() }

    private fun joinBody(lines: List<String>): String = trimmingBlankEdges(lines).joinToString("\n")

    /** Extracts the text of a numbered list, dropping empty template placeholders. */
    private fun parseSteps(lines: List<String>): List<String> {
        val steps = mutableListOf<String>()
        for (raw in lines) {
            val trimmed = raw.trimSpaces()
            if (trimmed.isEmpty()) continue
            val dot = trimmed.indexOf('.')
            if (dot > 0 && trimmed.substring(0, dot).all { it.isDigit() }) {
                val text = trimmed.substring(dot + 1).trimSpaces()
                if (text.isNotEmpty()) steps += text
            } else if (steps.isNotEmpty()) {
                steps[steps.lastIndex] = steps.last() + " " + trimmed
            }
        }
        return steps
    }

    /**
     * Reads the `- **Author** (YYYY-MM-DD): body` bullets emitted by [IssuesMarkdownSerializer].
     * Lines that are not a bullet continue the preceding comment's body.
     */
    private fun parseComments(lines: List<String>): List<Comment> {
        val comments = mutableListOf<Comment>()
        for (raw in trimmingBlankEdges(lines)) {
            val comment = parseCommentBullet(raw)
            if (comment != null) {
                comments += comment
            } else if (comments.isNotEmpty()) {
                val continuation = if (raw.startsWith("  ")) raw.substring(2) else raw
                val last = comments.last()
                comments[comments.lastIndex] = last.copy(body = last.body + "\n" + continuation)
            }
        }
        return comments
    }

    private fun parseCommentBullet(line: String): Comment? {
        if (!line.startsWith("- **")) return null
        val afterBullet = line.substring(4)
        val closeStars = afterBullet.indexOf("**")
        if (closeStars < 0) return null
        val author = afterBullet.substring(0, closeStars)

        var remainder = afterBullet.substring(closeStars + 2).trimSpaces()
        var createdAt = IssueDate.today()
        if (remainder.startsWith("(")) {
            val closeParen = remainder.indexOf(')')
            if (closeParen >= 0) {
                IssueDate.dateFrom(remainder.substring(1, closeParen))?.let { createdAt = it }
                remainder = remainder.substring(closeParen + 1)
            }
        }
        if (remainder.startsWith(":")) remainder = remainder.substring(1)

        return Comment(author = author, createdAt = createdAt, body = remainder.trimSpaces())
    }
}
