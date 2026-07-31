package com.druware.issueskit

import java.time.Instant
import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue
import org.junit.jupiter.api.Test

class LegacyMarkdownImporterTest {

    // MARK: - Bundled standard sample

    @Test
    fun `legacy sample imports with the preamble captured`() {
        val original = testResource("Issues.md")
        val model = LegacyMarkdownImporter.importDocument(original)

        assertTrue(model.issues.isEmpty())
        assertTrue(model.openIssues.isEmpty())
        assertTrue(model.resolvedIssues.isEmpty())
        assertTrue(original.startsWith(model.export.preambleMarkdown))
        assertTrue(model.export.preambleMarkdown.endsWith("---\n\n"))
        assertTrue(model.export.preambleMarkdown.startsWith("# Issues\n"))
        assertEquals(IssuesDocumentModel.SUPPORTED_SCHEMA_VERSION, model.schemaVersion)
        // The sample carries no entries, so exporting it reproduces the file byte for byte.
        assertEquals(original, IssuesMarkdownSerializer.export(model))
    }

    @Test
    fun `legacy import without any section throws`() {
        assertEquals(
            IssuesError.MissingOpenSection,
            assertFailsWith<IssuesError> {
                LegacyMarkdownImporter.importDocument("# Issues\n\nNo sections here at all.\n")
            },
        )
    }

    // MARK: - Fully populated entry

    @Test
    fun `legacy full entry maps every field`() {
        val model = LegacyMarkdownImporter.importDocument(FULL_LEGACY_ENTRY)
        val issue = model.issues.single()

        assertEquals(7, issue.number)
        assertEquals("Login button does nothing", issue.title)
        assertEquals(IssueType.BUG, issue.type)
        assertEquals(IssuePriority.HIGH, issue.priority)
        assertEquals(IssueStatus.IN_PROGRESS, issue.status)
        assertEquals("2026-05-01", IssueDate.stringFrom(issue.reported))
        assertEquals("Dru", issue.reportedBy)
        assertEquals("Views", issue.area)
        assertEquals(listOf("ui", "regression"), issue.labels)
        assertEquals(listOf("dru", "sam"), issue.assignees)
        assertEquals("v1.0", issue.milestone)
        assertEquals(3.5, issue.estimate)
        assertEquals("The login button is inert.\nIt should authenticate.", issue.description)
        assertEquals(listOf("Open the app", "Tap Login"), issue.stepsToReproduce)
        assertEquals("macOS 27.0, build 1234", issue.environment)
        assertEquals("Possibly a missing action binding.", issue.notes)
        assertEquals("Not yet fixed.", issue.resolution)
        assertEquals(1, issue.comments.size)
        assertEquals("Sam", issue.comments.single().author)
        assertEquals("Reproduced on my machine.\nSame stack trace.", issue.comments.single().body)
        assertEquals(listOf("2026-05-02"), issue.comments.map { IssueDate.stringFrom(it.createdAt) })
        assertEquals(listOf(RemoteLink(RemoteProvider.Github, identifier = "412")), issue.remoteLinks)
        assertTrue(model.export.preambleMarkdown.startsWith("# Issues\n"))
    }

    @Test
    fun `legacy import assigns fresh identity and timestamps`() {
        val before = Instant.now()
        val model = LegacyMarkdownImporter.importDocument(FULL_LEGACY_ENTRY)
        val other = LegacyMarkdownImporter.importDocument(FULL_LEGACY_ENTRY)
        val issue = model.issues.single()
        val twin = other.issues.single()

        assertNotEquals(issue.uuid, twin.uuid)
        assertTrue(issue.createdAt >= before)
        assertTrue(issue.updatedAt >= before)
    }

    @Test
    fun `legacy import falls back to defaults on unknown display strings`() {
        val markdown = """
            # Issues

            ## Open

            ### #013 — Weird values

            - **Type:** Epic
            - **Priority:** Whenever
            - **Status:** Vibing
            - **Reported:** not-a-date

            ---

            ## Resolved

            _No resolved issues._
        """.trimIndent()
        val issue = LegacyMarkdownImporter.importDocument(markdown).issues.single()

        assertEquals(IssueType.TASK, issue.type)
        assertEquals(IssuePriority.MEDIUM, issue.priority)
        assertEquals(IssueStatus.OPEN, issue.status)
        // The unparsable date is ignored rather than fatal; the entry keeps today's date.
        assertEquals(IssueDate.stringFrom(IssueDate.today()), IssueDate.stringFrom(issue.reported))
    }

    @Test
    fun `legacy malformed entry does not abort the import`() {
        val markdown = """
            # Issues

            ## Open

            ### not-an-issue heading with no number

            random text
            - **Type:** Bug

            ### #020 — A real one

            - **Type:** Feature

            ## Resolved

            _No resolved issues._
        """.trimIndent()
        val model = LegacyMarkdownImporter.importDocument(markdown)

        assertEquals(1, model.issues.size)
        assertEquals(20, model.issues.single().number)
        assertEquals(IssueType.FEATURE, model.issues.single().type)
    }

    @Test
    fun `legacy unknown body section lands in notes`() {
        val markdown = """
            # Issues

            ## Open

            ### #030 — Has extra section

            - **Type:** Task

            **Design Considerations**

            Some extra prose that is not a known section.

            ---

            ## Resolved

            _No resolved issues._
        """.trimIndent()
        val model = LegacyMarkdownImporter.importDocument(markdown)
        val issue = model.issues.single()

        assertEquals(
            "**Design Considerations**\n\nSome extra prose that is not a known section.",
            issue.notes,
        )
        assertContains(IssuesMarkdownSerializer.export(model), "Some extra prose")
    }

    // MARK: - Free-form / MLM sample

    @Test
    fun `free-form sample imports all issues`() {
        val model = LegacyMarkdownImporter.importDocument(testResource("MLM-issues.md"))
        assertEquals(5, model.issues.size)
        assertEquals(5, model.openIssues.size)
    }

    @Test
    fun `free-form sample first issue fields`() {
        val issue = LegacyMarkdownImporter.importDocument(testResource("MLM-issues.md")).issues.first()

        assertEquals(1, issue.number)
        assertEquals("Login endpoint never triggers the configured account lockout", issue.title)
        assertEquals(IssuePriority.HIGH, issue.priority)
        assertEquals(IssueStatus.OPEN, issue.status)
        assertEquals("API (Auth)", issue.area)
        assertTrue(issue.description.isNotEmpty())
        assertEquals(3, issue.stepsToReproduce.size)
        assertTrue(issue.resolution.isNotEmpty())
    }

    @Test
    fun `free-form sample maps severity to priority`() {
        val model = LegacyMarkdownImporter.importDocument(testResource("MLM-issues.md"))
        assertEquals(
            listOf(
                IssuePriority.HIGH,
                IssuePriority.MEDIUM,
                IssuePriority.MEDIUM,
                IssuePriority.MEDIUM,
                IssuePriority.LOW,
            ),
            model.issues.map { it.priority },
        )
    }

    @Test
    fun `free-form sample maps component to area`() {
        val model = LegacyMarkdownImporter.importDocument(testResource("MLM-issues.md"))
        assertEquals("API (Auth)", model.issues[0].area)
        assertEquals("Web", model.issues[4].area)
    }

    @Test
    fun `free-form sample maps proposed fix to resolution and notes to notes`() {
        val issue = LegacyMarkdownImporter.importDocument(testResource("MLM-issues.md")).issues.first()
        assertContains(issue.resolution, "SignInManager.CheckPasswordSignInAsync")
        assertEquals("None.", issue.notes)
    }

    @Test
    fun `free-form category headings drive the issue type`() {
        val markdown = """
            # Project

            ## Bugs

            ### Something broke

            - **Severity:** Critical

            ## Enhancements

            ### ~~Add dark mode~~ ✓ Fixed

            - **Severity:** Low

            ## Questions

            ### Why is it like this
        """.trimIndent()
        val model = LegacyMarkdownImporter.importDocument(markdown)

        assertEquals(listOf(IssueType.BUG, IssueType.FEATURE, IssueType.QUESTION), model.issues.map { it.type })
        assertEquals(listOf(1, 2, 3), model.issues.map { it.number })
        assertEquals("Add dark mode", model.issues[1].title)
        assertEquals(IssueStatus.RESOLVED, model.issues[1].status)
        assertEquals(IssueStatus.OPEN, model.issues[0].status)
        assertEquals("", model.export.preambleMarkdown)
    }

    // MARK: - Export / import stability

    @Test
    fun `export import export is stable`() {
        val resolved = Issue(
            number = 3,
            title = "Crash on launch",
            type = IssueType.BUG,
            priority = IssuePriority.CRITICAL,
            status = IssueStatus.RESOLVED,
            labels = listOf("startup"),
            assignees = listOf("dru"),
            milestone = "v0.9",
            area = "App",
            estimate = 2.0,
            reportedBy = "Sam",
            reported = day("2026-04-01"),
            description = "It crashes.",
            stepsToReproduce = listOf("Launch", "Watch it die"),
            environment = "iOS 27.0",
            notes = "Stack trace attached.",
            resolution = "Fixed the nil unwrap.",
            comments = listOf(
                Comment(author = "Dru", createdAt = day("2026-04-02"), body = "First line.\nSecond line."),
            ),
        )
        val open = Issue(
            number = 4,
            title = "Add dark mode",
            type = IssueType.FEATURE,
            priority = IssuePriority.LOW,
            area = "Views",
            reportedBy = "Dru",
            reported = day("2026-04-05"),
            description = "Please.",
        )
        val model = IssuesDocumentModel(issues = listOf(open, resolved))

        val first = IssuesMarkdownSerializer.export(model)
        val reimported = LegacyMarkdownImporter.importDocument(first)
        val second = IssuesMarkdownSerializer.export(reimported)

        assertEquals(first, second)
        assertEquals(1, reimported.openIssues.size)
        assertEquals(1, reimported.resolvedIssues.size)
    }

    private companion object {
        val FULL_LEGACY_ENTRY = """
            # Issues

            Preamble text that must survive verbatim.

            ## Open

            ### #007 — Login button does nothing

            - **Type:** Bug
            - **Priority:** High
            - **Status:** In Progress
            - **Reported:** 2026-05-01
            - **Reported by:** Dru
            - **Area:** Views
            - **Labels:** ui, regression
            - **Assignees:** dru, sam
            - **Milestone:** v1.0
            - **Estimate:** 3.5
            - **GitHub:** 412

            **Description**

            The login button is inert.
            It should authenticate.

            **Steps to reproduce**

            1. Open the app
            2. Tap Login

            **Environment**

            macOS 27.0, build 1234

            **Notes / Investigation**

            Possibly a missing action binding.

            **Resolution**

            Not yet fixed.

            **Comments**

            - **Sam** (2026-05-02): Reproduced on my machine.
              Same stack trace.

            ---

            ## Resolved

            _No resolved issues._
        """.trimIndent()
    }
}
