package com.druware.ihaveissues.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.unit.dp
import com.druware.ihaveissues.R
import com.druware.ihaveissues.ui.ProjectSettingsDraft

/**
 * The document's project identity and tracker coordinates.
 *
 * Both integration blocks sit behind a toggle: turning one off removes it from the document
 * entirely rather than leaving a block of empty strings behind.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ProjectSettingsScreen(
    draft: ProjectSettingsDraft,
    onChange: ((ProjectSettingsDraft) -> ProjectSettingsDraft) -> Unit,
    onCancel: () -> Unit,
    onSave: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Surface(modifier = modifier.fillMaxSize()) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text("Project Settings") },
                    navigationIcon = {
                        IconButton(onClick = onCancel) {
                            Icon(painterResource(R.drawable.ic_close), contentDescription = "Cancel")
                        }
                    },
                    actions = { TextButton(onClick = onSave) { Text("Save") } },
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
                SettingsSection("Project")
                OutlinedTextField(
                    value = draft.name,
                    onValueChange = { value -> onChange { it.copy(name = value) } },
                    label = { Text("Name") },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = draft.summary,
                    onValueChange = { value -> onChange { it.copy(summary = value) } },
                    label = { Text("Summary") },
                    minLines = 2,
                    modifier = Modifier.fillMaxWidth(),
                )

                SettingsSection("GitHub")
                ToggleRow("Configure GitHub", draft.gitHubEnabled) { value ->
                    onChange { it.copy(gitHubEnabled = value) }
                }
                if (draft.gitHubEnabled) {
                    IdentifierField("Owner", draft.gitHubOwner) { value ->
                        onChange { it.copy(gitHubOwner = value) }
                    }
                    IdentifierField("Repository", draft.gitHubRepository) { value ->
                        onChange { it.copy(gitHubRepository = value) }
                    }
                    OutlinedTextField(
                        value = draft.gitHubDefaultLabels,
                        onValueChange = { value -> onChange { it.copy(gitHubDefaultLabels = value) } },
                        label = { Text("Default labels") },
                        supportingText = { Text("Separate multiple entries with commas.") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    OutlinedTextField(
                        value = draft.gitHubDefaultAssignees,
                        onValueChange = { value -> onChange { it.copy(gitHubDefaultAssignees = value) } },
                        label = { Text("Default assignees") },
                        supportingText = { Text("Separate multiple entries with commas.") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    OutlinedTextField(
                        value = draft.gitHubDefaultMilestone,
                        onValueChange = { value -> onChange { it.copy(gitHubDefaultMilestone = value) } },
                        label = { Text("Default milestone") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Text(
                        text = "The access token is never written to the document. It stays in the " +
                            "platform keystore, because `.issues` files get committed to repositories.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }

                SettingsSection("Azure DevOps")
                ToggleRow("Configure Azure DevOps", draft.azureEnabled) { value ->
                    onChange { it.copy(azureEnabled = value) }
                }
                if (draft.azureEnabled) {
                    IdentifierField("Organization", draft.azureOrganization) { value ->
                        onChange { it.copy(azureOrganization = value) }
                    }
                    IdentifierField("Project", draft.azureProject) { value ->
                        onChange { it.copy(azureProject = value) }
                    }
                    OutlinedTextField(
                        value = draft.azureTeam,
                        onValueChange = { value -> onChange { it.copy(azureTeam = value) } },
                        label = { Text("Team") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    OutlinedTextField(
                        value = draft.azureAreaPath,
                        onValueChange = { value -> onChange { it.copy(azureAreaPath = value) } },
                        label = { Text("Area path") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    OutlinedTextField(
                        value = draft.azureIterationPath,
                        onValueChange = { value -> onChange { it.copy(azureIterationPath = value) } },
                        label = { Text("Iteration path") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    OutlinedTextField(
                        value = draft.azureWorkItemType,
                        onValueChange = { value -> onChange { it.copy(azureWorkItemType = value) } },
                        label = { Text("Default work item type") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Text(
                        text = "Azure DevOps coordinates are stored, but this build has no Azure " +
                            "DevOps sync — neither does the Apple app.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}

@Composable
private fun SettingsSection(title: String) {
    Text(
        text = title,
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.primary,
        modifier = Modifier.padding(top = 8.dp),
    )
}

@Composable
private fun ToggleRow(label: String, checked: Boolean, onCheckedChange: (Boolean) -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(text = label, style = MaterialTheme.typography.bodyLarge)
        Switch(checked = checked, onCheckedChange = onCheckedChange)
    }
}

/** A field for a repository or organization slug: no autocapitalization, no autocorrect. */
@Composable
private fun IdentifierField(label: String, value: String, onValueChange: (String) -> Unit) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        label = { Text(label) },
        singleLine = true,
        keyboardOptions = KeyboardOptions(
            capitalization = KeyboardCapitalization.None,
            autoCorrectEnabled = false,
        ),
        modifier = Modifier.fillMaxWidth(),
    )
}
