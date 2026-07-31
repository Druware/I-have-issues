/*
 * SoupHttpClient.h -- issueskit::HttpClient over libsoup 3.
 *
 * The factory is declared here rather than in the shared library: each platform
 * supplies its own HttpClient implementation, and libs/issueskit must not know
 * that any of them exist. Haiku declares CreateServicesKitHttpClient() in
 * apps/Haiku/src/github/ServicesKitHttpClient.h for exactly the same reason.
 *
 * THREADING. issueskit::HttpClient::Perform() blocks, so it must never be
 * called from the GTK main loop. The returned client owns a SoupSession that is
 * created on first use and is NOT safe to share between threads: create one
 * client per worker thread, use it there, and destroy it there.
 * GitHubSyncDialog does exactly that -- the client is created inside the sync
 * thread and deleted before it exits.
 */
#ifndef IHAVEISSUES_SOUP_HTTP_CLIENT_H
#define IHAVEISSUES_SOUP_HTTP_CLIENT_H

#include <issueskit/HttpClient.h>

namespace ihaveissues {

//! Creates the libsoup client. The caller owns the returned object.
issueskit::HttpClient* CreateSoupHttpClient();

} // namespace ihaveissues

#endif // IHAVEISSUES_SOUP_HTTP_CLIENT_H
