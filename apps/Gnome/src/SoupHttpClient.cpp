/*
 * SoupHttpClient.cpp -- libsoup 3 implementation of issueskit::HttpClient.
 *
 * ###########################################################################
 * # NEVER COMPILED. No GTK, libsoup or pkg-config was available on the      #
 * # machine this was written on. Every libsoup call carries a VERIFY note.  #
 * #                                                                        #
 * # If any of it is wrong, rewrite THIS FILE ONLY. The seam is the shared   #
 * # <issueskit/HttpClient.h>, which no platform code may alter.            #
 * ###########################################################################
 *
 * Target: libsoup 3.0 (the GLib-3/GTK4-era API). libsoup 2.4 is a different
 * library with a different soname and will not build against this file.
 */
#include "SoupHttpClient.h"

#include <glib.h>
#include <libsoup/soup.h>

namespace ihaveissues {

namespace {

//! Seconds a single request may take before libsoup gives up.
const guint kTimeoutSeconds = 30;


class SoupHttpClient : public issueskit::HttpClient {
public:
								SoupHttpClient();
	virtual						~SoupHttpClient();

	virtual	issueskit::HttpResponse Perform(const std::string& method,
									const std::string& url,
									const std::vector<issueskit::HttpHeader>&
										headers,
									const std::string& body);

private:
			SoupSession*		_Session();

			SoupSession*		fSession;
};


SoupHttpClient::SoupHttpClient()
	:
	fSession(NULL)
{
}


SoupHttpClient::~SoupHttpClient()
{
	g_clear_object(&fSession);
}


SoupSession*
SoupHttpClient::_Session()
{
	if (fSession == NULL) {
		// DO NOT set "user-agent" here. GitHub rejects a request with no
		// User-Agent, and GitHubSyncService now sends one itself on every
		// request it issues -- including the pages it follows out of the Link
		// header. It arrives in Perform()'s `headers` argument like any other
		// header and is appended verbatim below. Setting it on the session as
		// well would make "which value actually reaches GitHub" a libsoup
		// detail that cannot be checked from the shared library's tests, and
		// the three desktop ports would drift apart. The HttpClient contract in
		// libs/issueskit/README.md is the rule: send the given headers
		// verbatim, add nothing GitHub-specific.
		//
		// VERIFY: SoupSession's "timeout" GObject property -- a guint of whole
		// seconds, where 0 means "no timeout". g_object_new is used in
		// preference to soup_session_new_with_options() because the property
		// name is checked against the introspected class at runtime and
		// produces a loud warning if wrong, where a mistyped varargs option is
		// silently ignored.
		fSession = SOUP_SESSION(g_object_new(SOUP_TYPE_SESSION,
			"timeout", kTimeoutSeconds,
			NULL));
	}
	return fSession;
}


issueskit::HttpResponse
SoupHttpClient::Perform(const std::string& method, const std::string& url,
	const std::vector<issueskit::HttpHeader>& headers, const std::string& body)
{
	issueskit::HttpResponse response;

	// VERIFY: soup_message_new(const char* method, const char* uri_string)
	// returns NULL when the URI does not parse. GitHubSyncService::_BuildUrl
	// hands over an already percent-encoded absolute https URL.
	SoupMessage* message = soup_message_new(method.c_str(), url.c_str());
	if (message == NULL) {
		response.transportError = "The request URL is not valid.";
		return response;
	}

	// VERIFY: soup_message_get_request_headers() returns a borrowed
	// SoupMessageHeaders*; soup_message_headers_append(h, name, value) appends
	// without replacing. GitHubSyncService never sends a header twice.
	//
	// VERIFY, and check this one on the wire rather than by reading: that with
	// SoupSession:user-agent left unset, libsoup neither strips nor overrides
	// the User-Agent appended here, and does not substitute a "libsoup/3.x" of
	// its own. GitHub 403s a request with no User-Agent, so if libsoup does
	// interfere, every request fails identically and the symptom is a sync that
	// reports 403 on everything.
	SoupMessageHeaders* requestHeaders = soup_message_get_request_headers(message);
	for (size_t i = 0; i < headers.size(); i++) {
		soup_message_headers_append(requestHeaders, headers[i].name.c_str(),
			headers[i].value.c_str());
	}

	if (!body.empty()) {
		// VERIFY: soup_message_set_request_body_from_bytes(SoupMessage*,
		// const char* content_type, GBytes*) -- it takes its own reference on
		// the GBytes, so unreffing here is correct. The Content-Type it sets
		// duplicates the one GitHubSyncService already puts in `headers`;
		// libsoup uses the explicitly set body content type, so the two agree.
		GBytes* bytes = g_bytes_new(body.data(), body.size());
		soup_message_set_request_body_from_bytes(message, "application/json",
			bytes);
		g_bytes_unref(bytes);
	}

	// VERIFY: soup_session_send_and_read(SoupSession*, SoupMessage*,
	// GCancellable*, GError**) is the SYNCHRONOUS whole-body read added in
	// libsoup 3.0. It blocks, which is why this is only ever reached from a
	// worker thread (see SoupHttpClient.h).
	GError* error = NULL;
	GBytes* responseBody = soup_session_send_and_read(_Session(), message, NULL,
		&error);

	if (responseBody == NULL) {
		// The request never reached the server: DNS, TLS, connection refused,
		// timeout. That is exactly what transportSucceeded == false means.
		response.transportError = error != NULL && error->message != NULL
			? std::string(error->message)
			: std::string("The request did not reach api.github.com. Check the "
				"network connection.");
		g_clear_error(&error);
		g_object_unref(message);
		return response;
	}
	g_clear_error(&error);

	response.transportSucceeded = true;

	// VERIFY: soup_message_get_status() returns a SoupStatus (an enum whose
	// values are the numeric HTTP status codes) in libsoup 3.
	response.statusCode = (int)soup_message_get_status(message);

	gsize size = 0;
	const char* data = (const char*)g_bytes_get_data(responseBody, &size);
	if (data != NULL && size > 0)
		response.body.assign(data, size);
	g_bytes_unref(responseBody);

	// Response headers are not decoration: GitHub milestone pagination reads
	// `Link: ...; rel="next"` out of them.
	//
	// VERIFY: SoupMessageHeadersIter is a stack-allocated struct initialised by
	// soup_message_headers_iter_init(); soup_message_headers_iter_next() yields
	// borrowed name/value pointers and returns FALSE when exhausted.
	SoupMessageHeaders* responseHeaders
		= soup_message_get_response_headers(message);
	if (responseHeaders != NULL) {
		SoupMessageHeadersIter iter;
		const char* name = NULL;
		const char* value = NULL;
		soup_message_headers_iter_init(&iter, responseHeaders);
		while (soup_message_headers_iter_next(&iter, &name, &value)) {
			if (name == NULL)
				continue;
			issueskit::HttpHeader entry;
			entry.name = name;
			entry.value = value != NULL ? value : "";
			response.headers.push_back(entry);
		}
	}

	g_object_unref(message);
	return response;
}

} // unnamed namespace


issueskit::HttpClient*
CreateSoupHttpClient()
{
	return new SoupHttpClient();
}

} // namespace ihaveissues
