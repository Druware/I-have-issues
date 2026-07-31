package com.druware.ihaveissues.ui.screens

import android.content.res.Configuration
import androidx.compose.runtime.Composable
import androidx.compose.ui.tooling.preview.Preview
import com.druware.ihaveissues.github.SyncResult
import com.druware.ihaveissues.ui.GitHubSyncDraft
import com.druware.ihaveissues.ui.IssueDraft
import com.druware.ihaveissues.ui.IssuesListDetail
import com.druware.ihaveissues.ui.ProjectSettingsDraft
import com.druware.ihaveissues.ui.theme.IHaveIssuesTheme
import com.druware.issueskit.Comment
import com.druware.issueskit.GitHubIntegration
import com.druware.issueskit.IntegrationSettings
import com.druware.issueskit.Issue
import com.druware.issueskit.IssuePriority
import com.druware.issueskit.IssueStatus
import com.druware.issueskit.IssueType
import com.druware.issueskit.IssuesDocumentModel
import com.druware.issueskit.ProjectInfo
import com.druware.issueskit.Relation
import com.druware.issueskit.RelationKind
import com.druware.issueskit.RemoteLink
import com.druware.issueskit.RemoteProvider
import com.druware.issueskit.ResolutionKind
import java.time.Instant
import java.util.UUID

/*
 * Previews at both form factors.
 *
 * The adaptive scaffold reads the window size class, so rendering the same composable at a phone
 * width and at a tablet width is what exercises the single-pane and two-pane branches. No emulator
 * is involved, and no code in the app decides the layout from these numbers.
 */

private const val PHONE = "spec:width=411dp,height=891dp"
private const val PHONE_LANDSCAPE = "spec:width=891dp,height=411dp"
private const val TABLET = "spec:width=1280dp,height=800dp"

// MARK: - Fixtures

private val FIRST_ID = UUID.fromString("3B8A0C51-2F44-4E6D-B0A7-1C9E5D4F8A02")
private val SECOND_ID = UUID.fromString("A1F4C7D2-5E90-4B3A-8C61-0D2E7F5A9B44")
private val REPORTED: Instant = Instant.parse("2026-05-01T00:00:00Z")

private val sampleModel = IssuesDocumentModel(
    project = ProjectInfo(name = "I Have Issues", summary = "A document-based issue tracker."),
    integrations = IntegrationSettings(
        github = GitHubIntegration(owner = "openbcm", repository = "i-have-issues"),
    ),
    issues = listOf(
        Issue(
            uuid = FIRST_ID,
            number = 1,
            title = "Login button does nothing",
            type = IssueType.BUG,
            priority = IssuePriority.HIGH,
            status = IssueStatus.IN_PROGRESS,
            labels = listOf("regression", "ui"),
            assignees = listOf("dru"),
            milestone = "v1.0",
            area = "Views",
            estimate = 3.5,
            reportedBy = "dru",
            reported = REPORTED,
            description = "Tapping **Login** has no effect. No network request is made.",
            stepsToReproduce = listOf("Launch with no saved session", "Enter credentials", "Tap Login"),
            environment = "Pixel 8, Android 16",
            notes = "The action binding was dropped when the view was split.",
            comments = listOf(
                Comment(author = "sam", createdAt = REPORTED, body = "Only on a cold launch."),
            ),
            relations = listOf(Relation(RelationKind.BLOCKS, SECOND_ID)),
            remoteLinks = listOf(
                RemoteLink(
                    provider = RemoteProvider.Github,
                    identifier = "412",
                    url = "https://github.com/openbcm/i-have-issues/issues/412",
                ),
            ),
        ),
        Issue(
            uuid = SECOND_ID,
            number = 2,
            title = "Add keyboard shortcuts",
            type = IssueType.FEATURE,
            priority = IssuePriority.LOW,
            reported = REPORTED,
        ),
        Issue(
            number = 3,
            title = "",
            type = IssueType.QUESTION,
            priority = IssuePriority.CRITICAL,
            status = IssueStatus.RESOLVED,
            resolutionKind = ResolutionKind.FIXED,
            reported = REPORTED,
            resolution = "Answered in the design review.",
        ),
    ),
)

private val sampleIssue = sampleModel.issues.first()

// MARK: - Adaptive layout

/** Compact width: one pane, list only, until an issue is picked. */
@Preview(name = "List/detail — phone", device = PHONE, showBackground = true)
@Composable
private fun ListDetailPhonePreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        IssuesListDetail(
            model = sampleModel,
            documentName = "Example.issues",
            selectedIssueId = null,
            onSelect = {},
            onAddIssue = {},
            onEditIssue = {},
            onDeleteIssue = {},
            onProjectSettings = {},
            onExportMarkdown = {},
            onImportMarkdown = {},
            onSyncToGitHub = {},
            onCloseDocument = {},
        )
    }
}

/** Expanded width: both panes at once, with the selection driving the detail pane in place. */
@Preview(name = "List/detail — tablet", device = TABLET, showBackground = true)
@Composable
private fun ListDetailTabletPreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        IssuesListDetail(
            model = sampleModel,
            documentName = "Example.issues",
            selectedIssueId = FIRST_ID,
            onSelect = {},
            onAddIssue = {},
            onEditIssue = {},
            onDeleteIssue = {},
            onProjectSettings = {},
            onExportMarkdown = {},
            onImportMarkdown = {},
            onSyncToGitHub = {},
            onCloseDocument = {},
        )
    }
}

/** A phone on its side is a medium window, so the split appears there too. */
@Preview(name = "List/detail — phone landscape", device = PHONE_LANDSCAPE, showBackground = true)
@Composable
private fun ListDetailPhoneLandscapePreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        IssuesListDetail(
            model = sampleModel,
            documentName = "Example.issues",
            selectedIssueId = FIRST_ID,
            onSelect = {},
            onAddIssue = {},
            onEditIssue = {},
            onDeleteIssue = {},
            onProjectSettings = {},
            onExportMarkdown = {},
            onImportMarkdown = {},
            onSyncToGitHub = {},
            onCloseDocument = {},
        )
    }
}

// MARK: - Individual screens

@Preview(name = "List pane", device = PHONE, showBackground = true)
@Composable
private fun ListPanePreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        IssueListPane(
            projectName = sampleModel.project.name,
            documentName = "Example.issues",
            openIssues = sampleModel.openIssues,
            resolvedIssues = sampleModel.resolvedIssues,
            selectedIssueId = FIRST_ID,
            onSelect = {},
            onAddIssue = {},
            onProjectSettings = {},
            onExportMarkdown = {},
            onImportMarkdown = {},
            onSyncToGitHub = {},
            onCloseDocument = {},
        )
    }
}

@Preview(name = "List pane — empty document", device = PHONE, showBackground = true)
@Composable
private fun EmptyListPanePreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        IssueListPane(
            projectName = "",
            documentName = "Untitled.issues",
            openIssues = emptyList(),
            resolvedIssues = emptyList(),
            selectedIssueId = null,
            onSelect = {},
            onAddIssue = {},
            onProjectSettings = {},
            onExportMarkdown = {},
            onImportMarkdown = {},
            onSyncToGitHub = {},
            onCloseDocument = {},
        )
    }
}

@Preview(name = "Detail pane", device = PHONE, showBackground = true)
@Preview(
    name = "Detail pane — dark",
    device = PHONE,
    showBackground = true,
    uiMode = Configuration.UI_MODE_NIGHT_YES or Configuration.UI_MODE_TYPE_NORMAL,
)
@Composable
private fun DetailPanePreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        IssueDetailPane(
            issue = sampleIssue,
            model = sampleModel,
            showBackButton = true,
            onBack = {},
            onEdit = {},
            onDelete = {},
        )
    }
}

@Preview(name = "Edit form", device = PHONE, showBackground = true)
@Composable
private fun EditScreenPreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        IssueEditScreen(
            draft = IssueDraft.of(sampleIssue, isNew = false),
            onChange = {},
            onCancel = {},
            onSave = {},
        )
    }
}

@Preview(name = "Project settings", device = PHONE, showBackground = true)
@Composable
private fun ProjectSettingsPreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        ProjectSettingsScreen(
            draft = ProjectSettingsDraft.of(sampleModel),
            onChange = {},
            onCancel = {},
            onSave = {},
        )
    }
}

@Preview(name = "GitHub sync", device = PHONE, showBackground = true)
@Composable
private fun GitHubSyncPreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        GitHubSyncScreen(
            draft = GitHubSyncDraft(
                isConfigured = true,
                owner = "openbcm",
                repository = "i-have-issues",
                hasStoredToken = true,
                result = SyncResult(
                    created = 2,
                    updated = 1,
                    failed = 2,
                    // Two identical strings on purpose: the list must show both rows.
                    errors = listOf("#004: Not Found", "#009: Not Found"),
                ),
            ),
            onTokenChange = {},
            onRemoveToken = {},
            onSync = {},
            onDismiss = {},
        )
    }
}

@Preview(name = "Start screen", device = PHONE, showBackground = true)
@Preview(name = "Start screen — tablet", device = TABLET, showBackground = true)
@Composable
private fun StartScreenPreview() {
    IHaveIssuesTheme(dynamicColor = false) {
        StartScreen(isLoading = false, onNewDocument = {}, onOpenDocument = {})
    }
}
