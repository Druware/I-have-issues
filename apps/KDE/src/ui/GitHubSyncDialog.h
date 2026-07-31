/*
 * GitHubSyncDialog.h -- the "Sync to GitHub" sheet.
 *
 * Owner and repository are read-only here: they belong to the document and are
 * edited once, in Project Settings.
 *
 * The token field only ever holds what the user just typed. A stored token is
 * never read back into the UI -- the sheet needs to know one EXISTS, not what it
 * is, and KWalletTokenStore::HasToken() answers that without decrypting. Leaving
 * the field blank means "keep what is stored", not "delete it"; removal is the
 * Remove button's job.
 *
 * The sync itself runs on a QThread. See SyncWorker.h for the whole threading
 * design; the short version is that everything the worker needs is copied before
 * the thread starts, and one SyncOutcome comes back over a queued signal.
 *
 * MainWindow reads issuesChanged()/issues() after exec() returns, whatever the
 * dialog's result code: a completed sync must not be thrown away because the
 * user then pressed Escape.
 */
#ifndef IHAVEISSUES_GITHUB_SYNC_DIALOG_H
#define IHAVEISSUES_GITHUB_SYNC_DIALOG_H

#include <memory>
#include <vector>

#include <QDialog>

#include <issueskit/IssueModel.h>
#include <issueskit/TokenStore.h>

#include "github/SyncWorker.h"

class QCloseEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QThread;

namespace ihaveissues
{

class GitHubSyncDialog : public QDialog
{
    Q_OBJECT

public:
    GitHubSyncDialog(const issueskit::IssuesDocumentModel &model,
                     QWidget *parent = nullptr);
    ~GitHubSyncDialog() override;

    //! Whether a sync completed and rewrote the issue array.
    bool issuesChanged() const;

    //! The rewritten issue array. Only meaningful when issuesChanged() is true.
    const std::vector<issueskit::Issue> &issues() const;

protected:
    //! Refuses to close while a sync is in flight.
    void closeEvent(QCloseEvent *event) override;

public Q_SLOTS:
    //! Escape and the Done button both land here; blocked while syncing.
    void reject() override;

private Q_SLOTS:
    void startSync();
    void removeToken();
    void updateEnabledState();
    void finishSync(const ihaveissues::SyncOutcome &outcome);
    /*! The Done button.
     *
     *  Deliberately NOT called done(): QDialog::done(int) is virtual, and a
     *  same-named slot in a subclass would hide it and read like an override
     *  that isn't one.
     */
    void finishAndClose();

private:
    void buildLayout();
    void populate();

    //! Persists a newly typed token. False means "do not dismiss".
    bool saveTypedToken();

    //! The exact gate the Apple sheet's canSync computed property applies.
    bool canSync() const;

    void setSummary(const QString &text);
    void stopThread();

    issueskit::IssuesDocumentModel m_model;
    std::unique_ptr<issueskit::TokenStore> m_tokenStore;

    bool m_hasStoredToken;
    bool m_isSyncing;
    bool m_issuesChanged;
    std::vector<issueskit::Issue> m_syncedIssues;

    QThread *m_thread;
    SyncWorker *m_worker;

    QLabel *m_ownerLabel;
    QLabel *m_repositoryLabel;
    QLabel *m_tokenStateLabel;
    QLabel *m_summaryLabel;
    QLineEdit *m_tokenEdit;
    QPushButton *m_removeButton;
    QPushButton *m_syncButton;
    QPushButton *m_doneButton;
    QProgressBar *m_progress;
    QListWidget *m_errorList;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_GITHUB_SYNC_DIALOG_H
