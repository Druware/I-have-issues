# I Have Issues

A simple app and user interface for managing a project's `.issues` file — one that Claude or
another coding agent can parse and process.

An `.issues` file is a single UTF-8 JSON document holding a project's identity, its issue-tracker
coordinates, its label / milestone / people catalogs, and the issues themselves. It lives in the
repository it tracks, so it reviews cleanly in a diff and an agent can read it without a network
call or an API token.

## Apps

| Platform | Path | Stack |
|---|---|---|
| macOS, iOS | [`apps/Apple`](apps/Apple) | Swift 6, SwiftUI, `DocumentGroup` |
| Windows | [`apps/Windows`](apps/Windows) | C#, .NET 10, WPF with the built-in Fluent theme |

Both apps read and write the same document, **byte for byte**, so one `.issues` file can be edited
from either platform without the file churning in git. Each shares the same split: a UI-free
`IssuesKit` library owning the format, and a thin app on top of it.

## The format

[`apps/Apple/Docs/IssuesFormat.md`](apps/Apple/Docs/IssuesFormat.md) is the specification and is
authoritative for both apps. In short:

- **`schemaVersion` is required.** A build refuses a document written by a newer one rather than
  silently dropping what it does not understand.
- **Decoding is tolerant.** Absent keys take their documented default, unknown keys survive a round
  trip through an older build, and an unknown enum value falls back to that enum's default — with
  two deliberate exceptions, `resolutionKind` and `remoteLinks[].provider`, where guessing would
  misreport why work stopped or aim a sync at the wrong remote issue.
- **No credentials, ever.** `.issues` files are committed, so a token stored in one would be
  published to every fork and CI log. Tokens live in the OS credential store — Keychain on Apple,
  Credential Manager on Windows — keyed by the non-secret coordinates in the document.
- **Markdown is an export, not the source of truth.** It is produced from a document, and read
  *into* one — once — from pre-`.issues` files.

## Samples

- [`apps/Apple/sample/Example.issues`](apps/Apple/sample/Example.issues) — a populated document; both
  apps' test suites use it as ground truth for byte-identical output.
- [`apps/Apple/sample/Template-Issues.md`](apps/Apple/sample/Template-Issues.md) — the hand-authored
  markdown layout the format grew out of.
- [`apps/Apple/IHaveIssues-Issues.issues`](apps/Apple/IHaveIssues-Issues.issues) — this project's own
  issue list.

## Licence

Available under two licences — take it under whichever you prefer:

- **GPLv3**, free of charge for any use. Full text in [`LICENSE`](LICENSE).
- **Commercial**, for distributing a derived work without the source disclosure the GPLv3
  requires. Contact <dru@druware.com>.

[`COPYING`](COPYING) explains the choice and what each option asks of you.
[`NOTICE`](NOTICE) lists third-party components and the notices you must retain — they apply under
either option.

Dependencies are kept permissive so both options stay available. The Windows app takes exactly
one, [Markdig](https://github.com/xoofx/markdig) (BSD-2-Clause), for markdown parsing; the Apple app
takes none. The reasoning is in
[`apps/Windows/README.md`](apps/Windows/README.md#dependencies-and-licensing).
