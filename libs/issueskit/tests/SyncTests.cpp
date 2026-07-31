/*
 * SyncTests.cpp -- GitHubSyncService against a stub HttpClient.
 *
 * GitHubSyncService talks to the network only through the HttpClient interface,
 * so the whole sync flow -- endpoints, headers, payload shape, merge rules,
 * pagination, error accumulation -- is testable with no network and no Be API.
 */
#include "TestSupport.h"

#include <string>
#include <vector>

#include <issueskit/GitHubSyncService.h>
#include <issueskit/HttpClient.h>
#include <issueskit/IssueDate.h>
#include <issueskit/IssueModel.h>

namespace issueskit {
namespace tests {

namespace {

//! One canned reply, matched by a substring of the request URL.
struct StubReply {
	std::string	urlContains;
	int			status;
	std::string	body;
	std::string	linkHeader;
};


/*!	Records every request and replies from a script.

	The first reply whose urlContains matches is used and then consumed, so a
	script can hand back a different page on each call to the same URL.
*/
class StubClient : public HttpClient {
public:
	StubClient()
		:
		fDefaultStatus(200)
	{
	}

	virtual HttpResponse Perform(const std::string& method,
		const std::string& url, const std::vector<HttpHeader>& headers,
		const std::string& body)
	{
		std::string headerText;
		for (size_t i = 0; i < headers.size(); i++)
			headerText += headers[i].name + "=" + headers[i].value + ";";
		log.push_back(method + " " + url + " | " + headerText + " | " + body);

		HttpResponse response;
		response.transportSucceeded = true;
		response.statusCode = fDefaultStatus;

		for (size_t i = 0; i < fReplies.size(); i++) {
			if (url.find(fReplies[i].urlContains) == std::string::npos)
				continue;
			response.statusCode = fReplies[i].status;
			response.body = fReplies[i].body;
			if (!fReplies[i].linkHeader.empty()) {
				HttpHeader link;
				link.name = "Link";
				link.value = fReplies[i].linkHeader;
				response.headers.push_back(link);
			}
			fReplies.erase(fReplies.begin() + (long)i);
			return response;
		}

		// Default behaviour when the script has nothing to say.
		response.body = url.find("milestones") != std::string::npos
			? "[{\"title\":\"v1.0\",\"number\":4}]"
			: "{\"number\":412,"
			  "\"html_url\":\"https://github.com/o/r/issues/412\","
			  "\"updated_at\":\"2026-05-07T16:42:00Z\"}";
		return response;
	}

	void AddReply(const std::string& urlContains, const std::string& body,
		const std::string& linkHeader = "", int status = 200)
	{
		StubReply reply;
		reply.urlContains = urlContains;
		reply.status = status;
		reply.body = body;
		reply.linkHeader = linkHeader;
		fReplies.push_back(reply);
	}

	void SetDefaultStatus(int status) { fDefaultStatus = status; }

	std::vector<std::string>	log;

private:
	std::vector<StubReply>		fReplies;
	int							fDefaultStatus;
};


GitHubIntegration
MakeIntegration()
{
	GitHubIntegration integration;
	// A deliberately hostile owner: spaces, and a slash that must not be able to
	// invent extra path segments.
	integration.owner = "open bcm/../x";
	integration.repository = "i-have-issues";
	integration.defaultLabels.push_back("triage");
	integration.defaultAssignees.push_back("dru");
	return integration;
}

} // unnamed namespace


int
RunSyncTests(TestRun& run)
{
	// --- A full push over four issues ---------------------------------------
	{
		GitHubIntegration g = MakeIntegration();

		Issue a;
		a.number = 7;
		a.title = "Login button does nothing";
		a.type = kIssueTypeBug;
		a.priority = kIssuePriorityHigh;
		a.status = kIssueStatusOpen;
		a.reportedBy = "dru";
		a.area = "Views";
		IssueDate::Parse("2026-05-01", a.reported);
		a.description = "It is inert.";
		a.stepsToReproduce.push_back("Open");
		a.stepsToReproduce.push_back("Tap");
		a.notes = "notes here";
		a.labels.push_back("ui");
		a.milestone = std::string("v1.0");

		// Resolved with no existing link: create, then close.
		Issue b;
		b.number = 8;
		b.status = kIssueStatusResolved;

		// An existing GitHub link whose identifier is not a number.
		Issue c;
		c.number = 9;
		RemoteLink bad;
		bad.provider = RemoteProvider::GitHub();
		bad.identifier = "abc";
		c.remoteLinks.push_back(bad);

		// A gitlab link must never be mistaken for a GitHub one.
		Issue d;
		d.number = 10;
		RemoteLink gitlab;
		gitlab.provider = RemoteProvider("gitlab");
		gitlab.identifier = "55";
		d.remoteLinks.push_back(gitlab);

		std::vector<Issue> issues;
		issues.push_back(a);
		issues.push_back(b);
		issues.push_back(c);
		issues.push_back(d);

		StubClient client;
		GitHubSyncService service("tok3n", g, &client);
		SyncResult result;
		std::string fatal;

		CHECK(run, service.Sync(issues, result, fatal), "the sync completes");
		CHECK(run, result.created == 3, "three issues created");
		CHECK(run, result.updated == 0, "nothing updated");
		CHECK(run, result.failed == 1,
			"the unparsable remote identifier fails just its own issue");
		CHECK(run, result.errors.size() == 1
			&& result.errors[0].find("#009:") == 0,
			"a per-issue error is prefixed with its display number");

		CHECK(run, client.log[0].find(
			"GET https://api.github.com/repos/open%20bcm%2F..%2Fx/"
			"i-have-issues/milestones?state=all&per_page=100") == 0,
			"path segments are percent-encoded and a typed slash cannot split them");

		CHECK(run, client.log[1].find("Authorization=Bearer tok3n;")
			!= std::string::npos, "the Authorization header is sent");
		CHECK(run, client.log[1].find("Accept=application/vnd.github+json;")
			!= std::string::npos, "the Accept header is sent");
		CHECK(run, client.log[1].find("X-GitHub-Api-Version=2022-11-28;")
			!= std::string::npos, "the API version header is sent");

		// GitHub answers 403 to a request with no User-Agent, so its absence
		// would break sync outright on any transport that does not add one of
		// its own. The service sets it precisely so no transport has to.
		CHECK(run, client.log[1].find("User-Agent=IHaveIssues/1.0;")
			!= std::string::npos, "the User-Agent header is sent on a create POST");
		CHECK(run, client.log[0].find("User-Agent=IHaveIssues/1.0;")
			!= std::string::npos, "the User-Agent header is sent on a milestones GET");
		CHECK(run, client.log[3].find("User-Agent=IHaveIssues/1.0;")
			!= std::string::npos, "the User-Agent header is sent on an update PATCH");
		{
			// Stronger than the three above: no request of any kind may escape
			// without it, including ones added later.
			bool everyRequest = true;
			for (size_t i = 0; i < client.log.size(); i++) {
				if (client.log[i].find("User-Agent=IHaveIssues/1.0;")
					== std::string::npos) {
					everyRequest = false;
				}
			}
			CHECK(run, everyRequest,
				"every request the service issues carries the User-Agent header");
		}

		CHECK(run, client.log[1].find("POST https://api.github.com/repos/") == 0,
			"a new issue is created with POST");
		CHECK(run, client.log[1].find("\"state\"") == std::string::npos,
			"create sends no state field");
		CHECK(run, client.log[1].find("\"milestone\" : 4") != std::string::npos,
			"a milestone name is resolved to its GitHub number");
		CHECK(run, client.log[1].find("\"ui\"") < client.log[1].find("\"triage\""),
			"labels merge with the issue's own values first");

		CHECK(run, client.log[1].find(
			"**Type:** Bug | **Priority:** High | **Status:** Open")
			!= std::string::npos, "the synthesized body opens with the metadata line");
		CHECK(run, client.log[1].find(
			"**Reported:** 2026-05-01 \xC2\xB7 dru | **Area:** Views")
			!= std::string::npos, "the reported line uses a middle dot");
		CHECK(run, client.log[1].find("---\\n**Description**")
			!= std::string::npos, "the description section is included");
		CHECK(run, client.log[1].find(
			"**Steps to Reproduce**\\n\\n1. Open\\n2. Tap") != std::string::npos,
			"steps are numbered in the body");
		CHECK(run, client.log[1].find(
			"*Synced from local issue tracker \xC2\xB7 #007*")
			!= std::string::npos, "the body ends with the sync footer");

		CHECK(run, client.log[2].find("POST ") == 0,
			"the resolved issue is created first");
		CHECK(run, client.log[2].find("\"title\" : \"Untitled Issue\"")
			!= std::string::npos, "a blank title becomes Untitled Issue");
		CHECK(run, client.log[3].find("PATCH ") == 0
			&& client.log[3].find("\"state\" : \"closed\"") != std::string::npos,
			"a resolved issue is created then closed with a second request");

		CHECK(run, issues[3].remoteLinks.size() == 2,
			"the gitlab link is kept and a github link appended");
		CHECK(run, issues[3].remoteLinks[0].provider.RawValue() == "gitlab",
			"the gitlab link is preserved verbatim");
		CHECK(run, issues[3].remoteLinks[1].provider.IsGitHub()
			&& issues[3].remoteLinks[1].identifier == "412",
			"the new link records the GitHub issue number");
		CHECK(run, issues[3].remoteLinks[1].url.has_value()
			&& *issues[3].remoteLinks[1].url
				== "https://github.com/o/r/issues/412",
			"the new link records the html_url");
		CHECK(run, issues[3].remoteLinks[1].remoteUpdatedAt.has_value(),
			"the new link records the remote updated_at");
	}

	// --- 401 aborts the whole sync ------------------------------------------
	{
		GitHubIntegration g = MakeIntegration();
		Issue a;
		a.number = 1;

		StubClient client;
		client.SetDefaultStatus(401);
		GitHubSyncService service("bad", g, &client);

		std::vector<Issue> issues;
		issues.push_back(a);
		issues.push_back(a);
		SyncResult result;
		std::string fatal;

		CHECK(run, !service.Sync(issues, result, fatal),
			"a 401 aborts the whole sync instead of failing every issue");
		CHECK(run, fatal.find("Invalid or expired GitHub token") == 0,
			"the 401 message names the token");
	}

	// --- The milestone request is only made when one is named ---------------
	{
		GitHubIntegration g;
		g.owner = "o";
		g.repository = "r";

		Issue plain;
		plain.number = 1;

		StubClient client;
		GitHubSyncService service("t", g, &client);
		std::vector<Issue> issues;
		issues.push_back(plain);
		SyncResult result;
		std::string fatal;
		service.Sync(issues, result, fatal);

		CHECK(run, !client.log.empty()
			&& client.log[0].find("milestones") == std::string::npos,
			"milestones are fetched only when some issue names one");
	}

	// --- Pagination: a milestone on page 2 still resolves --------------------
	{
		GitHubIntegration g;
		g.owner = "o";
		g.repository = "r";

		Issue late;
		late.number = 1;
		late.milestone = std::string("v9.9");	// only present on page 2

		StubClient client;
		client.AddReply("milestones?state=all&per_page=100",
			"[{\"title\":\"v1.0\",\"number\":1}]",
			"<https://api.github.com/repos/o/r/milestones?state=all&per_page=100"
			"&page=2>; rel=\"next\", "
			"<https://api.github.com/repos/o/r/milestones?state=all&per_page=100"
			"&page=3>; rel=\"last\"");
		client.AddReply("page=2",
			"[{\"title\":\"v9.9\",\"number\":242}]",
			"<https://api.github.com/repos/o/r/milestones?state=all&per_page=100"
			"&page=3>; rel=\"next\"");
		client.AddReply("page=3", "[{\"title\":\"v10.0\",\"number\":300}]", "");

		GitHubSyncService service("t", g, &client);
		std::vector<Issue> issues;
		issues.push_back(late);
		SyncResult result;
		std::string fatal;

		CHECK(run, service.Sync(issues, result, fatal),
			"a paginated milestone list syncs");
		CHECK(run, client.log.size() == 4,
			"three milestone pages are fetched, then the issue is pushed");
		CHECK(run, client.log[1].find("page=2") != std::string::npos,
			"the rel=next link on page 1 is followed");
		CHECK(run, client.log[2].find("page=3") != std::string::npos,
			"the rel=next link on page 2 is followed");
		CHECK(run, client.log[3].find("POST ") == 0,
			"pagination stops when no rel=next link remains");
		CHECK(run, client.log[3].find("\"milestone\" : 242")
			!= std::string::npos,
			"a milestone found only on page 2 still resolves to its number");
		// Followed pages come from a URL in the Link header rather than one the
		// service built, so confirm they are still fully decorated.
		CHECK(run, client.log[1].find("User-Agent=IHaveIssues/1.0;")
				!= std::string::npos
			&& client.log[2].find("User-Agent=IHaveIssues/1.0;")
				!= std::string::npos,
			"pages followed from the Link header also carry the User-Agent header");
	}

	// --- Pagination stops on a self-referential Link header ------------------
	{
		GitHubIntegration g;
		g.owner = "o";
		g.repository = "r";

		Issue one;
		one.number = 1;
		one.milestone = std::string("v1.0");

		StubClient client;
		// A server that points page 1 at itself must not spin the loop forever.
		client.AddReply("milestones", "[{\"title\":\"v1.0\",\"number\":7}]",
			"<https://api.github.com/repos/o/r/milestones?state=all"
			"&per_page=100>; rel=\"next\"");

		GitHubSyncService service("t", g, &client);
		std::vector<Issue> issues;
		issues.push_back(one);
		SyncResult result;
		std::string fatal;

		CHECK(run, service.Sync(issues, result, fatal),
			"a self-referential Link header does not hang the sync");
		CHECK(run, client.log.size() == 2,
			"a Link header pointing at the current page ends pagination");
		CHECK(run, client.log[1].find("\"milestone\" : 7") != std::string::npos,
			"page 1 milestones still resolve");
	}

	// --- A rel=next link with no next relation is ignored ---------------------
	{
		GitHubIntegration g;
		g.owner = "o";
		g.repository = "r";

		Issue one;
		one.number = 1;
		one.milestone = std::string("v1.0");

		StubClient client;
		client.AddReply("milestones", "[{\"title\":\"v1.0\",\"number\":5}]",
			"<https://api.github.com/repos/o/r/milestones?page=9>; rel=\"last\"");

		GitHubSyncService service("t", g, &client);
		std::vector<Issue> issues;
		issues.push_back(one);
		SyncResult result;
		std::string fatal;
		service.Sync(issues, result, fatal);

		CHECK(run, client.log.size() == 2,
			"a Link header with only rel=last fetches no further page");
	}

	return run.Report("GitHub sync");
}

} // namespace tests
} // namespace issueskit
