package com.druware.ihaveissues.ui

import androidx.annotation.DrawableRes
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.graphics.Color
import com.druware.ihaveissues.R
import com.druware.issueskit.IssuePriority
import com.druware.issueskit.IssueStatus
import com.druware.issueskit.IssueType

/*
 * UI-only decoration for the issue enums — the analogue of the Apple app's `IssuePresentation.swift`.
 *
 * It lives here rather than in `:issueskit` for the same reason it lives outside `IssuesKit` on
 * Apple: an icon and a colour are presentation, not part of the document format. Keeping the whole
 * mapping in one file means there is exactly one place to change when the palette moves.
 *
 * Every colour is a Material **theme role**, never a hard-coded hex value, so the palette follows
 * light/dark mode and Android 12+ dynamic colour. That mirrors Apple's stated rationale for using
 * system semantic colours instead of fixed ones.
 */

// MARK: - Type

/** The glyph for an issue type. Apple: `ladybug.fill` / `sparkles` / `checklist` / `questionmark.circle.fill`. */
@get:DrawableRes
val IssueType.iconRes: Int
    get() = when (this) {
        IssueType.BUG -> R.drawable.ic_issue_type_bug
        IssueType.FEATURE -> R.drawable.ic_issue_type_feature
        IssueType.TASK -> R.drawable.ic_issue_type_task
        IssueType.QUESTION -> R.drawable.ic_issue_type_question
    }

/** The tint for an issue type, drawn from the current colour scheme. */
val IssueType.tint: Color
    @Composable @ReadOnlyComposable
    get() = when (this) {
        IssueType.BUG -> MaterialTheme.colorScheme.error
        IssueType.FEATURE -> MaterialTheme.colorScheme.tertiary
        IssueType.TASK -> MaterialTheme.colorScheme.primary
        IssueType.QUESTION -> MaterialTheme.colorScheme.secondary
    }

// MARK: - Priority

/** The glyph for a priority. Apple: arrow-down / equals / triangle / octagon. */
@get:DrawableRes
val IssuePriority.iconRes: Int
    get() = when (this) {
        IssuePriority.LOW -> R.drawable.ic_issue_priority_low
        IssuePriority.MEDIUM -> R.drawable.ic_issue_priority_medium
        IssuePriority.HIGH -> R.drawable.ic_issue_priority_high
        IssuePriority.CRITICAL -> R.drawable.ic_issue_priority_critical
    }

/** The tint for a priority, escalating from muted through tertiary to error. */
val IssuePriority.tint: Color
    @Composable @ReadOnlyComposable
    get() = when (this) {
        IssuePriority.LOW -> MaterialTheme.colorScheme.onSurfaceVariant
        IssuePriority.MEDIUM -> MaterialTheme.colorScheme.secondary
        IssuePriority.HIGH -> MaterialTheme.colorScheme.tertiary
        IssuePriority.CRITICAL -> MaterialTheme.colorScheme.error
    }

// MARK: - Status

/** The glyph for a status. Apple: `circle` / `clock` / `hand.raised.fill` / `checkmark.circle.fill`. */
@get:DrawableRes
val IssueStatus.iconRes: Int
    get() = when (this) {
        IssueStatus.OPEN -> R.drawable.ic_issue_status_open
        IssueStatus.IN_PROGRESS -> R.drawable.ic_issue_status_in_progress
        IssueStatus.BLOCKED -> R.drawable.ic_issue_status_blocked
        IssueStatus.RESOLVED -> R.drawable.ic_issue_status_resolved
    }

/** The tint for a status. */
val IssueStatus.tint: Color
    @Composable @ReadOnlyComposable
    get() = when (this) {
        IssueStatus.OPEN -> MaterialTheme.colorScheme.onSurfaceVariant
        IssueStatus.IN_PROGRESS -> MaterialTheme.colorScheme.primary
        IssueStatus.BLOCKED -> MaterialTheme.colorScheme.tertiary
        IssueStatus.RESOLVED -> MaterialTheme.colorScheme.primary
    }
