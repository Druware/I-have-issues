/*
 * QtHttpClient.h -- issueskit::HttpClient over QNetworkAccessManager.
 *
 * The factory is declared here rather than in the shared library: every port
 * supplies its own HttpClient implementation and libs/issueskit must never learn
 * that any of them exist. The Haiku port declares
 * ihaveissues::CreateServicesKitHttpClient() in exactly the same way.
 *
 * THREAD AFFINITY. The returned client owns a QNetworkAccessManager, and a QNAM
 * may only be used from the thread it was created on. issueskit::HttpClient is a
 * blocking interface, so the client must be created *and* used on a thread that
 * is allowed to block -- i.e. the sync worker thread, never the GUI thread. Call
 * this function from SyncWorker::run(), which is where the object graph for one
 * sync is built. See SyncWorker.h for the whole threading design.
 */
#ifndef IHAVEISSUES_QT_HTTP_CLIENT_H
#define IHAVEISSUES_QT_HTTP_CLIENT_H

#include <issueskit/HttpClient.h>

namespace ihaveissues
{

/*! Creates the QNetworkAccessManager-backed client. The caller owns it.
 *
 *  Must be called on the thread that will call Perform().
 */
issueskit::HttpClient *createQtHttpClient();

} // namespace ihaveissues

#endif // IHAVEISSUES_QT_HTTP_CLIENT_H
