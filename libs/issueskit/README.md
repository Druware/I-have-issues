# issueskit

The shared core of *I Have Issues*: the `.issues` file format, the domain model,
markdown export/import, and GitHub sync — everything that must behave **identically** on
every platform.

Pure C++17 and the standard library. **No Be API, no GTK, no Qt.** That invariant is what
makes it shareable, and it is enforced by a script rather than by review (see
[Portability](#portability-invariant)).

Consumers today:

| Port | Location | Status |
|---|---|---|
| Haiku | `apps/Haiku/` | complete (unbuilt — no Haiku machine available) |
| GNOME | `apps/Gnome/` | to be written |
| KDE | `apps/KDE/` | to be written |

The Apple (`apps/Apple/`, Swift) and Android (`apps/Android/`, Kotlin) ports are
independent implementations of the same format, not consumers of this library. They are the
reason the format rules below are non-negotiable.

---

## Layout

```
libs/issueskit/
  include/issueskit/    public headers — consumers add `include/` to their include path
  src/                  implementations
  tests/                167 assertions; plain POSIX build, runs anywhere
  check-portable.sh     enforces the no-toolkit invariant
```

Include style is `#include <issueskit/IssueModel.h>`. Add `libs/issueskit/include` to the
include path; nothing else is needed.

Everything lives in `namespace issueskit`. Platform apps use their own namespace
(`ihaveissues` for Haiku), so the boundary is visible at every call site.

---

## What's here

### The format (the part that must never diverge)

| Header | Purpose |
|---|---|
| `IssueModel.h` | `Issue`, `Comment`, `Relation`, `RemoteLink`, catalogs, integrations, `IssuesDocumentModel` |
| `IssueEnums.h` | the six enumerations with their exact on-disk raw values |
| `IssuesJsonCoder.h` | read/write `.issues`, **byte-exact** |
| `JsonValue.h` / `JsonParser.h` | the hand-rolled writer and reader |
| `IssueDate.h` | fixed-UTC calendar maths |
| `IssuesMarkdown.h` | markdown export |
| `LegacyMarkdownImporter.h` | one-way import of pre-`.issues` markdown |
| `IssuesError.h` | the error type, with the exact user-facing wording |
| `StringUtils.h`, `Uuid.h` | small helpers |

### The GitHub sync flow

| Header | Purpose |
|---|---|
| `GitHubSyncService.h` | the whole push-only sync: endpoints, headers, payloads, milestone pagination, per-issue error accumulation, 401 abort |
| `HttpClient.h` | the two-method interface the service talks through |
| `TokenStore.h` | where the personal access token lives |

`GitHubSyncService` is portable because it never touches a socket directly — it goes
through `HttpClient`. That is also why the sync flow is fully testable with no network.

---

## What each platform must supply

Only three things. Everything else is done for you.

### 1. An `HttpClient`

```cpp
class MyHttpClient : public issueskit::HttpClient {
    issueskit::HttpResponse Perform(const std::string& method,
                                    const std::string& url,
                                    const std::vector<issueskit::HttpHeader>& headers,
                                    const std::string& body) override;
};
```

Blocking, called from a worker thread, never from the UI thread. Populate `statusCode`,
`body`, and `headers` (the `Link` header is required — milestone pagination reads it), or
set `transportSucceeded = false` with a `transportError` when the request never reached the
server.

**Send the `headers` you are given, verbatim, and add nothing GitHub-specific.** The
service is the only component that knows it is talking to GitHub; a transport's job is to
move bytes. In particular it already sets **`User-Agent: IHaveIssues/1.0`** on every
request — GitHub *requires* a User-Agent and answers **403** without one, so this is not
optional decoration. Do not add your own: if your HTTP library has a convenience setter
(Haiku's `BHttpRequest::SetUserAgent`, libsoup's session property, Qt's
`QNetworkRequest::UserAgentHeader`), the explicit header from `headers` must win. That
precedence is deliberate — it is what makes all the ports send an identical value.

Declare your factory in *your* header. `HttpClient.h` deliberately declares none, so this
library never names a concrete implementation.

- Haiku: Services Kit — `apps/Haiku/src/github/ServicesKitHttpClient.{h,cpp}`
- GNOME: libsoup is the natural choice
- KDE: `QNetworkAccessManager`

### 2. A `TokenStore`

```cpp
class MyTokenStore : public issueskit::TokenStore {
    bool Save(const std::string& account, const std::string& token) override;
    bool Load(const std::string& account, std::string& outToken) override;
    bool HasToken(const std::string& account) override;
    bool Remove(const std::string& account) override;
};
```

**Do not reimplement the account key.** `TokenStore::AccountFor()` is shared and derives
`"<owner>/<repository>"`, trimmed and lowercased. All ports must derive it identically or a
token saved on one desktop is invisible to another opening the same document.

`HasToken` must answer **without exposing the secret to the caller**. If your backend cannot
test for existence without decrypting, read and discard internally — but the token must not
leave the implementation. (Haiku's `BKeyStore` is exactly this case.)

#### `AccountFor()` refusals, and one rough edge

Both are pinned by tests so they cannot drift, and both are **identical on every port**
because the derivation is shared — so neither can cause a token saved on one desktop to be
invisible on another.

1. **A `/` in either coordinate is rejected: `AccountFor()` returns `""`.** This is defined
   behaviour, not a limitation. Without it the key is plain concatenation, so owner `a/b` +
   repo `c` and owner `a` + repo `b/c` both derived `a/b/c` — a token saved for one
   repository could be handed to a different one, which is exactly what keying exists to
   prevent. No legal GitHub name contains a slash, so nothing valid is refused.
   `GitHubSyncService` already percent-encodes `/` out of every URL path segment for the
   same reason; this is the matching guard for the key. Callers need no special handling —
   `""` already means "no token operation is possible".

2. **Only spaces and tabs are trimmed.** `Trim()` mirrors Swift's `.whitespaces`, which
   excludes `\r` and `\n`. An owner or repository carrying a stray newline — from a
   hand-edited `.issues` file, or from a client that wrote one — derives a *different* key
   from the same repository spelled cleanly, and the symptom would be "sync forgot my
   token". **Deliberately left as-is:** `Trim()` is used throughout the format layer, so
   widening it would change document parsing for all five clients — far beyond token keys.
   Tests pin the current behaviour so the consequence stays visible.

- Haiku: `BKeyStore` — `apps/Haiku/src/github/KeyStoreTokenStore.{h,cpp}`
- GNOME: libsecret
- KDE: KWallet

### 3. The UI

All of it. This library has no opinion about presentation. `apps/Haiku/src/ui/` is a
worked example of the full surface: document window, list/detail split, add/edit dialog,
project settings, sync sheet.

---

## Rules you must not break

These are format-level and enforced by the test suite:

1. **JSON output is byte-exact.** Two-space indent, `" : "` around the colon (a space
   *before* it too), keys sorted at every level, `/` unescaped, empty containers rendered as
   bracket / blank line / bracket, ISO-8601 UTC whole seconds, trailing newline. `.issues`
   files are committed to git; reformatting one rewrites the whole file and destroys its
   diffs. Use `IssuesJsonCoder`, never a general-purpose JSON library.
2. **`schemaVersion` is required** and a newer version than supported is refused, never
   silently opened.
3. **Unknown enum values fall back to that enum's default** — except `resolutionKind`
   (becomes unset; never invent a close reason) and `remoteLinks[].provider` (preserved
   verbatim; coercing an unknown provider to `github` would point sync at an unrelated
   issue).
4. **`relations[].issueID` has no default.** A relation pointing nowhere is not a relation.
5. **Dates for `reported` and `milestone.dueOn` are fixed-UTC calendar days.** Never use
   local time — a reported date must not shift by a day for a user east or west of GMT.

---

## Build story

The library is a handful of `.cpp` files with no dependencies, so every consumer just
compiles `src/*.cpp` and adds `include/` to its include path. **Glob the sources** rather
than listing them, so adding a file cannot silently leave a port behind.

### Tests (plain POSIX, works everywhere)

```sh
cd libs/issueskit/tests
make test          # or: ./run-tests.sh
```

### Haiku

`apps/Haiku/Makefile` (makefile-engine) pulls the sources in directly:

```make
ISSUESKIT_DIR  = ../../libs/issueskit
ISSUESKIT_SRCS = $(wildcard $(ISSUESKIT_DIR)/src/*.cpp)
SRCS = $(ISSUESKIT_SRCS) ...
LOCAL_INCLUDE_PATHS = $(ISSUESKIT_DIR)/include ...
```

### GNOME (Meson) — for the GNOME agent to write

Not written here. The shape is a static library the app links:

```meson
issueskit_inc = include_directories('../../libs/issueskit/include')
issueskit_lib = static_library('issueskit',
  files(run_command('sh', '-c',
    'ls ../../libs/issueskit/src/*.cpp', check: true).stdout().split()),
  include_directories: issueskit_inc,
  cpp_args: ['-std=c++17'])
```

Meson discourages globbing; if you prefer, list the sources explicitly in a
`libs/issueskit/meson.build` and have both consumers use it — that is a fine alternative,
just keep one list.

### KDE (CMake) — for the KDE agent to write

```cmake
file(GLOB ISSUESKIT_SRCS ${CMAKE_SOURCE_DIR}/../../libs/issueskit/src/*.cpp)
add_library(issueskit STATIC ${ISSUESKIT_SRCS})
target_include_directories(issueskit PUBLIC
    ${CMAKE_SOURCE_DIR}/../../libs/issueskit/include)
target_compile_features(issueskit PUBLIC cxx_std_17)
```

Requires C++17 (`std::optional`, `std::mutex`). Haiku's gcc2 hybrid compiler cannot build
this.

---

## Portability invariant

`libs/issueskit` must include **nothing** but the C++ standard library and its own headers.

```sh
cd libs/issueskit && ./check-portable.sh
```

The script greps `#include` lines for Be API, GTK/GLib/libsecret/libsoup, and Qt/KDE
headers, exits non-zero on a hit, and on success prints every non-standard include so the
dependency surface is visible. It matches include lines only, so prose in comments may name
these platforms to explain rationale.

Verified catching all five families: `<Application.h>`, `<gtk/gtk.h>`, `<QString>`,
`<libsecret/secret.h>`, `<KWallet>`.

Run it in CI, or at least before merging a change to this directory.

---

## Tests

167 assertions, no external framework, non-zero exit on any failure:

- **Core format contract** (77) — the rules above, mirroring the Apple test suite.
- **TokenStore account keys** (36) — case folding, whitespace trimming, the blank
  coordinate contract, slash rejection, and the trimming edge described below.
- **GitHub sync** (44) — the full flow against a stub `HttpClient`: endpoints, headers
  (including the mandatory `User-Agent` on every request), payload shape, label/assignee
  merge, milestone pagination, create-then-close, 401 abort.
- **Byte-exact round trip** (10) — decode `apps/Apple/sample/Example.issues` and re-encode
  it; the output must match the original byte for byte.

This suite is the **only executable verification shared by all the C++ ports**, and it runs
on any machine with a C++17 compiler. Run it before and after any change here. If you add
behaviour to the library, add assertions in the same commit.

Not yet in-tree: a parser fuzz harness (20 000 mutations) and a UUID uniqueness check
(200 000 generated), both of which have been run against this code but as throwaway
programs. Worth adding as optional long-running targets.
