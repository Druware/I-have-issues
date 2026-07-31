package com.druware.ihaveissues.github

import com.druware.ihaveissues.github.FakeHttpClient.Companion.created
import com.druware.ihaveissues.github.FakeHttpClient.Companion.failure
import com.druware.ihaveissues.github.FakeHttpClient.Companion.ok
import com.druware.issueskit.GitHubIntegration
import com.druware.issueskit.Issue
import com.druware.issueskit.IssuePriority
import com.druware.issueskit.IssueStatus
import com.druware.issueskit.IssueType
import com.druware.issueskit.RemoteLink
import com.druware.issueskit.RemoteProvider
import java.time.Instant
import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.junit.jupiter.api.Test

/**
 * The GitHub push, exercised entirely against [FakeHttpClient].
 *
 * Nothing here touches the network or the Android keystore, so the whole suite runs on the JVM.
 */
class GitHubSyncServiceTest {

    private val http = FakeHttpClient()
    private val now: Instant = Instant.parse("2026-07-31T10:00:00Z")
    private val service = GitHubSyncService(http) { now }

    private val integration = GitHubIntegration(owner = "openbcm", repository = "i-have-issues")
    private val token = "ghp_token"

    private suspend fun sync(
        issues: List<Issue>,
        integration: GitHubIntegration = this.integration,
    ): SyncOutcome = service.sync(issues, integration, token)

    private fun body(index: Int): JsonObject =
        Json.parseToJsonElement(http.requests[index].body!!).jsonObject

    private fun gitHubLink(number: String) =
        RemoteLink(provider = RemoteProvider.Github, identifier = number)

    // MARK: - Routing

    @Test
    fun `an issue with no GitHub link is created`() = runTest {
        http.respondWith { created(number = 412) }

        val outcome = sync(listOf(Issue(number = 7, title = "Login button does nothing")))

        assertEquals(listOf("POST /repos/openbcm/i-have-issues/issues"), http.routes())
        assertEquals(SyncResult(created = 1), outcome.result)
        val link = outcome.issues.single().remoteLinks.single()
        assertEquals(RemoteProvider.Github, link.provider)
        assertEquals("412", link.identifier)
        assertEquals(now, link.lastSyncedAt)
    }

    @Test
    fun `an issue that already has a GitHub link is updated in place`() = runTest {
        http.respondWith { ok("""{"number": 412, "updated_at": "2026-07-30T09:00:00Z"}""") }
        val issue = Issue(number = 7, remoteLinks = listOf(gitHubLink("412")))

        val outcome = sync(listOf(issue))

        assertEquals(listOf("PATCH /repos/openbcm/i-have-issues/issues/412"), http.routes())
        assertEquals(SyncResult(updated = 1), outcome.result)
        val link = outcome.issues.single().remoteLinks.single()
        assertEquals(Instant.parse("2026-07-30T09:00:00Z"), link.remoteUpdatedAt)
        assertEquals(now, link.lastSyncedAt)
    }

    @Test
    fun `a link to another provider is not mistaken for a GitHub one`() = runTest {
        http.respondWith { created(number = 5) }
        val issue = Issue(
            number = 7,
            remoteLinks = listOf(RemoteLink(provider = RemoteProvider.Other("gitlab"), identifier = "412")),
        )

        val outcome = sync(listOf(issue))

        assertEquals(listOf("POST /repos/openbcm/i-have-issues/issues"), http.routes())
        assertEquals(2, outcome.issues.single().remoteLinks.size)
    }

    @Test
    fun `a terse update response never erases what was already known`() = runTest {
        http.respondWith { ok("""{"number": 412}""") }
        val issue = Issue(
            number = 7,
            remoteLinks = listOf(
                gitHubLink("412").copy(
                    url = "https://github.test/412",
                    remoteUpdatedAt = Instant.parse("2026-01-01T00:00:00Z"),
                ),
            ),
        )

        val link = sync(listOf(issue)).issues.single().remoteLinks.single()

        assertEquals("https://github.test/412", link.url)
        assertEquals(Instant.parse("2026-01-01T00:00:00Z"), link.remoteUpdatedAt)
        assertEquals(now, link.lastSyncedAt)
    }

    @Test
    fun `a locally resolved issue is created and then closed by a second request`() = runTest {
        http.respondWith { request ->
            if (request.method == "POST") created(number = 412) else ok("""{"number": 412}""")
        }
        val issue = Issue(number = 7, status = IssueStatus.RESOLVED)

        val outcome = sync(listOf(issue))

        assertEquals(
            listOf(
                "POST /repos/openbcm/i-have-issues/issues",
                "PATCH /repos/openbcm/i-have-issues/issues/412",
            ),
            http.routes(),
        )
        // GitHub's create endpoint has no `state`, so only the follow-up carries it.
        assertNull(body(0)["state"])
        assertEquals("closed", body(1)["state"]?.jsonPrimitive?.content)
        assertEquals(SyncResult(created = 1), outcome.result)
    }

    @Test
    fun `an update always carries the state, open included`() = runTest {
        http.respondWith { ok("""{"number": 412}""") }

        sync(listOf(Issue(number = 7, remoteLinks = listOf(gitHubLink("412")))))

        assertEquals("open", body(0)["state"]?.jsonPrimitive?.content)
    }

    // MARK: - Field mapping

    @Test
    fun `defaults are merged in behind the issue's own labels and assignees`() = runTest {
        http.respondWith { created(number = 1) }
        val issue = Issue(
            number = 7,
            labels = listOf("regression", "triage"),
            assignees = listOf("dru"),
        )

        sync(
            listOf(issue),
            integration.copy(
                defaultLabels = listOf("triage", "needs-review"),
                defaultAssignees = listOf("dru", "sam"),
            ),
        )

        val payload = body(0)
        assertEquals(
            listOf("regression", "triage", "needs-review"),
            payload["labels"]!!.jsonArray.map { it.jsonPrimitive.content },
        )
        assertEquals(
            listOf("dru", "sam"),
            payload["assignees"]!!.jsonArray.map { it.jsonPrimitive.content },
        )
    }

    @Test
    fun `an issue with no labels or assignees sends neither key`() = runTest {
        http.respondWith { created(number = 1) }

        sync(listOf(Issue(number = 7)))

        assertNull(body(0)["labels"])
        assertNull(body(0)["assignees"])
    }

    @Test
    fun `a blank title becomes a placeholder rather than an empty one`() = runTest {
        http.respondWith { created(number = 1) }

        sync(listOf(Issue(number = 7)))

        assertEquals("Untitled Issue", body(0)["title"]?.jsonPrimitive?.content)
    }

    @Test
    fun `the body carries the metadata GitHub has nowhere else to put`() = runTest {
        http.respondWith { created(number = 1) }
        val issue = Issue(
            number = 7,
            type = IssueType.BUG,
            priority = IssuePriority.HIGH,
            status = IssueStatus.IN_PROGRESS,
            reported = Instant.parse("2026-05-01T00:00:00Z"),
            reportedBy = "dru",
            area = "Views",
            description = "The button is inert.",
            stepsToReproduce = listOf("Open the app", "Tap Login"),
            notes = "Missing action binding.",
            resolution = "Rebound the action.",
        )

        sync(listOf(issue))

        assertEquals(
            """
            **Type:** Bug | **Priority:** High | **Status:** In Progress

            **Reported:** 2026-05-01 · dru | **Area:** Views

            ---
            **Description**

            The button is inert.

            **Steps to Reproduce**

            1. Open the app
            2. Tap Login

            **Notes / Investigation**

            Missing action binding.

            **Resolution**

            Rebound the action.

            ---
            *Synced from local issue tracker · #007*
            """.trimIndent(),
            body(0)["body"]?.jsonPrimitive?.content,
        )
    }

    // MARK: - Milestones

    @Test
    fun `no milestone is named, so the milestone list is never fetched`() = runTest {
        http.respondWith { created(number = 1) }

        sync(listOf(Issue(number = 7)))

        assertEquals(listOf("POST /repos/openbcm/i-have-issues/issues"), http.routes())
    }

    @Test
    fun `a named milestone is resolved to the number GitHub identifies it by`() = runTest {
        http.respondWith { request ->
            if (request.method == "GET") {
                ok("""[{"title": "v1.0", "number": 3}, {"title": "v1.1", "number": 4}]""")
            } else {
                created(number = 1)
            }
        }

        sync(listOf(Issue(number = 7, milestone = "v1.1")))

        assertEquals(
            listOf(
                "GET /repos/openbcm/i-have-issues/milestones?state=all&per_page=100",
                "POST /repos/openbcm/i-have-issues/issues",
            ),
            http.routes(),
        )
        assertEquals(4, body(1)["milestone"]?.jsonPrimitive?.content?.toInt())
    }

    @Test
    fun `the document's default milestone applies when the issue names none`() = runTest {
        http.respondWith { request ->
            if (request.method == "GET") ok("""[{"title": "v1.0", "number": 3}]""") else created(1)
        }

        sync(listOf(Issue(number = 7)), integration.copy(defaultMilestone = "v1.0"))

        assertEquals(3, body(1)["milestone"]?.jsonPrimitive?.content?.toInt())
    }

    @Test
    fun `a milestone with no match on the server is omitted, never sent as a title`() = runTest {
        http.respondWith { request ->
            if (request.method == "GET") ok("""[{"title": "v1.0", "number": 3}]""") else created(1)
        }

        val outcome = sync(listOf(Issue(number = 7, milestone = "v9.9")))

        assertNull(body(1)["milestone"])
        assertEquals(SyncResult(created = 1), outcome.result)
    }

    @Test
    fun `milestones past the first page are followed through the Link header`() = runTest {
        val secondPage = "https://api.github.com/repos/openbcm/i-have-issues/milestones?page=2"
        http.respondWith { request ->
            when {
                request.method != "GET" -> created(number = 1)
                request.url.endsWith("page=2") -> ok("""[{"title": "v2.0", "number": 101}]""")
                else -> ok(
                    """[{"title": "v1.0", "number": 3}]""",
                    mapOf("Link" to listOf("<$secondPage>; rel=\"next\", <$secondPage>; rel=\"last\"")),
                )
            }
        }

        sync(listOf(Issue(number = 7, milestone = "v2.0")))

        assertEquals(
            listOf(
                "GET /repos/openbcm/i-have-issues/milestones?state=all&per_page=100",
                "GET /repos/openbcm/i-have-issues/milestones?page=2",
                "POST /repos/openbcm/i-have-issues/issues",
            ),
            http.routes(),
        )
        // Only visible because page two was read: Apple stops at the first hundred.
        assertEquals(101, body(2)["milestone"]?.jsonPrimitive?.content?.toInt())
    }

    @Test
    fun `a next link pointing back at a page already read does not loop`() = runTest {
        val first = "https://api.github.com/repos/openbcm/i-have-issues/milestones?state=all&per_page=100"
        http.respondWith { request ->
            if (request.method == "GET") {
                ok("""[]""", mapOf("Link" to listOf("<$first>; rel=\"next\"")))
            } else {
                created(number = 1)
            }
        }

        sync(listOf(Issue(number = 7, milestone = "v1.0")))

        assertEquals(1, http.requests.count { it.method == "GET" })
    }

    @Test
    fun `a next link pointing off the API host is not followed with the token`() = runTest {
        http.respondWith { request ->
            if (request.method == "GET") {
                ok("""[]""", mapOf("Link" to listOf("<https://evil.test/steal>; rel=\"next\"")))
            } else {
                created(number = 1)
            }
        }

        sync(listOf(Issue(number = 7, milestone = "v1.0")))

        assertTrue(http.requests.none { it.url.startsWith("https://evil.test") })
    }

    @Test
    fun `the Link header parser only follows rel next`() {
        assertEquals(
            "https://api.github.com/x?page=2",
            nextPageLink("<https://api.github.com/x?page=2>; rel=\"next\", <https://a/x?page=9>; rel=\"last\""),
        )
        assertNull(nextPageLink("<https://api.github.com/x?page=1>; rel=\"prev\""))
        assertNull(nextPageLink(null))
        assertNull(nextPageLink(""))
    }

    // MARK: - Path encoding

    @Test
    fun `a slash typed into the owner cannot invent extra path segments`() = runTest {
        http.respondWith { created(number = 1) }

        sync(
            listOf(Issue(number = 7)),
            GitHubIntegration(owner = "openbcm/../attacker", repository = "i-have-issues"),
        )

        val url = http.requests.single().url
        assertEquals(
            "https://api.github.com/repos/openbcm%2F..%2Fattacker/i-have-issues/issues",
            url,
        )
        // The path still has exactly the four segments the coordinates named.
        assertEquals(4, url.removePrefix("https://api.github.com/").split('/').size)
    }

    @Test
    fun `characters a URL cannot carry raw are percent-encoded`() {
        assertEquals("open%20bcm", encodePathSegment("open bcm"))
        assertEquals("a%3Fb%23c", encodePathSegment("a?b#c"))
        assertEquals("i-have_issues.v1~2", encodePathSegment("i-have_issues.v1~2"))
        assertEquals("caf%C3%A9", encodePathSegment("café"))
    }

    @Test
    fun `a blank repository is refused rather than collapsing the path`() = runTest {
        val outcome = sync(listOf(Issue(number = 7)), integration.copy(repository = ""))

        assertTrue(http.requests.isEmpty())
        assertEquals(1, outcome.result.failed)
        assertContains(outcome.result.errors.single(), "#007")
    }

    // MARK: - Error semantics

    @Test
    fun `a 401 aborts the whole sync rather than failing issue by issue`() = runTest {
        http.respondWith { failure(401) }

        assertFailsWith<GitHubSyncException.Unauthorized> {
            sync(listOf(Issue(number = 1), Issue(number = 2)))
        }
        // The second issue was never attempted: a rejected token would reject it too.
        assertEquals(1, http.requests.size)
    }

    @Test
    fun `a 401 part way through stops the sync and leaves earlier work intact`() = runTest {
        http.respondWith { request ->
            if (request.body!!.contains("#001")) created(number = 1) else failure(401)
        }

        assertFailsWith<GitHubSyncException.Unauthorized> {
            sync(listOf(Issue(number = 1), Issue(number = 2), Issue(number = 3)))
        }

        assertEquals(2, http.requests.size)
    }

    @Test
    fun `other per-issue failures accumulate and the sync carries on`() = runTest {
        http.respondWith { request ->
            when {
                request.body!!.contains("#002") -> failure(404, "Not Found")
                request.body!!.contains("#003") -> failure(422, "Validation Failed")
                else -> created(number = 1)
            }
        }

        val outcome = sync(listOf(Issue(number = 1), Issue(number = 2), Issue(number = 3), Issue(number = 4)))

        assertEquals(2, outcome.result.created)
        assertEquals(2, outcome.result.failed)
        assertEquals(listOf("#002: Not Found", "#003: Validation Failed"), outcome.result.errors)
        // Every issue was attempted.
        assertEquals(4, http.requests.size)
    }

    @Test
    fun `a failure with no message from GitHub still names the status`() = runTest {
        http.respondWith { failure(500) }

        val outcome = sync(listOf(Issue(number = 1)))

        assertEquals(listOf("#001: HTTP 500"), outcome.result.errors)
    }

    @Test
    fun `an existing link whose identifier is not a number fails only that issue`() = runTest {
        http.respondWith { created(number = 9) }
        val broken = Issue(number = 1, remoteLinks = listOf(gitHubLink("not-a-number")))

        val outcome = sync(listOf(broken, Issue(number = 2)))

        assertEquals(1, outcome.result.failed)
        assertEquals(1, outcome.result.created)
        assertContains(outcome.result.errors.single(), "\"not-a-number\" is not an issue number")
        // Nothing was sent for the broken issue, and its link was left untouched.
        assertEquals(listOf("POST /repos/openbcm/i-have-issues/issues"), http.routes())
        assertEquals("not-a-number", outcome.issues.first().remoteLinks.single().identifier)
    }

    @Test
    fun `a create response with no issue number fails that issue`() = runTest {
        http.respondWith { ok("""{"documentation_url": "https://docs.github.test"}""") }

        val outcome = sync(listOf(Issue(number = 1)))

        assertEquals(1, outcome.result.failed)
        assertTrue(outcome.issues.single().remoteLinks.isEmpty())
    }

    // MARK: - Headers

    @Test
    fun `every request is authenticated and version-pinned`() = runTest {
        http.respondWith { created(number = 1) }

        sync(listOf(Issue(number = 1)))

        assertEquals(
            mapOf(
                "Authorization" to "Bearer ghp_token",
                "Accept" to "application/vnd.github+json",
                "X-GitHub-Api-Version" to "2022-11-28",
                "Content-Type" to "application/json",
            ),
            http.requests.single().headers,
        )
    }

    @Test
    fun `the push reads nothing back beyond the response, so it stays one way`() = runTest {
        http.respondWith { created(number = 412) }
        val issue = Issue(number = 1, title = "Local title", labels = listOf("ui"))

        val synced = sync(listOf(issue)).issues.single()

        // Only the remote link changed; no field was overwritten from GitHub.
        assertEquals(issue, synced.copy(remoteLinks = emptyList()))
        assertFalse(synced.remoteLinks.isEmpty())
    }
}
