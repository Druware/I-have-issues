---
name: issues-file
description: Parse, understand, and resolve issues from the .issues file in this project
---

# Working with `.issues` Files — I Have Issues Project

This project tracks work in `IHaveIssues-Issues.issues` at the repository root. It is a JSON document managed by the **I Have Issues** macOS/iOS app.

> For the full format specification, see the generic skill at `~/.claude/skills/issues-file-spec.md`.

## Key Source Files

| Purpose | Path |
|---|---|
| Issue data model | `IssuesKit/Sources/IssuesKit/Issue.swift` |
| Enums (raw values on disk) | `IssuesKit/Sources/IssuesKit/IssueEnums.swift` |
| JSON encode/decode | `IssuesKit/Sources/IssuesKit/IssuesJSONCoder.swift` |
| Document model + helpers | `IssuesKit/Sources/IssuesKit/IssuesDocumentModel.swift` |
| Markdown export | `IssuesKit/Sources/IssuesKit/IssuesMarkdownSerializer.swift` |
| Legacy markdown import | `IssuesKit/Sources/IssuesKit/LegacyMarkdownImporter.swift` |
| Main UI | `IHaveIssues/App/ContentView.swift` |
| Issue detail view | `IHaveIssues/App/IssueDetailView.swift` |
| Issue edit form | `IHaveIssues/App/IssueEditView.swift` |
| GitHub sync logic | `IHaveIssues/App/GitHubSyncService.swift` |
| GitHub sync UI | `IHaveIssues/App/GitHubSyncView.swift` |
| Document file type | `IHaveIssues/App/IssuesDocument.swift` |

## Workflow: Resolving an Issue

1. Read `IHaveIssues-Issues.issues`.
2. Find open issues: `"status"` is `"open"`, `"inProgress"`, or `"blocked"`.
3. Implement the fix in the appropriate source file(s).
4. Verify with `XcodeRefreshCodeIssuesInFile` after every edit.
5. Update the issue in the JSON file:
   - `"status"` → `"resolved"`
   - `"resolutionKind"` → `"fixed"` (or applicable variant)
   - `"resolution"` → one or two sentences: what changed and where (`File.swift:line`)
   - `"updatedAt"` → current UTC timestamp (`"2026-07-31T19:00:00Z"`)
   - `"closedAt"` → same timestamp

## Known Active Issues (as of 2026-07-31)

| # | Title | Area | Priority |
|---|---|---|---|
| #2 | Sync error list misbehaves with duplicate error messages | GitHubSyncView | Low |
| #3 | Relations/remote links use array offset as SwiftUI identity | IssueDetailView | Low |
| #4 | Milestone lookup capped at 100, no pagination | GitHubSyncService | Medium |
| #5 | Resolved issues create-then-close on first sync | GitHubSyncService | Low |
| #6 | GitHub sync is push-only — no pull from remote | GitHubSyncService | Medium |

## Confirmed Non-Issues from Code Review

These were flagged during review but verified correct:

- `integration.defaultMilestone?.isEmpty == false` — valid Swift; `Optional<Bool> == false` correctly returns `false` for nil and `false` for `.some(true)`.
- `errorAlert?.title ?? ""` in the alert modifier — empty title is never shown because `errorPresentation` is false when `errorAlert` is nil.
- `GitHubSyncView` Done button — correctly calls `if saveSettings() { dismiss() }` and will NOT dismiss on validation failure.
- ISO8601 date parsing with `try?` in `remoteIssue(from:)` — nil result is intentional and handled by `record(_:into:)`.
