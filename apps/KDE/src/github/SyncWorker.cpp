/*
 * SyncWorker.cpp
 */
#include "SyncWorker.h"

#include <memory>

#include <QString>

#include <issueskit/GitHubSyncService.h>
#include <issueskit/HttpClient.h>

#include "QtHttpClient.h"

namespace ihaveissues
{

SyncOutcome::SyncOutcome()
    : completed(false)
    , created(0)
    , updated(0)
    , failed(0)
{
}

void registerSyncMetaTypes()
{
    // VERIFY: Qt 6 registers metatypes for signal parameters automatically when
    // the type is complete and Q_DECLARE_METATYPE'd, so this call is belt and
    // braces. It is cheap, idempotent, and removes any doubt about whether the
    // queued finished() connection can marshal a SyncOutcome.
    qRegisterMetaType<ihaveissues::SyncOutcome>("ihaveissues::SyncOutcome");
}

SyncWorker::SyncWorker(const std::string &token,
                       const issueskit::GitHubIntegration &integration,
                       const std::vector<issueskit::Issue> &issues,
                       QObject *parent)
    : QObject(parent)
    , m_token(token)
    , m_integration(integration)
    , m_issues(issues)
{
}

SyncWorker::~SyncWorker() = default;

void SyncWorker::run()
{
    SyncOutcome outcome;

    // Created HERE, on the worker thread: the client owns a
    // QNetworkAccessManager, and a QNAM belongs to the thread that constructed
    // it. Destroying it at the end of this scope also destroys every reply it
    // parented, so nothing outlives the thread.
    const std::unique_ptr<issueskit::HttpClient> client(createQtHttpClient());

    issueskit::GitHubSyncService service(m_token, m_integration, client.get());

    // Sync() rewrites the vector in place; the dialog replaces the document's
    // issue array with it wholesale.
    std::vector<issueskit::Issue> issues = m_issues;
    issueskit::SyncResult result;
    std::string fatalError;

    outcome.completed = service.Sync(issues, result, fatalError);
    outcome.created = result.created;
    outcome.updated = result.updated;
    outcome.failed = result.failed;

    // Every error is carried across, duplicates included. The Apple sheet keys
    // its list by the error string itself, so two identical messages collide and
    // one silently vanishes (its own issue #2); this port lists them all.
    for (size_t i = 0; i < result.errors.size(); i++) {
        outcome.errors.append(QString::fromStdString(result.errors[i]));
    }

    if (outcome.completed) {
        outcome.issues = issues;
    } else {
        outcome.fatalError = fatalError;
    }

    Q_EMIT finished(outcome);
}

} // namespace ihaveissues
