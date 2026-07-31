package com.druware.ihaveissues.ui.screens

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import com.druware.ihaveissues.R
import com.druware.ihaveissues.ui.formatEstimate
import com.druware.ihaveissues.ui.iconRes
import com.druware.ihaveissues.ui.inlineMarkdown
import com.druware.ihaveissues.ui.tint
import com.druware.issueskit.Issue
import com.druware.issueskit.IssueDate
import com.druware.issueskit.IssuesDocumentModel
import java.time.ZoneId
import java.time.format.DateTimeFormatter

/**
 * The read-only view of one issue.
 *
 * Sections follow the Apple app's order and each is omitted entirely when empty, so a sparse issue
 * reads as a short card rather than a page of blank headings. Editing happens in a separate form.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun IssueDetailPane(
    issue: Issue,
    model: IssuesDocumentModel,
    showBackButton: Boolean,
    onBack: () -> Unit,
    onEdit: (Issue) -> Unit,
    onDelete: (Issue) -> Unit,
    modifier: Modifier = Modifier,
) {
    Scaffold(
        modifier = modifier,
        topBar = {
            TopAppBar(
                title = { Text(issue.displayNumber) },
                navigationIcon = {
                    if (showBackButton) {
                        IconButton(onClick = onBack) {
                            Icon(painterResource(R.drawable.ic_arrow_back), contentDescription = "Back to list")
                        }
                    }
                },
                actions = {
                    IconButton(onClick = { onEdit(issue) }) {
                        Icon(painterResource(R.drawable.ic_edit), contentDescription = "Edit issue")
                    }
                    IconButton(onClick = { onDelete(issue) }) {
                        Icon(
                            painter = painterResource(R.drawable.ic_delete),
                            contentDescription = "Delete issue",
                            tint = MaterialTheme.colorScheme.error,
                        )
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
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            HeaderCard(issue)
            TextSection("Description", issue.description)
            StepsSection(issue.stepsToReproduce)
            TextSection("Environment", issue.environment)
            TextSection("Notes / Investigation", issue.notes)
            TextSection("Resolution", issue.resolution)
            CommentsSection(issue)
            RelatedIssuesSection(issue, model)
            RemoteLinksSection(issue)
        }
    }
}

@Composable
private fun HeaderCard(issue: Issue) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(issue.displayNumber, style = MaterialTheme.typography.labelLarge)
            Text(
                text = issue.title.trim().ifEmpty { "Untitled" },
                style = MaterialTheme.typography.headlineSmall,
            )
            HorizontalDivider(modifier = Modifier.padding(vertical = 4.dp))

            MetadataRow("Type", issue.type.displayName, issue.type.iconRes, issue.type.tint)
            MetadataRow("Priority", issue.priority.displayName, issue.priority.iconRes, issue.priority.tint)
            MetadataRow("Status", issue.status.displayName, issue.status.iconRes, issue.status.tint)
            issue.resolutionKind?.let { MetadataRow("Resolution", it.displayName) }
            MetadataRow("Reported", IssueDate.stringFrom(issue.reported))
            if (issue.reportedBy.isNotBlank()) MetadataRow("Reported by", issue.reportedBy)
            if (issue.area.isNotBlank()) MetadataRow("Area", issue.area)
            if (issue.labels.isNotEmpty()) MetadataRow("Labels", issue.labels.joinToString(", "))
            if (issue.assignees.isNotEmpty()) MetadataRow("Assignees", issue.assignees.joinToString(", "))
            issue.milestone?.takeIf { it.isNotBlank() }?.let { MetadataRow("Milestone", it) }
            issue.estimate?.let { MetadataRow("Estimate", formatEstimate(it)) }
        }
    }
}

/** One label/value line. Long values wrap instead of clipping. */
@Composable
private fun MetadataRow(label: String, value: String, iconRes: Int? = null, tint: Color? = null) {
    Row(verticalAlignment = Alignment.Top) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.width(104.dp),
        )
        if (iconRes != null) {
            Icon(
                painter = painterResource(iconRes),
                contentDescription = null,
                tint = tint ?: MaterialTheme.colorScheme.onSurface,
                modifier = Modifier
                    .padding(end = 6.dp)
                    .width(18.dp),
            )
        }
        Text(text = value, style = MaterialTheme.typography.bodyMedium)
    }
}

@Composable
private fun SectionTitle(title: String) {
    Text(text = title, style = MaterialTheme.typography.titleMedium)
}

@Composable
private fun TextSection(title: String, body: String) {
    if (body.isBlank()) return
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        SectionTitle(title)
        Text(text = inlineMarkdown(body), style = MaterialTheme.typography.bodyMedium)
    }
}

@Composable
private fun StepsSection(steps: List<String>) {
    if (steps.isEmpty()) return
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        SectionTitle("Steps to Reproduce")
        steps.forEachIndexed { index, step ->
            Row(verticalAlignment = Alignment.Top) {
                Text(
                    text = "${index + 1}.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.width(28.dp),
                )
                Text(text = inlineMarkdown(step), style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

@Composable
private fun CommentsSection(issue: Issue) {
    if (issue.comments.isEmpty()) return
    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        SectionTitle("Comments")
        // Comments carry their own stable id, so the list never keys on position.
        issue.comments.forEach { comment ->
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(
                    text = listOfNotNull(
                        comment.author.ifBlank { null },
                        DATE_FORMAT.format(comment.createdAt.atZone(ZoneId.of("UTC"))),
                    ).joinToString(" · "),
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(text = inlineMarkdown(comment.body), style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

@Composable
private fun RelatedIssuesSection(issue: Issue, model: IssuesDocumentModel) {
    if (issue.relations.isEmpty()) return
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        SectionTitle("Related Issues")
        // Keyed conceptually by the target uuid, never by array position — Apple's index keys are a
        // known bug in its own tracker (#3).
        issue.relations.forEach { relation ->
            val target = model.issue(relation.issueID)
            Row(verticalAlignment = Alignment.Top) {
                Text(
                    text = relation.kind.displayName,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.width(112.dp),
                )
                Text(
                    text = target?.let { "${it.displayNumber} ${it.title}" } ?: "Missing issue",
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
        }
    }
}

@Composable
private fun RemoteLinksSection(issue: Issue) {
    if (issue.remoteLinks.isEmpty()) return
    val uriHandler = LocalUriHandler.current
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        SectionTitle("Remote Links")
        // Identified by provider + identifier, the pair that actually names a remote item.
        issue.remoteLinks.forEach { link ->
            val label = "${link.provider.displayName} #${link.identifier}"
            val url = link.url
            if (url.isNullOrBlank()) {
                Text(text = label, style = MaterialTheme.typography.bodyMedium)
            } else {
                Text(
                    text = label,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.primary,
                    textDecoration = TextDecoration.Underline,
                    modifier = Modifier.clickable { uriHandler.openUri(url) },
                )
            }
        }
    }
}

private val DATE_FORMAT: DateTimeFormatter = DateTimeFormatter.ofPattern("uuuu-MM-dd")
