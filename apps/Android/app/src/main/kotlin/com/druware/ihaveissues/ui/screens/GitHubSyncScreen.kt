package com.druware.ihaveissues.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.key
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import com.druware.ihaveissues.R
import com.druware.ihaveissues.ui.GitHubSyncDraft

/**
 * Pushing the document's issues to GitHub.
 *
 * Owner and repository are shown, not edited: they belong to the document and are set once, in
 * Project Settings. The token field is masked and starts empty every time — a stored token is never
 * read back into it.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GitHubSyncScreen(
    draft: GitHubSyncDraft,
    onTokenChange: (String) -> Unit,
    onRemoveToken: () -> Unit,
    onSync: () -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Surface(modifier = modifier.fillMaxSize()) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text("Sync to GitHub") },
                    navigationIcon = {
                        IconButton(onClick = onDismiss) {
                            Icon(painterResource(R.drawable.ic_close), contentDescription = "Close")
                        }
                    },
                    actions = {
                        if (draft.isSyncing) {
                            CircularProgressIndicator(
                                modifier = Modifier
                                    .padding(horizontal = 16.dp)
                                    .size(20.dp),
                                strokeWidth = 2.dp,
                            )
                        } else {
                            TextButton(onClick = onSync, enabled = draft.canSync) { Text("Sync") }
                        }
                    },
                )
            },
        ) { innerPadding ->
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding)
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 16.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                SyncSection("GitHub Repository")
                if (draft.isConfigured) {
                    ReadOnlyRow("Owner", draft.owner)
                    ReadOnlyRow("Repository", draft.repository)
                } else {
                    Text(
                        text = "No GitHub repository configured",
                        style = MaterialTheme.typography.bodyLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                Caption("Set the owner and repository in Project Settings.")

                SyncSection("Authentication")
                OutlinedTextField(
                    value = draft.enteredToken,
                    onValueChange = onTokenChange,
                    label = { Text("Personal access token") },
                    singleLine = true,
                    visualTransformation = PasswordVisualTransformation(),
                    keyboardOptions = KeyboardOptions(
                        capitalization = KeyboardCapitalization.None,
                        autoCorrectEnabled = false,
                        keyboardType = KeyboardType.Password,
                    ),
                    modifier = Modifier.fillMaxWidth(),
                )
                if (draft.hasStoredToken) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween,
                    ) {
                        Text(
                            text = "Token saved",
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        TextButton(onClick = onRemoveToken) {
                            Text("Remove", color = MaterialTheme.colorScheme.error)
                        }
                    }
                }
                Caption(
                    "Requires the repo scope. Create one at github.com/settings/tokens. The token " +
                        "is stored in the Android keystore, never in the document. Entering a token " +
                        "replaces the saved one; leave the field blank to keep it.",
                )

                draft.errorMessage?.let { message ->
                    Text(
                        text = message,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.error,
                    )
                }

                draft.result?.let { result ->
                    SyncSection("Last Sync")
                    if (result.created > 0) Text("${result.created} created")
                    if (result.updated > 0) Text("${result.updated} updated")
                    if (result.failed > 0) {
                        Text("${result.failed} failed", color = MaterialTheme.colorScheme.error)
                    }
                    // Keyed by position, not by the text: two issues can fail for the same reason,
                    // and identical strings used as identity would collapse into one row.
                    result.errors.forEachIndexed { index, error ->
                        key(index) {
                            Text(
                                text = error,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SyncSection(title: String) {
    Text(
        text = title,
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.primary,
        modifier = Modifier.padding(top = 8.dp),
    )
}

@Composable
private fun ReadOnlyRow(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(text = label, style = MaterialTheme.typography.bodyLarge)
        Text(
            text = value.ifBlank { "—" },
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun Caption(text: String) {
    Text(
        text = text,
        style = MaterialTheme.typography.bodySmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}
