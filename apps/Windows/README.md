# I Have Issues — Windows

A WPF port of the Apple app, on .NET 10 and C#, using WPF's built-in Fluent theme.

It reads and writes the same `.issues` documents as the macOS/iOS build — byte for byte — so one
file can be edited from either platform without churning in git. The format is specified in
[`../Apple/Docs/IssuesFormat.md`](../Apple/Docs/IssuesFormat.md); that document is authoritative for
both apps.

## Layout

```
apps/Windows/
├── IHaveIssues.slnx
├── Directory.Build.props
├── src/
│   ├── IssuesKit/                 net10.0 class library — the format, no UI
│   │   ├── Issue.cs               Issue, Comment, Relation, RemoteLink
│   │   ├── IssueEnums.cs          raw values (on disk) and display names (presentation only)
│   │   ├── Catalog.cs             label / milestone / people catalogs
│   │   ├── Integrations.cs        tracker coordinates — never credentials
│   │   ├── IssuesDocumentModel.cs document root, schema gate, export settings
│   │   ├── IssueDate.cs           the fixed YYYY-MM-DD calendar
│   │   ├── IssuesError.cs         user-facing failures
│   │   ├── IssuesMarkdownSerializer.cs / LegacyMarkdownImporter.cs
│   │   └── Json/
│   │       ├── IssuesJsonCoder.cs the only place that reads or writes the format
│   │       └── SwiftJson.cs       the byte-compatible writer (see below)
│   └── IHaveIssues/               net10.0-windows WPF app
│       ├── MainWindow.xaml        sidebar + detail, the counterpart of ContentView.swift
│       ├── Views/                 detail view and the three dialogs
│       ├── ViewModels/            state and commands; no window is owned by a view model
│       ├── Services/              document I/O, GitHub sync, credential storage
│       ├── Presentation/          converters, markdown rendering, link safety
│       └── Themes/Styles.xaml     Fluent styles and classification tints
└── tests/
    ├── IssuesKit.Tests/           the Swift suite, ported, plus byte-compatibility tests
    └── IHaveIssues.UiTests/       XAML load smoke test and detail-projection tests
```

## Build and run

```powershell
dotnet build apps/Windows/IHaveIssues.slnx
dotnet test  apps/Windows/IHaveIssues.slnx
dotnet run --project apps/Windows/src/IHaveIssues            # empty document
dotnet run --project apps/Windows/src/IHaveIssues -- path.issues
```

Requires the .NET 10 SDK. Windows 11 gets the Fluent theme's Mica backdrop; Windows 10 gets the
same theme without it.

## Byte compatibility with the Apple build

The `.issues` file is committed to the project it tracks, so it must not rewrite itself when it
moves between platforms. Foundation's `JSONEncoder` — which the Apple build uses with
`[.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]` — formats JSON in four ways that
`Utf8JsonWriter` cannot reproduce, so `Json/SwiftJson.cs` writes the format directly:

| | Apple build, and this one | `Utf8JsonWriter` |
|---|---|---|
| Name separator | `"key" : value` | `"key": value` |
| Empty object / array | three lines, blank line between the brackets | `{}` / `[]` |
| Escaping | only `"`, `\`, and control characters | also `/`, `<`, `>`, `&`, non-ASCII |
| UUIDs | uppercase | lowercase |

Plus two-space indent, LF endings, ordinal-sorted keys, second-precision UTC ISO-8601 dates, and a
trailing newline.

`ReEncodingAnAppleWrittenDocumentReproducesItByteForByte` pins this against
`../Apple/sample/Example.issues`, copied verbatim into the test fixtures. A document written by
hand or by a coding agent — as `../Apple/IHaveIssues-Issues.issues` was — may have keys out of
order; saving normalizes it once and is stable thereafter, which
`ReEncodingAHandEditedDocumentNormalizesItWithoutChangingMeaning` pins.

## Dependencies and licensing

This project is dual-licensed — **GPLv3 or a commercial licence** — which constrains what it can
depend on. A copyleft dependency (GPL, or LGPL with its relinking obligations) would make the
commercial arm unshippable. Every dependency must therefore be **permissive**: GPLv3-compatible
*and* sublicensable into a proprietary edition without disclosing source.

There is exactly one:

| Package | Version | Licence | Why |
|---|---|---|---|
| [Markdig](https://github.com/xoofx/markdig) | 1.3.2 | BSD-2-Clause | CommonMark parser |

BSD-2-Clause is attribution-only, has no patent or copyleft clause, and is compatible with both
arms. Its copyright notice must be retained in binary distributions under either licence — the text
to reproduce is in [`NOTICE`](../../NOTICE), and [`COPYING`](../../COPYING) covers the dual licence
itself.

### Why Markdig alone, and not a WPF markdown control

Three candidates were considered. All are permissive, so licence alone did not decide it:

| Package | Licence | Newest target | Status |
|---|---|---|---|
| **Markdig** | BSD-2-Clause | **`net10.0`** | Actively maintained, 71M downloads |
| Markdig.Wpf | MIT | `net5.0-windows` | Last release 0.5.0.1 |
| MdXaml | MIT | `net6.0-windows` | Maintained, but no `net10.0` target |

Markdig is the only one that ships a `net10.0` target; the two renderers stop at .NET 5 and .NET 6.
For an app expected to track .NET for years, depending on a stale WPF control is the larger risk —
and a second dependency means a second attribution obligation to track in *both* licence arms.

So Markdig parses and `Presentation/MarkdownRenderer.cs` renders its AST to a `FlowDocument`
(~250 lines). That also buys something a library would not: output styled with the Fluent theme
brushes, attached by `SetResourceReference` so it follows a light/dark switch at runtime, rather
than a library's own fixed palette.

Headings, emphasis, strikethrough, inline and fenced code, lists, block quotes, thematic breaks,
and links are rendered. Raw HTML is shown as written rather than interpreted.

### Two safety rules in the renderer

An `.issues` file arrives from a repository and may have been written by anyone — a colleague, a
stranger's pull request, or a coding agent. Its markdown is untrusted input:

- **Only `http`, `https`, and `mailto` links are clickable.** Anything else — `file:`,
  `javascript:`, `ms-msdt:`, a UNC path — renders as plain text and never reaches the shell.
  Handing an arbitrary URI to `ShellExecute` would let a document launch a local executable or a
  registered protocol handler off an innocuous-looking link. `Presentation/ExternalLink.cs` is the
  single gate, used by the markdown body and the remote-links list alike.
- **Images are never fetched.** An image is rendered as its alt text beside a link to the source.
  Loading a remote image on open would report the reader's IP address and the moment they opened
  the document to whoever wrote it.

Both are covered by tests in `MarkdownRendererTests`.

## Credentials

As on the Apple build, no token is ever written to an `.issues` file. The GitHub personal access
token lives in **Windows Credential Manager** (the counterpart of the Keychain), one item per
repository, keyed `IHaveIssues-GitHubToken:<owner>/<repository>`. A token issued for one repository
is never sent to another. The document holds only non-secret coordinates.

## Differences from the Apple build

Deliberate, and each for a reason:

- **Milestone lookup pages to the end.** The Apple build reads only the first hundred milestones,
  which silently drops the milestone from any issue whose milestone sorts later. This is issue #4
  in the Apple project's own list; fixing it here was five lines.
- **CRLF markdown imports correctly.** `LegacyMarkdownImporter` strips a trailing carriage return
  per line. Markdown authored on Windows is common, and the reference importer would otherwise
  leave a stray `\r` at the end of every parsed value. Export is always LF.
- **The sidebar groups instead of listing two sections.** Open entries appear above resolved ones,
  each group ordered by display number. WPF list grouping omits an empty group rather than showing
  a "No open issues" placeholder in it.
- **Documents are opened and saved explicitly.** SwiftUI's `DocumentGroup` has no WPF equivalent, so
  `Services/IssuesDocument.cs` provides open, save, and unsaved-change tracking. "Dirty" is decided
  by comparing encoded bytes with what was last written, which is exact rather than approximate.
- **Unhandled failures are logged** to `%LOCALAPPDATA%\IHaveIssues\error.log`, since a Windows app
  has no console to fall back on.

## Testing

`dotnet test` runs 81 tests.

`IssuesKit.Tests` (44) is the Apple project's `IssuesKitTests` ported section for section — the same
fixtures, the same assertions — plus the byte-compatibility and CRLF tests the second
implementation makes necessary.

`IHaveIssues.UiTests` (37) covers three things:

- **Window loading** — every window is constructed so its XAML is parsed and its resource
  references resolved. A build alone proves nothing about XAML; this caught two real defects, an
  invalid `FontFamily` value and a globalization setting incompatible with WPF.
- **Markdown rendering** — structure, emphasis, lists, code, quotes, and links, plus the link-scheme
  and no-image-fetch rules above, and a set of malformed inputs that must not throw.
- **The detail projection** — which rows appear, which are omitted when empty, and a relation
  pointing at a deleted issue.

Not covered: expanding the item and detail `DataTemplate`s, which needs a window that has actually
laid out. Verify those by running the app against a real document.
