/*
 * QtHttpClient.cpp
 *
 * ###########################################################################
 * # The impedance mismatch, stated plainly.                                 #
 * #                                                                         #
 * # issueskit::HttpClient::Perform() is SYNCHRONOUS: it must not return      #
 * # until the response is complete. QNetworkAccessManager is ASYNCHRONOUS    #
 * # and delivers its results through the event loop of the thread that owns  #
 * # it.                                                                      #
 * #                                                                          #
 * # The bridge is a nested QEventLoop -- but only ever on the sync worker     #
 * # thread, never on the GUI thread. A nested loop on the GUI thread would    #
 * # re-enter widget code while the user's click is still on the stack, which  #
 * # is how dialogs get deleted underneath their own handlers. On a dedicated  #
 * # worker thread there are no widgets, no user input and no timers except    #
 * # the network stack's own, so a nested loop there is exactly the intended   #
 * # use of QEventLoop.                                                        #
 * #                                                                          #
 * # SyncWorker::run() therefore constructs this client on the worker thread,  #
 * # uses it there, and destroys it there.                                     #
 * ###########################################################################
 */
#include "QtHttpClient.h"

#include <QByteArray>
#include <QEventLoop>
#include <QIODevice>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

namespace ihaveissues
{

namespace
{

//! Aborts a stalled request rather than pinning the worker thread forever.
const int kTransferTimeoutMs = 30000;

class QtHttpClient : public issueskit::HttpClient
{
public:
    QtHttpClient();
    ~QtHttpClient() override;

    issueskit::HttpResponse Perform(const std::string &method,
                                    const std::string &url,
                                    const std::vector<issueskit::HttpHeader> &headers,
                                    const std::string &body) override;

private:
    QNetworkAccessManager m_manager;
};

QtHttpClient::QtHttpClient() = default;

QtHttpClient::~QtHttpClient() = default;

issueskit::HttpResponse QtHttpClient::Perform(const std::string &method,
                                              const std::string &url,
                                              const std::vector<issueskit::HttpHeader> &headers,
                                              const std::string &body)
{
    issueskit::HttpResponse response;

    // GitHubSyncService::_BuildUrl already percent-encoded every path segment, so
    // the string is a valid encoded URL and must be parsed as one -- re-encoding
    // it would double-escape the '%' characters.
    //
    // VERIFY: QUrl::fromEncoded() takes QByteArrayView from Qt 6.3 and
    // const QByteArray & before that. QByteArray converts to either, so this call
    // is source-compatible across 6.x.
    const QUrl requestUrl = QUrl::fromEncoded(QByteArray::fromStdString(url));
    if (!requestUrl.isValid()) {
        response.transportError = "The request URL is not valid.";
        return response;
    }

    QNetworkRequest request(requestUrl);

    // Sent verbatim, and NOTHING GitHub-specific is added to them. In particular
    // the User-Agent that GitHub requires already arrives in `headers`:
    // issueskit::GitHubSyncService sets it on every request, including the pages
    // it follows from the Link header. Setting one here too would make the value
    // that reaches GitHub an ordering detail of setRawHeader, and the desktop
    // ports would quietly disagree about what this client calls itself. The rule
    // is part of the HttpClient contract -- see libs/issueskit/README.md.
    for (const issueskit::HttpHeader &header : headers) {
        request.setRawHeader(QByteArray::fromStdString(header.name),
                             QByteArray::fromStdString(header.value));
    }

    // VERIFY: QNetworkRequest::setTransferTimeout(int msecs) exists from Qt 5.15
    // and is still present in Qt 6.5. Qt 6.7 added a std::chrono overload and
    // later deprecated the int one; if a future toolchain warns, switch to
    // setTransferTimeout(std::chrono::milliseconds{kTransferTimeoutMs}).
    request.setTransferTimeout(kTransferTimeoutMs);

    // Redirects are deliberately NOT configured: Qt 6's default redirect policy
    // is already NoLessSafeRedirectPolicy, which is what this app wants. Setting
    // the attribute by hand means stuffing an enum into a QVariant, and that only
    // round-trips if the enum is a registered metatype -- so the default is both
    // safer and less code.

    const QByteArray verb = QByteArray::fromStdString(method);
    const QByteArray payload = QByteArray::fromStdString(body);

    // VERIFY: both sendCustomRequest overloads used here exist in Qt 6 --
    //   sendCustomRequest(const QNetworkRequest &, const QByteArray &, QIODevice *)
    //   sendCustomRequest(const QNetworkRequest &, const QByteArray &, const QByteArray &)
    // The static_cast disambiguates the null-body case. Using one custom-verb
    // entry point for GET/POST/PATCH alike keeps the method string authoritative.
    QNetworkReply *reply = payload.isEmpty()
        ? m_manager.sendCustomRequest(request, verb, static_cast<QIODevice *>(nullptr))
        : m_manager.sendCustomRequest(request, verb, payload);
    if (reply == nullptr) {
        response.transportError = "The HTTP request could not be started.";
        return response;
    }

    // The nested loop. See the banner at the top of this file for why this is
    // only ever entered on the sync worker thread.
    if (!reply->isFinished()) {
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // The body is read before the status is examined: GitHub returns a JSON error
    // document with 4xx responses, and GitHubSyncService quotes it back to the
    // user in the per-issue error list.
    const QByteArray data = reply->readAll();
    response.body.assign(data.constData(), static_cast<size_t>(data.size()));

    const QVariant statusAttribute =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    // The mapping that matters: a 401 or a 404 is NOT a transport failure. The
    // attribute is only absent when nothing ever came back from the server (DNS,
    // TLS, connection refused, timeout), which is precisely the case
    // transportSucceeded == false describes. Reporting a 401 as a transport error
    // would defeat GitHubSyncService's 401-aborts-the-whole-sync rule.
    if (!statusAttribute.isValid()) {
        const QString message = reply->errorString();
        response.transportError = message.isEmpty()
            ? std::string("The request did not reach api.github.com. Check the "
                          "network connection.")
            : message.toStdString();
        reply->deleteLater();
        return response;
    }

    response.transportSucceeded = true;
    response.statusCode = statusAttribute.toInt();

    // Response headers are not decoration: GitHub milestone pagination reads
    // Link: ...; rel="next" out of them.
    //
    // VERIFY: QNetworkReply::rawHeaderPairs() returning
    // QList<QNetworkReply::RawHeaderPair> (a QPair<QByteArray, QByteArray>) is
    // long-stable. Qt 6.7 added headers() returning QHttpHeaders; that newer API
    // is avoided on purpose so this builds on 6.5.
    const QList<QNetworkReply::RawHeaderPair> pairs = reply->rawHeaderPairs();
    for (const QNetworkReply::RawHeaderPair &pair : pairs) {
        issueskit::HttpHeader entry;
        entry.name.assign(pair.first.constData(), static_cast<size_t>(pair.first.size()));
        entry.value.assign(pair.second.constData(), static_cast<size_t>(pair.second.size()));
        response.headers.push_back(entry);
    }

    // The reply is parented to m_manager, so it would be freed when this client is
    // destroyed even if the deferred delete never ran. deleteLater() gets it out
    // of the way at the next nested loop instead of letting replies pile up for
    // the length of a sync.
    reply->deleteLater();
    return response;
}

} // unnamed namespace

issueskit::HttpClient *createQtHttpClient()
{
    return new QtHttpClient();
}

} // namespace ihaveissues
