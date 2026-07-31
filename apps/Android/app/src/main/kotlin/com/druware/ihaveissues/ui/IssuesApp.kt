package com.druware.ihaveissues.ui

import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarDuration
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.SnackbarResult
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.adaptive.ExperimentalMaterial3AdaptiveApi
import androidx.compose.material3.adaptive.layout.AnimatedPane
import androidx.compose.material3.adaptive.layout.ListDetailPaneScaffoldRole
import androidx.compose.material3.adaptive.navigation.NavigableListDetailPaneScaffold
import androidx.compose.material3.adaptive.navigation.rememberListDetailPaneScaffoldNavigator
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.druware.ihaveissues.document.DocumentTypes
import com.druware.ihaveissues.ui.screens.GitHubSyncScreen
import com.druware.ihaveissues.ui.screens.IssueDetailPane
import com.druware.ihaveissues.ui.screens.IssueEditScreen
import com.druware.ihaveissues.ui.screens.IssueListPane
import com.druware.ihaveissues.ui.screens.NoIssueSelected
import com.druware.ihaveissues.ui.screens.ProjectSettingsScreen
import com.druware.ihaveissues.ui.screens.StartScreen
import com.druware.issueskit.Issue
import com.druware.issueskit.IssuesDocumentModel
import java.util.UUID
import kotlinx.coroutines.launch

/**
 * The root of the UI: document pickers, autosave lifecycle, and the list/detail scaffold.
 *
 * Everything below this is stateless and previewable; this is the one composable that knows about
 * the view model and the Android activity result APIs.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun IssuesApp(state: IssuesUiState, viewModel: IssuesViewModel, modifier: Modifier = Modifier) {
    val snackbarHostState = remember { SnackbarHostState() }

    val openDocument = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let { viewModel.openDocument(it.toString()) }
    }
    val createDocument = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument(DocumentTypes.ISSUES_MIME_TYPE),
    ) { uri -> uri?.let { viewModel.createDocument(it.toString()) } }
    val exportMarkdown = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument(DocumentTypes.MARKDOWN_MIME_TYPE),
    ) { uri -> uri?.let { viewModel.exportMarkdown(it.toString()) } }
    val importMarkdown = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let { viewModel.requestLegacyImport(it.toString()) }
    }

    // Backgrounding must not lose the last edit: flush whatever the debounce is still holding.
    val lifecycleOwner = LocalLifecycleOwner.current
    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_STOP) viewModel.flush()
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }

    // An autosave that fails silently loses work, so the failure is a snackbar with a retry and the
    // document stays dirty until a write actually succeeds.
    LaunchedEffect(state.saveFailure) {
        val failure = state.saveFailure ?: return@LaunchedEffect
        val result = snackbarHostState.showSnackbar(
            message = "Could not save: $failure",
            actionLabel = "Retry",
            duration = SnackbarDuration.Indefinite,
        )
        if (result == SnackbarResult.ActionPerformed) viewModel.flush()
    }

    LaunchedEffect(state.alert) {
        val alert = state.alert ?: return@LaunchedEffect
        snackbarHostState.showSnackbar(alert)
        viewModel.consumeAlert()
    }

    Scaffold(
        modifier = modifier,
        snackbarHost = { SnackbarHost(snackbarHostState) },
        // Insets are handled by the per-pane scaffolds below, which are the ones that draw bars.
        contentWindowInsets = WindowInsets(0, 0, 0, 0),
    ) { innerPadding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding),
        ) {
            val model = state.model
            if (model == null) {
                StartScreen(
                    isLoading = state.isLoading,
                    onNewDocument = { createDocument.launch(DocumentTypes.DEFAULT_DOCUMENT_NAME) },
                    onOpenDocument = { openDocument.launch(DocumentTypes.OPEN_ISSUES_TYPES) },
                )
            } else {
                IssuesListDetail(
                    model = model,
                    documentName = state.documentName,
                    selectedIssueId = state.selectedIssueId,
                    onSelect = viewModel::selectIssue,
                    onAddIssue = viewModel::beginAddIssue,
                    onEditIssue = viewModel::beginEditIssue,
                    onDeleteIssue = viewModel::requestDelete,
                    onProjectSettings = viewModel::beginProjectSettings,
                    onExportMarkdown = { exportMarkdown.launch(viewModel.markdownExportName()) },
                    onImportMarkdown = { importMarkdown.launch(DocumentTypes.OPEN_MARKDOWN_TYPES) },
                    onSyncToGitHub = viewModel::beginGitHubSync,
                    onCloseDocument = viewModel::closeDocument,
                )
            }

            when (val editor = state.editor) {
                is Editor.IssueForm -> {
                    BackHandler(onBack = viewModel::cancelEditor)
                    IssueEditScreen(
                        draft = editor.draft,
                        onChange = viewModel::updateIssueDraft,
                        onCancel = viewModel::cancelEditor,
                        onSave = viewModel::saveIssueDraft,
                    )
                }

                is Editor.ProjectSettings -> {
                    BackHandler(onBack = viewModel::cancelEditor)
                    ProjectSettingsScreen(
                        draft = editor.draft,
                        onChange = viewModel::updateSettingsDraft,
                        onCancel = viewModel::cancelEditor,
                        onSave = viewModel::saveProjectSettings,
                    )
                }

                is Editor.GitHubSync -> {
                    // Back goes through the same dismissal as the close button, so a token typed
                    // with no repository configured is reported rather than silently discarded.
                    BackHandler(onBack = viewModel::dismissGitHubSync)
                    GitHubSyncScreen(
                        draft = editor.draft,
                        onTokenChange = viewModel::updateSyncToken,
                        onRemoveToken = viewModel::removeStoredToken,
                        onSync = viewModel::performSync,
                        onDismiss = viewModel::dismissGitHubSync,
                    )
                }

                null -> Unit
            }
        }
    }

    when (val confirmation = state.confirmation) {
        is Confirmation.DeleteIssue -> AlertDialog(
            onDismissRequest = viewModel::cancelConfirmation,
            title = { Text("Delete ${confirmation.issue.displayNumber}?") },
            text = { Text("This removes the issue from the document. This cannot be undone from here.") },
            confirmButton = { TextButton(onClick = viewModel::confirmDelete) { Text("Delete") } },
            dismissButton = { TextButton(onClick = viewModel::cancelConfirmation) { Text("Cancel") } },
        )

        is Confirmation.ReplaceWithMarkdown -> AlertDialog(
            onDismissRequest = viewModel::cancelConfirmation,
            title = { Text("Replace this document with the imported markdown?") },
            text = {
                Text("Every issue and setting in this document is discarded and replaced by the markdown file.")
            },
            confirmButton = { TextButton(onClick = viewModel::confirmLegacyImport) { Text("Replace") } },
            dismissButton = { TextButton(onClick = viewModel::cancelConfirmation) { Text("Cancel") } },
        )

        null -> Unit
    }
}

/**
 * The list/detail split.
 *
 * Nothing here branches on a hard-coded width. `NavigableListDetailPaneScaffold` asks
 * `currentWindowAdaptiveInfo()` for the window size class and decides for itself: on a compact
 * window it shows one pane at a time and selecting an issue navigates to the detail pane, and on a
 * medium or expanded window it shows both side by side and selection just updates the detail pane in
 * place. That makes split-screen and folded/unfolded states correct for free, which a `dp` threshold
 * or a `sw600dp` resource qualifier would not.
 *
 * Back is wired through the same navigator, so on a phone the system back gesture returns to the
 * list rather than leaving the app, and on a tablet — where there is nothing to go back to — it
 * falls through to the system as it should.
 */
@OptIn(ExperimentalMaterial3AdaptiveApi::class)
@Composable
fun IssuesListDetail(
    model: IssuesDocumentModel,
    documentName: String,
    selectedIssueId: UUID?,
    onSelect: (UUID?) -> Unit,
    onAddIssue: () -> Unit,
    onEditIssue: (Issue) -> Unit,
    onDeleteIssue: (Issue) -> Unit,
    onProjectSettings: () -> Unit,
    onExportMarkdown: () -> Unit,
    onImportMarkdown: () -> Unit,
    onSyncToGitHub: () -> Unit,
    onCloseDocument: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val navigator = rememberListDetailPaneScaffoldNavigator<String>()
    val scope = rememberCoroutineScope()

    // Deleting the issue the detail pane is showing leaves a single-pane phone staring at an empty
    // placeholder, so the selection going away takes the pane back to the list with it.
    LaunchedEffect(selectedIssueId) {
        if (selectedIssueId == null && navigator.canNavigateBack()) navigator.navigateBack()
    }

    NavigableListDetailPaneScaffold(
        navigator = navigator,
        modifier = modifier,
        listPane = {
            AnimatedPane {
                IssueListPane(
                    projectName = model.project.name,
                    documentName = documentName,
                    openIssues = model.openIssues,
                    resolvedIssues = model.resolvedIssues,
                    selectedIssueId = selectedIssueId,
                    onSelect = { issue ->
                        onSelect(issue.uuid)
                        scope.launch {
                            navigator.navigateTo(ListDetailPaneScaffoldRole.Detail, issue.uuid.toString())
                        }
                    },
                    onAddIssue = onAddIssue,
                    onProjectSettings = onProjectSettings,
                    onExportMarkdown = onExportMarkdown,
                    onImportMarkdown = onImportMarkdown,
                    onSyncToGitHub = onSyncToGitHub,
                    onCloseDocument = onCloseDocument,
                )
            }
        },
        detailPane = {
            AnimatedPane {
                val issue = selectedIssueId?.let(model::issue)
                if (issue == null) {
                    NoIssueSelected()
                } else {
                    IssueDetailPane(
                        issue = issue,
                        model = model,
                        showBackButton = navigator.canNavigateBack(),
                        onBack = { scope.launch { navigator.navigateBack() } },
                        onEdit = onEditIssue,
                        onDelete = onDeleteIssue,
                    )
                }
            }
        },
    )
}
