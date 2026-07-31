package com.druware.issueskit

import java.util.UUID
import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import org.junit.jupiter.api.Test

class IssuesDocumentModelTest {

    @Test
    fun `nextNumber is one past the highest`() {
        val model = IssuesDocumentModel(
            issues = listOf(Issue(number = 3), Issue(number = 7), Issue(number = 5)),
        )
        assertEquals(8, model.nextNumber)
    }

    @Test
    fun `nextNumber starts at one when empty`() {
        assertEquals(1, IssuesDocumentModel().nextNumber)
    }

    @Test
    fun `nextNumber does not reuse gaps left by deletions`() {
        val model = IssuesDocumentModel(issues = listOf(Issue(number = 9)))
        assertEquals(10, model.nextNumber)
    }

    @Test
    fun `issue lookup by identity`() {
        val model = IssuesDocumentModel(
            issues = listOf(Issue(uuid = issueA, number = 1, title = "Wanted"), Issue(number = 2)),
        )
        assertEquals("Wanted", model.issue(issueA)?.title)
        assertNull(model.issue(UUID.randomUUID()))
    }

    @Test
    fun `makeEmpty carries the template preamble`() {
        val model = IssuesDocumentModel.makeEmpty()

        assertTrue(model.issues.isEmpty())
        assertEquals(IssuesDocumentModel.SUPPORTED_SCHEMA_VERSION, model.schemaVersion)
        assertEquals(ExportSettings.DEFAULT_PREAMBLE_MARKDOWN, model.export.preambleMarkdown)

        val exported = IssuesMarkdownSerializer.export(model)
        assertContains(exported, "## Open")
        assertContains(exported, "## Resolved")
    }

    @Test
    fun `display number is zero padded`() {
        assertEquals("#007", Issue(number = 7).displayNumber)
        assertEquals("#123", Issue(number = 123).displayNumber)
        assertEquals("#1234", Issue(number = 1234).displayNumber)
    }

    @Test
    fun `isResolved tracks the resolved status only`() {
        assertTrue(Issue(status = IssueStatus.RESOLVED).isResolved)
        for (status in listOf(IssueStatus.OPEN, IssueStatus.IN_PROGRESS, IssueStatus.BLOCKED)) {
            assertTrue(!Issue(status = status).isResolved)
        }
    }

    // MARK: - closedAt bookkeeping (deliberate deviation from the Apple app, which never sets it)

    @Test
    fun `resolving an issue stamps closedAt`() {
        val resolved = Issue(number = 1).withStatus(IssueStatus.RESOLVED, referenceDate)

        assertEquals(IssueStatus.RESOLVED, resolved.status)
        assertEquals(referenceDate, resolved.closedAt)
    }

    @Test
    fun `reopening an issue clears closedAt`() {
        val resolved = Issue(number = 1).withStatus(IssueStatus.RESOLVED, referenceDate)
        val reopened = resolved.withStatus(IssueStatus.IN_PROGRESS, referenceDate.plusSeconds(60))

        assertEquals(IssueStatus.IN_PROGRESS, reopened.status)
        assertNull(reopened.closedAt)
    }

    @Test
    fun `re-resolving keeps the original closedAt`() {
        val resolved = Issue(number = 1).withStatus(IssueStatus.RESOLVED, referenceDate)
        val again = resolved.withStatus(IssueStatus.RESOLVED, referenceDate.plusSeconds(3600))

        assertEquals(referenceDate, again.closedAt)
    }

    @Test
    fun `resolving an issue that was already resolved without a stamp backfills it`() {
        val legacy = Issue(number = 1, status = IssueStatus.RESOLVED, closedAt = null)
        val stamped = legacy.withStatus(IssueStatus.RESOLVED, referenceDate)

        assertEquals(referenceDate, stamped.closedAt)
    }

    @Test
    fun `closedAt survives a JSON round trip`() {
        val model = IssuesDocumentModel(
            issues = listOf(Issue(number = 1, reported = referenceDate).withStatus(IssueStatus.RESOLVED, referenceDate)),
        )
        val decoded = IssuesJSONCoder.decode(IssuesJSONCoder.encode(model))

        assertNotNull(decoded.issues.single().closedAt)
        assertEquals(referenceDate, decoded.issues.single().closedAt)
    }

    // MARK: - Enum contract

    @Test
    fun `enum raw values are the on-disk format`() {
        assertEquals(listOf("bug", "feature", "task", "question"), IssueType.entries.map { it.raw })
        assertEquals(listOf("low", "medium", "high", "critical"), IssuePriority.entries.map { it.raw })
        assertEquals(listOf("open", "inProgress", "blocked", "resolved"), IssueStatus.entries.map { it.raw })
        assertEquals(
            listOf("fixed", "wontFix", "duplicate", "cannotReproduce", "byDesign"),
            ResolutionKind.entries.map { it.raw },
        )
        assertEquals(
            listOf("blocks", "blockedBy", "duplicateOf", "relatedTo", "parent", "child"),
            RelationKind.entries.map { it.raw },
        )
    }

    @Test
    fun `enum display names are exact`() {
        assertEquals(listOf("Bug", "Feature", "Task", "Question"), IssueType.entries.map { it.displayName })
        assertEquals(listOf("Low", "Medium", "High", "Critical"), IssuePriority.entries.map { it.displayName })
        assertEquals(
            listOf("Open", "In Progress", "Blocked", "Resolved"),
            IssueStatus.entries.map { it.displayName },
        )
        assertEquals(
            listOf("Fixed", "Won't Fix", "Duplicate", "Cannot Reproduce", "By Design"),
            ResolutionKind.entries.map { it.displayName },
        )
        assertEquals(
            listOf("Blocks", "Blocked By", "Duplicate Of", "Related To", "Parent", "Child"),
            RelationKind.entries.map { it.displayName },
        )
    }

    @Test
    fun `display name lookup is case insensitive and returns null when unknown`() {
        assertEquals(IssueStatus.IN_PROGRESS, IssueStatus.fromDisplayName("in progress"))
        assertEquals(IssuePriority.CRITICAL, IssuePriority.fromDisplayName("  CRITICAL "))
        assertNull(IssueType.fromDisplayName("Epic"))
        assertEquals(ResolutionKind.WONT_FIX, ResolutionKind.fromDisplayName("won't fix"))
    }

    @Test
    fun `unknown resolution kind raw values decode to null`() {
        assertNull(ResolutionKind.fromRaw("ascended"))
        assertEquals(ResolutionKind.FIXED, ResolutionKind.fromRaw("fixed"))
    }
}
