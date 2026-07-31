# The `.issues` document format

`.issues` is the source of truth for an *I Have Issues* document: a single UTF-8 JSON file
holding the project's identity, its issue-tracker integration coordinates, the label /
milestone / people catalogs, the markdown export settings, and the issues themselves.

Markdown is no longer authoritative. It is produced from a document by
`IssuesMarkdownSerializer.export(_:)` and read *into* one — once, from pre-`.issues` files —
by `LegacyMarkdownImporter.importDocument(from:)`.

Read and write the format through `IssuesJSONCoder`; do not hand-roll a `JSONEncoder`.

---

## Rules

### No credentials, ever

`.issues` files are committed to project repositories. A token stored here is published to
everyone with read access, to every fork, and to every CI log.

The `integrations` object therefore carries only non-secret coordinates — owner, repository,
organization, project, default labels and paths. Personal access tokens, OAuth tokens, and
passwords belong in the **Keychain**, keyed by those coordinates. Never add a field for one.

### Schema versioning

- `schemaVersion` is a required integer. A file without it is not an `.issues` document and
  decoding throws `IssuesError.missingSchemaVersion`.
- The current version is **1** (`IssuesDocumentModel.supportedSchemaVersion`).
- A file whose `schemaVersion` is **greater** than the supported version throws
  `IssuesError.unsupportedSchemaVersion(found:supported:)` — an older build must refuse a
  newer file rather than silently drop what it does not understand.
- A file whose `schemaVersion` is **lower** is accepted. Only version 1 exists today, so
  there is nothing to migrate yet; the migration seam is the `guard` in
  `IssuesDocumentModel.init(from:)`, which is where a future version reads the old shape,
  transforms it, and sets `schemaVersion` to the migrated value.
- Bump the version only alongside a migration.

### Decode tolerance

The format is forward- and backward-tolerant by design:

- **Absent keys** fall back to the documented default — every optional and every collection.
- **Unknown keys** are ignored, not fatal, so a newer build's extra fields survive being
  opened in an older one (they are dropped on the next save).
- **Unknown enum values** fall back to that enum's default rather than throwing, with two
  deliberate exceptions:
  - `resolutionKind` decodes to `null` — inventing a close reason would misreport why work
    stopped.
  - `remoteLinks[].provider` preserves the unrecognized value verbatim as `.other(raw)` and
    re-encodes it byte-identically. A provider is *sync identity*: coercing an unknown
    provider to `github` would aim a GitHub sync at whatever issue happens to share the
    identifier in the user's real repository, and re-saving would make that permanent.
    Sync code must gate on `provider.isGitHub` / `provider.isAzureDevOps`, which are `false`
    for every unknown provider.

### On-disk shape

`IssuesJSONCoder` writes diff-friendly output so `.issues` files review cleanly in git:
pretty-printed, keys sorted alphabetically at every level, slashes unescaped, dates as
ISO-8601 (`2026-05-01T00:00:00Z`, UTC, second precision), and a trailing newline. Encoding
the same model twice produces identical bytes.

Note that ISO-8601 encoding truncates sub-second precision — a `Date` does not survive a
round trip at finer than one-second resolution.

---

## Top level

| Field | Type | Default when absent | Notes |
|---|---|---|---|
| `schemaVersion` | Int | **required** | Version gate; see above. |
| `project` | Project | empty project (fresh `id`) | Project identity. |
| `integrations` | Integrations | `{}` | Tracker coordinates. **No credentials.** |
| `labels` | [Label] | `[]` | Label catalog, so pickers work offline. |
| `milestones` | [Milestone] | `[]` | Milestone catalog, so pickers work offline. |
| `people` | [Person] | `[]` | People catalog, so pickers work offline. |
| `export` | Export | default preamble | Markdown export settings. |
| `issues` | [Issue] | `[]` | The issues. |

### Project

| Field | Type | Default | Notes |
|---|---|---|---|
| `id` | UUID | fresh UUID | Stable project identity. |
| `name` | String | `""` | |
| `summary` | String | `""` | |

### Integrations

| Field | Type | Default | Notes |
|---|---|---|---|
| `github` | GitHub \| null | `null` | Absent when the project has no GitHub remote. |
| `azureDevOps` | AzureDevOps \| null | `null` | Absent when the project has no ADO remote. |

**GitHub**

| Field | Type | Default | Notes |
|---|---|---|---|
| `owner` | String | `""` | User or organization. |
| `repository` | String | `""` | |
| `defaultLabels` | [String] | `[]` | Applied to newly pushed issues. |
| `defaultAssignees` | [String] | `[]` | Applied to newly pushed issues. |
| `defaultMilestone` | String \| null | `null` | |

**AzureDevOps**

| Field | Type | Default | Notes |
|---|---|---|---|
| `organization` | String | `""` | |
| `project` | String | `""` | |
| `team` | String \| null | `null` | |
| `areaPath` | String \| null | `null` | |
| `iterationPath` | String \| null | `null` | |
| `defaultWorkItemType` | String | `"Issue"` | e.g. `Bug`, `Task`, `User Story`. |

### Label

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | String | `""` | The value used in `Issue.labels`. |
| `colorHex` | String \| null | `null` | `RRGGBB` or `#RRGGBB`. |
| `description` | String | `""` | |

### Milestone

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | String | `""` | The value used in `Issue.milestone`. |
| `dueOn` | Date \| null | `null` | ISO-8601. |
| `isClosed` | Bool | `false` | |

### Person

| Field | Type | Default | Notes |
|---|---|---|---|
| `handle` | String | `""` | The value used in `Issue.assignees` / `Issue.reportedBy`. |
| `displayName` | String | `""` | |
| `email` | String | `""` | |

### Export

| Field | Type | Default | Notes |
|---|---|---|---|
| `preambleMarkdown` | String | the template preamble | Everything emitted before `## Open`. |

The default is `ExportSettings.defaultPreambleMarkdown` — the `# Issues` title, the "How to
use this file" section, and the fenced entry template.

---

## Issue

| Field | Type | Default | Notes |
|---|---|---|---|
| `uuid` | UUID | fresh UUID | **Stable sync identity.** Never renumbered; referenced by `relations`. |
| `number` | Int | `0` | Human display number, rendered `#007`. Renumbering is safe. |
| `title` | String | `""` | |
| `type` | IssueType | `"task"` | |
| `priority` | IssuePriority | `"medium"` | |
| `status` | IssueStatus | `"open"` | |
| `resolutionKind` | ResolutionKind \| null | `null` | Why it closed; `null` while open. |
| `labels` | [String] | `[]` | Label names; see the `labels` catalog. |
| `assignees` | [String] | `[]` | Person handles; see the `people` catalog. |
| `milestone` | String \| null | `null` | Milestone name; see the `milestones` catalog. |
| `area` | String | `""` | Free-text component, e.g. `Networking`. |
| `estimate` | Double \| null | `null` | Project's own unit (points, hours). |
| `reportedBy` | String | `""` | Person handle. |
| `reported` | Date | now | Day-normalized; the only date the markdown export carries. |
| `createdAt` | Date | now | Record creation, for conflict detection. |
| `updatedAt` | Date | now | Last local edit, for conflict detection. |
| `closedAt` | Date \| null | `null` | |
| `description` | String | `""` | Markdown body. |
| `stepsToReproduce` | [String] | `[]` | One entry per numbered step. |
| `environment` | String | `""` | OS / build / device. |
| `notes` | String | `""` | Investigation notes. Legacy import parks unrecognized sections here. |
| `resolution` | String | `""` | What was changed, and where. |
| `comments` | [Comment] | `[]` | |
| `relations` | [Relation] | `[]` | |
| `remoteLinks` | [RemoteLink] | `[]` | |

### Comment

| Field | Type | Default | Notes |
|---|---|---|---|
| `id` | UUID | fresh UUID | |
| `author` | String | `""` | Person handle. |
| `createdAt` | Date | now | |
| `body` | String | `""` | Markdown. |

### Relation

| Field | Type | Default | Notes |
|---|---|---|---|
| `kind` | RelationKind | `"relatedTo"` | |
| `issueID` | UUID | **required** | The other issue's `uuid`. A relation pointing nowhere is not a relation, so this key must be present. |

### RemoteLink

| Field | Type | Default | Notes |
|---|---|---|---|
| `provider` | RemoteProvider | `"github"` *(when the key is absent)* | An unrecognized value is kept verbatim; it is never coerced to a known provider. |
| `identifier` | String | `""` | GitHub issue number, or ADO work item id. |
| `url` | URL \| null | `null` | |
| `lastSyncedAt` | Date \| null | `null` | When this app last pushed or pulled. |
| `remoteUpdatedAt` | Date \| null | `null` | The remote item's own timestamp. |

---

## Enumerations

Raw values are stable lowerCamelCase identifiers and are **part of the format** — never
change them. The display strings are presentation only (UI and markdown export) and may be
reworded freely; they are never persisted.

| Enum | Raw values | Display names | Default |
|---|---|---|---|
| `IssueType` | `bug`, `feature`, `task`, `question` | Bug, Feature, Task, Question | `task` |
| `IssuePriority` | `low`, `medium`, `high`, `critical` | Low, Medium, High, Critical | `medium` |
| `IssueStatus` | `open`, `inProgress`, `blocked`, `resolved` | Open, In Progress, Blocked, Resolved | `open` |
| `ResolutionKind` | `fixed`, `wontFix`, `duplicate`, `cannotReproduce`, `byDesign` | Fixed, Won't Fix, Duplicate, Cannot Reproduce, By Design | *none* — unknown decodes to `null` |
| `RelationKind` | `blocks`, `blockedBy`, `duplicateOf`, `relatedTo`, `parent`, `child` | Blocks, Blocked By, Duplicate Of, Related To, Parent, Child | `relatedTo` |
| `RemoteProvider` | `github`, `azureDevOps` | GitHub, Azure DevOps | *none* — unknown is kept verbatim as `.other(raw)` |

`ResolutionKind` maps onto GitHub's `state_reason` and Azure DevOps' *Resolved Reason*.

`RemoteProvider` is not `CaseIterable` — `.other(_:)` has no finite set of values. UI pickers
should offer `RemoteProvider.selectableCases`, which lists only the providers this build can
actually sync with.

---

## Complete example

```json
{
  "export" : {
    "preambleMarkdown" : "# Issues\n\n---\n\n"
  },
  "integrations" : {
    "azureDevOps" : {
      "areaPath" : "IHaveIssues\\Client",
      "defaultWorkItemType" : "Bug",
      "iterationPath" : "IHaveIssues\\Sprint 1",
      "organization" : "openbcm",
      "project" : "IHaveIssues",
      "team" : "Core"
    },
    "github" : {
      "defaultAssignees" : [
        "dru"
      ],
      "defaultLabels" : [
        "triage"
      ],
      "defaultMilestone" : "v1.0",
      "owner" : "openbcm",
      "repository" : "i-have-issues"
    }
  },
  "issues" : [
    {
      "area" : "Views",
      "assignees" : [
        "dru"
      ],
      "closedAt" : "2026-05-07T00:00:00Z",
      "comments" : [
        {
          "author" : "sam",
          "body" : "Reproduced on my machine.",
          "createdAt" : "2026-05-06T00:00:00Z",
          "id" : "6C2D9F30-8B15-4A77-9E3C-2D5A1B4F7E88"
        }
      ],
      "createdAt" : "2026-05-05T00:00:00Z",
      "description" : "The login button is inert.",
      "environment" : "macOS 27.0 (build 27A123)",
      "estimate" : 3.5,
      "labels" : [
        "regression"
      ],
      "milestone" : "v1.0",
      "notes" : "Missing action binding.",
      "number" : 7,
      "priority" : "high",
      "relations" : [
        {
          "issueID" : "A1F4C7D2-5E90-4B3A-8C61-0D2E7F5A9B44",
          "kind" : "blocks"
        }
      ],
      "remoteLinks" : [
        {
          "identifier" : "412",
          "lastSyncedAt" : "2026-05-07T00:01:00Z",
          "provider" : "github",
          "remoteUpdatedAt" : "2026-05-07T00:00:00Z",
          "url" : "https://github.com/openbcm/i-have-issues/issues/412"
        }
      ],
      "reported" : "2026-05-01T00:00:00Z",
      "reportedBy" : "dru",
      "resolution" : "Rebound the action in LoginView.",
      "resolutionKind" : "fixed",
      "status" : "resolved",
      "stepsToReproduce" : [
        "Open the app",
        "Tap Login"
      ],
      "title" : "Login button does nothing",
      "type" : "bug",
      "updatedAt" : "2026-05-07T00:00:00Z",
      "uuid" : "3B8A0C51-2F44-4E6D-B0A7-1C9E5D4F8A02"
    }
  ],
  "labels" : [
    {
      "colorHex" : "#D73A4A",
      "description" : "Worked before, broken now",
      "name" : "regression"
    }
  ],
  "milestones" : [
    {
      "dueOn" : "2026-06-30T00:00:00Z",
      "isClosed" : false,
      "name" : "v1.0"
    }
  ],
  "people" : [
    {
      "displayName" : "Dru",
      "email" : "dru@openbcm.com",
      "handle" : "dru"
    }
  ],
  "project" : {
    "id" : "9E1C4B2A-6D3F-4C8E-9A21-7F0B5D6E1234",
    "name" : "I Have Issues",
    "summary" : "A document-based issue tracker for small projects."
  },
  "schemaVersion" : 1
}
```

A minimal document is much smaller — everything but `schemaVersion` has a default:

```json
{
  "schemaVersion" : 1,
  "issues" : []
}
```

---

## Markdown export

`IssuesMarkdownSerializer.export(_:)` emits `export.preambleMarkdown` verbatim, then `## Open`
and `## Resolved` sections, routing each issue by `status == .resolved`. Entries keep the
original hand-authored template shape and use the enums' **display names**:

```markdown
### #007 — Login button does nothing

- **Type:** Bug
- **Priority:** High
- **Status:** Resolved
- **Reported:** 2026-05-01
- **Reported by:** dru
- **Area:** Views
- **Labels:** regression
- **Assignees:** dru
- **Milestone:** v1.0
- **Estimate:** 3.5

**Description**

The login button is inert.

**Steps to reproduce**

1. Open the app
2. Tap Login

**Environment**

macOS 27.0 (build 27A123)

**Notes / Investigation**

Missing action binding.

**Resolution**

Rebound the action in LoginView.

**Comments**

- **sam** (2026-05-06): Reproduced on my machine.
```

`Labels`, `Assignees`, `Milestone`, `Estimate`, `**Environment**`, and `**Comments**` appear
only when populated. Fields the template has no slot for — `uuid`, `createdAt`, `updatedAt`,
`closedAt`, `resolutionKind`, `relations`, `remoteLinks` — are **not** exported. Export is
lossy; the `.issues` file is the source of truth.

`LegacyMarkdownImporter` reads back everything the exporter emits, so
export → import → export is stable for the fields markdown can represent. It also reads the
older hand-authored layouts (including free-form `## Bugs` / `## Enhancements` files and
`- **Severity:**` / `- **Component:**` bullets), assigns fresh `uuid`s, sets `createdAt` and
`updatedAt` to the import time, and appends any unrecognized `**Header**` section to `notes`
rather than dropping it.
