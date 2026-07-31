package com.druware.ihaveissues.github

import com.druware.issueskit.GitHubIntegration
import com.druware.issueskit.Issue
import com.druware.issueskit.IssueDate
import com.druware.issueskit.RemoteLink
import com.druware.issueskit.RemoteProvider
import java.time.Instant
import java.time.format.DateTimeParseException
import java.util.Locale
import kotlinx.coroutines.CancellationException
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray

/** How a sync went: what it pushed, what it could not, and why. */
data class SyncResult(
    val created: Int = 0,
    val updated: Int = 0,
    val failed: Int = 0,
    /** One `"#NNN: reason"` entry per issue that failed. */
    val errors: List<String> = emptyList(),
)

/** The issues as they stand after a sync — remote links filled in — and the summary. */
data class SyncOutcome(val issues: List<Issue>, val result: SyncResult)

/** A failure the sync itself understands, as opposed to a transport or parse failure. */
sealed class GitHubSyncException(message: String) : Exception(message) {

    /** HTTP 401. Fatal to the whole sync: the same token would fail every remaining request. */
    class Unauthorized :
        GitHubSyncException("Invalid or expired GitHub token. Check your personal access token.")

    /** Any other non-2xx, carrying GitHub's own `message` when it sent one. */
    class ApiError(message: String) : GitHubSyncException(message)

    class InvalidResponse : GitHubSyncException("Unexpected response from GitHub API.")

    class InvalidRepository : GitHubSyncException(
        "The GitHub owner and repository in Project Settings do not form a valid URL.",
    )

    class InvalidRemoteIdentifier(identifier: String) :
        GitHubSyncException("The existing GitHub link \"$identifier\" is not an issue number.")
}

/**
 * Pushes local issues to GitHub.
 *
 * **One way only.** Nothing is read back beyond the create/update responses: there is no pull, no
 * diff and no conflict resolution, and `lastSyncedAt`/`remoteUpdatedAt` are recorded as metadata
 * that nothing yet compares. Comments and relations have no GitHub representation here at all.
 *
 * The token is a parameter of [sync] rather than a property, so it exists only for the length of one
 * call and is never held by, logged by, or written into anything this class owns.
 */
class GitHubSyncService(
    private val http: HttpClient,
    private val clock: () -> Instant = Instant::now,
) {

    /**
     * Pushes every issue in [issues] to the repository named by [integration].
     *
     * Returns the issues with their GitHub links filled in or refreshed, plus a summary. A failure
     * on one issue is recorded and the sync moves on; only an authentication failure aborts, because
     * a rejected token would reject every request that followed.
     */
    suspend fun sync(
        issues: List<Issue>,
        integration: GitHubIntegration,
        token: String,
    ): SyncOutcome {
        val milestones = milestoneNumbersIfNeeded(issues, integration, token)
        val updated = issues.toMutableList()
        var result = SyncResult()

        for (index in updated.indices) {
            val issue = updated[index]
            try {
                val linkIndex = issue.remoteLinks.indexOfFirst { it.provider.isGitHub }
                if (linkIndex >= 0) {
                    val identifier = issue.remoteLinks[linkIndex].identifier
                    val number = identifier.toIntOrNull()
                        ?: throw GitHubSyncException.InvalidRemoteIdentifier(identifier)
                    val remote = updateIssue(issue, number, integration, token, milestones)
                    val links = issue.remoteLinks.toMutableList()
                    links[linkIndex] = record(remote, links[linkIndex])
                    updated[index] = issue.copy(remoteLinks = links)
                    result = result.copy(updated = result.updated + 1)
                } else {
                    var remote = createIssue(issue, integration, token, milestones)
                    if (issue.isResolved) {
                        // GitHub's create endpoint has no `state`, so an issue that is already
                        // resolved locally is created open and then closed by a second request.
                        remote = updateIssue(issue, remote.number, integration, token, milestones)
                            ?: remote
                    }
                    updated[index] = issue.copy(
                        remoteLinks = issue.remoteLinks + RemoteLink(
                            provider = RemoteProvider.Github,
                            identifier = remote.number.toString(),
                            url = remote.htmlUrl,
                            lastSyncedAt = clock(),
                            remoteUpdatedAt = remote.updatedAt,
                        ),
                    )
                    result = result.copy(created = result.created + 1)
                }
            } catch (cancellation: CancellationException) {
                throw cancellation
            } catch (unauthorized: GitHubSyncException.Unauthorized) {
                throw unauthorized
            } catch (error: Exception) {
                result = result.copy(
                    failed = result.failed + 1,
                    errors = result.errors + "${issue.displayNumber}: ${error.syncMessage()}",
                )
            }
        }

        return SyncOutcome(issues = updated, result = result)
    }

    /**
     * Refreshes an existing link.
     *
     * The remote fields are only overwritten when the response actually carried them, so a terse
     * reply never erases what was already known.
     */
    private fun record(remote: RemoteIssue?, link: RemoteLink): RemoteLink {
        val stamped = link.copy(lastSyncedAt = clock())
        if (remote == null) return stamped
        return stamped.copy(
            url = remote.htmlUrl ?: stamped.url,
            remoteUpdatedAt = remote.updatedAt ?: stamped.remoteUpdatedAt,
        )
    }

    private suspend fun createIssue(
        issue: Issue,
        integration: GitHubIntegration,
        token: String,
        milestones: Map<String, Int>,
    ): RemoteIssue {
        val response = perform(
            HttpRequest(
                method = "POST",
                url = apiUrl(integration, listOf("issues")),
                headers = headers(token),
                body = payload(issue, integration, milestones, includeState = false),
            ),
        )
        return remoteIssue(response.body) ?: throw GitHubSyncException.InvalidResponse()
    }

    private suspend fun updateIssue(
        issue: Issue,
        number: Int,
        integration: GitHubIntegration,
        token: String,
        milestones: Map<String, Int>,
    ): RemoteIssue? {
        val response = perform(
            HttpRequest(
                method = "PATCH",
                url = apiUrl(integration, listOf("issues", number.toString())),
                headers = headers(token),
                body = payload(issue, integration, milestones, includeState = true),
            ),
        )
        return remoteIssue(response.body)
    }

    // MARK: - Milestones

    /**
     * Maps milestone titles to the numbers GitHub identifies them by.
     *
     * Fetched once per sync and only when some issue — or the document default — actually names a
     * milestone, so a project that does not use them never pays for the round trip.
     *
     * Every page is followed via the `Link: rel="next"` header. The Apple app stops at the first
     * hundred and silently loses the rest, which is its own known bug #4.
     */
    private suspend fun milestoneNumbersIfNeeded(
        issues: List<Issue>,
        integration: GitHubIntegration,
        token: String,
    ): Map<String, Int> {
        val isNamed = issues.any { !it.milestone.isNullOrEmpty() } ||
            !integration.defaultMilestone.isNullOrEmpty()
        if (!isNamed) return emptyMap()

        val numbers = mutableMapOf<String, Int>()
        // A `next` link that points back at a page already read would otherwise loop forever.
        val visited = mutableSetOf<String>()
        var next: String? = apiUrl(integration, listOf("milestones"), MILESTONE_QUERY)
        while (true) {
            val url = next ?: break
            if (!visited.add(url)) break
            val response = perform(HttpRequest(method = "GET", url = url, headers = headers(token)))
            val items = parseArray(response.body) ?: break
            for (item in items) {
                val entry = item as? JsonObject ?: continue
                val title = entry["title"]?.stringOrNull() ?: continue
                val number = entry["number"]?.intOrNull() ?: continue
                numbers[title] = number
            }
            // The `next` URL comes from the response, and the token travels with every request, so
            // only a link that stays on the API host is followed.
            next = nextPageLink(response.header("Link"))?.takeIf { it.startsWith("$API_BASE/") }
        }
        return numbers
    }

    // MARK: - Payload

    private fun payload(
        issue: Issue,
        integration: GitHubIntegration,
        milestones: Map<String, Int>,
        includeState: Boolean,
    ): String = buildJsonObject {
        put("title", issueTitle(issue))
        put("body", issueBody(issue))
        val labels = merged(issue.labels, integration.defaultLabels)
        if (labels.isNotEmpty()) putJsonArray("labels") { labels.forEach { add(JsonPrimitive(it)) } }
        val assignees = merged(issue.assignees, integration.defaultAssignees)
        if (assignees.isNotEmpty()) {
            putJsonArray("assignees") { assignees.forEach { add(JsonPrimitive(it)) } }
        }
        // GitHub identifies a milestone by number, not title, so a name with no match in the
        // repository is omitted — sending the title would fail the whole request.
        val name = issue.milestone ?: integration.defaultMilestone
        val number = name?.let(milestones::get)
        if (number != null) put("milestone", number)
        if (includeState) put("state", if (issue.isResolved) "closed" else "open")
    }.toString()

    /** The issue's own values first, then any document default it does not already carry. */
    private fun merged(own: List<String>, defaults: List<String>): List<String> =
        own + defaults.filterNot { own.contains(it) }

    private fun issueTitle(issue: Issue): String =
        issue.title.ifEmpty { "Untitled Issue" }

    /**
     * The markdown body pushed to GitHub.
     *
     * Deliberately not `issue.description` alone: the remote issue has to carry the metadata the
     * local document holds in separate fields, because GitHub has nowhere else to put it.
     */
    private fun issueBody(issue: Issue): String {
        val parts = mutableListOf<String>()

        parts += "**Type:** ${issue.type.displayName} | **Priority:** ${issue.priority.displayName}" +
            " | **Status:** ${issue.status.displayName}"

        var dateLine = "**Reported:** ${IssueDate.stringFrom(issue.reported)}"
        if (issue.reportedBy.isNotEmpty()) dateLine += " · ${issue.reportedBy}"
        if (issue.area.isNotEmpty()) dateLine += " | **Area:** ${issue.area}"
        parts += dateLine

        if (issue.description.isNotEmpty()) {
            parts += "---\n**Description**\n\n${issue.description}"
        }
        if (issue.stepsToReproduce.isNotEmpty()) {
            val numbered = issue.stepsToReproduce
                .mapIndexed { index, step -> "${index + 1}. $step" }
                .joinToString("\n")
            parts += "**Steps to Reproduce**\n\n$numbered"
        }
        if (issue.notes.isNotEmpty()) {
            parts += "**Notes / Investigation**\n\n${issue.notes}"
        }
        if (issue.resolution.isNotEmpty()) {
            parts += "**Resolution**\n\n${issue.resolution}"
        }

        parts += "---\n*Synced from local issue tracker · ${issue.displayNumber}*"
        return parts.joinToString("\n\n")
    }

    // MARK: - Requests

    private fun headers(token: String): Map<String, String> = mapOf(
        "Authorization" to "Bearer $token",
        "Accept" to "application/vnd.github+json",
        "X-GitHub-Api-Version" to GITHUB_API_VERSION,
        "Content-Type" to "application/json",
    )

    /**
     * Builds a URL under `/repos/<owner>/<repository>/`, percent-encoding every path segment.
     *
     * Owner and repository are typed by the user, so they may hold spaces or anything else a URL
     * cannot carry raw. An empty segment throws rather than collapsing the path.
     */
    private fun apiUrl(
        integration: GitHubIntegration,
        path: List<String>,
        query: String? = null,
    ): String {
        val segments = listOf("repos", integration.owner, integration.repository) + path
        return buildString {
            append(API_BASE)
            for (segment in segments) {
                if (segment.isEmpty()) throw GitHubSyncException.InvalidRepository()
                append('/').append(encodePathSegment(segment))
            }
            if (query != null) append('?').append(query)
        }
    }

    private suspend fun perform(request: HttpRequest): HttpResponse {
        val response = http.send(request)
        return when {
            response.statusCode in 200..299 -> response
            response.statusCode == 401 -> throw GitHubSyncException.Unauthorized()
            else -> throw GitHubSyncException.ApiError(
                parseObject(response.body)?.get("message")?.stringOrNull()
                    ?: "HTTP ${response.statusCode}",
            )
        }
    }

    // MARK: - Responses

    /** A GitHub issue as this app reads it back from a create or update response. */
    private data class RemoteIssue(val number: Int, val htmlUrl: String?, val updatedAt: Instant?)

    private fun remoteIssue(body: String): RemoteIssue? {
        val json = parseObject(body) ?: return null
        val number = json["number"]?.intOrNull() ?: return null
        return RemoteIssue(
            number = number,
            htmlUrl = json["html_url"]?.stringOrNull(),
            updatedAt = json["updated_at"]?.stringOrNull()?.let(::parseInstant),
        )
    }

    private companion object {
        const val API_BASE = "https://api.github.com"
        const val GITHUB_API_VERSION = "2022-11-28"
        const val MILESTONE_QUERY = "state=all&per_page=100"
    }
}

// MARK: - Encoding and parsing helpers

/**
 * Percent-encodes one path segment, leaving only `A-Z a-z 0-9 - . _ ~` alone.
 *
 * `/` is **deliberately** encoded: an owner or repository typed as `a/b/c` must become one segment,
 * not three, or a user could steer a request at a path the repository coordinates never named.
 * Encoding runs over UTF-8 bytes, so a non-ASCII name produces a valid URL rather than raw bytes.
 */
internal fun encodePathSegment(segment: String): String = buildString {
    for (byte in segment.toByteArray(Charsets.UTF_8)) {
        val value = byte.toInt() and 0xFF
        val char = value.toChar()
        val isUnreserved = char in 'A'..'Z' || char in 'a'..'z' || char in '0'..'9' ||
            char in PATH_SEGMENT_UNRESERVED
        if (isUnreserved) append(char) else append('%').append(HEX.format(Locale.ROOT, value))
    }
}

/**
 * The `next` URL from a `Link` header, or `null` when the page is the last one.
 *
 * Shape: `<https://api.github.com/…?page=2>; rel="next", <…?page=9>; rel="last"`.
 */
internal fun nextPageLink(header: String?): String? {
    if (header.isNullOrBlank()) return null
    for (link in header.split(',')) {
        val parts = link.split(';')
        val target = parts.firstOrNull()?.trim().orEmpty()
        if (!target.startsWith('<') || !target.endsWith('>') || target.length <= 2) continue
        val isNext = parts.drop(1).any {
            it.trim().replace("\"", "").equals("rel=next", ignoreCase = true)
        }
        if (isNext) return target.substring(1, target.length - 1)
    }
    return null
}

private const val PATH_SEGMENT_UNRESERVED = "-._~"
private const val HEX = "%02X"

private fun parseObject(body: String): JsonObject? =
    runCatching { Json.parseToJsonElement(body).jsonObject }.getOrNull()

private fun parseArray(body: String): JsonArray? =
    runCatching { Json.parseToJsonElement(body).jsonArray }.getOrNull()

private fun JsonElement.stringOrNull(): String? =
    (this as? JsonPrimitive)?.takeIf { it.isString }?.content

private fun JsonElement.intOrNull(): Int? =
    (this as? JsonPrimitive)?.takeUnless { it.isString }?.content?.toIntOrNull()

private fun parseInstant(text: String): Instant? = try {
    Instant.parse(text)
} catch (_: DateTimeParseException) {
    null
}

/** The reason shown against a failed issue. Never carries a token: no message here is built from one. */
private fun Throwable.syncMessage(): String =
    message?.takeIf { it.isNotBlank() } ?: (this::class.simpleName ?: "Unknown error")
