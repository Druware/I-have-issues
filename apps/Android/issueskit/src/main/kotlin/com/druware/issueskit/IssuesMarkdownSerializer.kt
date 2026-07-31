package com.druware.issueskit

import java.util.Locale

/**
 * Exports an [IssuesDocumentModel] as markdown.
 *
 * Markdown is an **export** format, not the source of truth — the `.issues` JSON is (see
 * [IssuesJSONCoder]). The export keeps the shape of the original hand-authored template so
 * generated files still read naturally in a repository: [ExportSettings.preambleMarkdown]
 * verbatim, then `## Open` and `## Resolved` sections built from [Issue.isResolved].
 *
 * Fields the template has no slot for (stable UUIDs, timestamps, resolution kind, relations,
 * remote links) are omitted. [LegacyMarkdownImporter] can read back everything this emits.
 */
object IssuesMarkdownSerializer {

    /** Renders a model as a complete markdown document. */
    fun export(model: IssuesDocumentModel): String {
        val openBody = sectionBody(model.openIssues, "_No open issues._")
        val resolvedBody = sectionBody(model.resolvedIssues, "_No resolved issues._")

        return model.export.preambleMarkdown +
            "## Open\n\n" +
            openBody +
            "\n\n---\n\n## Resolved\n\n" +
            resolvedBody +
            "\n"
    }

    // MARK: - Sections

    private fun sectionBody(issues: List<Issue>, emptyText: String): String =
        if (issues.isEmpty()) emptyText else issues.joinToString("\n\n") { export(it) }

    private fun export(issue: Issue): String {
        val lines = mutableListOf<String>()
        lines += "### #${formatNumber(issue.number)} — ${issue.title}"
        lines += ""
        lines += "- **Type:** ${issue.type.displayName}"
        lines += "- **Priority:** ${issue.priority.displayName}"
        lines += "- **Status:** ${issue.status.displayName}"
        lines += "- **Reported:** ${IssueDate.stringFrom(issue.reported)}"
        lines += "- **Reported by:** ${issue.reportedBy}"
        lines += "- **Area:** ${issue.area}"
        lines.appendMetadata("Labels", issue.labels.joinToString(", "))
        lines.appendMetadata("Assignees", issue.assignees.joinToString(", "))
        lines.appendMetadata("Milestone", issue.milestone ?: "")
        issue.estimate?.let { lines += "- **Estimate:** ${formatEstimate(it)}" }

        lines.appendTextSection("Description", issue.description)
        lines.appendSteps(issue.stepsToReproduce)
        lines.appendTextSection("Environment", issue.environment)
        lines.appendTextSection("Notes / Investigation", issue.notes)
        lines.appendTextSection("Resolution", issue.resolution)
        lines.appendComments(issue.comments)

        return lines.joinToString("\n")
    }

    // MARK: - Pieces

    private fun MutableList<String>.appendMetadata(key: String, value: String) {
        if (value.isEmpty()) return
        this += "- **$key:** $value"
    }

    private fun MutableList<String>.appendTextSection(header: String, content: String) {
        if (content.isEmpty()) return
        this += ""
        this += "**$header**"
        this += ""
        this += content.split("\n")
    }

    private fun MutableList<String>.appendSteps(steps: List<String>) {
        if (steps.isEmpty()) return
        this += ""
        this += "**Steps to reproduce**"
        this += ""
        steps.forEachIndexed { offset, step -> this += "${offset + 1}. $step" }
    }

    /**
     * Emits one bullet per comment. Continuation lines are indented two spaces, which is how
     * [LegacyMarkdownImporter] recognizes them as belonging to the preceding comment.
     */
    private fun MutableList<String>.appendComments(comments: List<Comment>) {
        if (comments.isEmpty()) return
        this += ""
        this += "**Comments**"
        this += ""
        for (comment in comments) {
            val bodyLines = comment.body.split("\n")
            val date = IssueDate.stringFrom(comment.createdAt)
            this += "- **${comment.author}** ($date): ${bodyLines.firstOrNull() ?: ""}"
            bodyLines.drop(1).forEach { this += "  $it" }
        }
    }

    // MARK: - Formatting

    private fun formatNumber(number: Int): String = "%03d".format(Locale.ROOT, number)

    /** Renders whole estimates without a decimal point so `3` survives an export/import cycle. */
    private fun formatEstimate(estimate: Double): String {
        val rounded = Math.rint(estimate)
        if (rounded != estimate || !estimate.isFinite() ||
            estimate < Long.MIN_VALUE.toDouble() || estimate > Long.MAX_VALUE.toDouble()
        ) {
            return estimate.toString()
        }
        return estimate.toLong().toString()
    }
}
