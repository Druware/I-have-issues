/*
 * HttpClient.h -- the one seam between the shared sync logic and the network.
 *
 * GitHubSyncService talks to GitHub exclusively through this interface, so the
 * whole sync flow is portable and testable while every platform keeps its own
 * HTTP stack:
 *
 *     Haiku   Services Kit (BUrlRequest)   apps/Haiku/.../ServicesKitHttpClient
 *     GNOME   libsoup                      (to be supplied by the GNOME app)
 *     KDE     QNetworkAccessManager        (to be supplied by the KDE app)
 *
 * Each port declares its own factory in its own header; this library must never
 * learn that any of them exist. Two methods and three plain structs is the
 * entire contract.
 *
 * The stub client in the test suite is a fourth implementation, which is what
 * lets the sync flow be verified with no network on any machine.
 *
 * This header is part of the portable library and must stay free of any
 * platform toolkit.
 */
#ifndef ISSUESKIT_HTTP_CLIENT_H
#define ISSUESKIT_HTTP_CLIENT_H

#include <string>
#include <vector>

namespace issueskit {

struct HttpHeader {
	std::string	name;
	std::string	value;
};


/*!	One HTTP response.

	Everything here is defined inline on purpose. A platform's concrete client is
	the only compiled artefact that pulls in its toolkit, so keeping the value
	types and the abstract base header-only lets GitHubSyncService be built and
	tested against a stub client with no toolkit present at all.
*/
struct HttpResponse {
					HttpResponse()
						:
						transportSucceeded(false),
						statusCode(0)
					{
					}

	//! False when the request never reached the server (DNS, TLS, socket).
	bool			transportSucceeded;
	//! Only meaningful when transportSucceeded is true.
	int				statusCode;
	std::string		body;
	std::vector<HttpHeader>	headers;
	//! Set when transportSucceeded is false.
	std::string		transportError;

	/*!	The value of \a name, matched case-insensitively per RFC 7230.

		Returns an empty string when the header is absent. When a header appears
		more than once the first occurrence wins; none of the headers this app
		reads are legally repeatable.
	*/
	std::string		HeaderValue(const std::string& name) const
					{
						for (size_t i = 0; i < headers.size(); i++) {
							if (headers[i].name.size() != name.size())
								continue;
							bool same = true;
							for (size_t j = 0; j < name.size() && same; j++) {
								char a = headers[i].name[j];
								char b = name[j];
								if (a >= 'A' && a <= 'Z')
									a = (char)(a - 'A' + 'a');
								if (b >= 'A' && b <= 'Z')
									b = (char)(b - 'A' + 'a');
								same = a == b;
							}
							if (same)
								return headers[i].value;
						}
						return std::string();
					}
};


class HttpClient {
public:
	virtual						~HttpClient() {}

	/*!	Performs one request and blocks until it completes.

		Called from a worker thread, never from a BLooper's message thread.

		\param method "GET", "POST", "PATCH", ...
		\param url An absolute https URL, already percent-encoded.
		\param headers Sent verbatim.
		\param body The request body, empty for GET.
	*/
	virtual	HttpResponse		Perform(const std::string& method,
									const std::string& url,
									const std::vector<HttpHeader>& headers,
									const std::string& body) = 0;
};


// No factory is declared here on purpose. Each platform declares its own -- see
// apps/Haiku/src/github/ServicesKitHttpClient.h -- so this library never names a
// concrete implementation.

} // namespace issueskit

#endif // ISSUESKIT_HTTP_CLIENT_H
