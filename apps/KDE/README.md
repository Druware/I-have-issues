# I Have Issues — KDE

A native KDE port of the *I Have Issues* document-based issue tracker, in C++ against
**Qt 6** and **KDE Frameworks 6**. It reads and writes the same `.issues` files as the
Apple, Android, Haiku and GNOME versions.

---

## ⚠️ Verification status — read this first

**Nothing in this directory has ever been compiled, linked, `moc`'d or run.** It was
written on a macOS machine with no Qt, no KDE Frameworks, no `cmake` and no `moc`
available. Treat the first `cmake -B build` as the start of a porting session, not a
formality.

What *has* been verified, and how:

| Area | Status |
|---|---|
| `libs/issueskit/` (format, model, sync) | **Compiled and tested** by its own suite — 162 checks |
| Every `issueskit` call this port makes | **Syntax-checked** — see [What was actually checked](#what-was-actually-checked) |
| Everything Qt or KDE in `src/` | **Never compiled. Never run.** |

Every API this port is not fully certain of carries a `// VERIFY:` comment at the exact
line. The ranked list is [below](#risks-to-check-on-a-real-kde-system-highest-first).

---

## Build

```sh
cd apps/KDE
cmake -B build
cmake --build build
```

To install (into `~/.local` by default when `CMAKE_INSTALL_PREFIX` is set that way):

```sh
cmake --install build
update-mime-database ~/.local/share/mime
update-desktop-database ~/.local/share/applications
```

The last two lines are what make `.issues` files open in this app from the file manager;
CMake installs the definitions but cannot refresh the system caches.

### Minimum versions, and why

| Dependency | Minimum | Reason |
|---|---|---|
| **Qt** | **6.5.0** | The first Qt 6 LTS. Every Qt call here predates it — `QNetworkAccessManager::sendCustomRequest`, `QNetworkRequest::setTransferTimeout(int)`, `QNetworkReply::rawHeaderPairs`, `QStyledItemDelegate`, `QSplitter`, `QStackedWidget`. The Qt 6.7+ additions that would have been convenient (`QHttpHeaders`, the `std::chrono` `setTransferTimeout` overload, `QDateTime`'s `QTimeZone`-only overloads) are avoided **on purpose** so the tree stays buildable on 6.5. |
| **KDE Frameworks** | **6.0.0** | The first KF6 release, which itself requires Qt 6.5 — so the two floors are consistent. Every KF class used (`KXmlGuiWindow`, `KActionCollection`, `KStandardAction`, `KMessageBox`, `KLocalizedString`, `KAboutData`, `KColorScheme`, `KWallet::Wallet`) exists in 6.0. |
| **extra-cmake-modules** | **6.0.0** | ECM is versioned in lockstep with the frameworks. |
| **C++** | **17** | `libs/issueskit` requires it (`std::optional`, `std::mutex`). |

### Runtime dependencies

- Qt 6 Core, Widgets, Network
- KF6 CoreAddons, I18n, XmlGui, Config, ConfigWidgets, WidgetsAddons, Wallet
- A running **KWallet** (`kwalletd6`) — only needed to store or read a GitHub token; the
  app is fully usable without it, and simply reports that a token could not be saved.
- An icon theme. Breeze is assumed; a theme missing an icon degrades to a painted
  letter badge rather than to a blank row (see `IssuePresentation.cpp`).

`find_package(KF6 COMPONENTS …)` lists `ConfigWidgets` and `WidgetsAddons` explicitly even
though `XmlGui` already depends on both, because this code includes `<KStandardAction>`,
`<KColorScheme>`, `<KMessageBox>` and `<KStandardGuiItem>` directly. A target should link
what it includes rather than inherit it by accident.

---

## Layout

```
apps/KDE/
  CMakeLists.txt                        ECM build; compiles libs/issueskit into a static lib
  ihaveissuesui.rc                      KXmlGui menu and toolbar layout
  com.druware.IHaveIssues.desktop       application entry, .issues association
  com.druware.IHaveIssues.metainfo.xml  AppStream metadata
  com.druware.IHaveIssues.xml           shared-mime-info: .issues ⊂ application/json
  src/github/                           the two platform adapters, plus the sync thread
  src/ui/                               the window, the model/delegate, and the dialogs
```

The format, the domain model and the GitHub sync flow are **not** here — they live in
`libs/issueskit/`, shared with the Haiku and GNOME ports, along with their test suite. See
`libs/issueskit/README.md`.

Naming follows **KDE/Qt convention** — `m_memberVariable`, `camelCaseMethods()`, four-space
indent — rather than the Haiku port's `fMember` / `_PrivateMethod` / tabs. Each port follows
its own platform's house style; the Haiku README says the same thing about Haiku's. App code
is in `namespace ihaveissues`, the shared library in `namespace issueskit`, so the boundary
is visible at every call site.

---

## Design decisions worth knowing

### The adaptive layout: `QStackedWidget` + `QSplitter`, in plain QWidgets

SwiftUI's `NavigationSplitView` collapses to a single navigating stack on a compact width.
QWidgets has no such container, so `MainWindow` does it explicitly. There are two
arrangements, both children of one root `QStackedWidget`:

- **wide** — the list pane and the detail pane side by side in a `QSplitter`;
- **narrow** — the same two widgets inside a second `QStackedWidget`, one visible at a
  time, with a **Back to List** action in the toolbar.

The two panes are **moved** between the arrangements (`addWidget()` reparents) rather than
duplicated, so there is exactly one list view and one detail view for the window's lifetime
and neither the selection nor either pane's scroll position is lost when the mode flips.
(The splitter's own divider position does reset to the default stretch, since the splitter
has no children while the window is narrow.)

**The threshold is one named constant**, `kNarrowLayoutWidth` in `MainWindow.cpp`. It is the
only width comparison in the port; if a second one ever appears, one of them is wrong.
`applyLayoutMode()` is a no-op when the mode has not actually changed, which is what keeps
`resizeEvent()` from thrashing the widget tree on every pixel.

**Why QWidgets and not Kirigami/QML.** Nothing here can be compiled, and Kirigami is a much
larger, faster-moving API surface whose errors would surface only at runtime, as a blank
window with warnings on stderr. QWidgets fails at compile time instead, which on an unbuilt
tree is worth a great deal. `KPageWidget` was also considered and rejected: it is a
settings-dialog navigator, not a list/detail collapse.

### Threading: QNetworkAccessManager behind a synchronous interface

`issueskit::HttpClient::Perform()` is **synchronous**. `QNetworkAccessManager` is
**asynchronous** and has **thread affinity** — it may only be used from the thread that
created it, and it delivers results through that thread's event loop. The full design is
documented at the top of `src/github/SyncWorker.h`; the short version:

1. `GitHubSyncDialog` creates a `QThread` and a `SyncWorker`. The worker is given **copies**
   of the token, the `GitHubIntegration` and the issue vector. It holds no pointer into the
   dialog, so nothing it touches can be destroyed underneath it.
2. The worker is `moveToThread()`'d and `QThread::started` is connected to `SyncWorker::run()`
   — a queued connection, so `run()` executes on the worker thread inside `QThread::exec()`.
3. `run()` calls `createQtHttpClient()` **on the worker thread**, so the QNAM is constructed,
   used and destroyed there. That is what makes its affinity correct.
4. `QtHttpClient::Perform()` blocks by spinning a **nested `QEventLoop`** — but only ever on
   the worker thread. A nested loop on the GUI thread would re-enter widget code while the
   user's click is still on the stack, which is how dialogs get deleted underneath their own
   handlers. The worker thread has no widgets and no user input, so the same trick is exactly
   what `QEventLoop` is for.
5. `finished(SyncOutcome)` crosses back as a queued signal; the outcome is **copied** into
   the event and delivered on the GUI thread.
6. The dialog's slot calls `QThread::wait()` before deleting the worker and the thread, so
   teardown is deterministic rather than dependent on `deleteLater` ordering. The dialog
   **refuses to close while a sync is in flight** (`reject()` and `closeEvent()` both), the
   same rule the Haiku port applies.

Nothing is shared between the two threads: not the model, not the token store, not the
widgets. The only crossings are the constructor arguments going out and one `SyncOutcome`
coming back.

**KWallet is touched only on the GUI thread.** Opening a wallet is a D-Bus round trip that
may raise a modal unlock prompt, and `KWallet::Wallet` is a `QObject` with thread affinity.
`GitHubSyncDialog` therefore reads the token *before* starting the thread and copies the
resulting string across.

**The 401 mapping matters.** `QtHttpClient` reports `transportSucceeded = false` **only**
when the reply carries no HTTP status code at all (DNS, TLS, refused connection, timeout).
A 401 or 404 is a perfectly good response with a status code, and reporting it as a
transport failure would defeat `GitHubSyncService`'s rule that a 401 aborts the whole sync.

### Project Settings is a `QDialog`, not a `KConfigDialog`

`KConfigDialog` exists to edit *application* settings backed by a `KConfigSkeleton` in the
user's config file. Everything on this sheet is **per-document** state, serialised into the
`.issues` file and committed to git with it. Wiring it through `KConfigSkeleton` would mean
either storing document data in `~/.config` or writing a sham skeleton — both worse than a
form. The GitHub and Azure DevOps blocks are checkable `QGroupBox`es, which is the Qt
spelling of "this whole block is behind a toggle".

### The list is a real `QAbstractItemModel`

`IssueListModel` is a two-level tree — two fixed group rows, "Open" and "Resolved", each
holding its issues in document order, or one unselectable muted placeholder when empty.
Index encoding is one sentinel: issue rows carry their parent group's row number as
`internalId()`, group rows carry `quintptr(-1)`.

Rows are addressed by **uuid** through `indexForUuid()` / `uuidAt()`, never by array offset.
Deleting an issue therefore cannot silently move the selection onto a different one — the
Apple detail view's known issue #3, avoided by construction rather than by care.

`IssueItemDelegate` draws the issue rows (type icon, bold elided title, dim `#NNN`, priority
dot) and hands group headers and placeholders straight to `QStyledItemDelegate`, so those
pick up the style's own look.

---

## Risks to check on a real KDE system, highest first

Every one of these is marked `// VERIFY:` at the exact line in the source.

### 1. `src/github/KWalletTokenStore.cpp` — KWallet (highest risk)

The whole file is one API assumption. Confirm:

1. The include is `<KWallet>` and the class is `KWallet::Wallet`, supplied by the
   `KF6::Wallet` target from `find_package(KF6 COMPONENTS Wallet)`.
2. `KWallet::Wallet::openWallet(const QString &, WId, OpenType)` returns a heap `Wallet *`
   the **caller owns**, and `nullptr` on failure.
3. `KWallet::Wallet::NetworkWallet()` is the right wallet for stored credentials
   (`LocalWallet()` is the alternative on some setups).
4. `writePassword` / `readPassword` / `removeEntry` return **`0` on success** and non-zero
   on failure — this is the KWallet convention, *not* a bool. Getting this backwards would
   silently report every save as failed.
5. `hasEntry()`, `hasFolder()`, `createFolder()`, `setFolder()` return `bool`.
6. **The first call may block on a modal unlock prompt.** Everything in this file must run
   on the GUI thread in response to a user action — never on the sync worker thread, never
   during window construction.
7. `GitHubSyncDialog`'s constructor calls `winId()` to give the prompt a parent. That forces
   a native window handle earlier than it would otherwise exist; under Wayland there is no
   X window id and KWallet ignores the argument, so the prompt appears unparented.

### 2. `src/github/QtHttpClient.cpp` — QNetworkAccessManager

1. Both `sendCustomRequest` overloads used —
   `(const QNetworkRequest &, const QByteArray &, QIODevice *)` and
   `(const QNetworkRequest &, const QByteArray &, const QByteArray &)`. The
   `static_cast<QIODevice *>(nullptr)` is what disambiguates the empty-body case.
2. `QNetworkRequest::setTransferTimeout(int msecs)` — present since Qt 5.15; Qt 6.7 added a
   `std::chrono` overload and later deprecated the `int` one. A deprecation warning on a
   newer toolchain is the expected symptom.
3. `QUrl::fromEncoded()` — takes `QByteArrayView` from Qt 6.3, `const QByteArray &` before.
   `QByteArray` converts to either, so the call should be source-compatible across 6.x.
4. `QNetworkReply::rawHeaderPairs()` returning `QList<QNetworkReply::RawHeaderPair>`.
   **Response headers are not decoration** — GitHub milestone pagination reads
   `Link: …; rel="next"` out of them. If this list comes back empty, pagination silently
   stops after the first page.
5. Qt 6's default redirect policy really is `NoLessSafeRedirectPolicy`, which is why the
   attribute is deliberately *not* set by hand.
6. This client sends the `headers` argument **verbatim and adds nothing**. GitHub requires a
   `User-Agent` and answers 403 without one, but that header is set centrally by
   `issueskit::GitHubSyncService` on every request — including the pages it follows from the
   `Link` header — so it already arrives in `headers`. Adding one here as well would make the
   value that reaches GitHub an ordering detail of `setRawHeader()`, and the desktop ports
   would drift apart on what they call themselves. The rule is part of the `HttpClient`
   contract in `libs/issueskit/README.md`; do not re-add it.

### 3. KF6 API names that were renamed between KF5 and KF6

| Call | Where | What to check |
|---|---|---|
| `KMessageBox::warningTwoActionsCancel(...)` returning `PrimaryAction` / `SecondaryAction` / `Cancel` | `MainWindow::confirmDiscardChanges` | The KF6 replacement for KF5's `warningYesNoCancel`. |
| `KMessageBox::warningContinueCancel(...)` returning `KMessageBox::Continue` | `MainWindow::deleteIssue`, `MainWindow::importMarkdown` | Survived the KF6 rename, unlike its `YesNo` siblings. |
| `KMessageBox::error(QWidget *, text, title)` | `MainWindow::showError` | KF5's `sorry()`/`error()` pair was consolidated. |
| `KStandardAction::openNew/open/save/saveAs/quit/preferences` with **pointer-to-member** slots | `MainWindow::setupActions` | KF6 also ships a newer `KStandardActions` namespace in KGuiAddons; if `KStandardAction` has gone from KConfigWidgets, the calls map one to one. |
| `KGuiItem::assign(QPushButton *, const KGuiItem &)` | `GitHubSyncDialog::buildLayout` | If it has moved, set text and icon by hand. |
| `KLocalizedString::setApplicationDomain(...)` | `main.cpp` | KF6 takes `const QByteArray &`; a string literal converts implicitly, so the line compiles against both signatures. |
| `KXmlGuiWindow::setupGUI(StandardWindowOptions, const QString &)` | `MainWindow` ctor | Option set and argument order. |

### 4. Installation paths and the `ui.rc`

1. `${KDE_INSTALL_KXMLGUIDIR}` must resolve to whatever KF6's `KXMLGUIFactory` actually
   searches. The `.rc` installs to `<that>/ihaveissues/`, and `ihaveissues` must match the
   component name — the first argument of `KAboutData` in `main.cpp`. A mismatch produces a
   window with **no menu bar and no toolbar** and nothing else wrong, which is a confusing
   symptom.
2. `ihaveissuesui.rc` uses `append="save_merge"` to place the two markdown actions after
   Save As. If KF6's `ui_standards.rc` renamed that merge point the actions land at the end
   of the File menu instead — cosmetic.
3. The `version="1"` attribute must be bumped on every change to the file, or KXmlGui keeps
   serving a user's cached copy from `~/.local/share/kxmlgui5/`.

### 5. Theme icon names

`IssuePresentation.cpp` names Breeze icons (`tools-report-bug`, `draw-star`, `view-task`,
`dialog-question`, `chronometer`, `dialog-ok-apply`, …). An icon theme is data, not API. A
name that no longer exists falls through to the next candidate and finally to a painted,
tinted letter badge, so nothing breaks — the icons just get plainer. Worth tightening once
someone can see them.

### 6. Smaller Qt assumptions

- `Qt::CTRL | Qt::Key_I` yields a `QKeyCombination` in Qt 6, which `QKeySequence` accepts.
- `QStyleOptionViewItem::features &= ~HasDecoration` plus clearing `text`/`icon` before
  `drawControl(CE_ItemViewItem)` is the standard way to let the style paint the background
  and selection while the delegate paints the content.
- Qt's supported CSS subset includes `white-space: pre-wrap`, which is what preserves line
  breaks in the detail pane's free-text blocks.

---

## Deviations from the other apps

### Intentional fixes (matching the Haiku port)

- **`closedAt` is maintained.** Saving an issue whose status becomes *Resolved* stamps
  `closedAt = now`, and moving it off *Resolved* clears it. No Apple code path ever writes
  that field — item 2 on the Apple app's own gap list. (`IssueEditDialog::commit`)
- **Every sync error is listed.** The Apple sheet keys its error list by the error string,
  so two identical messages collide and one silently vanishes (its issue #2). This uses a
  `QListWidget` with one row per error, duplicates included. (`GitHubSyncDialog::finishSync`)
- **List rows are keyed by `uuid`**, never by array offset (the Apple detail view's issue #3).
- **Milestone lookup is paginated** — handled inside the shared `GitHubSyncService`, so this
  port gets it for free. Apple stops after the first 100 (its issue #4).

### Platform necessities

- **The detail pane is a read-only `QTextBrowser`**, and markdown in body text is shown
  as-is rather than rendered. Qt can render markdown (`QTextDocument::setMarkdown`), but
  mixing a rendered sub-document into a structured, escaped section list is a correctness
  risk that cannot be checked on an unbuilt tree — and the Haiku port shows body text
  verbatim too, so the two desktop ports agree. Same sections, same order, same
  omit-when-empty rules. URLs in **Remote Links** *are* clickable.
- **The issue editor puts its five long-text sections in a `QTabWidget`**, like the Haiku
  editor's `BTabView`, rather than one very tall scrolling form.
- **"Reported" is a `QDateEdit`**, and both directions go through `issueskit::IssueDate`
  rather than `QDateTime`. A `QDate` carries no time zone, and `IssueDate` is fixed-UTC by
  construction, so there is no code path on which a local time zone could shift the stored
  day. An unparsable value keeps the previous date rather than jumping to today.
- **Type and priority indicators use themed icons and `KColorScheme` colours** rather than
  SF Symbols. The Apple source's rationale — "system semantic colors (not hard-coded hex) so
  they adapt to light/dark and contrast" — is exactly what `KColorScheme` provides, so unlike
  Haiku this port needed no literal RGB. Apple's yellow/orange priority pair collapses onto
  KDE's single `NeutralText`, so the ramp is grey → normal → neutral → negative.
- **Project Settings does not pre-seed GitHub coordinates from legacy `UserDefaults`** — KDE
  has no such legacy state, so the fields start blank. Haiku does the same.
- **`ExportSettings.preambleMarkdown` is reached as `model.exportSettings`** because `export`
  is a C++ keyword. The JSON key is still `"export"`.
- **Saving uses `QSaveFile`** (write to a temporary, then rename), so a failure part way
  through cannot truncate a `.issues` file that is already committed.

### Deliberately **not** done

- **No search, filter or sort.** The Apple app has none; adding them is product work, not a
  port.
- **No Azure DevOps sync.** None exists on Apple either. The data model and the Project
  Settings fields are present because the *format* has them, and the settings sheet says so.
- **GitHub sync is push-only.** No pull, no fetch-and-diff, no conflict resolution —
  `lastSyncedAt` and `remoteUpdatedAt` are recorded as metadata and nothing reads them back.
  This matches Apple (its issue #6) and Haiku exactly. Two-way sync would be new behaviour.
- **Dangling relations are left dangling.** Deleting an issue that others point at leaves
  those relations pointing at a missing `uuid`; the detail pane prints "Missing issue" and
  nothing is silently rewritten. Same as Apple and Haiku.
- **No catalog-management UI** for `labels` / `milestones` / `people`. The Apple app has
  none either; the fields are edited as comma-separated free text, exactly as it does.

---

## Known TODOs

There are **no silent stubs**: every function in this tree is fully implemented. What is
missing is scaffolding, not behaviour.

1. **No application icon.** `com.druware.IHaveIssues.desktop`, the AppStream metadata and
   the MIME definition all name the icon `com.druware.IHaveIssues`, but no icon file is
   installed — `main.cpp` falls back to the stock `tools-report-bug` theme icon. The Haiku
   port ships without an icon for the same reason. Cosmetic only. *Fix: add SVG/PNG icons
   and an `ecm_install_icons()` call to `CMakeLists.txt`.*
2. **No `po/` directory.** Every user-visible string goes through `i18n()`/`i18nc()` and the
   `CMakeLists.txt` calls `ki18n_install(po)` as soon as such a directory exists, but no
   catalogue has been extracted. *Fix: run the KDE `extract-messages` scripting once there is
   a translation workflow.*
3. **One document per process.** `File ▸ New` and `File ▸ Open` replace the contents of the
   current window after the unsaved-changes prompt, rather than opening a second window. The
   Apple app gets multi-document windows free from `DocumentGroup`; the Haiku port spawns a
   window per document. Matching that here means a small document-manager class, and it was
   left out rather than half-done. *Not a stub — the single-window path is complete and
   correct.*
4. **No KDE-specific test target.** The format, model and sync logic are covered by
   `libs/issueskit/tests` (162 checks, runs anywhere with a C++17 compiler). Everything left
   in this directory needs a running KDE system to exercise.

---

## What was actually checked

No Qt or KDE header exists on the machine this was written on, so no file in `src/` could be
syntax-checked as it stands — every one of them includes Qt.

What *was* checked: a harness reproducing **every `issueskit` call this port makes**, with
the same argument and result types — the two adapter subclasses (`HttpClient`, `TokenStore`)
with their exact override signatures, `GitHubSyncService::Sync`, the whole of
`IssueEditDialog::commit`/`populate`, the detail pane's field walk, `IssuesJsonCoder`
encode/decode, `IssuesMarkdownSerializer::Export`, `LegacyMarkdownImporter::Import`,
`TokenStore::AccountFor`, and the `StringUtils` / `IssueDate` helpers. It compiles clean:

```sh
clang++ -std=c++17 -fsyntax-only -Wall -Wextra -I ../../libs/issueskit/include harness.cpp
```

So the boundary with the shared library is verified. Everything on the Qt and KDE side of
that boundary is not.

Run the shared suite before and after any change here:

```sh
cd libs/issueskit/tests && make test
```
