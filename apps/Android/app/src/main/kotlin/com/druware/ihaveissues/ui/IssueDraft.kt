package com.druware.ihaveissues.ui

import com.druware.issueskit.AzureDevOpsIntegration
import com.druware.issueskit.GitHubIntegration
import com.druware.issueskit.IntegrationSettings
import com.druware.issueskit.Issue
import com.druware.issueskit.IssuePriority
import com.druware.issueskit.IssueStatus
import com.druware.issueskit.IssueType
import com.druware.issueskit.IssuesDocumentModel
import com.druware.issueskit.ResolutionKind
import java.time.Instant
import java.time.temporal.ChronoUnit
import java.util.UUID

/**
 * The editable copy of an issue, exactly as the Apple app's `IssueEditView` works: the form mutates
 * a local draft and the document only changes when the user saves, so Cancel is a true discard.
 *
 * Milestone, estimate, labels, assignees and steps are held as the text the user typed rather than
 * as their parsed values. Parsing on every keystroke would fight the user — deleting the last digit
 * of an estimate would blank the field — so the text is authoritative until [toIssue] runs.
 */
data class IssueDraft(
    /** The issue being edited, carrying the identity and the fields the form does not touch. */
    val original: Issue,
    val isNew: Boolean,
    val title: String,
    val type: IssueType,
    val priority: IssuePriority,
    val status: IssueStatus,
    val resolutionKind: ResolutionKind?,
    val reported: Instant,
    val reportedBy: String,
    val area: String,
    val milestoneText: String,
    val estimateText: String,
    val labelsText: String,
    val assigneesText: String,
    val description: String,
    val stepsText: String,
    val environment: String,
    val notes: String,
    val resolution: String,
) {

    /**
     * Folds the draft back into an [Issue].
     *
     * The status goes through [Issue.withStatus] rather than a plain `copy`, so closing an issue
     * stamps `closedAt` and reopening one clears it. The Apple app sets `status` directly and never
     * records when an issue closed; this is the deliberate fix, not a divergence by accident.
     */
    fun toIssue(now: Instant = Instant.now()): Issue = original.copy(
        title = title,
        type = type,
        priority = priority,
        resolutionKind = resolutionKind,
        labels = splitCommaSeparated(labelsText),
        assignees = splitCommaSeparated(assigneesText),
        milestone = optionalText(milestoneText),
        area = area,
        estimate = parseEstimate(estimateText),
        reportedBy = reportedBy,
        reported = reported,
        updatedAt = now,
        description = description,
        stepsToReproduce = splitLines(stepsText),
        environment = environment,
        notes = notes,
        resolution = resolution,
    ).withStatus(status, now)

    companion object {

        /** Opens [issue] for editing. */
        fun of(issue: Issue, isNew: Boolean): IssueDraft = IssueDraft(
            original = issue,
            isNew = isNew,
            title = issue.title,
            type = issue.type,
            priority = issue.priority,
            status = issue.status,
            resolutionKind = issue.resolutionKind,
            reported = issue.reported,
            reportedBy = issue.reportedBy,
            area = issue.area,
            milestoneText = issue.milestone.orEmpty(),
            estimateText = issue.estimate?.let(::formatEstimate).orEmpty(),
            labelsText = issue.labels.joinToString(", "),
            assigneesText = issue.assignees.joinToString(", "),
            description = issue.description,
            stepsText = issue.stepsToReproduce.joinToString("\n"),
            environment = issue.environment,
            notes = issue.notes,
            resolution = issue.resolution,
        )
    }
}

/** The editable copy of the document's project identity and integration coordinates. */
data class ProjectSettingsDraft(
    val name: String,
    val summary: String,
    val gitHubEnabled: Boolean,
    val gitHubOwner: String,
    val gitHubRepository: String,
    val gitHubDefaultLabels: String,
    val gitHubDefaultAssignees: String,
    val gitHubDefaultMilestone: String,
    val azureEnabled: Boolean,
    val azureOrganization: String,
    val azureProject: String,
    val azureTeam: String,
    val azureAreaPath: String,
    val azureIterationPath: String,
    val azureWorkItemType: String,
) {

    /**
     * Folds the draft back into the document.
     *
     * A disabled toggle removes the whole integration block, and a blank optional field becomes
     * `null` rather than `""`, so the document never carries an empty string where the format means
     * "not configured".
     */
    fun apply(model: IssuesDocumentModel): IssuesDocumentModel = model.copy(
        project = model.project.copy(name = name.trim(), summary = summary),
        integrations = IntegrationSettings(
            github = if (!gitHubEnabled) null else GitHubIntegration(
                owner = gitHubOwner.trim(),
                repository = gitHubRepository.trim(),
                defaultLabels = splitCommaSeparated(gitHubDefaultLabels),
                defaultAssignees = splitCommaSeparated(gitHubDefaultAssignees),
                defaultMilestone = optionalText(gitHubDefaultMilestone),
            ),
            azureDevOps = if (!azureEnabled) null else AzureDevOpsIntegration(
                organization = azureOrganization.trim(),
                project = azureProject.trim(),
                team = optionalText(azureTeam),
                areaPath = optionalText(azureAreaPath),
                iterationPath = optionalText(azureIterationPath),
                defaultWorkItemType = optionalText(azureWorkItemType) ?: DEFAULT_WORK_ITEM_TYPE,
            ),
        ),
    )

    companion object {

        /** The Azure DevOps work item type the format defaults to. */
        const val DEFAULT_WORK_ITEM_TYPE = "Issue"

        /**
         * Opens [model]'s settings for editing.
         *
         * Unlike the Apple app this never pre-seeds the GitHub owner/repository from stored
         * preferences: that was a migration convenience for a pre-document Apple build, and there is
         * no Android state to migrate from.
         */
        fun of(model: IssuesDocumentModel): ProjectSettingsDraft {
            val github = model.integrations.github
            val azure = model.integrations.azureDevOps
            return ProjectSettingsDraft(
                name = model.project.name,
                summary = model.project.summary,
                gitHubEnabled = github != null,
                gitHubOwner = github?.owner.orEmpty(),
                gitHubRepository = github?.repository.orEmpty(),
                gitHubDefaultLabels = github?.defaultLabels?.joinToString(", ").orEmpty(),
                gitHubDefaultAssignees = github?.defaultAssignees?.joinToString(", ").orEmpty(),
                gitHubDefaultMilestone = github?.defaultMilestone.orEmpty(),
                azureEnabled = azure != null,
                azureOrganization = azure?.organization.orEmpty(),
                azureProject = azure?.project.orEmpty(),
                azureTeam = azure?.team.orEmpty(),
                azureAreaPath = azure?.areaPath.orEmpty(),
                azureIterationPath = azure?.iterationPath.orEmpty(),
                azureWorkItemType = azure?.defaultWorkItemType ?: DEFAULT_WORK_ITEM_TYPE,
            )
        }
    }
}

// MARK: - Document mutations

/**
 * Replaces the entry with the same [Issue.uuid], or appends when the document has no such entry.
 *
 * Matching on `uuid` rather than on list position is what lets Add and Edit share one path, exactly
 * as the Apple app's `apply(_:)` does.
 */
internal fun IssuesDocumentModel.upserting(issue: Issue): IssuesDocumentModel {
    val index = issues.indexOfFirst { it.uuid == issue.uuid }
    val updated = if (index < 0) issues + issue else issues.toMutableList().apply { this[index] = issue }
    return copy(issues = updated)
}

/**
 * Removes the entry with [id].
 *
 * Relations in other issues that point at [id] are deliberately left dangling, matching Apple: the
 * detail view renders them as "Missing issue" rather than the document silently rewriting entries
 * the user did not ask to change.
 */
internal fun IssuesDocumentModel.removing(id: UUID): IssuesDocumentModel =
    copy(issues = issues.filterNot { it.uuid == id })

/**
 * A blank entry numbered one past the highest in the document.
 *
 * `reported` is normalized to UTC midnight the way `IssueDate.today()` does, so the day a user in
 * Auckland files an issue is the same day it reads back in Los Angeles.
 */
internal fun IssuesDocumentModel.newIssue(now: Instant = Instant.now()): Issue = Issue(
    number = nextNumber,
    reported = now.truncatedTo(ChronoUnit.DAYS),
    createdAt = now,
    updatedAt = now,
)

// MARK: - Text parsing

/**
 * Splits the comma-separated text the Labels and Assignees fields hold.
 *
 * Entries are trimmed and blanks dropped, so `"ui, , regression,"` yields `["ui", "regression"]`.
 */
internal fun splitCommaSeparated(text: String): List<String> =
    text.split(',').map { it.trim() }.filter { it.isNotEmpty() }

/** Splits the one-per-line text the Steps to Reproduce field holds, dropping blank lines. */
internal fun splitLines(text: String): List<String> =
    text.lines().map { it.trim() }.filter { it.isNotEmpty() }

/**
 * Parses the Estimate field.
 *
 * Unparsable text becomes `null` rather than an error, matching Apple's `Double(trimmed)` — the
 * field is optional, so refusing to save over a typo would be worse than dropping it. Non-finite
 * values are rejected too: the format has no way to write an infinity.
 */
internal fun parseEstimate(text: String): Double? =
    text.trim().toDoubleOrNull()?.takeIf { it.isFinite() }

/** Renders an estimate the way markdown export does: `3` for a whole number, otherwise `3.5`. */
internal fun formatEstimate(value: Double): String =
    if (value == value.toLong().toDouble()) value.toLong().toString() else value.toString()

/** A trimmed optional field: blank means "not set", never an empty string. */
internal fun optionalText(text: String): String? = text.trim().ifEmpty { null }
