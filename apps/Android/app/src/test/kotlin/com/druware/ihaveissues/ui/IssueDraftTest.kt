package com.druware.ihaveissues.ui

import com.druware.issueskit.Issue
import com.druware.issueskit.IssuePriority
import com.druware.issueskit.IssueStatus
import com.druware.issueskit.IssueType
import com.druware.issueskit.ResolutionKind
import java.time.Instant
import kotlin.test.assertEquals
import kotlin.test.assertNull
import org.junit.jupiter.api.Test

/** The form's text-buffer parsing, and the `closedAt` rule a status change has to honour. */
class IssueDraftTest {

    private val now: Instant = Instant.parse("2026-07-31T10:00:00Z")
    private val earlier: Instant = Instant.parse("2026-05-01T09:15:00Z")

    private fun draft(issue: Issue = Issue()) = IssueDraft.of(issue, isNew = false)

    // MARK: - Comma-separated fields

    @Test
    fun `comma separated text is trimmed and blank entries are dropped`() {
        assertEquals(listOf("ui", "regression"), splitCommaSeparated("ui, regression"))
        assertEquals(listOf("ui", "regression"), splitCommaSeparated("  ui ,, regression , "))
        assertEquals(emptyList(), splitCommaSeparated(""))
        assertEquals(emptyList(), splitCommaSeparated("   ,  , "))
        assertEquals(listOf("one two"), splitCommaSeparated("one two"))
    }

    @Test
    fun `labels and assignees round trip through their text buffers`() {
        val issue = Issue(labels = listOf("ui", "regression"), assignees = listOf("dru", "sam"))
        val edited = draft(issue).toIssue(now)

        assertEquals(listOf("ui", "regression"), edited.labels)
        assertEquals(listOf("dru", "sam"), edited.assignees)
    }

    @Test
    fun `steps are one per line and blank lines are dropped`() {
        assertEquals(listOf("Open the app", "Tap Login"), splitLines("Open the app\n\n  Tap Login  \n"))
        assertEquals(emptyList(), splitLines("\n \n"))
    }

    // MARK: - Estimate

    @Test
    fun `an unparsable estimate is dropped rather than rejected`() {
        assertNull(parseEstimate("about three"))
        assertNull(parseEstimate(""))
        assertNull(parseEstimate("   "))
        assertNull(parseEstimate("3,5"))
    }

    @Test
    fun `a parsable estimate survives surrounding whitespace`() {
        assertEquals(3.5, parseEstimate(" 3.5 "))
        assertEquals(4.0, parseEstimate("4"))
    }

    @Test
    fun `non finite estimates are refused because the format cannot write them`() {
        assertNull(parseEstimate("Infinity"))
        assertNull(parseEstimate("NaN"))
    }

    @Test
    fun `saving a typo in the estimate field clears the value instead of failing`() {
        val issue = Issue(estimate = 3.5)
        val edited = draft(issue).copy(estimateText = "three and a half").toIssue(now)

        assertNull(edited.estimate)
    }

    @Test
    fun `a whole estimate presents without a decimal point`() {
        assertEquals("3", formatEstimate(3.0))
        assertEquals("3.5", formatEstimate(3.5))
        assertEquals("3", IssueDraft.of(Issue(estimate = 3.0), isNew = false).estimateText)
    }

    // MARK: - Optional fields

    @Test
    fun `a blank milestone becomes null rather than an empty string`() {
        assertNull(draft(Issue(milestone = "v1.0")).copy(milestoneText = "   ").toIssue(now).milestone)
        assertEquals("v1.0", draft(Issue()).copy(milestoneText = " v1.0 ").toIssue(now).milestone)
    }

    // MARK: - closedAt

    @Test
    fun `closing an issue stamps closedAt`() {
        val open = Issue(status = IssueStatus.OPEN, closedAt = null)

        val closed = draft(open).copy(status = IssueStatus.RESOLVED).toIssue(now)

        assertEquals(IssueStatus.RESOLVED, closed.status)
        assertEquals(now, closed.closedAt)
    }

    @Test
    fun `reopening an issue clears closedAt`() {
        val closed = Issue(status = IssueStatus.RESOLVED, closedAt = earlier)

        val reopened = draft(closed).copy(status = IssueStatus.IN_PROGRESS).toIssue(now)

        assertEquals(IssueStatus.IN_PROGRESS, reopened.status)
        assertNull(reopened.closedAt)
    }

    @Test
    fun `editing an already closed issue keeps the original closedAt`() {
        val closed = Issue(status = IssueStatus.RESOLVED, closedAt = earlier)

        val edited = draft(closed).copy(title = "Renamed").toIssue(now)

        assertEquals(earlier, edited.closedAt)
        assertEquals("Renamed", edited.title)
    }

    // MARK: - Identity and untouched fields

    @Test
    fun `saving preserves identity and stamps updatedAt`() {
        val issue = Issue(number = 7, createdAt = earlier, updatedAt = earlier)

        val edited = draft(issue).copy(title = "New title").toIssue(now)

        assertEquals(issue.uuid, edited.uuid)
        assertEquals(7, edited.number)
        assertEquals(earlier, edited.createdAt)
        assertEquals(now, edited.updatedAt)
    }

    @Test
    fun `resolution kind can be cleared back to none`() {
        val issue = Issue(status = IssueStatus.RESOLVED, resolutionKind = ResolutionKind.FIXED)

        val edited = draft(issue).copy(resolutionKind = null).toIssue(now)

        assertNull(edited.resolutionKind)
    }

    @Test
    fun `every editable field is carried back onto the issue`() {
        val edited = draft(Issue())
            .copy(
                title = "Login button does nothing",
                type = IssueType.BUG,
                priority = IssuePriority.CRITICAL,
                reportedBy = "dru",
                area = "Views",
                description = "Inert.",
                stepsText = "Launch\nTap Login",
                environment = "Pixel 8",
                notes = "Missing binding.",
                resolution = "Rebound.",
            )
            .toIssue(now)

        assertEquals("Login button does nothing", edited.title)
        assertEquals(IssueType.BUG, edited.type)
        assertEquals(IssuePriority.CRITICAL, edited.priority)
        assertEquals("dru", edited.reportedBy)
        assertEquals("Views", edited.area)
        assertEquals("Inert.", edited.description)
        assertEquals(listOf("Launch", "Tap Login"), edited.stepsToReproduce)
        assertEquals("Pixel 8", edited.environment)
        assertEquals("Missing binding.", edited.notes)
        assertEquals("Rebound.", edited.resolution)
    }
}
