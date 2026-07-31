package com.druware.issueskit

import java.time.Instant
import java.util.UUID

/**
 * A single issue-tracker entry stored in an `.issues` document.
 *
 * Identity is split in two on purpose. [uuid] is the stable sync identity used by relations and
 * remote links and never changes; [number] is the human display number (`#007`) and may be
 * renumbered without breaking anything.
 *
 * Every field carries a default, so a fresh entry is one line:
 * `Issue(number = model.nextNumber, title = "Login button does nothing")`.
 */
data class Issue(
    /** The stable sync identity, independent of [number]. */
    val uuid: UUID = UUID.randomUUID(),
    /** The human display number rendered as `#NNN`. */
    val number: Int = 0,
    val title: String = "",
    val type: IssueType = IssueType.DEFAULT,
    val priority: IssuePriority = IssuePriority.DEFAULT,
    val status: IssueStatus = IssueStatus.DEFAULT,
    /** Why the issue was closed, or `null` while it is still open. */
    val resolutionKind: ResolutionKind? = null,
    val labels: List<String> = emptyList(),
    val assignees: List<String> = emptyList(),
    /** The name of a [Milestone] in the document catalog, or `null`. */
    val milestone: String? = null,
    val area: String = "",
    /** The effort estimate in whatever unit the project uses (points, hours), or `null`. */
    val estimate: Double? = null,
    val reportedBy: String = "",
    /** The reported date, normalized to the start of its day in UTC — see [IssueDate]. */
    val reported: Instant = Instant.now(),
    val createdAt: Instant = Instant.now(),
    val updatedAt: Instant = Instant.now(),
    /** When the issue was closed, or `null` while it is still open — see [withStatus]. */
    val closedAt: Instant? = null,
    val description: String = "",
    val stepsToReproduce: List<String> = emptyList(),
    val environment: String = "",
    val notes: String = "",
    val resolution: String = "",
    val comments: List<Comment> = emptyList(),
    val relations: List<Relation> = emptyList(),
    val remoteLinks: List<RemoteLink> = emptyList(),
) {
    /** The stable identity, for list keys and lookups. */
    val id: UUID get() = uuid

    /** Whether the entry belongs in the `## Resolved` section on export. */
    val isResolved: Boolean get() = status == IssueStatus.RESOLVED

    /** The display number rendered as `#NNN`. */
    val displayNumber: String get() = "#%03d".format(java.util.Locale.ROOT, number)

    /**
     * Moves the entry to [newStatus], keeping [closedAt] honest.
     *
     * Closing stamps [closedAt]; reopening clears it. The rule lives here rather than in the UI so
     * no caller can forget it — the Apple app never stamps `closedAt` at all, which leaves resolved
     * issues with no record of when they closed.
     */
    fun withStatus(newStatus: IssueStatus, now: Instant = Instant.now()): Issue = copy(
        status = newStatus,
        closedAt = if (newStatus == IssueStatus.RESOLVED) closedAt ?: now else null,
    )
}

/** A discussion entry attached to an [Issue]. */
data class Comment(
    val id: UUID = UUID.randomUUID(),
    val author: String = "",
    val createdAt: Instant = Instant.now(),
    val body: String = "",
)

/** A typed link from one issue to another within the same document. */
data class Relation(
    val kind: RelationKind = RelationKind.DEFAULT,
    /** The [Issue.uuid] of the other issue. Required — a relation pointing nowhere is not one. */
    val issueID: UUID,
)

/** A pointer from a local issue to its counterpart in an external tracker. */
data class RemoteLink(
    val provider: RemoteProvider = RemoteProvider.Github,
    /** The provider's identifier — a GitHub issue number, or an Azure DevOps work item id. */
    val identifier: String = "",
    /**
     * Stored as text rather than a parsed URL so a document's bytes survive a round trip unchanged;
     * URL normalization would silently rewrite what the user typed.
     */
    val url: String? = null,
    /** When this app last pushed or pulled the remote item. */
    val lastSyncedAt: Instant? = null,
    /** The remote item's own last-modified timestamp, for conflict detection. */
    val remoteUpdatedAt: Instant? = null,
)
