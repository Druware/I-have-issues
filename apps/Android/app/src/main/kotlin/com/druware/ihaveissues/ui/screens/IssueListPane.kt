package com.druware.ihaveissues.ui.screens

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.druware.ihaveissues.R
import com.druware.ihaveissues.ui.iconRes
import com.druware.ihaveissues.ui.tint
import com.druware.issueskit.Issue
import java.util.UUID

/**
 * The list of issues, grouped into Open and Resolved in document order.
 *
 * There is no search, filter or sort — the Apple app has none, and adding them here would be a
 * product change rather than parity.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun IssueListPane(
    projectName: String,
    documentName: String,
    openIssues: List<Issue>,
    resolvedIssues: List<Issue>,
    selectedIssueId: UUID?,
    onSelect: (Issue) -> Unit,
    onAddIssue: () -> Unit,
    onProjectSettings: () -> Unit,
    onExportMarkdown: () -> Unit,
    onImportMarkdown: () -> Unit,
    onSyncToGitHub: () -> Unit,
    onCloseDocument: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Scaffold(
        modifier = modifier,
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text(
                            text = projectName.trim().ifEmpty { "Issues" },
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                        // Which file is open. Apple gets this from the document window's title.
                        if (documentName.isNotBlank()) {
                            Text(
                                text = documentName,
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis,
                            )
                        }
                    }
                },
                actions = {
                    DocumentOverflowMenu(
                        onProjectSettings = onProjectSettings,
                        onExportMarkdown = onExportMarkdown,
                        onImportMarkdown = onImportMarkdown,
                        onSyncToGitHub = onSyncToGitHub,
                        onCloseDocument = onCloseDocument,
                    )
                },
            )
        },
        floatingActionButton = {
            FloatingActionButton(onClick = onAddIssue) {
                Icon(painterResource(R.drawable.ic_add), contentDescription = "Add issue")
            }
        },
    ) { innerPadding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding),
        ) {
            issueSection("Open", openIssues, "No open issues", selectedIssueId, onSelect)
            issueSection("Resolved", resolvedIssues, "No resolved issues", selectedIssueId, onSelect)
        }
    }
}

/** One Open/Resolved group. Rows are keyed by `uuid` so reordering never reuses the wrong row state. */
private fun LazyListScope.issueSection(
    title: String,
    issues: List<Issue>,
    emptyText: String,
    selectedIssueId: UUID?,
    onSelect: (Issue) -> Unit,
) {
    item(key = "header-$title") { SectionHeader(title) }
    if (issues.isEmpty()) {
        item(key = "empty-$title") {
            Text(
                text = emptyText,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
            )
        }
    } else {
        items(issues, key = { it.uuid }) { issue ->
            IssueRow(issue, isSelected = issue.uuid == selectedIssueId, onClick = { onSelect(issue) })
        }
    }
    item(key = "divider-$title") { HorizontalDivider() }
}

@Composable
private fun SectionHeader(title: String) {
    Text(
        text = title,
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.primary,
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 16.dp, end = 16.dp, top = 16.dp, bottom = 4.dp),
    )
}

/**
 * One row: type icon, title, `#NNN`, priority icon.
 *
 * Deliberately no status indicator, labels or assignees — the Apple row carries none either.
 */
@Composable
private fun IssueRow(issue: Issue, isSelected: Boolean, onClick: () -> Unit) {
    ListItem(
        modifier = Modifier.clickable(onClick = onClick),
        colors = ListItemDefaults.colors(
            containerColor = if (isSelected) {
                MaterialTheme.colorScheme.secondaryContainer
            } else {
                Color.Transparent
            },
        ),
        leadingContent = {
            Icon(
                painter = painterResource(issue.type.iconRes),
                contentDescription = "Type: ${issue.type.displayName}",
                tint = issue.type.tint,
            )
        },
        headlineContent = {
            Text(
                text = issue.title.trim().ifEmpty { "Untitled" },
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        },
        supportingContent = { Text(issue.displayNumber) },
        trailingContent = {
            Icon(
                painter = painterResource(issue.priority.iconRes),
                contentDescription = "Priority: ${issue.priority.displayName}",
                tint = issue.priority.tint,
            )
        },
    )
}

/** Project settings, markdown import/export, and GitHub sync. */
@Composable
private fun DocumentOverflowMenu(
    onProjectSettings: () -> Unit,
    onExportMarkdown: () -> Unit,
    onImportMarkdown: () -> Unit,
    onSyncToGitHub: () -> Unit,
    onCloseDocument: () -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    Box {
        IconButton(onClick = { expanded = true }) {
            Icon(painterResource(R.drawable.ic_more_vert), contentDescription = "Document actions")
        }
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            DropdownMenuItem(
                text = { Text("Project Settings") },
                onClick = { expanded = false; onProjectSettings() },
            )
            // Android has no document browser, so without a way back to the start screen a user
            // could never open a second document.
            DropdownMenuItem(
                text = { Text("Close Document") },
                onClick = { expanded = false; onCloseDocument() },
            )
            DropdownMenuItem(
                text = { Text("Export as Markdown…") },
                onClick = { expanded = false; onExportMarkdown() },
            )
            DropdownMenuItem(
                text = { Text("Import Legacy Markdown…") },
                onClick = { expanded = false; onImportMarkdown() },
            )
            HorizontalDivider()
            DropdownMenuItem(
                text = { Text("Sync to GitHub…") },
                leadingIcon = {
                    Icon(painterResource(R.drawable.ic_sync), contentDescription = null)
                },
                onClick = { expanded = false; onSyncToGitHub() },
            )
        }
    }
}

/** Shown in the detail pane on a wide screen when the user has not picked an issue yet. */
@Composable
fun NoIssueSelected(modifier: Modifier = Modifier) {
    Box(modifier = modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Text(
            text = "No Issue Selected",
            style = MaterialTheme.typography.titleMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}
