package com.druware.issueskit

import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import org.junit.jupiter.api.Test

class IssuesMarkdownSerializerTest {

    @Test
    fun `export routes issues to their sections`() {
        val model = IssuesDocumentModel(
            export = ExportSettings("# Issues\n\n"),
            issues = listOf(
                Issue(number = 1, title = "Open one", status = IssueStatus.OPEN),
                Issue(number = 2, title = "Done one", status = IssueStatus.RESOLVED),
            ),
        )
        val output = IssuesMarkdownSerializer.export(model)

        val openHeading = output.indexOf("## Open")
        val resolvedHeading = output.indexOf("## Resolved")
        val openEntry = output.indexOf("Open one")
        val doneEntry = output.indexOf("Done one")

        assertTrue(openEntry > openHeading)
        assertTrue(openEntry < resolvedHeading)
        assertTrue(doneEntry > resolvedHeading)
    }

    @Test
    fun `blocked and in-progress issues stay in the open section`() {
        val model = IssuesDocumentModel(
            issues = listOf(
                Issue(number = 1, title = "Blocked one", status = IssueStatus.BLOCKED),
                Issue(number = 2, title = "Busy one", status = IssueStatus.IN_PROGRESS),
            ),
        )
        assertEquals(2, model.openIssues.size)
        assertTrue(model.resolvedIssues.isEmpty())

        val output = IssuesMarkdownSerializer.export(model)
        assertContains(output, "_No resolved issues._")
    }

    @Test
    fun `export uses display names not raw values`() {
        val issue = Issue(
            number = 1,
            title = "Busy",
            type = IssueType.FEATURE,
            priority = IssuePriority.CRITICAL,
            status = IssueStatus.IN_PROGRESS,
        )
        val output = IssuesMarkdownSerializer.export(IssuesDocumentModel(issues = listOf(issue)))

        assertContains(output, "- **Status:** In Progress")
        assertContains(output, "- **Type:** Feature")
        assertContains(output, "- **Priority:** Critical")
        assertFalse(output.contains("inProgress"))
    }

    @Test
    fun `export emits optional fields when populated`() {
        val issue = Issue(
            number = 9,
            title = "Rich entry",
            labels = listOf("ui", "regression"),
            assignees = listOf("dru"),
            milestone = "v1.0",
            estimate = 3.0,
            environment = "macOS 27.0",
            comments = listOf(Comment(author = "Sam", createdAt = day("2026-05-02"), body = "Looks right.")),
        )
        val output = IssuesMarkdownSerializer.export(IssuesDocumentModel(issues = listOf(issue)))

        assertContains(output, "- **Labels:** ui, regression")
        assertContains(output, "- **Assignees:** dru")
        assertContains(output, "- **Milestone:** v1.0")
        assertContains(output, "- **Estimate:** 3")
        assertContains(output, "**Environment**")
        assertContains(output, "macOS 27.0")
        assertContains(output, "**Comments**")
        assertContains(output, "- **Sam** (2026-05-02): Looks right.")
    }

    @Test
    fun `export omits optional fields when empty`() {
        val output = IssuesMarkdownSerializer.export(
            IssuesDocumentModel(issues = listOf(Issue(number = 9, title = "Bare entry"))),
        )

        assertFalse(output.contains("**Labels:**"))
        assertFalse(output.contains("**Assignees:**"))
        assertFalse(output.contains("**Milestone:**"))
        assertFalse(output.contains("**Estimate:**"))
        assertFalse(output.contains("**Environment**"))
        assertFalse(output.contains("**Comments**"))
    }

    @Test
    fun `issue numbers are zero-padded to three digits with an em dash`() {
        val output = IssuesMarkdownSerializer.export(
            IssuesDocumentModel(issues = listOf(Issue(number = 7, title = "Padded"))),
        )
        assertContains(output, "### #007 — Padded")
    }

    @Test
    fun `fractional estimates keep their decimal point`() {
        val output = IssuesMarkdownSerializer.export(
            IssuesDocumentModel(issues = listOf(Issue(number = 1, estimate = 3.5))),
        )
        assertContains(output, "- **Estimate:** 3.5")
    }

    @Test
    fun `comment continuation lines are indented two spaces`() {
        val issue = Issue(
            number = 1,
            comments = listOf(
                Comment(author = "Sam", createdAt = day("2026-05-02"), body = "First.\nSecond."),
            ),
        )
        val output = IssuesMarkdownSerializer.export(IssuesDocumentModel(issues = listOf(issue)))
        assertContains(output, "- **Sam** (2026-05-02): First.\n  Second.")
    }

    @Test
    fun `fields with no markdown slot are dropped on export`() {
        val output = IssuesMarkdownSerializer.export(makeFullModel())

        assertFalse(output.contains(issueA.toString()))
        assertFalse(output.contains("resolutionKind"))
        assertFalse(output.contains("github.com"))
    }
}
