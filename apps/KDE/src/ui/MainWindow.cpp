/*
 * MainWindow.cpp
 */
#include "MainWindow.h"

#include <string>

#include <QAbstractItemView>
#include <QAction>
#include <QByteArray>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QResizeEvent>
#include <QSaveFile>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <KActionCollection>
#include <KGuiItem>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardAction>
#include <KStandardGuiItem>

#include <issueskit/IssueDate.h>
#include <issueskit/IssuesError.h>
#include <issueskit/IssuesJsonCoder.h>
#include <issueskit/IssuesMarkdown.h>
#include <issueskit/LegacyMarkdownImporter.h>
#include <issueskit/StringUtils.h>

#include "GitHubSyncDialog.h"
#include "IssueDetailWidget.h"
#include "IssueEditDialog.h"
#include "IssueItemDelegate.h"
#include "IssueListModel.h"
#include "ProjectSettingsDialog.h"

using issueskit::Issue;
using issueskit::IssuesDocumentModel;
using issueskit::IssuesError;

namespace ihaveissues
{

namespace
{

/*! THE collapse threshold, in device-independent pixels.
 *
 *  Below this the window shows one pane at a time with a Back action; at or
 *  above it, the splitter. This is the only width comparison in the port -- if a
 *  second one ever appears, one of them is wrong.
 */
const int kNarrowLayoutWidth = 720;

//! The list pane's share of the splitter when both panes are visible.
const int kListPaneStretch = 1;
const int kDetailPaneStretch = 2;

const char *const kUntitledFileName = "Untitled.issues";

QString issuesFilter()
{
    return i18n("Issue Documents (*.issues);;All Files (*)");
}

QString markdownFilter()
{
    return i18n("Markdown (*.md *.markdown);;All Files (*)");
}

} // unnamed namespace

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
    , m_dirty(false)
    , m_narrowLayout(false)
    , m_layoutModeApplied(false)
    , m_rootStack(nullptr)
    , m_splitter(nullptr)
    , m_narrowStack(nullptr)
    , m_listPane(nullptr)
    , m_listView(nullptr)
    , m_listModel(nullptr)
    , m_listDelegate(nullptr)
    , m_detailView(nullptr)
    , m_addIssueAction(nullptr)
    , m_editIssueAction(nullptr)
    , m_deleteIssueAction(nullptr)
    , m_exportMarkdownAction(nullptr)
    , m_importMarkdownAction(nullptr)
    , m_githubSyncAction(nullptr)
    , m_backAction(nullptr)
{
    buildCentralWidget();
    setupActions();

    // setupGUI reads ihaveissuesui.rc out of
    // <kxmlguidir>/<KAboutData componentName>/ and builds the menu bar and
    // toolbar from it, then restores the saved window geometry and toolbar state.
    //
    // VERIFY: setupGUI(StandardWindowOptions, const QString &) and the Default
    // option set (ToolBar | Keys | StatusBar | Save | Create) are the KF6
    // spelling.
    setupGUI(KXmlGuiWindow::Default, QStringLiteral("ihaveissuesui.rc"));

    rebuildList();
    updateCaption();
    applyLayoutMode(width() < kNarrowLayoutWidth);
}

MainWindow::~MainWindow() = default;

// #pragma mark - Construction

void MainWindow::buildCentralWidget()
{
    m_listModel = new IssueListModel(this);
    m_listDelegate = new IssueItemDelegate(this);

    m_listView = new QTreeView();
    m_listView->setModel(m_listModel);
    m_listView->setItemDelegate(m_listDelegate);
    m_listView->setHeaderHidden(true);
    m_listView->setRootIsDecorated(true);
    m_listView->setExpandsOnDoubleClick(false);
    m_listView->setUniformRowHeights(false);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView->expandAll();

    connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::selectionChanged);
    connect(m_listView, &QTreeView::activated, this, &MainWindow::issueActivated);

    // The pane is a wrapper widget so the whole list can be reparented between
    // the splitter and the narrow stack as one thing.
    m_listPane = new QWidget();
    auto *listLayout = new QVBoxLayout(m_listPane);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->addWidget(m_listView);

    m_detailView = new IssueDetailWidget();

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setChildrenCollapsible(false);

    m_narrowStack = new QStackedWidget();

    // Start in the wide arrangement so neither pane is ever a parentless
    // top-level widget, even for the moment before applyLayoutMode() runs.
    m_splitter->addWidget(m_listPane);
    m_splitter->addWidget(m_detailView);
    m_splitter->setStretchFactor(0, kListPaneStretch);
    m_splitter->setStretchFactor(1, kDetailPaneStretch);

    m_rootStack = new QStackedWidget(this);
    m_rootStack->addWidget(m_splitter);
    m_rootStack->addWidget(m_narrowStack);
    setCentralWidget(m_rootStack);
}

void MainWindow::setupActions()
{
    // Standard actions first, so KStandardAction supplies the names
    // (file_new, file_open, file_save, file_save_as, file_quit,
    // options_configure) that ihaveissuesui.rc and ui_standards.rc refer to.
    //
    // VERIFY: the pointer-to-member overloads of KStandardAction used below. KF6
    // also ships a newer KStandardActions namespace in KGuiAddons; if
    // KStandardAction has been dropped from KConfigWidgets on the target
    // distribution, the calls map one to one onto KStandardActions.
    KStandardAction::openNew(this, &MainWindow::newDocument, actionCollection());
    KStandardAction::open(this, &MainWindow::openDocumentDialog, actionCollection());
    KStandardAction::save(this, &MainWindow::saveDocument, actionCollection());
    KStandardAction::saveAs(this, &MainWindow::saveDocumentAs, actionCollection());
    // Quit routes through close() rather than QCoreApplication::quit() so it
    // still goes past queryClose() and the unsaved-changes prompt.
    KStandardAction::quit(this, &MainWindow::close, actionCollection());

    // Project Settings is per-document rather than per-application, but it is the
    // one "configure this thing" sheet the app has, so it takes the standard
    // Preferences slot and the shortcut users expect.
    KStandardAction::preferences(this, &MainWindow::showProjectSettings,
                                 actionCollection());

    m_addIssueAction = new QAction(QIcon::fromTheme(QStringLiteral("list-add")),
                                   i18nc("@action", "Add Issue…"), this);
    actionCollection()->addAction(QStringLiteral("add_issue"), m_addIssueAction);
    actionCollection()->setDefaultShortcut(m_addIssueAction,
                                           QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(m_addIssueAction, &QAction::triggered, this, &MainWindow::addIssue);

    m_editIssueAction = new QAction(QIcon::fromTheme(QStringLiteral("document-edit")),
                                    i18nc("@action", "Edit Issue…"), this);
    actionCollection()->addAction(QStringLiteral("edit_issue"), m_editIssueAction);
    actionCollection()->setDefaultShortcut(m_editIssueAction,
                                           QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_editIssueAction, &QAction::triggered, this, &MainWindow::editIssue);

    m_deleteIssueAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                                      i18nc("@action", "Delete Issue"), this);
    actionCollection()->addAction(QStringLiteral("delete_issue"), m_deleteIssueAction);
    connect(m_deleteIssueAction, &QAction::triggered, this, &MainWindow::deleteIssue);

    m_exportMarkdownAction = new QAction(
        QIcon::fromTheme(QStringLiteral("document-export")),
        i18nc("@action", "Export as Markdown…"), this);
    actionCollection()->addAction(QStringLiteral("export_markdown"),
                                  m_exportMarkdownAction);
    connect(m_exportMarkdownAction, &QAction::triggered,
            this, &MainWindow::exportMarkdown);

    m_importMarkdownAction = new QAction(
        QIcon::fromTheme(QStringLiteral("document-import")),
        i18nc("@action", "Import Legacy Markdown…"), this);
    actionCollection()->addAction(QStringLiteral("import_markdown"),
                                  m_importMarkdownAction);
    connect(m_importMarkdownAction, &QAction::triggered,
            this, &MainWindow::importMarkdown);

    m_githubSyncAction = new QAction(
        QIcon::fromTheme(QStringLiteral("cloud-upload")),
        i18nc("@action", "Sync to GitHub…"), this);
    actionCollection()->addAction(QStringLiteral("github_sync"), m_githubSyncAction);
    connect(m_githubSyncAction, &QAction::triggered, this, &MainWindow::showGitHubSync);

    m_backAction = new QAction(QIcon::fromTheme(QStringLiteral("go-previous")),
                               i18nc("@action", "Back to List"), this);
    actionCollection()->addAction(QStringLiteral("go_back_to_list"), m_backAction);
    m_backAction->setVisible(false);
    connect(m_backAction, &QAction::triggered, this, &MainWindow::goBackToList);

    updateActionState();
}

// #pragma mark - Adaptive layout

void MainWindow::applyLayoutMode(bool narrow)
{
    // A resize can in principle reach here before the widgets exist.
    if (m_rootStack == nullptr) {
        return;
    }
    if (m_layoutModeApplied && narrow == m_narrowLayout) {
        return;
    }
    m_layoutModeApplied = true;
    m_narrowLayout = narrow;

    if (narrow) {
        // addWidget() reparents, which is what moves the panes rather than
        // copying them. There is still exactly one of each.
        m_narrowStack->addWidget(m_listPane);
        m_narrowStack->addWidget(m_detailView);
        m_rootStack->setCurrentWidget(m_narrowStack);

        // Land on whichever pane the current selection implies.
        const bool hasSelection = !m_selectedUuid.isEmpty();
        m_narrowStack->setCurrentWidget(hasSelection ? static_cast<QWidget *>(m_detailView)
                                                     : static_cast<QWidget *>(m_listPane));
    } else {
        m_splitter->addWidget(m_listPane);
        m_splitter->addWidget(m_detailView);
        m_splitter->setStretchFactor(0, kListPaneStretch);
        m_splitter->setStretchFactor(1, kDetailPaneStretch);
        m_rootStack->setCurrentWidget(m_splitter);
    }

    if (m_backAction != nullptr) {
        m_backAction->setVisible(narrow);
    }
    updateActionState();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    KXmlGuiWindow::resizeEvent(event);
    applyLayoutMode(event->size().width() < kNarrowLayoutWidth);
}

void MainWindow::goBackToList()
{
    if (m_narrowLayout) {
        m_narrowStack->setCurrentWidget(m_listPane);
        updateActionState();
    }
}

// #pragma mark - List

void MainWindow::rebuildList()
{
    // Captured first: setDocument() resets the model, which drops the selection
    // and so runs selectionChanged(), which clears m_selectedUuid. Reading the
    // member back afterwards would always find it empty.
    const QString wanted = m_selectedUuid;

    m_listModel->setDocument(m_model);
    m_listView->expandAll();

    // Selection is restored by uuid, never by row: a delete or a sync can
    // reorder or shorten either group, and an offset would then point at a
    // different issue.
    const QModelIndex index = m_listModel->indexForUuid(wanted);
    if (index.isValid()) {
        m_selectedUuid = wanted;
        m_listView->selectionModel()->setCurrentIndex(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        m_selectedUuid.clear();
        m_listView->selectionModel()->clearSelection();
        m_detailView->clearIssue();
    }
    updateActionState();
}

void MainWindow::selectionChanged(const QItemSelection &selected,
                                  const QItemSelection &deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)

    const QModelIndex current = m_listView->selectionModel()->currentIndex();
    m_selectedUuid = m_listModel->uuidAt(current);

    const Issue *issue = selectedIssue();
    if (issue != nullptr) {
        m_detailView->setIssue(*issue, m_model);
    } else {
        m_selectedUuid.clear();
        m_detailView->clearIssue();
    }
    updateActionState();
}

void MainWindow::issueActivated(const QModelIndex &index)
{
    if (!index.data(IssueListModel::IsIssueRole).toBool()) {
        return;
    }
    if (m_narrowLayout) {
        // Narrow mode: activating a row navigates to the detail pane, the way
        // NavigationSplitView pushes on a compact width.
        m_narrowStack->setCurrentWidget(m_detailView);
        updateActionState();
        return;
    }
    editIssue();
}

const Issue *MainWindow::selectedIssue() const
{
    if (m_selectedUuid.isEmpty()) {
        return nullptr;
    }
    return m_model.IssueWithID(m_selectedUuid.toStdString());
}

void MainWindow::updateActionState()
{
    // A model reset can reach here through selectionChanged() before
    // setupActions() has run, so this must survive being called too early.
    if (m_editIssueAction == nullptr) {
        return;
    }

    const bool hasSelection = selectedIssue() != nullptr;
    m_editIssueAction->setEnabled(hasSelection);
    m_deleteIssueAction->setEnabled(hasSelection);
    m_backAction->setEnabled(m_narrowLayout
                             && m_narrowStack->currentWidget() == m_detailView);
}

void MainWindow::updateCaption()
{
    QString name;
    if (!m_documentPath.isEmpty()) {
        name = QFileInfo(m_documentPath).fileName();
    } else {
        const std::string projectName = issueskit::Trim(m_model.project.name);
        name = projectName.empty()
            ? QString::fromLatin1(kUntitledFileName)
            : QString::fromStdString(projectName) + QStringLiteral(".issues");
    }
    // KMainWindow appends the modified marker in the platform's own idiom.
    setCaption(name, m_dirty);
}

// #pragma mark - Issue actions

void MainWindow::addIssue()
{
    Issue issue;
    issue.number = m_model.NextNumber();
    issue.reported = issueskit::IssueDate::Today();

    IssueEditDialog dialog(issue, this);
    if (dialog.exec() == QDialog::Accepted) {
        applyIssue(dialog.result());
    }
}

void MainWindow::editIssue()
{
    const Issue *issue = selectedIssue();
    if (issue == nullptr) {
        return;
    }

    // Copied before the dialog runs: the pointer aims into m_model.issues, which
    // applyIssue() rewrites.
    const Issue draft = *issue;
    IssueEditDialog dialog(draft, this);
    if (dialog.exec() == QDialog::Accepted) {
        applyIssue(dialog.result());
    }
}

void MainWindow::deleteIssue()
{
    const Issue *issue = selectedIssue();
    if (issue == nullptr) {
        return;
    }

    const std::string uuid = issue->uuid;
    const QString title = i18nc("@title:window", "Delete %1?",
                                QString::fromStdString(issue->DisplayNumber()));

    // VERIFY: KMessageBox::warningContinueCancel(QWidget *, const QString &text,
    // const QString &title, const KGuiItem &buttonContinue, ...) returning
    // KMessageBox::ButtonCode with the Continue / Cancel values. This is the KF6
    // name; KF5's warningYesNo family was renamed, but warningContinueCancel
    // itself survived.
    const KMessageBox::ButtonCode answer = KMessageBox::warningContinueCancel(
        this,
        i18n("This removes the issue from the document. This cannot be undone from "
             "here."),
        title,
        KGuiItem(i18nc("@action:button", "Delete Issue"),
                 QStringLiteral("edit-delete")),
        KStandardGuiItem::cancel());
    if (answer != KMessageBox::Continue) {
        return;
    }

    // Deleting leaves any relation that pointed here dangling, on purpose: the
    // detail pane shows "Missing issue" and nothing is silently rewritten. That
    // is the Apple and Haiku behaviour; changing it would be a product decision.
    for (size_t i = 0; i < m_model.issues.size(); i++) {
        if (m_model.issues[i].uuid == uuid) {
            m_model.issues.erase(m_model.issues.begin() + static_cast<long>(i));
            break;
        }
    }

    m_selectedUuid.clear();
    m_dirty = true;
    rebuildList();
    updateCaption();
}

void MainWindow::applyIssue(const Issue &issue)
{
    bool replaced = false;
    for (size_t i = 0; i < m_model.issues.size(); i++) {
        if (m_model.issues[i].uuid == issue.uuid) {
            m_model.issues[i] = issue;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_model.issues.push_back(issue);
    }

    m_selectedUuid = QString::fromStdString(issue.uuid);
    m_dirty = true;
    rebuildList();
    updateCaption();
}

void MainWindow::replaceIssues(const std::vector<Issue> &issues)
{
    m_model.issues = issues;
    m_dirty = true;
    rebuildList();
    updateCaption();
}

// #pragma mark - Dialogs

void MainWindow::showProjectSettings()
{
    ProjectSettingsDialog dialog(m_model, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    // Only the settings are taken; the issue array stays whatever this window
    // currently holds.
    m_model.project = dialog.project();
    m_model.integrations = dialog.integrations();
    m_dirty = true;
    updateCaption();
}

void MainWindow::showGitHubSync()
{
    GitHubSyncDialog dialog(m_model, this);
    dialog.exec();
    // Read regardless of the result code: a sync that completed must not be
    // thrown away because the user then pressed Escape.
    if (dialog.issuesChanged()) {
        replaceIssues(dialog.issues());
    }
}

void MainWindow::showError(const QString &title, const QString &message)
{
    // VERIFY: KMessageBox::error(QWidget *, const QString &text,
    // const QString &title) is the KF6 spelling (KF5's sorry()/error() pair was
    // consolidated).
    KMessageBox::error(this, message, title);
}

// #pragma mark - Files

bool MainWindow::readFile(const QString &path, std::string &outContents)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError) {
        return false;
    }
    outContents.assign(data.constData(), static_cast<size_t>(data.size()));
    return true;
}

bool MainWindow::writeFile(const QString &path, const std::string &contents)
{
    // QSaveFile writes to a temporary and renames on commit(), so a failure part
    // way through cannot truncate a .issues file that is already in git.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (!contents.empty()) {
        const qint64 written = file.write(contents.data(),
                                          static_cast<qint64>(contents.size()));
        if (written != static_cast<qint64>(contents.size())) {
            file.cancelWriting();
            return false;
        }
    }
    return file.commit();
}

bool MainWindow::openDocument(const QString &path)
{
    std::string contents;
    if (!readFile(path, contents)) {
        showError(i18nc("@title:window", "Open Failed"),
                  QString::fromStdString(
                      IssuesError::FileReadFailed(path.toStdString()).Message()));
        return false;
    }

    IssuesDocumentModel model;
    IssuesError error;
    if (!issueskit::IssuesJsonCoder::Decode(contents, model, error)) {
        showError(i18nc("@title:window", "Open Failed"),
                  QString::fromStdString(error.Message()));
        return false;
    }

    m_model = model;
    m_documentPath = path;
    m_dirty = false;
    m_selectedUuid.clear();
    rebuildList();
    updateCaption();
    return true;
}

bool MainWindow::saveToPath(const QString &path)
{
    std::string json;
    IssuesError error;
    if (!issueskit::IssuesJsonCoder::Encode(m_model, json, error)) {
        showError(i18nc("@title:window", "Save Failed"),
                  QString::fromStdString(error.Message()));
        return false;
    }

    if (!writeFile(path, json)) {
        showError(i18nc("@title:window", "Save Failed"),
                  QString::fromStdString(
                      IssuesError::FileWriteFailed(path.toStdString()).Message()));
        return false;
    }

    m_documentPath = path;
    m_dirty = false;
    updateCaption();
    return true;
}

bool MainWindow::saveDocument()
{
    if (m_documentPath.isEmpty()) {
        return saveDocumentAs();
    }
    return saveToPath(m_documentPath);
}

bool MainWindow::saveDocumentAs()
{
    const QString suggested = m_documentPath.isEmpty()
        ? QString::fromLatin1(kUntitledFileName)
        : m_documentPath;
    const QString path = QFileDialog::getSaveFileName(
        this, i18nc("@title:window", "Save Document"), suggested, issuesFilter());
    if (path.isEmpty()) {
        return false;
    }
    return saveToPath(path);
}

void MainWindow::newDocument()
{
    if (!confirmDiscardChanges()) {
        return;
    }
    m_model = IssuesDocumentModel();
    m_documentPath.clear();
    m_dirty = false;
    m_selectedUuid.clear();
    rebuildList();
    updateCaption();
}

void MainWindow::openDocumentDialog()
{
    if (!confirmDiscardChanges()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, i18nc("@title:window", "Open Document"), QString(), issuesFilter());
    if (path.isEmpty()) {
        return;
    }
    openDocument(path);
}

void MainWindow::exportMarkdown()
{
    const std::string projectName = issueskit::Trim(m_model.project.name);
    const QString suggested = (projectName.empty()
                                   ? QStringLiteral("Issues")
                                   : QString::fromStdString(projectName))
        + QStringLiteral(".md");

    const QString path = QFileDialog::getSaveFileName(
        this, i18nc("@title:window", "Export as Markdown"), suggested,
        markdownFilter());
    if (path.isEmpty()) {
        return;
    }

    const std::string markdown = issueskit::IssuesMarkdownSerializer::Export(m_model);
    if (!writeFile(path, markdown)) {
        showError(i18nc("@title:window", "Export Failed"),
                  QString::fromStdString(
                      IssuesError::FileWriteFailed(path.toStdString()).Message()));
    }
}

void MainWindow::importMarkdown()
{
    const QString path = QFileDialog::getOpenFileName(
        this, i18nc("@title:window", "Import Legacy Markdown"), QString(),
        markdownFilter());
    if (path.isEmpty()) {
        return;
    }

    // The same destructive confirmation the Apple app shows, word for word.
    const KMessageBox::ButtonCode answer = KMessageBox::warningContinueCancel(
        this,
        i18n("Replace this document with the imported markdown?\n\nEvery issue and "
             "setting in this document is discarded and replaced by the markdown "
             "file."),
        i18nc("@title:window", "Import Legacy Markdown"),
        KGuiItem(i18nc("@action:button", "Replace Contents"),
                 QStringLiteral("document-import")),
        KStandardGuiItem::cancel());
    if (answer != KMessageBox::Continue) {
        return;
    }

    std::string markdown;
    if (!readFile(path, markdown)) {
        showError(i18nc("@title:window", "Import Failed"),
                  QString::fromStdString(
                      IssuesError::FileReadFailed(path.toStdString()).Message()));
        return;
    }

    IssuesDocumentModel imported;
    IssuesError error;
    if (!issueskit::LegacyMarkdownImporter::Import(markdown, imported, error)) {
        showError(i18nc("@title:window", "Import Failed"),
                  QString::fromStdString(error.Message()));
        return;
    }

    // Wholesale replacement, never a merge -- and the document keeps whatever
    // file it was already attached to, so the import is not saved until the user
    // says so.
    m_model = imported;
    m_selectedUuid.clear();
    m_dirty = true;
    rebuildList();
    updateCaption();
}

// #pragma mark - Closing

bool MainWindow::confirmDiscardChanges()
{
    if (!m_dirty) {
        return true;
    }

    // VERIFY: KMessageBox::warningTwoActionsCancel is the KF6 replacement for
    // KF5's warningYesNoCancel, and returns PrimaryAction / SecondaryAction /
    // Cancel.
    const KMessageBox::ButtonCode answer = KMessageBox::warningTwoActionsCancel(
        this,
        i18n("This document has unsaved changes."),
        i18nc("@title:window", "Unsaved Changes"),
        KStandardGuiItem::save(),
        KStandardGuiItem::discard(),
        KStandardGuiItem::cancel());

    switch (answer) {
    case KMessageBox::PrimaryAction:
        return saveDocument();
    case KMessageBox::SecondaryAction:
        return true;
    default:
        return false;
    }
}

bool MainWindow::queryClose()
{
    return confirmDiscardChanges();
}

} // namespace ihaveissues
