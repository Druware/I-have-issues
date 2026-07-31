package com.druware.ihaveissues.ui

import com.druware.ihaveissues.github.SyncResult

/**
 * Everything the GitHub sync screen shows.
 *
 * [enteredToken] only ever holds what the user just typed. A stored token is never read back into
 * it: the screen needs to know that one exists, not what it is, which is what [hasStoredToken] is
 * for. Owner and repository are read-only here because they belong to the document and are edited
 * once, in Project Settings.
 */
data class GitHubSyncDraft(
    /** Whether the document carries a GitHub integration block at all. */
    val isConfigured: Boolean = false,
    val owner: String = "",
    val repository: String = "",
    val enteredToken: String = "",
    val hasStoredToken: Boolean = false,
    val isSyncing: Boolean = false,
    val result: SyncResult? = null,
    val errorMessage: String? = null,
) {

    /** Whether there is a repository to push to and a token to push with. */
    val canSync: Boolean
        get() = isConfigured &&
            owner.isNotBlank() &&
            repository.isNotBlank() &&
            (hasStoredToken || enteredToken.isNotBlank()) &&
            !isSyncing
}
