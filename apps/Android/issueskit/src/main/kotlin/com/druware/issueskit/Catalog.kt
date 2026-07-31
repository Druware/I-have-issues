package com.druware.issueskit

import java.time.Instant

/** A label the project uses, cached in the document so pickers work without network access. */
data class LabelDefinition(
    val name: String = "",
    /** A `RRGGBB` or `#RRGGBB` colour, or `null` when the label has no colour of its own. */
    val colorHex: String? = null,
    val description: String = "",
) {
    val id: String get() = name
}

/** A milestone the project uses, cached in the document so pickers work without network access. */
data class Milestone(
    val name: String = "",
    /** A day-normalized date — see [IssueDate]. */
    val dueOn: Instant? = null,
    val isClosed: Boolean = false,
) {
    val id: String get() = name
}

/**
 * Someone who can report or be assigned an issue, cached in the document so pickers work without
 * network access.
 */
data class Person(
    /** The tracker handle used in [Issue.assignees] and [Issue.reportedBy]. */
    val handle: String = "",
    val displayName: String = "",
    val email: String = "",
) {
    val id: String get() = handle
}
