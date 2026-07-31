package com.druware.issueskit

import java.util.UUID

/**
 * The full contents of an `.issues` document: project identity, tracker integration coordinates,
 * the label/milestone/people catalogs, export settings, and the issues.
 *
 * This is the source of truth. Markdown is produced from it by [IssuesMarkdownSerializer] and read
 * into it — once, from legacy files — by [LegacyMarkdownImporter].
 */
data class IssuesDocumentModel(
    /** The schema version the document was written with. */
    val schemaVersion: Int = SUPPORTED_SCHEMA_VERSION,
    val project: ProjectInfo = ProjectInfo(),
    val integrations: IntegrationSettings = IntegrationSettings(),
    /** The label catalog, so pickers work offline. */
    val labels: List<LabelDefinition> = emptyList(),
    /** The milestone catalog, so pickers work offline. */
    val milestones: List<Milestone> = emptyList(),
    /** The people catalog, so pickers work offline. */
    val people: List<Person> = emptyList(),
    val export: ExportSettings = ExportSettings(),
    val issues: List<Issue> = emptyList(),
) {
    /** The next display number: one past the highest existing number, or `1` when empty. */
    val nextNumber: Int get() = (issues.maxOfOrNull { it.number } ?: 0) + 1

    /** The entries that belong under `## Open`, in document order. */
    val openIssues: List<Issue> get() = issues.filter { !it.isResolved }

    /** The entries that belong under `## Resolved`, in document order. */
    val resolvedIssues: List<Issue> get() = issues.filter { it.isResolved }

    /** The issue with the given stable identity, or `null` when the document has no such issue. */
    fun issue(withID: UUID): Issue? = issues.firstOrNull { it.uuid == withID }

    companion object {
        /** The highest `schemaVersion` this build can read. Bump it only alongside a migration. */
        const val SUPPORTED_SCHEMA_VERSION = 1

        /** An empty document at the current schema version, used for new documents. */
        fun makeEmpty(): IssuesDocumentModel = IssuesDocumentModel()
    }
}

/** The identity of the project the document tracks. */
data class ProjectInfo(
    val id: UUID = UUID.randomUUID(),
    val name: String = "",
    val summary: String = "",
)

/** How the document is rendered when exported to markdown. */
data class ExportSettings(
    /** Everything emitted before the `## Open` heading — title, how-to text, and template. */
    val preambleMarkdown: String = DEFAULT_PREAMBLE_MARKDOWN,
) {
    companion object {
        /** The preamble a new document exports with. */
        val DEFAULT_PREAMBLE_MARKDOWN: String = """
            # Issues

            A running log of known issues, bugs, and feature requests.

            ## How to use this file

            - Add new issues under **Open**, most recent first.
            - Move issues to **Resolved** when closed, noting the fix and date.
            - Use the template below for each entry. Keep IDs sequential (e.g. `#001`).

            ### Template

            ```
            ### #000 — Short descriptive title

            - **Type:** Bug | Feature | Task | Question
            - **Priority:** Low | Medium | High | Critical
            - **Status:** Open | In Progress | Blocked | Resolved
            - **Reported:** YYYY-MM-DD
            - **Reported by:**
            - **Area:** (e.g. Networking, Views, Session)

            **Description**

            What is the problem or request? What is the expected vs. actual behavior?

            **Steps to reproduce** (bugs only)

            1.
            2.
            3.

            **Notes / Investigation**

            Findings, related files, or discussion.

            **Resolution** (when closed)

            What was changed, and where. Reference commit/PR if applicable.
            ```

            ---
        """.trimIndent() + "\n"
    }
}
