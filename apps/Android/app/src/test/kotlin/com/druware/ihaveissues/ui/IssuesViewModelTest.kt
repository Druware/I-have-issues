package com.druware.ihaveissues.ui

import androidx.lifecycle.SavedStateHandle
import com.druware.ihaveissues.document.DocumentStore
import com.druware.ihaveissues.github.FakeGitHubTokenStore
import com.druware.ihaveissues.github.FakeHttpClient
import com.druware.ihaveissues.github.GitHubSyncService
import com.druware.issueskit.GitHubIntegration
import com.druware.issueskit.IntegrationSettings
import com.druware.issueskit.Issue
import com.druware.issueskit.IssueStatus
import com.druware.issueskit.IssuesDocumentModel
import com.druware.issueskit.IssuesJSONCoder
import java.io.IOException
import java.time.Instant
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.jupiter.api.AfterEach
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test

/**
 * The view model's document lifecycle: add, edit, delete, and the autosave contract.
 *
 * `viewModelScope` runs on `Dispatchers.Main`, so the test dispatcher is installed as Main and every
 * assertion runs after `advanceUntilIdle()` — that is what makes the debounce observable rather than
 * a race.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class IssuesViewModelTest {

    private val dispatcher = StandardTestDispatcher()
    private val now: Instant = Instant.parse("2026-07-31T10:00:00Z")
    private lateinit var store: FakeDocumentStore
    private lateinit var tokens: FakeGitHubTokenStore
    private lateinit var http: FakeHttpClient

    private companion object {
        const val URI = "content://test/Example.issues"
    }

    @BeforeEach
    fun installMainDispatcher() {
        Dispatchers.setMain(dispatcher)
        store = FakeDocumentStore()
        tokens = FakeGitHubTokenStore()
        http = FakeHttpClient()
    }

    @AfterEach
    fun restoreMainDispatcher() {
        Dispatchers.resetMain()
    }

    private fun viewModel(savedState: SavedStateHandle = SavedStateHandle()) = IssuesViewModel(
        store = store,
        savedState = savedState,
        tokens = tokens,
        gitHub = GitHubSyncService(http),
        autosaveDelayMillis = 10,
        clock = { now },
    )

    private fun TestScope.openedViewModel(
        model: IssuesDocumentModel = IssuesDocumentModel.makeEmpty(),
    ): IssuesViewModel {
        store.files[URI] = IssuesJSONCoder.encode(model)
        val viewModel = viewModel()
        viewModel.openDocument(URI)
        advanceUntilIdle()
        return viewModel
    }

    // MARK: - Opening

    @Test
    fun `opening a document decodes it and takes a persistable permission`() = runTest(dispatcher) {
        val viewModel = openedViewModel(IssuesDocumentModel(issues = listOf(Issue(number = 1))))

        assertEquals(1, viewModel.state.value.model?.issues?.size)
        assertEquals(listOf(URI), store.persisted)
        assertFalse(viewModel.state.value.isDirty)
    }

    @Test
    fun `a document that will not decode surfaces an alert instead of opening`() = runTest(dispatcher) {
        store.files[URI] = "not json".toByteArray()
        val viewModel = viewModel()

        viewModel.openDocument(URI)
        advanceUntilIdle()

        assertNull(viewModel.state.value.model)
        assertNotNull(viewModel.state.value.alert)
    }

    @Test
    fun `the open document URI is restored from saved state after process death`() = runTest(dispatcher) {
        store.files[URI] = IssuesJSONCoder.encode(IssuesDocumentModel(issues = listOf(Issue(number = 4))))

        val restored = viewModel(SavedStateHandle(mapOf("documentUri" to URI)))
        advanceUntilIdle()

        assertEquals(URI, restored.state.value.documentUri)
        assertEquals(4, restored.state.value.model?.issues?.single()?.number)
    }

    @Test
    fun `creating a document writes an empty one before opening it`() = runTest(dispatcher) {
        val viewModel = viewModel()

        viewModel.createDocument(URI)
        advanceUntilIdle()

        assertEquals(emptyList(), viewModel.state.value.model?.issues)
        assertEquals(1, store.writeCount)
    }

    // MARK: - Add, edit, delete

    @Test
    fun `adding an issue appends it, selects it, and autosaves`() = runTest(dispatcher) {
        val viewModel = openedViewModel()

        viewModel.beginAddIssue()
        viewModel.updateIssueDraft { it.copy(title = "Login button does nothing") }
        viewModel.saveIssueDraft()
        advanceUntilIdle()

        val issue = viewModel.state.value.model?.issues?.single()
        assertEquals("Login button does nothing", issue?.title)
        assertEquals(1, issue?.number)
        assertEquals(issue?.uuid, viewModel.state.value.selectedIssueId)
        assertFalse(viewModel.state.value.isDirty)
        assertEquals(1, store.writeCount)
        assertEquals(1, store.decoded(URI).issues.size)
    }

    @Test
    fun `cancelling the form discards the draft`() = runTest(dispatcher) {
        val viewModel = openedViewModel()

        viewModel.beginAddIssue()
        viewModel.updateIssueDraft { it.copy(title = "Never saved") }
        viewModel.cancelEditor()
        advanceUntilIdle()

        assertEquals(emptyList(), viewModel.state.value.model?.issues)
        assertEquals(0, store.writeCount)
    }

    @Test
    fun `editing an issue replaces it rather than appending a copy`() = runTest(dispatcher) {
        val original = Issue(number = 1, title = "Original")
        val viewModel = openedViewModel(IssuesDocumentModel(issues = listOf(original)))

        viewModel.beginEditIssue(original)
        viewModel.updateIssueDraft { it.copy(title = "Renamed") }
        viewModel.saveIssueDraft()
        advanceUntilIdle()

        val issues = viewModel.state.value.model?.issues.orEmpty()
        assertEquals(1, issues.size)
        assertEquals("Renamed", issues.single().title)
        assertEquals(original.uuid, issues.single().uuid)
    }

    @Test
    fun `resolving an issue through the form stamps closedAt on disk`() = runTest(dispatcher) {
        val original = Issue(number = 1, status = IssueStatus.OPEN)
        val viewModel = openedViewModel(IssuesDocumentModel(issues = listOf(original)))

        viewModel.beginEditIssue(original)
        viewModel.updateIssueDraft { it.copy(status = IssueStatus.RESOLVED) }
        viewModel.saveIssueDraft()
        advanceUntilIdle()

        assertEquals(now, store.decoded(URI).issues.single().closedAt)
    }

    @Test
    fun `deleting an issue removes it and clears the selection`() = runTest(dispatcher) {
        val first = Issue(number = 1)
        val second = Issue(number = 2)
        val viewModel = openedViewModel(IssuesDocumentModel(issues = listOf(first, second)))
        viewModel.selectIssue(first.uuid)

        viewModel.requestDelete(first)
        viewModel.confirmDelete()
        advanceUntilIdle()

        assertEquals(listOf(second.uuid), viewModel.state.value.model?.issues?.map { it.uuid })
        assertNull(viewModel.state.value.selectedIssueId)
        assertEquals(1, store.decoded(URI).issues.size)
    }

    @Test
    fun `deleting an unselected issue leaves the selection alone`() = runTest(dispatcher) {
        val first = Issue(number = 1)
        val second = Issue(number = 2)
        val viewModel = openedViewModel(IssuesDocumentModel(issues = listOf(first, second)))
        viewModel.selectIssue(second.uuid)

        viewModel.requestDelete(first)
        viewModel.confirmDelete()
        advanceUntilIdle()

        assertEquals(second.uuid, viewModel.state.value.selectedIssueId)
    }

    @Test
    fun `cancelling the delete confirmation keeps the issue`() = runTest(dispatcher) {
        val issue = Issue(number = 1)
        val viewModel = openedViewModel(IssuesDocumentModel(issues = listOf(issue)))

        viewModel.requestDelete(issue)
        viewModel.cancelConfirmation()
        advanceUntilIdle()

        assertEquals(1, viewModel.state.value.model?.issues?.size)
        assertEquals(0, store.writeCount)
    }

    // MARK: - Autosave

    @Test
    fun `a burst of edits is debounced into a single write`() = runTest(dispatcher) {
        val viewModel = openedViewModel()

        repeat(5) { index ->
            viewModel.beginAddIssue()
            viewModel.updateIssueDraft { it.copy(title = "Issue $index") }
            viewModel.saveIssueDraft()
        }
        advanceUntilIdle()

        assertEquals(5, viewModel.state.value.model?.issues?.size)
        assertEquals(1, store.writeCount)
        assertEquals(5, store.decoded(URI).issues.size)
    }

    @Test
    fun `a failed write keeps the document dirty and reports the failure`() = runTest(dispatcher) {
        val viewModel = openedViewModel()
        store.failWith = IOException("The document was deleted.")

        viewModel.beginAddIssue()
        viewModel.saveIssueDraft()
        advanceUntilIdle()

        assertTrue(viewModel.state.value.isDirty)
        assertEquals("The document was deleted.", viewModel.state.value.saveFailure)
    }

    @Test
    fun `retrying after a failure writes and clears the failure`() = runTest(dispatcher) {
        val viewModel = openedViewModel()
        store.failWith = IOException("Out of space.")
        viewModel.beginAddIssue()
        viewModel.saveIssueDraft()
        advanceUntilIdle()

        store.failWith = null
        viewModel.flush()
        advanceUntilIdle()

        assertFalse(viewModel.state.value.isDirty)
        assertNull(viewModel.state.value.saveFailure)
        assertEquals(1, store.decoded(URI).issues.size)
    }

    @Test
    fun `flush writes without waiting for the debounce`() = runTest(dispatcher) {
        val viewModel = openedViewModel()
        viewModel.beginAddIssue()
        viewModel.saveIssueDraft()

        viewModel.flush()
        advanceUntilIdle()

        assertEquals(1, store.writeCount)
        assertFalse(viewModel.state.value.isDirty)
    }

    @Test
    fun `nothing is written when nothing changed`() = runTest(dispatcher) {
        val viewModel = openedViewModel()

        viewModel.flush()
        advanceUntilIdle()

        assertEquals(0, store.writeCount)
    }

    @Test
    fun `closing a document flushes the pending edit before letting go of it`() = runTest(dispatcher) {
        val viewModel = openedViewModel()
        viewModel.beginAddIssue()
        viewModel.saveIssueDraft()

        viewModel.closeDocument()
        advanceUntilIdle()

        assertNull(viewModel.state.value.documentUri)
        assertNull(viewModel.state.value.model)
        assertEquals(1, store.decoded(URI).issues.size)
    }

    @Test
    fun `a document with a failed save stays open so the retry is still reachable`() = runTest(dispatcher) {
        val viewModel = openedViewModel()
        store.failWith = IOException("Read-only volume.")
        viewModel.beginAddIssue()
        viewModel.saveIssueDraft()
        advanceUntilIdle()

        viewModel.closeDocument()
        advanceUntilIdle()

        assertEquals(URI, viewModel.state.value.documentUri)
        assertTrue(viewModel.state.value.isDirty)
    }

    // MARK: - Project settings

    @Test
    fun `saving project settings renames the project and autosaves`() = runTest(dispatcher) {
        val viewModel = openedViewModel()

        viewModel.beginProjectSettings()
        viewModel.updateSettingsDraft { it.copy(name = " I Have Issues ") }
        viewModel.saveProjectSettings()
        advanceUntilIdle()

        assertEquals("I Have Issues", viewModel.state.value.model?.project?.name)
        assertEquals("I Have Issues", store.decoded(URI).project.name)
    }

    // MARK: - Markdown

    @Test
    fun `the markdown export name falls back when the project is unnamed`() = runTest(dispatcher) {
        val viewModel = openedViewModel()

        assertEquals("Issues.md", viewModel.markdownExportName())
    }

    // MARK: - GitHub sync

    private fun gitHubModel() = IssuesDocumentModel(
        integrations = IntegrationSettings(
            github = GitHubIntegration(owner = "OpenBCM", repository = "I-Have-Issues"),
        ),
        issues = listOf(Issue(number = 1, title = "Login button does nothing")),
    )

    private fun syncDraft(viewModel: IssuesViewModel): GitHubSyncDraft =
        (viewModel.state.value.editor as Editor.GitHubSync).draft

    @Test
    fun `the sync screen reports a stored token without ever reading it`() = runTest(dispatcher) {
        tokens.save("ghp_secret", "openbcm/i-have-issues")
        val viewModel = openedViewModel(gitHubModel())

        viewModel.beginGitHubSync()

        val draft = syncDraft(viewModel)
        assertTrue(draft.hasStoredToken)
        assertEquals("", draft.enteredToken)
        assertTrue(draft.canSync)
    }

    @Test
    fun `sync is refused until a repository and a token are both present`() = runTest(dispatcher) {
        val viewModel = openedViewModel(gitHubModel())

        viewModel.beginGitHubSync()
        assertFalse(syncDraft(viewModel).canSync)

        viewModel.updateSyncToken("ghp_typed")
        assertTrue(syncDraft(viewModel).canSync)
    }

    @Test
    fun `a synced issue is stored through the document so autosave persists it`() = runTest(dispatcher) {
        tokens.save("ghp_secret", "openbcm/i-have-issues")
        http.respondWith { FakeHttpClient.created(number = 412, htmlUrl = "https://github.test/412") }
        val viewModel = openedViewModel(gitHubModel())

        viewModel.beginGitHubSync()
        viewModel.performSync()
        advanceUntilIdle()

        assertEquals(1, syncDraft(viewModel).result?.created)
        val link = store.decoded(URI).issues.single().remoteLinks.single()
        assertEquals("412", link.identifier)
        assertEquals("https://github.test/412", link.url)
    }

    @Test
    fun `a typed token is stored against the repository on dismissal`() = runTest(dispatcher) {
        val viewModel = openedViewModel(gitHubModel())

        viewModel.beginGitHubSync()
        viewModel.updateSyncToken("ghp_typed")
        viewModel.dismissGitHubSync()

        assertEquals("ghp_typed", tokens.tokens["openbcm/i-have-issues"])
        assertNull(viewModel.state.value.editor)
    }

    @Test
    fun `a blank token field keeps the stored token rather than clearing it`() = runTest(dispatcher) {
        tokens.save("ghp_secret", "openbcm/i-have-issues")
        val viewModel = openedViewModel(gitHubModel())

        viewModel.beginGitHubSync()
        viewModel.dismissGitHubSync()

        assertEquals("ghp_secret", tokens.tokens["openbcm/i-have-issues"])
    }

    @Test
    fun `Remove is the only way a stored token is cleared`() = runTest(dispatcher) {
        tokens.save("ghp_secret", "openbcm/i-have-issues")
        val viewModel = openedViewModel(gitHubModel())

        viewModel.beginGitHubSync()
        viewModel.removeStoredToken()

        assertFalse(tokens.tokens.containsKey("openbcm/i-have-issues"))
        assertFalse(syncDraft(viewModel).hasStoredToken)
    }

    @Test
    fun `a token typed with no repository configured is reported, not discarded`() = runTest(dispatcher) {
        val viewModel = openedViewModel()

        viewModel.beginGitHubSync()
        viewModel.updateSyncToken("ghp_typed")
        viewModel.dismissGitHubSync()

        assertNotNull(viewModel.state.value.editor)
        assertNotNull(syncDraft(viewModel).errorMessage)
        assertTrue(tokens.tokens.isEmpty())
    }

    @Test
    fun `an aborted sync surfaces the failure and leaves the issues alone`() = runTest(dispatcher) {
        tokens.save("ghp_expired", "openbcm/i-have-issues")
        http.respondWith { FakeHttpClient.failure(401) }
        val viewModel = openedViewModel(gitHubModel())

        viewModel.beginGitHubSync()
        viewModel.performSync()
        advanceUntilIdle()

        val draft = syncDraft(viewModel)
        assertFalse(draft.isSyncing)
        assertNotNull(draft.errorMessage)
        assertEquals(emptyList(), viewModel.state.value.model?.issues?.single()?.remoteLinks)
    }

    @Test
    fun `a legacy import replaces the whole document only after confirmation`() = runTest(dispatcher) {
        val viewModel = openedViewModel(IssuesDocumentModel(issues = listOf(Issue(number = 1))))
        val markdownUri = "content://test/legacy.md"
        store.files[markdownUri] = LEGACY_MARKDOWN.toByteArray()

        viewModel.requestLegacyImport(markdownUri)
        assertEquals(1, viewModel.state.value.model?.issues?.size)

        viewModel.confirmLegacyImport()
        advanceUntilIdle()

        val issues = viewModel.state.value.model?.issues.orEmpty()
        assertEquals(1, issues.size)
        assertEquals("Imported entry", issues.single().title)
        assertNull(viewModel.state.value.selectedIssueId)
    }
}

private val LEGACY_MARKDOWN = """
    # Issues

    ## Open

    ### #001 — Imported entry

    - **Type:** Bug

    **Description**

    From markdown.

    ## Resolved

""".trimIndent()

/** An in-memory [DocumentStore]; no Android framework involved. */
private class FakeDocumentStore : DocumentStore {

    val files = mutableMapOf<String, ByteArray>()
    val persisted = mutableListOf<String>()
    var writeCount = 0
    var failWith: Exception? = null

    override suspend fun read(uri: String): ByteArray =
        files[uri] ?: throw IOException("No such document: $uri")

    override suspend fun write(uri: String, bytes: ByteArray) {
        failWith?.let { throw it }
        writeCount++
        files[uri] = bytes
    }

    override suspend fun displayName(uri: String): String = uri.substringAfterLast('/')

    override fun takePersistablePermission(uri: String) {
        persisted += uri
    }

    fun decoded(uri: String): IssuesDocumentModel = IssuesJSONCoder.decode(files.getValue(uri))
}
