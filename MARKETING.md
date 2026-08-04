# I Have Issues

A schema-versioned, diff-friendly JSON file format for tracking project issues — plus a
family of native apps that read and write it. One file, in the repo, next to the code.

## The problem: `ISSUES.md` doesn't scale

Coding agents made `ISSUES.md` a de facto convention. Keep the work list next to the code,
in plain text, so an agent (or a human) can open it, read what's outstanding, and write back
what it fixed. This is a genuinely good pattern — it's why it spread. No server, no account,
no context-switch out of the editor, and it lives and dies with the commit history like
everything else in the repo.

The problem shows up as the list grows. A hand-written markdown list has no schema. At five
issues it's fine. At fifty, it drifts:

```markdown
### Bug: login button does nothing
- Severity: High
- Component: Views
Reported 2026-05-01 by dru

### Feature: dark mode toggle
- Priority: Medium
- Area: Settings
```

Same document, two shapes. `Severity` here, `Priority` there. `Component` vs. `Area`. No
stable identity for either entry — renumber the list and every cross-reference is now wrong.
No way to say "this blocks that" except prose. Merge two branches that both edited the file
and the conflict is textual, not semantic — git has no idea these are two edits to the same
issue's status field. And every tool, every script, every agent that wants to parse the file
has to guess at whatever shape this particular project settled on.

None of that is a markdown problem exactly — it's an *anything without a schema* problem.

## The insight

Keep everything that made `ISSUES.md` good — one file, in the repo, versioned with the code,
readable offline, no server, no account — and fix the one thing that was never true: give it
a schema.

`.issues` is a JSON document with required structure, versioned so it can change safely, and
written byte-for-byte the same way every time so it reviews cleanly in a diff. Markdown
isn't abandoned — it becomes an export target, not the source of truth.

## Before / after

**Before** — free-form, two entries, two shapes, no identity:

```markdown
### Bug: login button does nothing
- Severity: High
- Component: Views
Reported 2026-05-01 by dru
```

**After** — one entry, from the format's minimal example:

```json
{
  "schemaVersion" : 1,
  "issues" : [
    {
      "area" : "Views",
      "priority" : "high",
      "reportedBy" : "dru",
      "status" : "open",
      "title" : "Login button does nothing",
      "type" : "bug",
      "uuid" : "3B8A0C51-2F44-4E6D-B0A7-1C9E5D4F8A02"
    }
  ]
}
```

(Every other field — `labels`, `assignees`, `milestone`, `relations`, `comments`,
`remoteLinks`, and so on — has a documented default and simply doesn't appear until it's
used. A real document also carries a `project` block and catalogs for labels, milestones,
and people; see the full example in the spec.)

The `uuid` is permanent sync identity. The human-facing `number` (`#007` in the markdown
export) is display-only and safe to renumber. Relations point at `uuid`s, so reordering the
list never breaks a `blocks` reference.

## What makes the format itself the product

**Schema-versioned, not just "JSON."** `schemaVersion` is required. A file with a *newer*
version than a given build supports is refused outright — the build says so and stops —
rather than silently mangling fields it doesn't recognize. A file with an *older* version is
accepted. The migration seam is a single documented `guard` in the decoder; today there's
nothing to migrate, because only version 1 exists, but the seam is real and future-proofed.

**Diff-friendly by construction.** Pretty-printed, two-space indent, keys sorted
alphabetically at every nesting level, slashes unescaped, ISO-8601 UTC timestamps at
whole-second precision, a trailing newline. Encoding the same model twice produces identical
bytes. That's not a style preference — it's what makes a `.issues` file readable in a `git
diff` and reviewable in a PR the same way a source file is, instead of a machine-formatted
blob that rewrites itself every save.

**Tolerant decoding, deliberately, except where guessing would be actively wrong.** Absent
keys fall back to documented defaults. Unknown keys are ignored rather than fatal, so a
newer build's fields survive being opened in an older one. Unknown enum values fall back to
that enum's default — with two carved-out exceptions where a default would be a lie:
`resolutionKind` decodes to `null` rather than inventing a reason an issue was closed, and an
unrecognized `remoteLinks[].provider` is preserved verbatim (`.other(raw)`) and re-encoded
byte-identically rather than coerced to a known provider. The reasoning: a provider is *sync
identity* — silently mapping an unrecognized one to `github` could aim a real GitHub sync at
an unrelated issue in the user's actual repository. That's the kind of edge case that only
gets handled correctly when someone has already been burned by it once.

**Safe to commit, on purpose.** `.issues` files get committed, forked, and printed into CI
logs — so the format has no field for a credential and never will. `integrations` carries
only non-secret coordinates (owner, repository, org, project, default labels/paths). Tokens
live in the platform's own credential store — Keychain, KWallet, libsecret, `BKeyStore` — 
keyed off those coordinates, never the document.

**Real structure, not just a list.** Types, priorities, statuses, resolution kinds; label,
milestone, and people catalogs (so a picker UI works fully offline, no server round-trip);
assignees, estimates, free-text area, steps to reproduce, environment notes, resolution
text, threaded comments, and typed relations (`blocks`, `blockedBy`, `duplicateOf`,
`parent`, `child`, `relatedTo`) that reference other issues by `uuid`.

**Markdown didn't go away — it became an output.** The apps export a clean, human-readable
markdown rendering of the document (an `## Open` / `## Resolved` split, with the familiar
`### #007 — Title` / `- **Type:**` shape). There's also a one-way legacy importer that reads
existing hand-authored `Issues.md` files — including older free-form `## Bugs` /
`## Enhancements` layouts and `- **Severity:**` / `- **Component:**` bullets — assigns fresh
`uuid`s, and parks any section it doesn't recognize into the issue's `notes` field rather
than dropping it. Migrating an existing markdown backlog costs nothing and loses nothing.
Export, in the other direction, is intentionally lossy (a few fields like `uuid` and
`relations` have no slot in the markdown template) — because the `.issues` file, not the
markdown, is the source of truth.

**Agent-native.** The format exists so an agent can parse a project's issue list
deterministically instead of pattern-matching a wall of prose. The Apple app ships a Claude
Code skill (`apps/Apple/.claude/skills/issues-file.md`) that teaches an agent the concrete
loop: read the file, find issues whose `status` is `open` / `inProgress` / `blocked`,
implement the fix, and write the resolution back — `status: resolved`, a `resolutionKind`,
and a `resolution` string naming the file and line that changed.

## Two independent proofs the spec is real

A format is only as real as its second implementation. This one has two, built without
sharing code:

- **The C++ ports share one library**, `libs/issueskit` — pure C++17, standard library only,
  no platform toolkit linked in. That invariant isn't enforced by code review; it's enforced
  by `check-portable.sh`, a script that greps every `#include` for Be API, GTK/GLib, and
  Qt/KDE headers and fails the build on a hit. The library carries its own test suite (167
  assertions covering the format contract, token-store account-key derivation, and the
  GitHub sync flow against a stub HTTP client) that runs on any machine with a C++17
  compiler — Haiku, macOS, or Linux — with no toolkit installed at all.
- **The Swift (Apple) and Kotlin (Android) ports are independent reimplementations** of the
  same `.issues` format — not consumers of `libs/issueskit`. That's what makes the *written
  spec* the actual contract, rather than "whatever one codebase happens to do." The C++ suite
  (`RoundTripTests.cpp`) decodes the real `apps/Apple/sample/Example.issues` file and
  re-encodes it, asserting the result is byte-for-byte identical to the original (and
  reporting the first differing byte if it isn't). Android's suite goes further, decoding
  that same Apple-produced sample and asserting its own re-encoded bytes match it exactly —
  genuine cross-implementation byte parity, not just self-consistency — plus a separate
  determinism check that encoding one model twice yields identical output. The Swift suite
  verifies round-trip fidelity by model equality (`decode(encode(model)) == model`) and a
  trailing-newline check, rather than a byte comparison against a stored file.

## Six clients, honestly staged

This is a spec-first project. The format is the mature, tested artifact. The clients that
read and write it are at genuinely different stages of maturity, and that's worth stating
plainly rather than glossing over:

| Client | Toolkit | Status |
|---|---|---|
| **Apple** (macOS + iOS) | SwiftUI, Swift 6, strict concurrency | Most complete. Builds. Has a test suite (`IssuesKitTests`). |
| **Android** | Kotlin, Jetpack Compose | Implemented, builds, has tests. |
| **Haiku** | Be API (`libbe`) | Code complete, including UI. **Never built** — no Haiku machine was available while writing it. The shared format/sync core it depends on (`libs/issueskit`) *is* independently compiled and tested; only the Be-API-specific layer is unverified. |
| **GNOME** | GTK4 + libadwaita | Written, not yet built. Its own README says plainly: nothing in the directory has ever been compiled, linked, or run — treat the first `meson setup` as the start of a porting session, not a formality. |
| **KDE** | Qt 6 + KDE Frameworks 6 | Same status as GNOME: written, never compiled, linked, `moc`'d, or run. Its README says to treat the first `cmake -B build` the same way. |
| **Windows** | — | Directory exists. Empty. Planned only. |

For both GNOME and KDE, every uncertain platform API call is flagged with a `// VERIFY:` (or
`/* VERIFY: */`) comment at the exact line, ranked by risk, in each port's own README — the
authors' own account of what's confirmed versus assumed, not a marketing gloss.

**GitHub sync is currently push-only.** The format has room for two-way sync
(`lastSyncedAt`, `remoteUpdatedAt` fields exist per remote link), and the sync service pushes
issues, labels, assignees, and milestones to a GitHub repository — but there is no pull, no
fetch-and-diff, and no conflict resolution yet. This is a known, tracked gap (issue #6 in the
project's own `.issues` file), not an oversight discovered here. Azure DevOps coordinates
exist in the format for the same reason the GitHub ones do, but no Azure DevOps sync exists
on any client yet.

## Who this is for

- Projects already using (or wanting to use) an in-repo issue list read by a coding agent,
  who've hit the point where a markdown file's lack of structure is causing real friction —
  drifting fields, no stable IDs, painful merges.
- Developers who want their issue tracker to live in git history, in plain diffs, with no
  server and no account, and are fine with a desktop app (or hand-editing JSON) rather than
  a web UI.
- Anyone building or extending an agent workflow that needs to read and write issue state
  deterministically, without parsing free-form prose.
- Early adopters on macOS, iOS, or Android today; anyone willing to be the first to compile
  the GNOME or KDE port and report back what breaks.

## Who this is not for

- Large teams already happy with a hosted tracker (GitHub Issues, Jira, Linear) who need
  per-issue permissions, a web UI, notifications, or multi-team workflows — none of that is
  this project's job, and the one-way, push-only GitHub sync is not a replacement for using
  GitHub Issues directly.
- Anyone who needs the tracker itself to be a web application — every client here is native,
  by design, and there is no server component at all.
- Anyone who needs Windows or Linux desktop support *today* — Windows doesn't exist yet, and
  GNOME/KDE haven't been run outside a syntax check.
- Teams that need real-time collaborative editing of the issue list — `.issues` is a file,
  merged the way any other source file is merged.

## Further reading

- [`Docs/IssuesFormat.md`](Docs/IssuesFormat.md) — the full format specification: every
  field, every default, every enumeration, the complete worked example.
- [`libs/issueskit/README.md`](libs/issueskit/README.md) — the shared C++ core: what it
  does, what each platform must supply, the portability invariant, and the test suite.
