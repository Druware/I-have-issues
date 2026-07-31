/*
 * ServicesKitHttpClient.cpp -- classic Services Kit implementation.
 *
 * ###########################################################################
 * # THIS FILE IS THE HIGHEST-RISK FILE IN THE PROJECT.                      #
 * #                                                                         #
 * # It has never been compiled: no Haiku system was available. Every         #
 * # Services Kit call below carries a VERIFY comment naming what to check.   #
 * # If the classic API has been removed from the Haiku you are building on,  #
 * # rewrite THIS FILE ONLY against BHttpSession. The seam is the shared      #
 * # <issueskit/HttpClient.h>, which no platform code may alter.              #
 * ###########################################################################
 *
 * Target: Haiku R1/beta4 or R1/beta5, classic Services Kit, libbnetapi.so.
 */
#include "ServicesKitHttpClient.h"

// VERIFY: header names and locations.
//   /system/develop/headers/os/netservices/{Url,UrlRequest,UrlProtocolRoster,
//   UrlProtocolListener,HttpRequest,HttpHeaders,HttpResult,UrlResult}.h
//   Older trees put these under headers/os/net/ instead of headers/os/netservices/.
//   The makefile-engine adds every os/ subdirectory to the include path, so a
//   bare <HttpRequest.h> should resolve either way -- confirm on the box.
#include <HttpHeaders.h>
#include <HttpRequest.h>
#include <HttpResult.h>
#include <Url.h>
#include <UrlProtocolListener.h>
#include <UrlProtocolRoster.h>
#include <UrlRequest.h>

#include <DataIO.h>
#include <OS.h>
#include <String.h>

// VERIFY: the Services Kit was moved into BPrivate::Network around hrev54000.
//   On a tree older than that these types are in the global namespace and these
//   using-declarations will not compile -- delete them if so.
using BPrivate::Network::BHttpHeader;
using BPrivate::Network::BHttpHeaders;
using BPrivate::Network::BHttpRequest;
using BPrivate::Network::BHttpResult;
using BPrivate::Network::BUrlProtocolListener;
using BPrivate::Network::BUrlProtocolRoster;
using BPrivate::Network::BUrlRequest;

namespace ihaveissues {

namespace {

/*!	Collects the response body.

	The body is also written to the BMallocIO handed to MakeRequest; this
	listener exists only to notice transport-level failure, which the status code
	cannot express.
*/
class SyncListener : public BUrlProtocolListener {
public:
	SyncListener()
		:
		fFailed(false)
	{
	}

	// VERIFY: signature of RequestCompleted in the target release. In beta4 it
	// is `virtual void RequestCompleted(BUrlRequest* caller, bool success);`.
	virtual void RequestCompleted(BUrlRequest* caller, bool success)
	{
		(void)caller;
		if (!success)
			fFailed = true;
	}

	// VERIFY: DebugMessage's enum type is BUrlProtocolDebugMessage in beta4.
	// It is only overridden here to keep protocol chatter off stderr; if the
	// signature differs, deleting this override costs nothing.
	virtual void DebugMessage(BUrlRequest* caller,
		BPrivate::Network::BUrlProtocolDebugMessage type, const char* text)
	{
		(void)caller;
		(void)type;
		(void)text;
	}

	bool Failed() const { return fFailed; }

private:
	bool	fFailed;
};


class ServicesKitHttpClient : public issueskit::HttpClient {
public:
	virtual						~ServicesKitHttpClient();
	virtual	issueskit::HttpResponse	Perform(const std::string& method,
									const std::string& url,
									const std::vector<issueskit::HttpHeader>& headers,
									const std::string& body);
};


ServicesKitHttpClient::~ServicesKitHttpClient()
{
}


issueskit::HttpResponse
ServicesKitHttpClient::Perform(const std::string& method,
	const std::string& url, const std::vector<issueskit::HttpHeader>& headers,
	const std::string& body)
{
	issueskit::HttpResponse response;

	// VERIFY: BUrl(const char*, bool encodedUrl). The second argument tells the
	// parser the string is already percent-encoded, which it is -- see
	// GitHubSyncService::_BuildUrl.
	BUrl parsedUrl(url.c_str(), true);
	if (!parsedUrl.IsValid()) {
		response.transportError = "The request URL is not valid.";
		return response;
	}

	BMallocIO output;
	SyncListener listener;

	// VERIFY: MakeRequest's signature. Since roughly hrev53680 it is
	//   MakeRequest(const BUrl&, BDataIO* output, BUrlProtocolListener*,
	//               BUrlContext*)
	// On an older tree the BDataIO parameter does not exist and the call is
	//   MakeRequest(const BUrl&, BUrlProtocolListener*, BUrlContext*)
	// in which case the body must be gathered in SyncListener::DataReceived
	// instead of from `output`.
	BUrlRequest* request = BUrlProtocolRoster::MakeRequest(parsedUrl, &output,
		&listener);
	if (request == NULL) {
		response.transportError = "No protocol handler for this URL.";
		return response;
	}

	BHttpRequest* httpRequest = dynamic_cast<BHttpRequest*>(request);
	if (httpRequest == NULL) {
		delete request;
		response.transportError = "The URL is not an HTTP or HTTPS URL.";
		return response;
	}

	// SetMethod takes a plain string, so "PATCH" works without needing a
	// B_HTTP_PATCH constant (which the classic kit does not define).
	// VERIFY: BHttpRequest::SetMethod(const char* const).
	httpRequest->SetMethod(method.c_str());
	httpRequest->SetFollowLocation(true);
	httpRequest->SetMaxRedirections(5);

	// Do NOT call SetUserAgent() here. GitHubSyncService sends an explicit
	// User-Agent header on every request (GitHub answers 403 without one), and
	// setting it here as well would leave it ambiguous which value actually goes
	// out -- SetUserAgent versus the header supplied through AdoptHeaders below.
	// The point of setting it in the shared service is that every port sends one
	// identical value. This transport's job is to send the headers it is given,
	// verbatim, and add nothing of its own.

	// VERIFY: BHttpHeaders::AddHeader(const char* name, const char* value) and
	// BHttpRequest::AdoptHeaders(BHttpHeaders*). AdoptHeaders takes ownership,
	// which is why this allocates. Some trees also expose
	// SetHeaders(const BHttpHeaders&), which would let this use a stack object.
	BHttpHeaders* requestHeaders = new BHttpHeaders();
	for (size_t i = 0; i < headers.size(); i++) {
		requestHeaders->AddHeader(headers[i].name.c_str(),
			headers[i].value.c_str());
	}
	httpRequest->AdoptHeaders(requestHeaders);

	// VERIFY: AdoptInputData(BDataIO*, ssize_t) takes ownership of the BDataIO
	// and is what sets Content-Length for POST/PATCH bodies.
	if (!body.empty()) {
		BMallocIO* input = new BMallocIO();
		input->Write(body.data(), body.size());
		input->Seek(0, SEEK_SET);
		httpRequest->AdoptInputData(input, (ssize_t)body.size());
	}

	// VERIFY: BUrlRequest::Run() spawns the protocol thread and returns its id,
	// so wait_for_thread() is a correct synchronous wait. If Run() returns a
	// status_t instead on the target release, poll IsRunning() with snooze().
	thread_id requestThread = request->Run();
	if (requestThread < B_OK) {
		delete request;
		response.transportError = "The HTTP request could not be started.";
		return response;
	}
	status_t exitValue = B_OK;
	wait_for_thread(requestThread, &exitValue);

	// VERIFY: BUrlRequest::Result() returns a const BUrlResult&, and the HTTP
	// protocol's concrete result type BHttpResult carries StatusCode().
	const BHttpResult* result
		= dynamic_cast<const BHttpResult*>(&request->Result());
	if (listener.Failed() || result == NULL) {
		delete request;
		response.transportError
			= "The request did not reach api.github.com. Check the network "
			  "connection.";
		return response;
	}

	response.transportSucceeded = true;
	response.statusCode = (int)result->StatusCode();
	response.body.assign((const char*)output.Buffer(), output.BufferLength());

	// Response headers are needed for GitHub's `Link:` pagination header.
	//
	// VERIFY: BHttpResult::Headers() returns `const BHttpHeaders&`, and
	// BHttpHeaders exposes CountHeaders() plus HeaderAt(int32) returning a
	// `const BHttpHeader&` with Name()/Value() accessors. Some trees instead
	// offer only NameAt(int32) / operator[](const char*); if HeaderAt is
	// unavailable, iterate with NameAt() and look each value up by name.
	const BHttpHeaders& resultHeaders = result->Headers();
	for (int32 i = 0; i < resultHeaders.CountHeaders(); i++) {
		const BHttpHeader& header = resultHeaders.HeaderAt(i);
		if (header.Name() == NULL)
			continue;
		issueskit::HttpHeader entry;
		entry.name = header.Name();
		entry.value = header.Value() != NULL ? header.Value() : "";
		response.headers.push_back(entry);
	}

	delete request;
	return response;
}

} // unnamed namespace


issueskit::HttpClient*
CreateServicesKitHttpClient()
{
	return new ServicesKitHttpClient();
}

} // namespace ihaveissues
