# I Have Issues — GNOME

A native GNOME port of the *I Have Issues* document-based issue tracker, written in C++17
against the **GTK4 + libadwaita C API**. It reads and writes the same `.issues` files as the
Apple, Android and Haiku apps.

---

## ⚠️ Verification status — read this first

**Nothing in this directory has ever been compiled, linked or run.** It was written on
macOS, where no GTK4, libadwaita, libsoup, libsecret, pkg-config, Meson or Ninja exists.
Treat the first `meson setup` as the start of a porting session, not a formality.

What *has* actually been verified, and how:

| Area | Status |
|---|---|
| `libs/issueskit/` (format, model, sync) | **Compiled and tested** in its own tree — see `libs/issueskit/README.md` |
| Every `issueskit` call site this app makes | **Syntax-checked** — `clang++ -std=c++17 -Wall -Wextra -fsyntax-only`, clean |
| `src/IssuePresentation.{h,cpp}` | **Syntax-checked** against real GLib headers, clean |
| `src/IssueObject.{h,cpp}` (the `G_DECLARE_FINAL_TYPE` / `G_DEFINE_TYPE` GObject) | **Syntax-checked** against real GLib/GObject headers, clean |
| The GLib / GObject / GIO layer of `MainWindow`, `Application`, `GitHubSyncDialog` | **Syntax-checked** via an extracted transcript — `GActionEntry` arrays, `GMenu`, `G_VALUE_INIT`, `GListStore` over `IhiIssue`, `g_object_set_data_full`, `g_file_load_contents` / `g_file_replace_contents`, `g_thread_try_new` / `g_idle_add`, `g_date_time_*`, `g_application_open` — all clean |
| Every GTK4 and libadwaita call | **Never compiled.** No GTK on the machine |
| Every libsoup call (`src/SoupHttpClient.cpp`) | **Never compiled.** No libsoup on the machine |
| Every libsecret call (`src/SecretTokenStore.cpp`) | **Never compiled.** No libsecret on the machine |
| `meson.build` / `data/meson.build` / `src/meson.build` | **Never run.** No Meson on the machine |
| The `.desktop`, AppStream and MIME files | **Never validated.** `desktop-file-validate` and `appstreamcli` are wired into `meson test` so the first build checks them |

Every uncertain API carries a `/* VERIFY: */` comment at the exact line. The ranked list is
below.

---

## Build

```sh
cd apps/Gnome
meson setup build
ninja -C build
./build/src/ihaveissues
```

Install (and register the `.issues` file type):

```sh
ninja -C build install
```

Validate the desktop-integration files:

```sh
meson test -C build --suite data
```

### Build dependencies

| Package | Minimum | Why that minimum |
|---|---|---|
| `gtk4` | **4.14** | The version libadwaita 1.5 itself requires. `GtkFileDialog` (the async replacement for the 4.10-deprecated `GtkFileChooserNative`) needs only 4.10, so it is comfortably inside this. |
| `libadwaita-1` | **1.5** | `AdwDialog`, `AdwAlertDialog` and `AdwAboutDialog` all arrived in 1.5. The adaptive machinery — `AdwNavigationSplitView`, `AdwNavigationPage`, `AdwToolbarView`, `AdwBreakpoint`, `AdwSwitchRow` — is 1.4, and `AdwPasswordEntryRow` / `AdwEntryRow` is 1.2, so **the dialogs alone set the floor**. Dropping to 1.4 means replacing all five dialogs with `AdwWindow`-based ones. |
| `libsoup-3.0` | **3.0** | libsoup 2.4 is a *different library*, not an older one, and will not build against `SoupHttpClient.cpp`. |
| `libsecret-1` | **0.20** | The `secret_password_*` API is much older than this; 0.20 is simply what current distributions ship and pins nothing new. |
| Meson | **0.64** | `gnome.post_install(update_mime_database:)` landed in 0.64. |
| A C++17 compiler | — | The shared core uses `std::optional` and `std::mutex`. |

`meson.build` declares both `c` and `cpp`: `glib-compile-resources` emits C.

### Runtime dependencies

- GTK4, libadwaita, libsoup3, libsecret — as above.
- **A running secret service** (gnome-keyring, KWallet's Secret Service interface, KeePassXC's,
  …). Without one, the GitHub token cannot be stored or read; everything else works.
- `adwaita-icon-theme`, for the row and toolbar icons. A missing icon name renders as
  `image-missing` rather than failing — see risk 3.
- Network access to `api.github.com`, for sync only.

### Build risks

The shared core lives at `libs/issueskit`, **outside this Meson project**. Meson refuses
`files()` and `include_directories()` that climb out of the project root, so `meson.build`
hands the sources over as absolute path strings (globbed with `ls`, per
`libs/issueskit/README.md`) and the include path as a plain `-I` compile argument. Both
sidestep the sandbox check. **This has never been run.** If Meson rejects it:

1. `meson setup build -Dissueskit_dir=/abs/path/to/libs/issueskit` — the same mechanism, with
   the path given explicitly rather than derived.
2. Add a `libs/issueskit/meson.build` declaring the static library, put a `meson.build` at the
   repository root that does `subdir('libs/issueskit')` and `subdir('apps/Gnome')`, and
   configure from the repository root. This is the clean fix; it was not taken here because
   `libs/issueskit/` is not this port's to modify.

---

## Risks to check on a real GNOME box, highest first

Every one of these is marked `// VERIFY:` or `/* VERIFY: */` at the exact line in the source.

### 1. `src/SoupHttpClient.cpp` — libsoup 3 (highest risk)

The whole file. GitHub sync does not work at all if any of it is wrong, and none of it has
been compiled.

1. **`soup_session_send_and_read(session, message, cancellable, &error)`** — the synchronous
   whole-body read. This is the call the entire design rests on: `issueskit::HttpClient` is a
   blocking interface, so a synchronous libsoup call on a worker thread is what makes the
   seam work. If only the async form exists on the target, this file needs a private
   `GMainContext` per request instead.
2. **`SoupSession` thread affinity.** A session is created lazily and used only from the
   thread that made it; `GitHubSyncDialog` creates, uses and destroys the client entirely
   inside the sync thread. Confirm libsoup 3 is happy with a session built on a non-main
   thread with no `GMainContext` pushed.
3. **`soup_message_set_request_body_from_bytes(message, "application/json", bytes)`** — that it
   takes its own reference on the `GBytes` (so the `g_bytes_unref` right after is correct) and
   that the `Content-Type` it sets does not conflict with the one
   `GitHubSyncService` already puts in the header list.
4. **`SoupMessageHeadersIter`** — `soup_message_headers_iter_init` / `_next` yielding borrowed
   name/value pointers. **Response headers are not decoration**: GitHub milestone pagination
   reads `Link: …; rel="next"` out of them, and a repository with more than 100 milestones
   silently loses the later ones if this returns nothing.
5. **`soup_message_get_status()`** returning a `SoupStatus` whose values are the numeric HTTP
   status codes, and `soup_message_get_request_headers` / `_response_headers` returning
   borrowed pointers.
6. **`g_object_new(SOUP_TYPE_SESSION, "timeout", (guint)30, NULL)`** — that the property name
   exists. A mistyped GObject property produces a runtime warning, not a build failure.
   Note that the session deliberately sets **no** `user-agent`: see below.

### 2. `src/SecretTokenStore.cpp` — libsecret

1. **The `SecretSchema` aggregate initialiser.** It assumes the documented layout: `name`,
   `flags`, `SecretSchemaAttribute[32]`, then one `gint` and seven `gpointer` reserved fields.
   A short initialiser list is legal C++ and **would compile against a different layout while
   silently mis-assigning fields.** This is the one place in the file a compiler cannot help.
2. **`secret_password_store_sync` / `_lookup_sync` / `_clear_sync`** varargs shapes, and
   `SECRET_COLLECTION_DEFAULT`.
3. **Blocking the UI.** `HasToken` (dialog opening), `Save` and `Remove` (button presses) run
   these `*_sync` calls **on the main loop**. Normally they return immediately, but a locked
   keyring makes the secret service put a modal unlock prompt in front of the user first, and
   the UI is frozen until it is answered. `Load` is fine — it only ever runs on the sync
   worker thread. The fix, if this is unpleasant in practice, is the async libsecret API for
   those three call sites; the `issueskit::TokenStore` interface itself must stay synchronous.
4. **`<libsecret/secret.h>` compiling without `#define SECRET_API_SUBJECT_TO_CHANGE`.** Only
   the `SecretService`/`SecretItem` "complete" API ever needed that guard and nothing here
   uses it, but old releases applied it more broadly.
5. `secret_password_wipe()`, if present, is preferable to `secret_password_free()` — it zeroes
   the memory first.

### 3. Icon names — `src/IssuePresentation.cpp`, `src/IssueDetailView.cpp`, `src/MainWindow.cpp`

There is no GNOME equivalent of Apple's SF Symbols set, so the type/priority/status mapping is
an approximation. **A name the installed theme lacks renders as `image-missing`, not as a
build error**, which makes this the most likely thing to look visibly wrong on first run.
Check with `gtk4-icon-browser`:

| Used for | Icon name | Confidence |
|---|---|---|
| Type: bug | `dialog-warning-symbolic` | freedesktop standard |
| Type: feature | `starred-symbolic` | long-standing Adwaita |
| Type: task | `object-select-symbolic` | long-standing Adwaita |
| Type: question | `dialog-question-symbolic` | freedesktop standard |
| Priority: low / high | `go-down-symbolic` / `go-up-symbolic` | freedesktop standard |
| Priority: medium | `view-more-horizontal-symbolic` | Adwaita, less certain |
| Priority: critical | `emblem-important-symbolic` | freedesktop standard |
| Status: open | `media-record-symbolic` | Adwaita |
| Status: in progress | `content-loading-symbolic` | Adwaita |
| Status: blocked | `action-unavailable-symbolic` | Adwaita, least certain — `process-stop-symbolic` is the fallback |
| Status: resolved | `emblem-ok-symbolic` | long-standing Adwaita |
| Empty detail pane | `view-list-bullet-symbolic` | Adwaita — `view-list-symbolic` is the fallback |
| Reported date picker | `x-office-calendar-symbolic` | Adwaita |
| Toolbars / menu | `list-add-symbolic`, `open-menu-symbolic`, `document-edit-symbolic`, `user-trash-symbolic` | all standard |

Tints are **stock style classes only** (`error`, `warning`, `success`, `accent`, `dim-label`),
never literal RGB, so they follow the user's theme and accent colour. Haiku had to use literal
RGB because its `ui_color()` palette has no yellow/orange/pink roles; GNOME does not have that
problem.

### 4. `meson.build` — reaching outside the project root

See "Build risks" above. This is the only thing that can stop the build outright rather than
misbehaving at runtime.

Also unverified: that `gnome.compile_resources()` generates a self-registering resource (it
does unless `--manual-register` is passed, which it is not). If the style sheet turns out to be
missing at runtime, that assumption is where to look.

### 5. libadwaita 1.5 dialog and adaptive API

Compiled nowhere, but all of it is documented, stable API:

| Assumption | Where |
|---|---|
| `adw_breakpoint_condition_parse("max-width: 600px")`, `adw_breakpoint_new()` taking ownership of the condition, `adw_application_window_add_breakpoint()` taking ownership of the breakpoint | `MainWindow::_BuildUi` |
| `adw_breakpoint_add_setter(bp, object, "collapsed", &GValue)` — the explicit-`GValue` form was chosen over the `adw_breakpoint_add_setters()` varargs form deliberately, to avoid an untypechecked variadic list | `MainWindow::_BuildUi` |
| `adw_dialog_present()` consuming the dialog's floating reference | every dialog's `Present()` |
| `adw_dialog_set_child()`, `adw_dialog_set_content_width/height()`, `adw_dialog_set_can_close()` | every dialog |
| `adw_alert_dialog_add_responses(…, NULL)` varargs, `adw_alert_dialog_set_response_appearance()`, and the detailed `"response"` signal signature `(AdwAlertDialog*, const char*, gpointer)` | delete / import / close confirmations |
| `adw_preferences_row_set_title_lines(row, 1)` and `adw_action_row_set_subtitle_lines(row, 0)` — libadwaita 1.3; without them long titles wrap instead of ellipsizing and long values ellipsize instead of wrapping | list rows, detail rows |
| `AdwActionRow` interpreting its title as Pango markup (which is why every user string is passed through `g_markup_escape_text`) | `MainWindow::_CreateRow`, `IssueDetailView` |
| `AdwEntryRow` / `AdwPasswordEntryRow` implementing `GtkEditable`, so text is read with `gtk_editable_get_text()` and not a `GtkEntry` accessor | all three form dialogs |
| `AdwComboRow` + `GtkStringList` + `GTK_INVALID_LIST_POSITION` | `IssueEditDialog` |
| `AdwAboutDialog` (1.5). `AdwAboutWindow` is the 1.2 equivalent if this build predates it | `Application::ActionAbout` |

### 6. GTK4 details

| Assumption | Where |
|---|---|
| `GtkFileDialog` (4.10) and its `open`/`save` + `_finish` pairs; a dismissed dialog reporting `GTK_DIALOG_ERROR_DISMISSED`, which must **not** raise an alert | `MainWindow::_MakeFileDialog` and friends |
| `gtk_file_dialog_open()/save()` holding their own reference for the duration, so the caller's unref right after is correct | same |
| `GtkFileFilter` being a plain `GObject` in GTK4 (it was `GInitiallyUnowned` in GTK3), so `gtk_file_filter_new()` returns a full reference | `MainWindow::_MakeFileDialog` |
| `gtk_css_provider_load_from_resource()` not being among the loaders GTK 4.12 deprecated (`load_from_data`/`_file`/`_path` were) | `Application::LoadStyleSheet` |
| `gtk_style_context_add_provider_for_display()` still being the way to install a display-wide provider, `GtkStyleContext` being deprecated notwithstanding | same |
| `gtk_calendar_get_date()` returning an owned, **local-time** `GDateTime` — only its displayed Y/M/D are used, and they are re-parsed through the fixed-UTC `IssueDate::Parse` on save, so no time zone can shift a reported day | `IssueEditDialog::_OnCalendarDaySelected` |
| `gtk_list_box_bind_model()` + `gtk_list_box_set_placeholder()` behaving together (the placeholder showing when the bound model is empty) | `MainWindow::_BuildListSection` |

---

## Design notes

### The list pane

`GtkListBox` **bound to a `GListModel`** (`gtk_list_box_bind_model`) — a `GListStore` of
`IhiIssue`, one per issue, each carrying the issue's `uuid`. Not a hand-built
`GtkListBox`-of-widgets, and not `GtkListView`.

Why not `GtkListView`: the pane has two named sections ("Open", "Resolved") that must each show
their own placeholder when empty. Doing that in one `GtkListView` needs either
`gtk_list_view_set_header_factory` (4.12, `GtkListHeader`) or a `GtkTreeListModel`, and doing
it in two `GtkListView`s means coordinating two `GtkSingleSelection`s by hand. Neither is
something to write confidently with no compiler. `gtk_list_box_bind_model` gets the part that
actually matters — **stable list identity from a `GListModel`, never an array offset** — with
API that has been unchanged since GTK3. Issue lists are tens to hundreds of rows, so the
virtualisation `GtkListView` would add buys nothing here.

The two list boxes are coordinated into one logical selection with a re-entrancy guard
(`fUpdatingSelection`), which is the one piece of that design worth reading carefully.

### The transport adds no headers of its own

`SoupHttpClient` sends the `headers` argument verbatim and adds nothing. In particular it does
**not** set `SoupSession:user-agent`, even though GitHub 403s a request that arrives without a
`User-Agent`. The shared `GitHubSyncService` now sends one itself on every request it issues,
including the pages it follows out of the `Link` header, so a second value set on the session
would make "which one actually reaches GitHub" a libsoup implementation detail — untestable
from the shared library's suite, and free to differ between the three desktop ports. The
`HttpClient` contract in `libs/issueskit/README.md` is the rule: send the given headers
verbatim, add nothing GitHub-specific. There is a comment saying so at the exact line, so it
does not get re-added.

### Widget trees are built in code, not from GtkBuilder `.ui` templates

The GResource bundle carries the style sheet and nothing else. **This is a deliberate deviation
from GNOME house style**, taken for one reason: nothing here can be compiled or run, and a
mistyped property or class name in a `.ui` file is a silent runtime failure, where the same
mistake in code is a build error. On a machine that can actually build this, converting the
dialogs to `.ui` templates is a worthwhile follow-up.

### C++ objects driving C widgets

Windows and dialogs are plain C++ classes that own an `AdwApplicationWindow` / `AdwDialog`,
attached to it with `g_object_set_data_full()` so the C++ object is deleted when the widget is
finalised. They are **not** GObject subclasses, because they hold real C++ members
(`IssuesDocumentModel`, `std::string`, `std::shared_ptr`, `std::function`) and a GObject
instance struct is allocated and freed without running constructors or destructors.

`IhiIssue` — the list-model item — *is* a proper `G_DECLARE_FINAL_TYPE` / `G_DEFINE_TYPE`
GObject, because `GListStore` requires one. It deliberately holds only plain C strings and
enums for exactly the reason above.

Every asynchronous continuation (a `GtkFileDialog` callback, an `AdwAlertDialog` response, the
sync worker's `g_idle_add`) carries a `std::shared_ptr<bool>` "still alive" flag that the
owner's destructor clears, so a callback arriving after its window or dialog has gone does
nothing instead of writing into freed memory.

### Naming and formatting

`fMemberVariable`, `_PrivateMethod`, `PascalCase` methods, tab indentation, app code in
`namespace ihaveissues` — the same convention as `libs/issueskit` and `apps/Haiku`, so the
boundary between app and shared library is visible at every call site. That is repo house
style rather than GNOME house style; consistency with the siblings won.

---

## Deviations from the Apple app

### Intentional fixes (the same three the Haiku port made)

- **`closedAt` is maintained.** Saving an issue whose status becomes *Resolved* stamps
  `closedAt = now`, and moving it off *Resolved* clears it. No Apple code path ever writes that
  field — item 2 on the Apple app's own gap list. (`IssueEditDialog::_Save`)
- **Sync errors are all listed.** The Apple sheet keys its error list by the error string, so
  two identical messages collide and one silently vanishes (its issue #2). This lists every
  one. (`GitHubSyncDialog::_ShowErrors`)
- **List rows are keyed by `uuid`**, never by array offset (the Apple detail view's issue #3).
- Milestone lookup is paginated via `Link: rel="next"` — that fix lives in the shared
  `GitHubSyncService`, so this port gets it for free (Apple's issue #4).
- **The sync gate is `TokenStore::AccountFor()` itself**, not a hand-rolled owner/repository
  test. That shared helper returns an empty string for every case in which no token operation
  is possible — no integration, a blank owner or repository, *and* an owner or repository
  containing a slash — so the Sync button is disabled in exactly those cases and the sheet says
  which one applies. Re-deriving the rule locally would let it drift from the other ports, and
  the account key is the one thing that must never differ between them.

### Platform necessities

- **The detail pane shows markdown as-is** rather than rendering it, as on Haiku: GTK has no
  markdown renderer. Same sections, same order, same omit-when-empty rules.
- **"Reported" is a `YYYY-MM-DD` text field** with a `GtkCalendar` popover beside it. The text
  is what gets saved; the calendar only writes into it. Everything stays fixed-UTC, so no time
  zone can shift the day, and an unparsable entry keeps the previous date rather than silently
  jumping to today.
- **Type and priority are icons with stock style-class tints**, not SF Symbols (see risk 3).
- **Project Settings does not pre-seed GitHub coordinates from legacy `UserDefaults`** — GNOME
  has no such legacy state, so the fields start blank. Same as Haiku.
- **`SecretTokenStore::HasToken` reads the token and discards it.** Apple's Keychain can test
  for an item's existence without returning its data; libsecret's simple password API cannot.
  The value never leaves that function and never reaches view state, which is what the
  `issueskit::TokenStore` contract requires. (`secret_service_search_sync` with
  `SECRET_SEARCH_NONE` is the genuinely existence-only alternative, at the cost of the whole
  `SecretService`/`SecretItem` API surface.)
- **Two extra `IssuesError` cases** (`kFileReadFailed`, `kFileWriteFailed`) are used here.
  SwiftUI's `DocumentGroup` reports file I/O failures itself; this app opens and saves by hand.
- **A menu bar does not exist.** The Apple app's macOS menu commands become a `GMenu` primary
  menu in the sidebar header bar, plus `GAction` accelerators (`<Control>n/o/s/w/i/e`,
  `<Control><Shift>s`, `<Control>comma`, `<Control><Shift>g`).
- `ExportSettings.preambleMarkdown` is reached as `model.exportSettings` because `export` is a
  C++ keyword. The JSON key is still `"export"`.

### Deliberately **not** done

- **No search, filter or sort.** The Apple app has none; adding them is product work, not a
  port.
- **No Azure DevOps sync.** None exists on Apple either. The data model and the Project
  Settings fields are present because the *format* has them, and the settings group says so in
  its description rather than implying a working integration.
- **GitHub sync is push-only.** No pull, no fetch-and-diff, no conflict resolution.
- **Dangling relations are left dangling.** Deleting an issue that others point at leaves those
  relations pointing at a missing `uuid`; the detail pane prints "Missing issue" and nothing is
  silently rewritten. Same as Apple and Haiku.
- **No `GSettings` schema.** Nothing needed persisting outside the document — window geometry
  was not worth a schema, a `gschema.xml`, a compile step and a `glib-compile-schemas` install
  hook. If window-state persistence is ever wanted, that is when the schema gets added.

---

## Known TODOs

There are **no silent stubs**: every function in this tree is fully implemented. What is
missing is missing on purpose, and listed here.

1. **No application icon.** `data/meson.build` carries a `TODO:` at the exact spot. The
   `.desktop` file's `Icon=` and the MIME package's `<icon>` already name
   `com.druware.IHaveIssues`; dropping
   `data/icons/hicolor/scalable/apps/com.druware.IHaveIssues.svg` into place and adding one
   `install_data` line is the whole change. An icon is a design asset, and a placeholder that
   renders as a broken box is worse than none. Cosmetic only — GNOME Shell falls back to a
   generic icon.
2. **The AppStream metainfo has no `<screenshots>`.** Required for a store listing; impossible
   to produce without running the app. `appstreamcli validate` will report it as a warning, not
   an error.
3. **No `.ui` templates.** See "Design notes" — a follow-up for whoever has a working build.
4. **No translations.** The `.desktop` and metainfo files are plain, not `.in` templates; if
   translation is ever wanted they become `.desktop.in` / `.metainfo.xml.in` and go through
   `i18n.merge_file()`, and the C++ strings need `_()` wrapping and a `po/` directory.

---

## Layout

```
apps/Gnome/
  meson.build              the build; also compiles libs/issueskit straight in
  meson_options.txt        one option: -Dissueskit_dir, an escape hatch
  data/
    com.druware.IHaveIssues.desktop        launcher, claims *.issues
    com.druware.IHaveIssues.metainfo.xml   AppStream metadata
    com.druware.IHaveIssues.mime.xml       *.issues as a sub-class of application/json
    meson.build                            installs and validates the three above
  src/
    main.cpp               the entry point, and nothing else
    Application.*          AdwApplication, app actions, accelerators, style sheet
    MainWindow.*           one document: split view, list pane, actions, files
    IssueObject.*          IhiIssue -- the GObject the list model holds
    IssuePresentation.*    icon and style class per enum case (UI-only, as on every port)
    IssueDetailView.*      the read-only detail pane, rebuilt per selection
    IssueEditDialog.*      add/edit sheet
    ProjectSettingsDialog.* project info + the two integration blocks
    GitHubSyncDialog.*     token handling and the threaded sync run
    SoupHttpClient.*       platform adapter: issueskit::HttpClient over libsoup3
    SecretTokenStore.*     platform adapter: issueskit::TokenStore over libsecret
    ihaveissues.gresource.xml, style.css
    meson.build
```

The format, the domain model and the GitHub sync flow are **not** here — they live in
`libs/issueskit/`, shared with the Haiku and KDE ports, along with their test suite. See
`libs/issueskit/README.md`.

Only three things are this port's to supply, and they are the only three that name a GNOME
API: the `HttpClient`, the `TokenStore` and the UI.
