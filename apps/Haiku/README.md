# I Have Issues — Haiku

A native Haiku port of the `I Have Issues` document-based issue tracker. It reads and
writes the same `.issues` files as the Apple and Android apps.

---

## Build

On a Haiku machine:

```sh
cd apps/Haiku
make
```

## Test

The format, model and sync logic now live in the shared library, and so does their test
suite:

```sh
cd libs/issueskit/tests
make test          # or: ./run-tests.sh
```

167 assertions, no external framework, non-zero exit on any failure. **It builds and runs
unchanged on Haiku, macOS and Linux with nothing but a C++17 compiler**, which is what lets
it be run today, long before anyone reaches a Haiku box.

There is no Haiku-specific test target: everything left in this directory needs a running
Haiku system to exercise.

The byte-exact round-trip test needs `apps/Apple/sample/Example.issues`. The binary finds
it by walking up from its own location; pass an explicit path as the first argument if you
have moved it.

The binary lands in `objects.<arch>-*/IHaveIssues`. `make clean` and `make bindcatalogs`
behave as they do for any Haiku app — this uses the stock `makefile-engine` located via
`findpaths`.

Requirements:

- **A gcc that supports C++17.** Haiku's modern compiler (gcc 13.x on x86_64/x86_64 hybrid)
  does. The **gcc2 hybrid compiler cannot build this** — the core uses `std::optional`,
  `std::mutex` and lambdas. There is no BeOS binary-compatible build.
- Development headers for `libbe` and `libbnetapi` (both in the base system).

Adding files: `makefile-engine` derives object names from each source's **basename**, so no
two files in `SRCS` may share one. They currently don't — keep it that way.

---

## ⚠️ Verification status — read this first

**None of the Be API code in this tree has ever been compiled.** It was written on macOS,
where no Haiku headers, libraries or cross-compiler exist. Treat the first `make` as the
start of a porting session, not a formality.

What *has* been verified, and how:

| Area | Status |
|---|---|
| `libs/issueskit/` (format, model, sync) | **Compiled and tested** with `c++ -std=c++17 -Wall -Wextra` — no platform toolkit by design |
| Byte-exact `.issues` round trip | **Verified** against `apps/Apple/sample/Example.issues`: decode → encode reproduces the file byte for byte (6127 bytes) |
| Format/decode behavioural contract | **Verified** — 77 assertions covering the Apple test suite's documented rules |
| GitHub sync logic (`GitHubSyncService`) | **Compiled and tested** with a stub `HttpClient` — 39 assertions covering endpoints, headers, payloads, merge rules, milestone pagination, 401 abort, create-then-close |
| Parser robustness | **Fuzzed** — 20 000 mutations of a real document: no crash, no hang, re-encode always idempotent *(throwaway harness, not in-tree)* |
| UUID generation | **Verified** — 200 000 generated, all well-formed v4, all unique *(throwaway harness, not in-tree)* |
| Everything in `apps/Haiku/src/` — the UI and the two platform adapters | **Never compiled. Never run.** |

Everything in the first four rows is reproducible: run `cd libs/issueskit/tests && make test`.

The core being independently testable is the reason it was split out into
`libs/issueskit/` — keep the Be API out of it. `libs/issueskit/check-portable.sh` enforces
that mechanically. The same rule is why `<issueskit/HttpClient.h>` is header-only:
`GitHubSyncService` can be linked against a stub client with no libbe anywhere.

---

## Risks to check on a real Haiku box, highest first

Every one of these is marked `// VERIFY:` at the exact line in the source.

### 1. `src/github/ServicesKitHttpClient.cpp` — the Services Kit (highest risk by far)

Haiku has shipped **two** generations of HTTP API:

- **classic** — `BUrlProtocolRoster` / `BUrlRequest` / `BHttpRequest` / `BHttpHeaders` /
  `BUrlProtocolListener`, in `libbnetapi.so`, namespace `BPrivate::Network`;
- **new** — `BHttpSession` / `BHttpRequest` / `BHttpResult`, in `libnetservices2.a`,
  introduced with R1/beta5.

**This app targets the classic API**, because it is present across the most shipped
releases (R1/beta3, beta4, beta5). Everything about that choice lives in this one file —
`<issueskit/HttpClient.h>` is a two-method interface owned by the shared library, so
porting to `BHttpSession` means rewriting `ServicesKitHttpClient.cpp` and nothing else —
and it cannot affect the GNOME or KDE ports.

Specific things to confirm:

1. **`BUrlProtocolRoster::MakeRequest` signature.** Assumed
   `MakeRequest(const BUrl&, BDataIO* output, BUrlProtocolListener*, BUrlContext*)`. Trees
   older than ~hrev53680 have no `BDataIO*` parameter, in which case the response body must
   be gathered in `BUrlProtocolListener::DataReceived` instead of from the `BMallocIO`.
2. **The `BPrivate::Network` namespace.** The Services Kit moved into it around hrev54000.
   On an older tree the `using` declarations at the top of the file must be deleted.
3. **`BUrlRequest::Run()` returns a `thread_id`**, which is what makes the
   `wait_for_thread()` synchronous wait correct. If it returns a `status_t` on the target
   release, poll `IsRunning()` with `snooze()` instead.
4. **`BHttpRequest::AdoptHeaders(BHttpHeaders*)` takes ownership** (hence the `new`). Some
   trees also expose `SetHeaders(const BHttpHeaders&)`, which would be cleaner.
5. **`BHttpRequest::AdoptInputData(BDataIO*, ssize_t)`** is what sets `Content-Length` for
   POST/PATCH bodies. Confirm the body actually reaches GitHub.
6. **`BUrlRequest::Result()` downcasts to `const BHttpResult&`** for `StatusCode()`.
6b. **`BHttpResult::Headers()` returning `const BHttpHeaders&`, with
   `CountHeaders()` and `HeaderAt(int32)` giving a `BHttpHeader` that has
   `Name()`/`Value()`.** Response headers are not optional decoration — GitHub milestone
   pagination reads `Link:` from them. If `HeaderAt` is unavailable on your tree, iterate
   with `NameAt(int32)` and look each value up by name.
7. **`BUrlProtocolListener::RequestCompleted(BUrlRequest*, bool)` and `DebugMessage(...)`
   signatures.** `DebugMessage` is overridden only to silence protocol chatter; deleting
   that override costs nothing if the signature differs.
8. **Header locations** — `<HttpRequest.h>` etc. moved from `headers/os/net/` to
   `headers/os/netservices/`.
9. `SetMethod` takes a plain `const char*`, so `"PATCH"` needs no `B_HTTP_PATCH` constant
   (the classic kit does not define one). This should be safe on every release.

### 2. `src/github/KeyStoreTokenStore.cpp` — BKeyStore

The Keychain analogue. Assumed: `<KeyStore.h>` + `<Key.h>`, both in `libbe`, with

```cpp
BPasswordKey(const char* password, BKeyPurpose, const char* identifier,
             const char* secondaryIdentifier = NULL);
BKeyStore::GetKey(BKeyType, const char* identifier, const char* secondaryIdentifier,
                  bool secondaryIdentifierOptional, BKey&);
BKeyStore::AddKey(const BKey&);
BKeyStore::RemoveKey(const BKey&);
```

Also note: **the first key-store access may raise a modal system prompt** to unlock the
master keyring. That is why every call happens on a worker thread or in response to a
button press, never during window construction.

### 3. Layout and control APIs (`src/ui/`)

| Assumption | Where |
|---|---|
| `BLayoutBuilder::Group<>::AddSplit()` / `Grid<>::AddTextControl()` / `AddMenuField()` exist with the argument orders used | `MainWindow`, `IssueEditWindow`, `ProjectSettingsWindow` |
| `BLayoutBuilder::Grid<>(BBox*, …)` gives a `BBox` a layout, with a big top inset clearing the label | `ProjectSettingsWindow` |
| `BScrollView(name, target, flags, horizontal, vertical, border)` — the layout-friendly constructor | everywhere |
| `BStringItem` is declared in `<StringItem.h>` (not `<ListItem.h>`) | `MainWindow.cpp` |
| `BTextControl::TextView()` returns the wrapped `BTextView`, which has `HideTyping(true)` — there is no secure `BTextControl` | `GitHubSyncWindow` |
| `B_MODAL_SUBSET_WINDOW_FEEL` + `AddToSubset(parent)` is the right dialog modality; `B_FLOATING_SUBSET_WINDOW_FEEL` is the fallback if dialogs won't come forward | all three dialogs |
| `BAlert::Go()` called synchronously from a `BWindow` message thread does not deadlock against the modal subset windows that window owns | `MainWindow::QuitRequested`, `_DeleteIssue`, `_PerformImport` |
| `BFilePanel` **copies** the model `BMessage` it is handed (so the stack objects in `_BuildFilePanels` are correct and leak nothing) | `MainWindow::_BuildFilePanels` |
| `BMimeType::Install()` returning `B_FILE_EXISTS` on repeat launches is normal | `IHaveIssuesApp::_RegisterFileType` |

### 4. Makefile

`COMPILER_FLAGS` and `LINKER_FLAGS` are assumed to be the variables `makefile-engine`
honours; if the engine on your tree wants `CFLAGS`/`CXXFLAGS`, move `-std=c++17` there.
Also confirm `$(STDCPPLIBS)` is defined by the engine.

---

## The `.issues` format — why the writer looks the way it does

`.issues` files are committed to git and shared between three apps, so **byte-exact output
is a hard requirement**: a differently formatted save rewrites the whole file and destroys
its diffs.

Apple writes them with Foundation's `JSONEncoder` and
`[.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]`. `libs/issueskit/src/JsonValue.cpp`
reproduces that exactly:

- two spaces of indent per level;
- `" : "` between key and value — **a space before the colon as well as after**;
- keys sorted alphabetically at *every* nesting level;
- `/` never escaped, UTF-8 emitted raw;
- an empty array or object renders as the opening bracket, a **completely empty line**, then
  the closing bracket at the parent's indent:

  ```json
        "relations" : [

        ],
  ```
- ISO-8601 UTC timestamps at whole-second precision;
- a trailing newline.

Haiku's `BJson` is deliberately **not** used — its output format is not controllable. The
parser is hand-rolled for the same reason, plus so the core needs no `libbe`.

Dates use fixed-UTC civil arithmetic (`days_from_civil` / `civil_from_days`) rather than
`timegm`/`gmtime_r`, so a "reported" day can never shift for a user east or west of GMT and
the core has no libc timezone surface.

### Note on `apps/Apple/IHaveIssues-Issues.issues`

That file is **not in canonical key order** — its `closedAt`, `resolutionKind` and
`updatedAt` keys sit after `type` instead of sorted. It was hand-edited rather than written
by the app. Opening and saving it in *any* of the three apps will reorder those keys. The
re-encoded output is byte-for-byte the same length and semantically identical; only the
ordering differs. `apps/Apple/sample/Example.issues` *is* canonical, and this port
reproduces it exactly.

---

## Deviations from the Apple app

### Intentional fixes

- **`closedAt` is now maintained.** Saving an issue whose status becomes *Resolved* stamps
  `closedAt = now`, and moving it off *Resolved* clears it. No Apple code path ever writes
  that field — it is item 2 on the Apple app's own gap list. (`IssueEditWindow::_Save`)
- **Sync errors are all listed.** The Apple sheet keys its error list by the error string,
  so two identical messages collide and one silently vanishes (its issue #2). This lists
  every error. (`GitHubSyncWindow::_SyncFinished`)
- **List rows are keyed by `uuid`**, not by array offset (the Apple detail view's issue #3).
- **Milestone lookup is paginated.** The GitHub milestones request follows the
  `Link: …; rel="next"` header until it runs out, so a repository with more than 100
  milestones still resolves the later ones. Apple stops after the first page — its own known
  issue #4 — which silently pushes affected issues with no milestone at all. The Android
  client paginates too, so all three resolve the same names. A page cap and a repeat-URL
  check keep a malformed or self-referential `Link` header from spinning the loop.
  (`GitHubSyncService::_FetchMilestonesIfNeeded`)

### Platform necessities

- **The detail pane is a read-only `BTextView` with bold runs**, not a laid-out grid, and
  markdown in body text is shown as-is rather than rendered — Haiku's API has no markdown
  renderer. Same sections, same order, same omit-when-empty rules.
- **The issue editor puts its five long-text sections in a `BTabView`** instead of one long
  scrolling form. A Haiku window cannot scroll a whole form the way a SwiftUI `Form` does.
- **"Reported" is a `YYYY-MM-DD` text field**, because Haiku has no public date picker. The
  stored value is still fixed-UTC, so no time zone can shift it. An unparsable entry keeps
  the previous date rather than silently jumping to today.
- **Type and priority indicators are letter badges with tints** (`B`/`F`/`T`/`?`, `↓`/`=`/`!`/`!!`)
  rather than SF Symbols. Tints are literal RGB because Haiku's `ui_color()` palette has no
  yellow/orange/pink roles.
- **Project Settings does not pre-seed GitHub coordinates from legacy `UserDefaults`** —
  Haiku has no such legacy state, so the fields start blank.
- **`KeyStoreTokenStore::HasToken` reads the token and discards it.** Apple's Keychain can test
  for an item's existence without returning its data; `BKeyStore` has no equivalent query.
  The value never leaves that function and never reaches view state.
- **Two extra `IssuesError` cases** (`kFileReadFailed`, `kFileWriteFailed`). SwiftUI's
  `DocumentGroup` reports file I/O failures itself; Haiku opens and saves by hand.
- `ExportSettings.preambleMarkdown` is reached as `model.exportSettings` because `export` is
  a C++ keyword. The JSON key is still `"export"`.
- The legacy importer's letter-prefix detection (`MLM-001`) is ASCII-only, where Swift's
  `Character.isLetter` is Unicode-aware. No fixture in this repository is affected.

### Deliberately **not** done

- **No search, filter or sort.** The Apple app has none; adding them is product work, not a
  port.
- **No Azure DevOps sync.** None exists on Apple either. The data model and the Project
  Settings fields are present because the *format* has them.
- **GitHub sync is push-only.** No pull, no fetch-and-diff, no conflict resolution —
  `lastSyncedAt` and `remoteUpdatedAt` are recorded as metadata and nothing reads them back.
  This matches Apple exactly (its issue #6). Two-way sync would be new behaviour.
- **Dangling relations are left dangling.** Deleting an issue that others point at leaves
  those relations pointing at a missing `uuid`; the detail pane prints "Missing issue" and
  nothing is silently rewritten. Same as Apple.

---

## Known TODOs

1. **No application icon.** `IHaveIssues.rdef` has no `resource vector_icon`. An HVIF byte
   array can only be produced by Icon-O-Matic, and a wrong one renders as garbage rather
   than failing to build — so this ships with none rather than a broken one. The rdef
   contains step-by-step instructions for adding one. Cosmetic only.
2. **The fuzz and UUID harnesses are not in-tree.** Tracked in
   `libs/issueskit/README.md`, since that is where the code they exercise now lives.

There are no silent stubs: every function in the tree is fully implemented.

---

## Layout

```
apps/Haiku/
  Makefile              stock makefile-engine build
  IHaveIssues.rdef      signature, version, .issues file-type association (no icon yet)
  src/github/           the two platform adapters (Services Kit HTTP, BKeyStore tokens)
  src/ui/               the Be API application, windows, views and dialogs
```

The format, the domain model and the GitHub sync flow are **not** here — they live in
`libs/issueskit/`, shared with the GNOME and KDE ports, along with their test suite. See
`libs/issueskit/README.md`.

Naming follows Haiku convention: `fMemberVariable`, `_PrivateMethod`, tab indentation. App
code is in `namespace ihaveissues`; the shared library is in `namespace issueskit`, so the
boundary is visible at every call site. The plain data structs in `<issueskit/IssueModel.h>`
use public unprefixed fields, since they carry no
behaviour beyond their defaults.
