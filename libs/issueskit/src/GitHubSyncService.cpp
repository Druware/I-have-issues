/*
 * GitHubSyncService.cpp
 */
#include <issueskit/GitHubSyncService.h>

#include <cstdio>

#include <issueskit/IssueDate.h>
#include <issueskit/JsonParser.h>
#include <issueskit/JsonValue.h>
#include <issueskit/StringUtils.h>

namespace issueskit {

namespace {

/*!	The product identifier sent as User-Agent on every request.

	GitHub REQUIRES a User-Agent and answers 403 without one, so this is not
	decoration. GitHub asks for an application name; keep it a stable product
	identifier and change it only alongside a real release, since it is what
	appears in a repository owner's audit log.

	Defined once, here, so no request can be built without it and no port can
	drift to a different value.
*/
const char* const kUserAgent = "IHaveIssues/1.0";


/*!	Percent-encodes one URL path segment.

	The allowed set is alphanumerics plus "-._~" and DELIBERATELY excludes '/',
	so a stray slash typed into Owner or Repository cannot invent extra path
	components.
*/
std::string
EncodePathSegment(const std::string& segment)
{
	static const char* kHexDigits = "0123456789ABCDEF";
	std::string encoded;
	for (size_t i = 0; i < segment.size(); i++) {
		unsigned char c = (unsigned char)segment[i];
		bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_'
			|| c == '~';
		if (safe) {
			encoded += (char)c;
		} else {
			encoded += '%';
			encoded += kHexDigits[(c >> 4) & 0x0f];
			encoded += kHexDigits[c & 0x0f];
		}
	}
	return encoded;
}


/*!	Extracts the `rel="next"` URL from an RFC 8288 Link header.

	GitHub sends, on one line:

	    <https://api.github.com/...?page=2>; rel="next",
	    <https://api.github.com/...?page=9>; rel="last"

	Parsed structurally rather than by splitting on commas, because a URL may
	legally contain a comma. Each entry is `<URI>` followed by `;`-separated
	parameters, and `rel` may be quoted or bare and may carry several
	space-separated relation types.

	\return true when a next link was found.
*/
bool
NextPageUrl(const std::string& linkHeader, std::string& outUrl)
{
	size_t cursor = 0;
	while (cursor < linkHeader.size()) {
		// Find this entry's URI, delimited by angle brackets.
		size_t open = linkHeader.find('<', cursor);
		if (open == std::string::npos)
			return false;
		size_t close = linkHeader.find('>', open + 1);
		if (close == std::string::npos)
			return false;

		std::string url = Trim(linkHeader.substr(open + 1, close - open - 1));

		// Parameters run to the next top-level comma, which -- now that the URI
		// is consumed -- cannot be inside a URL any more.
		size_t paramsEnd = linkHeader.find(',', close + 1);
		if (paramsEnd == std::string::npos)
			paramsEnd = linkHeader.size();

		std::string params = linkHeader.substr(close + 1,
			paramsEnd - close - 1);
		std::vector<std::string> parts = Split(params, ';');
		for (size_t i = 0; i < parts.size(); i++) {
			std::string part = Trim(parts[i]);
			size_t equals = part.find('=');
			if (equals == std::string::npos)
				continue;
			if (ToLower(Trim(part.substr(0, equals))) != "rel")
				continue;

			std::string value = Trim(part.substr(equals + 1));
			if (value.size() >= 2 && value[0] == '"'
				&& value[value.size() - 1] == '"') {
				value = value.substr(1, value.size() - 2);
			}
			// rel may list several space-separated relation types.
			std::vector<std::string> relations = Split(value, ' ');
			for (size_t j = 0; j < relations.size(); j++) {
				if (ToLower(Trim(relations[j])) == "next" && !url.empty()) {
					outUrl = url;
					return true;
				}
			}
		}
		cursor = paramsEnd + 1;
	}
	return false;
}


//! Pulls GitHub's "message" field out of an error response, if there is one.
std::string
ApiErrorMessage(const std::string& json, int statusCode)
{
	JsonValue root;
	std::string parseError;
	if (JsonParser::Parse(json, root, parseError) && root.IsObject()) {
		const JsonValue* message = root.Find("message");
		if (message != NULL && message->IsString())
			return message->StringValue();
	}
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "HTTP %d", statusCode);
	return std::string(buffer);
}

} // unnamed namespace


SyncResult::SyncResult()
	:
	created(0),
	updated(0),
	failed(0)
{
}


GitHubSyncService::RemoteIssue::RemoteIssue()
	:
	number(0),
	hasUpdatedAt(false),
	updatedAt(0)
{
}


GitHubSyncService::GitHubSyncService(const std::string& token,
	const GitHubIntegration& integration, HttpClient* client)
	:
	fToken(token),
	fIntegration(integration),
	fClient(client)
{
}


// #pragma mark - Requests

bool
GitHubSyncService::_BuildUrl(const std::string& path, const std::string& query,
	std::string& outUrl)
{
	if (fIntegration.owner.empty() || fIntegration.repository.empty())
		return false;

	std::vector<std::string> segments;
	segments.push_back("repos");
	segments.push_back(fIntegration.owner);
	segments.push_back(fIntegration.repository);

	std::vector<std::string> extra = Split(path, '/');
	for (size_t i = 0; i < extra.size(); i++) {
		if (!extra[i].empty())
			segments.push_back(extra[i]);
	}

	std::string url = "https://api.github.com";
	for (size_t i = 0; i < segments.size(); i++) {
		if (segments[i].empty())
			return false;
		url += "/" + EncodePathSegment(segments[i]);
	}
	if (!query.empty())
		url += "?" + query;

	outUrl = url;
	return true;
}


GitHubSyncService::RequestOutcome
GitHubSyncService::_Send(const std::string& method, const std::string& path,
	const std::string& query, const std::string& body,
	std::string& outResponse, std::string& outError)
{
	std::string url;
	if (!_BuildUrl(path, query, url)) {
		outError = "The GitHub owner and repository in Project Settings do not "
			"form a valid URL.";
		return kRequestFailed;
	}

	std::string ignoredLink;
	return _Perform(method, url, body, outResponse, ignoredLink, outError);
}


GitHubSyncService::RequestOutcome
GitHubSyncService::_Perform(const std::string& method, const std::string& url,
	const std::string& body, std::string& outResponse,
	std::string& outLinkHeader, std::string& outError)
{
	std::vector<HttpHeader> headers;
	HttpHeader header;
	header.name = "Authorization";
	header.value = "Bearer " + fToken;
	headers.push_back(header);
	header.name = "Accept";
	header.value = "application/vnd.github+json";
	headers.push_back(header);
	header.name = "X-GitHub-Api-Version";
	header.value = "2022-11-28";
	headers.push_back(header);
	header.name = "Content-Type";
	header.value = "application/json";
	headers.push_back(header);
	// GitHub REJECTS requests with no User-Agent (403), so it is set here rather
	// than left to each transport. This service is the only place that knows it
	// is talking to GitHub; a transport's job is to move bytes, not to know one
	// API's rules. Setting it here also means every port sends the identical
	// value, and a transport that sets its own is overridden -- which is the
	// precedence we want.
	header.name = "User-Agent";
	header.value = kUserAgent;
	headers.push_back(header);

	HttpResponse response = fClient->Perform(method, url, headers, body);
	if (!response.transportSucceeded) {
		outError = response.transportError;
		return kRequestFailed;
	}
	if (response.statusCode == 401) {
		outError = "Invalid or expired GitHub token. Check your personal "
			"access token.";
		return kRequestUnauthorized;
	}
	if (response.statusCode < 200 || response.statusCode >= 300) {
		outError = ApiErrorMessage(response.body, response.statusCode);
		return kRequestFailed;
	}

	outResponse = response.body;
	outLinkHeader = response.HeaderValue("Link");
	return kRequestOK;
}


bool
GitHubSyncService::_ParseRemoteIssue(const std::string& json,
	RemoteIssue& outRemote)
{
	JsonValue root;
	std::string parseError;
	if (!JsonParser::Parse(json, root, parseError) || !root.IsObject())
		return false;

	const JsonValue* number = root.Find("number");
	if (number == NULL || !number->IsNumber())
		return false;
	outRemote.number = (int)number->IntegerValue();

	const JsonValue* htmlUrl = root.Find("html_url");
	if (htmlUrl != NULL && htmlUrl->IsString())
		outRemote.htmlUrl = htmlUrl->StringValue();

	const JsonValue* updatedAt = root.Find("updated_at");
	if (updatedAt != NULL && updatedAt->IsString()) {
		Timestamp parsed = 0;
		if (IssueDate::ParseISO8601(updatedAt->StringValue(), parsed)) {
			outRemote.hasUpdatedAt = true;
			outRemote.updatedAt = parsed;
		}
	}
	return true;
}


GitHubSyncService::RequestOutcome
GitHubSyncService::_CreateIssue(const Issue& issue, RemoteIssue& outRemote,
	std::string& outError)
{
	std::string response;
	// GitHub issues are always created open: the create endpoint has no `state`.
	RequestOutcome outcome = _Send("POST", "issues", "",
		_Payload(issue, false), response, outError);
	if (outcome != kRequestOK)
		return outcome;

	if (!_ParseRemoteIssue(response, outRemote)) {
		outError = "Unexpected response from GitHub API.";
		return kRequestFailed;
	}
	return kRequestOK;
}


GitHubSyncService::RequestOutcome
GitHubSyncService::_UpdateIssue(const Issue& issue, int number,
	RemoteIssue& outRemote, bool& outHasRemote, std::string& outError)
{
	outHasRemote = false;

	char path[64];
	snprintf(path, sizeof(path), "issues/%d", number);

	std::string response;
	RequestOutcome outcome = _Send("PATCH", path, "", _Payload(issue, true),
		response, outError);
	if (outcome != kRequestOK)
		return outcome;

	// A terse reply is not an error: _Record only overwrites what came back.
	outHasRemote = _ParseRemoteIssue(response, outRemote);
	return kRequestOK;
}


GitHubSyncService::RequestOutcome
GitHubSyncService::_FetchMilestonesIfNeeded(const std::vector<Issue>& issues,
	std::string& outError)
{
	bool isNamed = fIntegration.defaultMilestone.has_value()
		&& !fIntegration.defaultMilestone->empty();
	for (size_t i = 0; i < issues.size() && !isNamed; i++) {
		if (issues[i].milestone.has_value() && !issues[i].milestone->empty())
			isNamed = true;
	}
	if (!isNamed)
		return kRequestOK;

	std::string url;
	if (!_BuildUrl("milestones", "state=all&per_page=100", url)) {
		outError = "The GitHub owner and repository in Project Settings do not "
			"form a valid URL.";
		return kRequestFailed;
	}

	// Every page is followed via the Link: rel="next" header, so a repository
	// with more than 100 milestones still resolves the later ones. (The Apple
	// app stops after the first page -- its own known issue #4. The Haiku and
	// Android clients both paginate, so all three resolve the same names.)
	//
	// The page cap and the repeat-URL check exist only so a malformed or
	// self-referential Link header cannot spin this loop forever.
	const int kMaxPages = 100;
	std::string previousUrl;

	for (int page = 0; page < kMaxPages; page++) {
		std::string response;
		std::string linkHeader;
		RequestOutcome outcome = _Perform("GET", url, "", response, linkHeader,
			outError);
		if (outcome != kRequestOK)
			return outcome;

		JsonValue root;
		std::string parseError;
		if (!JsonParser::Parse(response, root, parseError) || !root.IsArray()) {
			// A malformed page is treated as "no more milestones" rather than a
			// sync failure, matching how the single-page version behaved.
			return kRequestOK;
		}

		for (size_t i = 0; i < root.CountItems(); i++) {
			const JsonValue& item = root.ItemAt(i);
			if (!item.IsObject())
				continue;
			const JsonValue* title = item.Find("title");
			const JsonValue* number = item.Find("number");
			if (title != NULL && title->IsString() && number != NULL
				&& number->IsNumber()) {
				// First page wins on a duplicate title, matching GitHub's own
				// ordering guarantees.
				if (fMilestoneNumbers.find(title->StringValue())
					== fMilestoneNumbers.end()) {
					fMilestoneNumbers[title->StringValue()]
						= (int)number->IntegerValue();
				}
			}
		}

		previousUrl = url;
		std::string nextUrl;
		if (!NextPageUrl(linkHeader, nextUrl))
			break;
		if (nextUrl == previousUrl)
			break;
		url = nextUrl;
	}
	return kRequestOK;
}


// #pragma mark - Payload

std::vector<std::string>
GitHubSyncService::_Merged(const std::vector<std::string>& own,
	const std::vector<std::string>& defaults) const
{
	// The issue's own values first, then any document default it does not
	// already carry.
	std::vector<std::string> merged = own;
	for (size_t i = 0; i < defaults.size(); i++) {
		bool present = false;
		for (size_t j = 0; j < own.size(); j++) {
			if (own[j] == defaults[i]) {
				present = true;
				break;
			}
		}
		if (!present)
			merged.push_back(defaults[i]);
	}
	return merged;
}


std::string
GitHubSyncService::_IssueTitle(const Issue& issue) const
{
	return issue.title.empty() ? std::string("Untitled Issue") : issue.title;
}


std::string
GitHubSyncService::_IssueBody(const Issue& issue) const
{
	std::vector<std::string> parts;

	parts.push_back(std::string("**Type:** ") + IssueTypeDisplayName(issue.type)
		+ " | **Priority:** " + IssuePriorityDisplayName(issue.priority)
		+ " | **Status:** " + IssueStatusDisplayName(issue.status));

	// U+00B7 MIDDLE DOT, as UTF-8, matching the Apple body exactly.
	std::string dateLine = "**Reported:** " + IssueDate::ToString(issue.reported);
	if (!issue.reportedBy.empty())
		dateLine += " \xC2\xB7 " + issue.reportedBy;
	if (!issue.area.empty())
		dateLine += " | **Area:** " + issue.area;
	parts.push_back(dateLine);

	if (!issue.description.empty())
		parts.push_back("---\n**Description**\n\n" + issue.description);

	if (!issue.stepsToReproduce.empty()) {
		std::vector<std::string> numbered;
		for (size_t i = 0; i < issue.stepsToReproduce.size(); i++) {
			char prefix[32];
			snprintf(prefix, sizeof(prefix), "%lu. ", (unsigned long)(i + 1));
			numbered.push_back(std::string(prefix) + issue.stepsToReproduce[i]);
		}
		parts.push_back("**Steps to Reproduce**\n\n" + Join(numbered, "\n"));
	}

	if (!issue.notes.empty())
		parts.push_back("**Notes / Investigation**\n\n" + issue.notes);
	if (!issue.resolution.empty())
		parts.push_back("**Resolution**\n\n" + issue.resolution);

	parts.push_back("---\n*Synced from local issue tracker \xC2\xB7 "
		+ issue.DisplayNumber() + "*");
	return Join(parts, "\n\n");
}


std::string
GitHubSyncService::_Payload(const Issue& issue, bool includeState) const
{
	JsonValue body = JsonValue::Object();
	body.Set("title", JsonValue::String(_IssueTitle(issue)));
	body.Set("body", JsonValue::String(_IssueBody(issue)));

	std::vector<std::string> labels = _Merged(issue.labels,
		fIntegration.defaultLabels);
	if (!labels.empty()) {
		JsonValue array = JsonValue::Array();
		for (size_t i = 0; i < labels.size(); i++)
			array.Append(JsonValue::String(labels[i]));
		body.Set("labels", array);
	}

	std::vector<std::string> assignees = _Merged(issue.assignees,
		fIntegration.defaultAssignees);
	if (!assignees.empty()) {
		JsonValue array = JsonValue::Array();
		for (size_t i = 0; i < assignees.size(); i++)
			array.Append(JsonValue::String(assignees[i]));
		body.Set("assignees", array);
	}

	// GitHub identifies a milestone by number, not title, so a name with no
	// match in the repository is omitted -- sending the title would fail the
	// whole request.
	std::string milestoneName;
	if (issue.milestone.has_value())
		milestoneName = *issue.milestone;
	else if (fIntegration.defaultMilestone.has_value())
		milestoneName = *fIntegration.defaultMilestone;
	if (!milestoneName.empty()) {
		std::map<std::string, int>::const_iterator found
			= fMilestoneNumbers.find(milestoneName);
		if (found != fMilestoneNumbers.end())
			body.Set("milestone", JsonValue::Integer(found->second));
	}

	if (includeState) {
		body.Set("state",
			JsonValue::String(issue.IsResolved() ? "closed" : "open"));
	}

	// The writer's pretty printing is irrelevant on the wire; reusing it keeps
	// escaping rules in exactly one place.
	return body.Write();
}


// #pragma mark - Sync

void
GitHubSyncService::_Record(const RemoteIssue& remote, bool hasRemote,
	RemoteLink& link) const
{
	link.lastSyncedAt = IssueDate::Now();
	if (!hasRemote)
		return;
	// Only overwrite what the response actually carried, so a terse reply never
	// erases what we already knew.
	if (!remote.htmlUrl.empty())
		link.url = remote.htmlUrl;
	if (remote.hasUpdatedAt)
		link.remoteUpdatedAt = remote.updatedAt;
}


bool
GitHubSyncService::Sync(std::vector<Issue>& issues, SyncResult& outResult,
	std::string& outFatalError)
{
	outResult = SyncResult();
	fMilestoneNumbers.clear();

	std::string error;
	RequestOutcome outcome = _FetchMilestonesIfNeeded(issues, error);
	if (outcome == kRequestUnauthorized) {
		outFatalError = error;
		return false;
	}
	if (outcome == kRequestFailed) {
		// The milestone lookup is the one request made before the per-issue
		// loop, so its failure aborts: without it every issue would be pushed
		// with the wrong milestone.
		outFatalError = error;
		return false;
	}

	for (size_t i = 0; i < issues.size(); i++) {
		Issue& issue = issues[i];
		error.clear();

		// The existing link is found via IsGitHub(), never a raw-string
		// comparison: an unknown provider must never look like GitHub.
		size_t linkIndex = issue.remoteLinks.size();
		for (size_t j = 0; j < issue.remoteLinks.size(); j++) {
			if (issue.remoteLinks[j].provider.IsGitHub()) {
				linkIndex = j;
				break;
			}
		}

		if (linkIndex < issue.remoteLinks.size()) {
			const std::string& identifier
				= issue.remoteLinks[linkIndex].identifier;
			int remoteNumber = 0;
			if (!ParseInt(identifier, remoteNumber)) {
				outResult.failed++;
				outResult.errors.push_back(issue.DisplayNumber()
					+ ": The existing GitHub link \"" + identifier
					+ "\" is not an issue number.");
				continue;
			}

			RemoteIssue remote;
			bool hasRemote = false;
			outcome = _UpdateIssue(issue, remoteNumber, remote, hasRemote,
				error);
			if (outcome == kRequestUnauthorized) {
				outFatalError = error;
				return false;
			}
			if (outcome != kRequestOK) {
				outResult.failed++;
				outResult.errors.push_back(issue.DisplayNumber() + ": " + error);
				continue;
			}
			_Record(remote, hasRemote, issue.remoteLinks[linkIndex]);
			outResult.updated++;
		} else {
			RemoteIssue remote;
			outcome = _CreateIssue(issue, remote, error);
			if (outcome == kRequestUnauthorized) {
				outFatalError = error;
				return false;
			}
			if (outcome != kRequestOK) {
				outResult.failed++;
				outResult.errors.push_back(issue.DisplayNumber() + ": " + error);
				continue;
			}

			// GitHub's create endpoint has no `state` parameter, so an already
			// resolved issue needs a second request to close it. Two requests
			// on first sync is a known inefficiency, not a bug.
			if (issue.IsResolved()) {
				RemoteIssue closed;
				bool hasClosed = false;
				RequestOutcome closeOutcome = _UpdateIssue(issue, remote.number,
					closed, hasClosed, error);
				if (closeOutcome == kRequestUnauthorized) {
					outFatalError = error;
					return false;
				}
				if (closeOutcome == kRequestOK && hasClosed)
					remote = closed;
			}

			RemoteLink link;
			link.provider = RemoteProvider::GitHub();
			char identifier[32];
			snprintf(identifier, sizeof(identifier), "%d", remote.number);
			link.identifier = identifier;
			if (!remote.htmlUrl.empty())
				link.url = remote.htmlUrl;
			link.lastSyncedAt = IssueDate::Now();
			if (remote.hasUpdatedAt)
				link.remoteUpdatedAt = remote.updatedAt;
			issue.remoteLinks.push_back(link);
			outResult.created++;
		}
	}

	return true;
}

} // namespace issueskit
