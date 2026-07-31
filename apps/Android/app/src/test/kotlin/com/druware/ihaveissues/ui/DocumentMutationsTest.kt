package com.druware.ihaveissues.ui

import com.druware.issueskit.AzureDevOpsIntegration
import com.druware.issueskit.GitHubIntegration
import com.druware.issueskit.IntegrationSettings
import com.druware.issueskit.Issue
import com.druware.issueskit.IssuesDocumentModel
import java.time.Instant
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue
import org.junit.jupiter.api.Test

/** The document-level operations the view model delegates to. */
class DocumentMutationsTest {

    private val now: Instant = Instant.parse("2026-07-31T10:00:00Z")

    @Test
    fun `upserting an unknown issue appends it`() {
        val model = IssuesDocumentModel(issues = listOf(Issue(number = 1)))
        val added = Issue(number = 2, title = "Second")

        val result = model.upserting(added)

        assertEquals(2, result.issues.size)
        assertEquals("Second", result.issues.last().title)
    }

    @Test
    fun `upserting a known issue replaces it in place`() {
        val first = Issue(number = 1, title = "First")
        val second = Issue(number = 2, title = "Second")
        val model = IssuesDocumentModel(issues = listOf(first, second))

        val result = model.upserting(first.copy(title = "Renamed"))

        assertEquals(2, result.issues.size)
        assertEquals("Renamed", result.issues[0].title)
        assertEquals("Second", result.issues[1].title)
    }

    @Test
    fun `removing drops only the named issue`() {
        val first = Issue(number = 1)
        val second = Issue(number = 2)
        val model = IssuesDocumentModel(issues = listOf(first, second))

        val result = model.removing(first.uuid)

        assertEquals(listOf(second.uuid), result.issues.map { it.uuid })
    }

    @Test
    fun `removing an issue other issues point at leaves the relations dangling`() {
        val target = Issue(number = 1)
        val source = Issue(
            number = 2,
            relations = listOf(com.druware.issueskit.Relation(issueID = target.uuid)),
        )
        val model = IssuesDocumentModel(issues = listOf(target, source))

        val result = model.removing(target.uuid)

        assertEquals(1, result.issues.size)
        assertEquals(1, result.issues.single().relations.size)
        assertNull(result.issue(target.uuid))
    }

    @Test
    fun `a new issue takes one past the highest number, not the count`() {
        val model = IssuesDocumentModel(
            issues = listOf(Issue(number = 3), Issue(number = 7), Issue(number = 5)),
        )

        assertEquals(8, model.newIssue(now).number)
    }

    @Test
    fun `the first issue in an empty document is number one`() {
        assertEquals(1, IssuesDocumentModel().newIssue(now).number)
    }

    @Test
    fun `a new issue is reported on a UTC day boundary`() {
        val issue = IssuesDocumentModel().newIssue(Instant.parse("2026-07-31T23:45:00Z"))

        assertEquals(Instant.parse("2026-07-31T00:00:00Z"), issue.reported)
    }

    // MARK: - Project settings

    @Test
    fun `turning an integration off removes the whole block`() {
        val model = IssuesDocumentModel(
            integrations = IntegrationSettings(
                github = GitHubIntegration(owner = "openbcm", repository = "i-have-issues"),
                azureDevOps = AzureDevOpsIntegration(organization = "acme"),
            ),
        )

        val result = ProjectSettingsDraft.of(model)
            .copy(gitHubEnabled = false, azureEnabled = false)
            .apply(model)

        assertNull(result.integrations.github)
        assertNull(result.integrations.azureDevOps)
    }

    @Test
    fun `blank optional integration fields become null, not empty strings`() {
        val model = IssuesDocumentModel()

        val result = ProjectSettingsDraft.of(model)
            .copy(
                gitHubEnabled = true,
                gitHubOwner = " openbcm ",
                gitHubRepository = "i-have-issues",
                gitHubDefaultMilestone = "   ",
                gitHubDefaultLabels = "triage, ,regression",
                azureEnabled = true,
                azureOrganization = "acme",
                azureTeam = "  ",
                azureWorkItemType = "  ",
            )
            .apply(model)

        val github = requireNotNull(result.integrations.github)
        assertEquals("openbcm", github.owner)
        assertNull(github.defaultMilestone)
        assertEquals(listOf("triage", "regression"), github.defaultLabels)

        val azure = requireNotNull(result.integrations.azureDevOps)
        assertNull(azure.team)
        assertEquals("Issue", azure.defaultWorkItemType)
    }

    @Test
    fun `settings never touch the issues`() {
        val model = IssuesDocumentModel(issues = listOf(Issue(number = 1, title = "Keep me")))

        val result = ProjectSettingsDraft.of(model).copy(name = "Renamed").apply(model)

        assertEquals("Renamed", result.project.name)
        assertTrue(result.issues == model.issues)
    }
}
