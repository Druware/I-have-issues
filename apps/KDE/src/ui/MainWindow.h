/*
 * MainWindow.h -- one .issues document window.
 *
 * The KDE analogue of the Apple app's ContentView: a list pane grouped into
 * "Open" and "Resolved" beside a read-only detail pane, with the menus and the
 * toolbar declared in ihaveissuesui.rc rather than built by hand.
 *
 * ---------------------------------------------------------------------------
 * ADAPTIVE LAYOUT
 * ---------------------------------------------------------------------------
 *
 * SwiftUI's NavigationSplitView collapses to a single navigating stack on a
 * compact width. QWidgets has no such container, so this window does it
 * explicitly:
 *
 *   wide   -- the list pane and the detail pane sit side by side in a QSplitter;
 *   narrow -- the same two widgets are reparented into a QStackedWidget and only
 *             one is shown at a time, with a Back action to return to the list.
 *
 * Both arrangements live in m_rootStack and the two panes are MOVED between them
 * rather than duplicated, so there is exactly one list view and one detail view
 * for the window's lifetime and no state is lost across a mode change.
 *
 * kNarrowLayoutWidth is the ONE number that decides which mode applies. There is
 * no second threshold anywhere.
 *
 * The window owns the model. Every mutation goes through applyIssue() or
 * deleteIssue(), which mark the document dirty and rebuild the list. Selection
 * is tracked by uuid, never by row.
 *
 * There is deliberately NO search, filter or sort: the Apple app has none, and
 * adding them would be new product, not a port.
 */
#ifndef IHAVEISSUES_MAIN_WINDOW_H
#define IHAVEISSUES_MAIN_WINDOW_H

#include <vector>

// QItemSelection and QModelIndex appear in slot signatures, so moc needs the
// complete types from this header alone -- a forward declaration is not enough.
#include <QItemSelection>
#include <QModelIndex>
#include <QString>

#include <KXmlGuiWindow>

#include <issueskit/IssueModel.h>

class QAction;
class QResizeEvent;
class QSplitter;
class QStackedWidget;
class QTreeView;
class QWidget;

namespace ihaveissues
{

class IssueDetailWidget;
class IssueItemDelegate;
class IssueListModel;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    //! Loads \a path into this window, replacing whatever it held.
    bool openDocument(const QString &path);

protected:
    //! Re-evaluates the wide/narrow layout mode.
    void resizeEvent(QResizeEvent *event) override;

    /*! Offers to save an unsaved document before closing.
     *
     *  queryClose() rather than closeEvent(): it is KMainWindow's own hook for
     *  exactly this, and KMainWindow::closeEvent() already consults it, so
     *  overriding the event instead would either duplicate the prompt or fight
     *  the session-management path.
     */
    bool queryClose() override;

private Q_SLOTS:
    void newDocument();
    /*! The Open action.
     *
     *  Deliberately not an overload of openDocument(const QString &): a
     *  pointer-to-member connect() cannot pick between two same-named slots
     *  without an explicit cast, and the cast is exactly the sort of thing that
     *  rots when a signature changes.
     */
    void openDocumentDialog();
    bool saveDocument();
    bool saveDocumentAs();
    void exportMarkdown();
    void importMarkdown();

    void addIssue();
    void editIssue();
    void deleteIssue();

    void showProjectSettings();
    void showGitHubSync();

    void goBackToList();
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void issueActivated(const QModelIndex &index);

private:
    void setupActions();
    void buildCentralWidget();

    /*! Moves the two panes into the arrangement \a narrow asks for.
     *
     *  A no-op when the mode has not actually changed, which is what keeps
     *  resizeEvent() from thrashing the widget tree on every pixel.
     */
    void applyLayoutMode(bool narrow);

    void rebuildList();
    void updateCaption();
    void updateActionState();

    const issueskit::Issue *selectedIssue() const;
    void applyIssue(const issueskit::Issue &issue);
    void replaceIssues(const std::vector<issueskit::Issue> &issues);

    bool readFile(const QString &path, std::string &outContents);
    bool writeFile(const QString &path, const std::string &contents);
    bool saveToPath(const QString &path);

    //! True when the user may proceed (saved, discarded, or nothing to save).
    bool confirmDiscardChanges();

    void showError(const QString &title, const QString &message);

    issueskit::IssuesDocumentModel m_model;
    QString m_documentPath;
    bool m_dirty;
    QString m_selectedUuid;

    //! False = splitter, true = single pane with Back. Set by applyLayoutMode().
    bool m_narrowLayout;
    //! Guards against applyLayoutMode() re-entering through a nested resize.
    bool m_layoutModeApplied;

    QStackedWidget *m_rootStack;
    QSplitter *m_splitter;
    QStackedWidget *m_narrowStack;
    QWidget *m_listPane;
    QTreeView *m_listView;
    IssueListModel *m_listModel;
    IssueItemDelegate *m_listDelegate;
    IssueDetailWidget *m_detailView;

    QAction *m_addIssueAction;
    QAction *m_editIssueAction;
    QAction *m_deleteIssueAction;
    QAction *m_exportMarkdownAction;
    QAction *m_importMarkdownAction;
    QAction *m_githubSyncAction;
    QAction *m_backAction;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_MAIN_WINDOW_H
