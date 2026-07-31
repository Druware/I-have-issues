/*
 * GitHubSyncDialog.cpp
 */
#include "GitHubSyncDialog.h"

#include <optional>
#include <string>

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QThread>
#include <QVBoxLayout>

#include <KGuiItem>
#include <KLocalizedString>
#include <KStandardGuiItem>

#include <issueskit/StringUtils.h>

#include "github/KWalletTokenStore.h"

using issueskit::GitHubIntegration;
using issueskit::IssuesDocumentModel;
using issueskit::TokenStore;

namespace ihaveissues
{

namespace
{

QString placeholderDash()
{
    return QStringLiteral("--");
}

} // unnamed namespace

GitHubSyncDialog::GitHubSyncDialog(const IssuesDocumentModel &model, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_hasStoredToken(false)
    , m_isSyncing(false)
    , m_issuesChanged(false)
    , m_thread(nullptr)
    , m_worker(nullptr)
    , m_ownerLabel(nullptr)
    , m_repositoryLabel(nullptr)
    , m_tokenStateLabel(nullptr)
    , m_summaryLabel(nullptr)
    , m_tokenEdit(nullptr)
    , m_removeButton(nullptr)
    , m_syncButton(nullptr)
    , m_doneButton(nullptr)
    , m_progress(nullptr)
    , m_errorList(nullptr)
{
    setWindowTitle(i18nc("@title:window", "Sync to GitHub"));
    setModal(true);

    // The one line in the sync UI that names a platform backend. Swapping in
    // libsecret or BKeyStore is a one-line change in the corresponding port.
    //
    // winId() is resolved here, on the GUI thread, so the wallet's unlock prompt
    // can be made transient for this dialog.
    //
    // VERIFY: calling winId() forces a native window handle to exist earlier than
    // it otherwise would. That is the usual KWallet idiom on X11. Under Wayland
    // there is no X window id to hand over and KWallet ignores the argument, so
    // the prompt appears unparented -- annoying, not broken.
    m_tokenStore = std::make_unique<KWalletTokenStore>(winId());

    buildLayout();
    populate();
    updateEnabledState();
    resize(520, 460);
}

GitHubSyncDialog::~GitHubSyncDialog()
{
    stopThread();
}

void GitHubSyncDialog::buildLayout()
{
    m_ownerLabel = new QLabel(placeholderDash(), this);
    m_repositoryLabel = new QLabel(placeholderDash(), this);
    m_ownerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_repositoryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_tokenEdit = new QLineEdit(this);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    m_tokenEdit->setPlaceholderText(i18n("Paste a personal access token"));
    connect(m_tokenEdit, &QLineEdit::textChanged,
            this, &GitHubSyncDialog::updateEnabledState);

    m_tokenStateLabel = new QLabel(this);

    m_removeButton = new QPushButton(this);
    // VERIFY: KStandardGuiItem::remove() and KGuiItem::assign(QPushButton *, const
    // KGuiItem &) are the KF6 way to give a button the standard destructive
    // label, icon and tooltip. If assign() has moved, set text and icon by hand.
    KGuiItem::assign(m_removeButton, KStandardGuiItem::remove());

    auto *coordinateHint =
        new QLabel(i18n("Set the owner and repository in Project Settings."), this);
    coordinateHint->setWordWrap(true);

    auto *tokenHint = new QLabel(
        i18n("Requires the repo scope. The token is stored in the system wallet, "
             "never in the document."),
        this);
    tokenHint->setWordWrap(true);

    auto *form = new QFormLayout();
    form->addRow(i18n("Owner:"), m_ownerLabel);
    form->addRow(i18n("Repository:"), m_repositoryLabel);
    form->addRow(QString(), coordinateHint);
    form->addRow(i18n("Personal access token:"), m_tokenEdit);

    auto *tokenStateRow = new QHBoxLayout();
    tokenStateRow->addWidget(m_tokenStateLabel);
    tokenStateRow->addStretch(1);
    tokenStateRow->addWidget(m_removeButton);
    form->addRow(QString(), tokenStateRow);
    form->addRow(QString(), tokenHint);

    m_progress = new QProgressBar(this);
    // An indeterminate bar: the shared sync service reports no per-issue
    // progress, and inventing one would be a lie about what is happening.
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // A QListWidget keeps one row per error, duplicates included. The Apple sheet
    // keys its list by the error string itself, so two identical messages collide
    // and one silently vanishes (its own issue #2).
    m_errorList = new QListWidget(this);
    m_errorList->setSelectionMode(QAbstractItemView::NoSelection);
    m_errorList->setVisible(false);

    auto *buttons = new QDialogButtonBox(this);
    m_syncButton = buttons->addButton(i18nc("@action:button", "Sync"),
                                      QDialogButtonBox::ApplyRole);
    m_doneButton = buttons->addButton(i18nc("@action:button", "Done"),
                                      QDialogButtonBox::AcceptRole);
    m_syncButton->setDefault(true);

    connect(m_syncButton, &QPushButton::clicked, this, &GitHubSyncDialog::startSync);
    connect(m_doneButton, &QPushButton::clicked, this, &GitHubSyncDialog::finishAndClose);
    connect(m_removeButton, &QPushButton::clicked, this, &GitHubSyncDialog::removeToken);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_progress);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_errorList, 1);
    layout->addWidget(buttons);

    m_tokenEdit->setFocus();
}

void GitHubSyncDialog::populate()
{
    const std::optional<GitHubIntegration> &integration = m_model.integrations.github;

    if (integration.has_value()) {
        m_ownerLabel->setText(integration->owner.empty()
                                  ? placeholderDash()
                                  : QString::fromStdString(integration->owner));
        m_repositoryLabel->setText(integration->repository.empty()
                                       ? placeholderDash()
                                       : QString::fromStdString(integration->repository));
    } else {
        m_ownerLabel->setText(placeholderDash());
        m_repositoryLabel->setText(placeholderDash());
        setSummary(i18n("No GitHub repository configured."));
    }

    // AccountFor() is the shared key derivation and the ONLY way this app is
    // allowed to name a wallet entry: all desktop ports must agree on it, or a
    // token saved in one is invisible to another opening the same document. It
    // also returns an empty string for coordinates it refuses to scope (blank, or
    // containing a slash), which is the signal that no token operation is
    // possible at all.
    const std::string account = TokenStore::AccountFor(integration);
    m_hasStoredToken = !account.empty() && m_tokenStore->HasToken(account);
    m_tokenStateLabel->setText(m_hasStoredToken ? i18n("Token saved")
                                                : i18n("No token saved"));
}

bool GitHubSyncDialog::canSync() const
{
    const std::optional<GitHubIntegration> &integration = m_model.integrations.github;
    const bool hasRepository = integration.has_value()
        && !issueskit::Trim(integration->owner).empty()
        && !issueskit::Trim(integration->repository).empty();
    const bool hasToken = m_hasStoredToken
        || !issueskit::Trim(m_tokenEdit->text().toStdString()).empty();
    return hasRepository && hasToken && !m_isSyncing;
}

void GitHubSyncDialog::updateEnabledState()
{
    m_syncButton->setEnabled(canSync());
    m_removeButton->setEnabled(m_hasStoredToken && !m_isSyncing);
    m_tokenEdit->setEnabled(!m_isSyncing);
    m_doneButton->setEnabled(!m_isSyncing);
    m_progress->setVisible(m_isSyncing);
}

void GitHubSyncDialog::setSummary(const QString &text)
{
    m_summaryLabel->setText(text);
}

// #pragma mark - Token handling

bool GitHubSyncDialog::saveTypedToken()
{
    const QString typed = m_tokenEdit->text();
    const std::string account = TokenStore::AccountFor(m_model.integrations.github);

    if (account.empty()) {
        // No usable repository means no account to scope a token to, so a typed
        // token cannot be stored anywhere. Say so rather than dropping it
        // silently -- the Apple sheet refuses to dismiss for the same reason.
        if (typed.isEmpty()) {
            return true;
        }
        setSummary(i18n("Set the owner and repository in Project Settings before "
                        "saving a token."));
        return false;
    }

    if (!typed.isEmpty()) {
        if (!m_tokenStore->Save(account, typed.toStdString())) {
            setSummary(i18n("The token could not be written to the system wallet."));
            return false;
        }
        m_hasStoredToken = true;
        m_tokenStateLabel->setText(i18n("Token saved"));
    }
    return true;
}

void GitHubSyncDialog::removeToken()
{
    const std::string account = TokenStore::AccountFor(m_model.integrations.github);
    if (account.empty()) {
        return;
    }

    m_tokenStore->Remove(account);
    m_hasStoredToken = false;
    m_tokenEdit->clear();
    m_tokenStateLabel->setText(i18n("No token saved"));
    updateEnabledState();
}

// #pragma mark - Sync

void GitHubSyncDialog::startSync()
{
    if (m_isSyncing || !canSync()) {
        return;
    }
    if (!saveTypedToken()) {
        return;
    }
    if (!m_model.integrations.github.has_value()) {
        return;
    }

    // The token is read HERE, on the GUI thread, because KWallet is D-Bus backed
    // and may raise a modal unlock prompt. Only the resulting string crosses to
    // the worker.
    const std::string account = TokenStore::AccountFor(m_model.integrations.github);
    std::string token;
    if (account.empty() || !m_tokenStore->Load(account, token)) {
        setSummary(i18n("No GitHub token saved for this repository. Enter one and "
                        "try again."));
        return;
    }

    m_errorList->clear();
    m_errorList->setVisible(false);

    m_thread = new QThread(this);
    m_worker = new SyncWorker(token, *m_model.integrations.github, m_model.issues);
    m_worker->moveToThread(m_thread);

    // Queued because the worker now lives on the other thread.
    connect(m_thread, &QThread::started, m_worker, &SyncWorker::run);
    // Queued back: the outcome is copied into the event and delivered here.
    connect(m_worker, &SyncWorker::finished, this, &GitHubSyncDialog::finishSync);

    m_isSyncing = true;
    updateEnabledState();
    setSummary(i18nc("@info sync is running", "Syncing…"));

    m_thread->start();
}

void GitHubSyncDialog::finishSync(const SyncOutcome &outcome)
{
    // The worker has emitted and is on its way out of run(); wait() makes the
    // teardown deterministic instead of leaning on deleteLater ordering.
    stopThread();

    m_isSyncing = false;

    if (!outcome.completed) {
        setSummary(outcome.fatalError.empty()
                       ? i18n("The sync did not complete.")
                       : QString::fromStdString(outcome.fatalError));
        updateEnabledState();
        return;
    }

    setSummary(i18nc("@info counts after a sync",
                     "%1 created, %2 updated, %3 failed.",
                     outcome.created, outcome.updated, outcome.failed));

    m_errorList->clear();
    for (const QString &error : outcome.errors) {
        m_errorList->addItem(error);
    }
    m_errorList->setVisible(!outcome.errors.isEmpty());

    // Kept locally too, so a second sync in the same session pushes the refreshed
    // remote links rather than the stale ones.
    m_model.issues = outcome.issues;
    m_syncedIssues = outcome.issues;
    m_issuesChanged = true;

    updateEnabledState();
}

void GitHubSyncDialog::stopThread()
{
    if (m_thread == nullptr) {
        return;
    }
    m_thread->quit();
    m_thread->wait();

    delete m_worker;
    m_worker = nullptr;
    delete m_thread;
    m_thread = nullptr;
}

// #pragma mark - Closing

void GitHubSyncDialog::finishAndClose()
{
    if (m_isSyncing) {
        return;
    }
    if (!saveTypedToken()) {
        return;
    }
    accept();
}

void GitHubSyncDialog::reject()
{
    // Closing mid-sync would leave the worker emitting into a dead dialog. Refuse
    // instead; the sync is short and every other control is already disabled.
    if (m_isSyncing) {
        return;
    }
    QDialog::reject();
}

void GitHubSyncDialog::closeEvent(QCloseEvent *event)
{
    if (m_isSyncing) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

bool GitHubSyncDialog::issuesChanged() const
{
    return m_issuesChanged;
}

const std::vector<issueskit::Issue> &GitHubSyncDialog::issues() const
{
    return m_syncedIssues;
}

} // namespace ihaveissues
