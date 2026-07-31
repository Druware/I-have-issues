package com.druware.ihaveissues.ui

import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.createSavedStateHandle
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import com.druware.ihaveissues.document.DocumentStore
import com.druware.ihaveissues.github.GitHubSyncService
import com.druware.ihaveissues.github.GitHubTokenStore
import com.druware.ihaveissues.github.UrlConnectionHttpClient
import com.druware.ihaveissues.github.gitHubTokenAccount
import com.druware.issueskit.Issue
import com.druware.issueskit.IssuesDocumentModel
import com.druware.issueskit.IssuesJSONCoder
import com.druware.issueskit.IssuesMarkdownSerializer
import com.druware.issueskit.LegacyMarkdownImporter
import java.time.Instant
import java.util.UUID
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext

/** What the app is currently showing on top of the list/detail panes. */
sealed interface Editor {

    /** The add/edit form, holding a draft copy of the issue. */
    data class IssueForm(val draft: IssueDraft) : Editor

    /** The project settings form. */
    data class ProjectSettings(val draft: ProjectSettingsDraft) : Editor

    /** The GitHub sync screen. */
    data class GitHubSync(val draft: GitHubSyncDraft) : Editor
}

/** A destructive action waiting on the user's confirmation. */
sealed interface Confirmation {

    /** Deleting [issue] from the document. */
    data class DeleteIssue(val issue: Issue) : Confirmation

    /** Replacing the whole document with the legacy markdown file at [uri]. */
    data class ReplaceWithMarkdown(val uri: String) : Confirmation
}

/** Everything the UI renders. */
data class IssuesUiState(
    /** The open document's URI, or `null` when the start screen should show. */
    val documentUri: String? = null,
    val documentName: String = "",
    val model: IssuesDocumentModel? = null,
    val selectedIssueId: UUID? = null,
    val isLoading: Boolean = false,
    val isSaving: Boolean = false,
    /** Whether the in-memory model differs from what is on disk. */
    val isDirty: Boolean = false,
    /** The last autosave failure, kept until a retry succeeds so the user cannot miss it. */
    val saveFailure: String? = null,
    /** A one-shot message for the snackbar — an open or import failure. */
    val alert: String? = null,
    val editor: Editor? = null,
    val confirmation: Confirmation? = null,
)

/**
 * Owns the open document and every change made to it.
 *
 * All mutation funnels through [mutate], so there is exactly one place that marks the document dirty
 * and exactly one place that schedules the autosave. Composables read [state] and call back in; they
 * never hold document state of their own.
 *
 * ### Autosave
 * The product decision is to save after every change. A per-keystroke write would hammer the
 * provider, so each mutation restarts a debounce timer and only the last change in a burst triggers
 * a write. The write itself runs under [writeMutex] and re-reads the current model *inside* the
 * lock, so a slow write can never land stale bytes on top of a newer edit, and it runs
 * [NonCancellable] so restarting the debounce can never abandon a half-written file. A failure
 * leaves the document dirty and surfaces through [IssuesUiState.saveFailure] with a retry — silent
 * autosave failure loses the user's work.
 */
class IssuesViewModel(
    private val store: DocumentStore,
    private val savedState: SavedStateHandle,
    private val tokens: GitHubTokenStore,
    private val gitHub: GitHubSyncService,
    private val autosaveDelayMillis: Long = AUTOSAVE_DEBOUNCE_MILLIS,
    private val clock: () -> Instant = Instant::now,
) : ViewModel() {

    private val _state = MutableStateFlow(IssuesUiState())
    val state: StateFlow<IssuesUiState> = _state.asStateFlow()

    private val writeMutex = Mutex()
    private var autosaveJob: Job? = null

    init {
        // Survives both configuration change and process death: the URI is the only thing worth
        // restoring, because the document itself is re-read from it.
        savedState.get<String>(KEY_DOCUMENT_URI)?.let { openDocument(it, takePermission = false) }
    }

    // MARK: - Opening and creating

    /** Opens the document at [uri], replacing whatever is open. */
    fun openDocument(uri: String, takePermission: Boolean = true) {
        autosaveJob?.cancel()
        _state.update { it.copy(isLoading = true, alert = null) }
        viewModelScope.launch {
            if (takePermission) store.takePersistablePermission(uri)
            try {
                val model = IssuesJSONCoder.decode(store.read(uri))
                savedState[KEY_DOCUMENT_URI] = uri
                _state.value = IssuesUiState(
                    documentUri = uri,
                    documentName = store.displayName(uri).orEmpty(),
                    model = model,
                )
            } catch (cancellation: CancellationException) {
                throw cancellation
            } catch (error: Exception) {
                _state.update { it.copy(isLoading = false, alert = error.readableMessage()) }
            }
        }
    }

    /** Creates an empty document at [uri] and opens it. */
    fun createDocument(uri: String) {
        autosaveJob?.cancel()
        _state.update { it.copy(isLoading = true, alert = null) }
        viewModelScope.launch {
            store.takePersistablePermission(uri)
            val model = IssuesDocumentModel.makeEmpty()
            try {
                store.write(uri, IssuesJSONCoder.encode(model))
                savedState[KEY_DOCUMENT_URI] = uri
                _state.value = IssuesUiState(
                    documentUri = uri,
                    documentName = store.displayName(uri).orEmpty(),
                    model = model,
                )
            } catch (cancellation: CancellationException) {
                throw cancellation
            } catch (error: Exception) {
                _state.update { it.copy(isLoading = false, alert = error.readableMessage()) }
            }
        }
    }

    /**
     * Returns to the start screen so another document can be opened.
     *
     * The pending autosave is flushed and the document is only let go once that write lands: closing
     * over the top of a failed save would silently discard the very edits the retry snackbar is
     * offering to save.
     */
    fun closeDocument() {
        autosaveJob?.cancel()
        viewModelScope.launch {
            withContext(NonCancellable) { performSave() }
            if (_state.value.saveFailure == null) {
                savedState.remove<String>(KEY_DOCUMENT_URI)
                _state.value = IssuesUiState()
            }
        }
    }

    // MARK: - Selection

    fun selectIssue(id: UUID?) {
        _state.update { it.copy(selectedIssueId = id) }
    }

    // MARK: - Issue editing

    /** Opens the form on a fresh entry numbered one past the highest in the document. */
    fun beginAddIssue() {
        val model = _state.value.model ?: return
        val draft = IssueDraft.of(model.newIssue(clock()), isNew = true)
        _state.update { it.copy(editor = Editor.IssueForm(draft)) }
    }

    /** Opens the form on a copy of [issue]. */
    fun beginEditIssue(issue: Issue) {
        _state.update { it.copy(editor = Editor.IssueForm(IssueDraft.of(issue, isNew = false))) }
    }

    /** Applies a field change to the open draft. */
    fun updateIssueDraft(transform: (IssueDraft) -> IssueDraft) {
        _state.update { current ->
            val editor = current.editor as? Editor.IssueForm ?: return@update current
            current.copy(editor = Editor.IssueForm(transform(editor.draft)))
        }
    }

    /** Commits the open draft into the document. */
    fun saveIssueDraft() {
        val editor = _state.value.editor as? Editor.IssueForm ?: return
        val issue = editor.draft.toIssue(clock())
        _state.update { it.copy(editor = null, selectedIssueId = issue.uuid) }
        mutate { it.upserting(issue) }
    }

    /** Discards the open draft. */
    fun cancelEditor() {
        _state.update { it.copy(editor = null) }
    }

    // MARK: - Project settings

    fun beginProjectSettings() {
        val model = _state.value.model ?: return
        _state.update { it.copy(editor = Editor.ProjectSettings(ProjectSettingsDraft.of(model))) }
    }

    fun updateSettingsDraft(transform: (ProjectSettingsDraft) -> ProjectSettingsDraft) {
        _state.update { current ->
            val editor = current.editor as? Editor.ProjectSettings ?: return@update current
            current.copy(editor = Editor.ProjectSettings(transform(editor.draft)))
        }
    }

    fun saveProjectSettings() {
        val editor = _state.value.editor as? Editor.ProjectSettings ?: return
        _state.update { it.copy(editor = null) }
        mutate { editor.draft.apply(it) }
    }

    // MARK: - GitHub sync

    /**
     * Opens the sync screen.
     *
     * Only whether a token exists is read, never the token itself — the screen has no use for the
     * secret, and the sync reads it straight from the store when it runs.
     */
    fun beginGitHubSync() {
        val model = _state.value.model ?: return
        val integration = model.integrations.github
        val account = gitHubTokenAccount(integration)
        _state.update {
            it.copy(
                editor = Editor.GitHubSync(
                    GitHubSyncDraft(
                        isConfigured = integration != null,
                        owner = integration?.owner.orEmpty(),
                        repository = integration?.repository.orEmpty(),
                        hasStoredToken = account != null && tokens.hasToken(account),
                    ),
                ),
            )
        }
    }

    /** Records what the user typed into the token field. */
    fun updateSyncToken(token: String) {
        updateSyncDraft { it.copy(enteredToken = token) }
    }

    /** Forgets the stored token for this repository. The only way to clear one. */
    fun removeStoredToken() {
        val account = gitHubTokenAccount(_state.value.model?.integrations?.github) ?: return
        tokens.delete(account)
        updateSyncDraft { it.copy(hasStoredToken = false, enteredToken = "") }
    }

    /** Leaves the sync screen, saving a typed token first — and staying put if it cannot be saved. */
    fun dismissGitHubSync() {
        if (saveSyncToken()) _state.update { it.copy(editor = null) }
    }

    /**
     * Pushes the document's issues to GitHub and folds the returned issues back in.
     *
     * The result goes through [mutate] like every other change, so the new remote links are marked
     * dirty and autosaved rather than living only in memory.
     */
    fun performSync() {
        if (_state.value.editor !is Editor.GitHubSync) return
        val integration = _state.value.model?.integrations?.github ?: return
        if (!saveSyncToken()) return

        val account = gitHubTokenAccount(integration)
        val token = account?.let(tokens::load)
        if (token == null) {
            updateSyncDraft {
                it.copy(errorMessage = "No GitHub token saved for this repository. Enter one and try again.")
            }
            return
        }

        val issues = _state.value.model?.issues ?: return
        updateSyncDraft { it.copy(isSyncing = true, errorMessage = null, result = null) }
        viewModelScope.launch {
            try {
                val outcome = gitHub.sync(issues, integration, token)
                mutate { it.copy(issues = outcome.issues) }
                updateSyncDraft { it.copy(isSyncing = false, result = outcome.result) }
            } catch (cancellation: CancellationException) {
                throw cancellation
            } catch (error: Exception) {
                updateSyncDraft { it.copy(isSyncing = false, errorMessage = error.readableMessage()) }
            }
        }
    }

    /**
     * Stores a typed token against the document's repository, reporting whether it is now safe to
     * leave the screen.
     *
     * A blank field means "keep what is stored", not "delete it" — the field starts empty every
     * time, so leaving it alone must never be destructive. With no repository configured there is no
     * account to scope an entry to, so a typed token cannot be stored anywhere: say so rather than
     * dropping it on the floor.
     */
    private fun saveSyncToken(): Boolean {
        val draft = (_state.value.editor as? Editor.GitHubSync)?.draft ?: return false
        val account = gitHubTokenAccount(_state.value.model?.integrations?.github)
        if (account == null) {
            if (draft.enteredToken.isEmpty()) return true
            updateSyncDraft {
                it.copy(
                    errorMessage = "Set the owner and repository in Project Settings before saving a token.",
                )
            }
            return false
        }
        if (draft.enteredToken.isNotEmpty()) {
            tokens.save(draft.enteredToken, account)
            updateSyncDraft { it.copy(hasStoredToken = true, errorMessage = null) }
        }
        return true
    }

    private fun updateSyncDraft(transform: (GitHubSyncDraft) -> GitHubSyncDraft) {
        _state.update { current ->
            val editor = current.editor as? Editor.GitHubSync ?: return@update current
            current.copy(editor = Editor.GitHubSync(transform(editor.draft)))
        }
    }

    // MARK: - Deleting

    fun requestDelete(issue: Issue) {
        _state.update { it.copy(confirmation = Confirmation.DeleteIssue(issue)) }
    }

    fun cancelConfirmation() {
        _state.update { it.copy(confirmation = null) }
    }

    /** Removes the issue named by the pending confirmation. */
    fun confirmDelete() {
        val pending = _state.value.confirmation as? Confirmation.DeleteIssue ?: return
        _state.update {
            it.copy(
                confirmation = null,
                selectedIssueId = it.selectedIssueId.takeUnless { id -> id == pending.issue.uuid },
            )
        }
        mutate { it.removing(pending.issue.uuid) }
    }

    // MARK: - Markdown

    /** The markdown for the open document, or `null` when no document is open. */
    fun markdownExport(): String? = _state.value.model?.let(IssuesMarkdownSerializer::export)

    /** The file name a markdown export should be offered under. */
    fun markdownExportName(): String {
        val name = _state.value.model?.project?.name?.trim().orEmpty()
        return (name.ifEmpty { "Issues" }) + ".md"
    }

    /** Writes the markdown export to [uri]. */
    fun exportMarkdown(uri: String) {
        val markdown = markdownExport() ?: return
        viewModelScope.launch {
            try {
                store.write(uri, markdown.toByteArray(Charsets.UTF_8))
            } catch (cancellation: CancellationException) {
                throw cancellation
            } catch (error: Exception) {
                _state.update { it.copy(alert = error.readableMessage()) }
            }
        }
    }

    /** Asks for confirmation before a legacy import replaces the document. */
    fun requestLegacyImport(uri: String) {
        _state.update { it.copy(confirmation = Confirmation.ReplaceWithMarkdown(uri)) }
    }

    /** Replaces the whole document with the imported markdown, as Apple does — this is not a merge. */
    fun confirmLegacyImport() {
        val pending = _state.value.confirmation as? Confirmation.ReplaceWithMarkdown ?: return
        _state.update { it.copy(confirmation = null) }
        viewModelScope.launch {
            try {
                val markdown = store.read(pending.uri).toString(Charsets.UTF_8)
                val imported = LegacyMarkdownImporter.importDocument(markdown)
                _state.update { it.copy(selectedIssueId = null) }
                mutate { imported }
            } catch (cancellation: CancellationException) {
                throw cancellation
            } catch (error: Exception) {
                _state.update { it.copy(alert = error.readableMessage()) }
            }
        }
    }

    // MARK: - Saving

    /**
     * Applies [transform] to the document, marks it dirty and restarts the autosave debounce.
     *
     * Every document change goes through here. Nothing else may touch `state.model`.
     */
    private fun mutate(transform: (IssuesDocumentModel) -> IssuesDocumentModel) {
        val current = _state.value.model ?: return
        _state.update { it.copy(model = transform(current), isDirty = true) }
        scheduleAutosave()
    }

    private fun scheduleAutosave() {
        if (_state.value.documentUri == null) return
        autosaveJob?.cancel()
        autosaveJob = viewModelScope.launch {
            delay(autosaveDelayMillis)
            // NonCancellable: cancelling the debounce must never interrupt a write in progress, or
            // the next restart would leave a truncated file on disk.
            withContext(NonCancellable) { performSave() }
        }
    }

    /**
     * Writes now, skipping the debounce.
     *
     * Called on `ON_STOP` so backgrounding the app never loses the last edit, and by the snackbar's
     * Retry action.
     */
    fun flush() {
        if (_state.value.documentUri == null) return
        autosaveJob?.cancel()
        autosaveJob = viewModelScope.launch {
            withContext(NonCancellable) { performSave() }
        }
    }

    /** Clears the one-shot alert once the snackbar has shown it. */
    fun consumeAlert() {
        _state.update { it.copy(alert = null) }
    }

    private suspend fun performSave() {
        writeMutex.withLock {
            // Read inside the lock: whatever a queued-behind write serializes must be the newest
            // model, never the one that was current when it was scheduled.
            val snapshot = _state.value
            val uri = snapshot.documentUri ?: return
            val model = snapshot.model ?: return
            if (!snapshot.isDirty && snapshot.saveFailure == null) return

            _state.update { it.copy(isSaving = true) }
            try {
                store.write(uri, IssuesJSONCoder.encode(model))
                _state.update {
                    it.copy(
                        isSaving = false,
                        saveFailure = null,
                        // Only clean if nothing changed while the write ran.
                        isDirty = it.model != model,
                    )
                }
            } catch (cancellation: CancellationException) {
                _state.update { it.copy(isSaving = false) }
                throw cancellation
            } catch (error: Exception) {
                _state.update {
                    it.copy(isSaving = false, isDirty = true, saveFailure = error.readableMessage())
                }
            }
        }
    }

    companion object {

        /**
         * How long a burst of edits is allowed to settle before a write.
         *
         * Long enough that typing a title is one write rather than thirty, short enough that a save
         * always lands before the user can reach for the home gesture.
         */
        const val AUTOSAVE_DEBOUNCE_MILLIS = 750L

        private const val KEY_DOCUMENT_URI = "documentUri"

        /** Builds the view model with the Storage Access Framework store and the keystore tokens. */
        fun factory(store: DocumentStore, tokens: GitHubTokenStore): ViewModelProvider.Factory =
            viewModelFactory {
                initializer {
                    IssuesViewModel(
                        store = store,
                        savedState = createSavedStateHandle(),
                        tokens = tokens,
                        gitHub = GitHubSyncService(UrlConnectionHttpClient()),
                    )
                }
            }
    }
}

/** The message shown to the user for [this], falling back to the type name for exceptions with none. */
internal fun Throwable.readableMessage(): String =
    message?.takeIf { it.isNotBlank() } ?: (this::class.simpleName ?: "Unknown error")
