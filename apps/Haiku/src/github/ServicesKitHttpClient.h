/*
 * ServicesKitHttpClient.h -- issueskit::HttpClient over Haiku's Services Kit.
 *
 * The factory is declared here rather than in the shared library: each platform
 * supplies its own HttpClient implementation, and libs/issueskit must not know
 * that any of them exist. The GNOME and KDE ports declare their own equivalents
 * (libsoup, QNetworkAccessManager) and the shared GitHubSyncService is happy
 * with any of them.
 */
#ifndef IHAVEISSUES_SERVICES_KIT_HTTP_CLIENT_H
#define IHAVEISSUES_SERVICES_KIT_HTTP_CLIENT_H

#include <issueskit/HttpClient.h>

namespace ihaveissues {

//! Creates the Services Kit client. The caller owns the returned object.
issueskit::HttpClient* CreateServicesKitHttpClient();

} // namespace ihaveissues

#endif // IHAVEISSUES_SERVICES_KIT_HTTP_CLIENT_H
