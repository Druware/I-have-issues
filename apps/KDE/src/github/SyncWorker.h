/*
 * SyncWorker.h -- one GitHub sync, on its own thread.
 *
 * ---------------------------------------------------------------------------
 * THE THREADING DESIGN, IN FULL
 * ---------------------------------------------------------------------------
 *
 * issueskit::GitHubSyncService::Sync() is a blocking call that issues a series
 * of HTTP requests through the blocking issueskit::HttpClient interface. It must
 * not run on the GUI thread. QNetworkAccessManager, meanwhile, is asynchronous
 * and has thread affinity: it may only be used from the thread it was created
 * on, and it delivers its results through that thread's event loop.
 *
 * Those two facts settle the design:
 *
 *   1. GitHubSyncDialog creates a QThread and a SyncWorker. The worker is given
 *      COPIES of everything it needs -- the token, the GitHubIntegration, and a
 *      snapshot of the issue vector. It holds no pointer or reference into the
 *      dialog, so nothing it touches can be destroyed underneath it.
 *
 *   2. The worker is moveToThread()'d onto the QThread, and QThread::started is
 *      connected to SyncWorker::run(). Because the worker now lives on the other
 *      thread, that connection is queued: run() executes there, inside
 *      QThread::exec().
 *
 *   3. run() creates the QNetworkAccessManager-backed HttpClient *on the worker
 *      thread* (via createQtHttpClient()), uses it there, and destroys it there.
 *      That is what makes the QNAM's affinity correct.
 *
 *   4. QtHttpClient::Perform() blocks by spinning a nested QEventLoop. That loop
 *      runs on the worker thread, whose only job is this sync -- there are no
 *      widgets to re-enter and no user input to process, which is exactly why the
 *      same trick would be unacceptable on the GUI thread.
 *
 *   5. When Sync() returns, the worker packs everything into a SyncOutcome and
 *      emits finished(). The dialog lives on the GUI thread, so that connection
 *      is queued too: the outcome is COPIED into the event and delivered on the
 *      GUI thread. Qt needs SyncOutcome to be a registered metatype for that,
 *      which registerSyncMetaTypes() guarantees.
 *
 *   6. The dialog's slot then calls QThread::wait() before deleting the worker
 *      and the thread, so teardown is deterministic rather than relying on
 *      deleteLater ordering. The dialog refuses to close while a sync is in
 *      flight, which is the same rule the Haiku port applies.
 *
 * Nothing is shared between the two threads: not the model, not the token store,
 * not the widgets. The only crossings are the constructor arguments going out
 * and one SyncOutcome coming back.
 */
#ifndef IHAVEISSUES_SYNC_WORKER_H
#define IHAVEISSUES_SYNC_WORKER_H

#include <string>
#include <vector>

#include <QMetaType>
#include <QObject>
#include <QStringList>

#include <issueskit/IssueModel.h>

namespace ihaveissues
{

/*! Everything one sync produced, as one copyable value.
 *
 *  Copyable and default-constructible on purpose: it crosses a queued signal, so
 *  Qt has to be able to store it in a QVariant.
 */
struct SyncOutcome {
    SyncOutcome();

    //! False when the sync aborted (bad token, unusable repository).
    bool completed;
    int created;
    int updated;
    int failed;
    //! One entry per failed issue, already formatted "#NNN: <message>".
    QStringList errors;
    //! Set only when completed is false.
    std::string fatalError;
    //! The rewritten issue array. Only meaningful when completed is true.
    std::vector<issueskit::Issue> issues;
};

//! Registers SyncOutcome with Qt's metatype system. Call once, from main().
void registerSyncMetaTypes();

class SyncWorker : public QObject
{
    Q_OBJECT

public:
    /*! \param token Copied; held only for the lifetime of this object.
     *  \param integration Copied.
     *  \param issues Copied -- the worker mutates its own snapshot.
     */
    SyncWorker(const std::string &token,
               const issueskit::GitHubIntegration &integration,
               const std::vector<issueskit::Issue> &issues,
               QObject *parent = nullptr);
    ~SyncWorker() override;

public Q_SLOTS:
    //! Runs the whole sync. Must execute on the worker thread.
    void run();

Q_SIGNALS:
    void finished(const ihaveissues::SyncOutcome &outcome);

private:
    std::string m_token;
    issueskit::GitHubIntegration m_integration;
    std::vector<issueskit::Issue> m_issues;
};

} // namespace ihaveissues

Q_DECLARE_METATYPE(ihaveissues::SyncOutcome)

#endif // IHAVEISSUES_SYNC_WORKER_H
