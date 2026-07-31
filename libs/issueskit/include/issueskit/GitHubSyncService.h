/*
 * GitHubSyncService.h -- pushes local issues to GitHub.
 *
 * PUSH ONLY. There is no pull, no fetch-and-diff and no conflict resolution,
 * because the Apple app has none: lastSyncedAt and remoteUpdatedAt are recorded
 * on every push as metadata, and nothing reads them back. Building two-way sync
 * would be new behaviour, not a port.
 *
 * Per-issue failures are accumulated into SyncResult and the loop continues.
 * The single exception is HTTP 401: a bad or expired token fails every
 * subsequent request identically, so it aborts the whole sync.
 */
#ifndef ISSUESKIT_GITHUB_SYNC_SERVICE_H
#define ISSUESKIT_GITHUB_SYNC_SERVICE_H

#include <map>
#include <string>
#include <vector>

#include <issueskit/HttpClient.h>
#include <issueskit/IssueModel.h>

namespace issueskit {

struct SyncResult {
					SyncResult();

	int							created;
	int							updated;
	int							failed;
	//! One entry per failed issue, formatted "#NNN: <message>".
	std::vector<std::string>	errors;
};


class GitHubSyncService {
public:
	/*!	\param token The personal access token, held only for this call's
			lifetime.
		\param integration The document's GitHub coordinates.
		\param client Borrowed, not owned; must outlive this service.
	*/
								GitHubSyncService(const std::string& token,
									const GitHubIntegration&
										integration,
									HttpClient* client);

	/*!	Pushes every issue in \a issues, updating them in place.

		\param issues Rewritten with refreshed remote links; the caller replaces
			the document's issue array with it wholesale.
		\param outResult The created/updated/failed summary.
		\param outFatalError Set only when the whole sync aborted.
		\return false when the sync aborted (bad token, unusable repository).
	*/
			bool				Sync(std::vector<Issue>& issues,
									SyncResult& outResult,
									std::string& outFatalError);

private:
			struct RemoteIssue {
								RemoteIssue();

				int				number;
				std::string		htmlUrl;		//!< empty when absent
				bool			hasUpdatedAt;
				Timestamp updatedAt;
			};

			enum RequestOutcome {
				kRequestOK,
				kRequestFailed,		//!< per-issue failure, keep going
				kRequestUnauthorized //!< abort the whole sync
			};

			RequestOutcome		_Send(const std::string& method,
									const std::string& path,
									const std::string& query,
									const std::string& body,
									std::string& outResponse,
									std::string& outError);

			/*!	Performs a request against an already-built absolute URL.

				Pagination needs this: GitHub's `Link:` header hands back whole
				URLs, which must be followed verbatim rather than rebuilt.
			*/
			RequestOutcome		_Perform(const std::string& method,
									const std::string& url,
									const std::string& body,
									std::string& outResponse,
									std::string& outLinkHeader,
									std::string& outError);

			bool				_BuildUrl(const std::string& path,
									const std::string& query,
									std::string& outUrl);

			RequestOutcome		_CreateIssue(const Issue& issue,
									RemoteIssue& outRemote,
									std::string& outError);
			RequestOutcome		_UpdateIssue(const Issue& issue,
									int number, RemoteIssue& outRemote,
									bool& outHasRemote, std::string& outError);
			RequestOutcome		_FetchMilestonesIfNeeded(
									const std::vector<Issue>& issues,
									std::string& outError);

			std::string			_Payload(const Issue& issue,
									bool includeState) const;
			std::string			_IssueTitle(
									const Issue& issue) const;
			std::string			_IssueBody(const Issue& issue) const;
			std::vector<std::string> _Merged(
									const std::vector<std::string>& own,
									const std::vector<std::string>& defaults)
									const;
			void				_Record(const RemoteIssue& remote,
									bool hasRemote,
									RemoteLink& link) const;
	static	bool				_ParseRemoteIssue(const std::string& json,
									RemoteIssue& outRemote);

			std::string			fToken;
			GitHubIntegration fIntegration;
			HttpClient*			fClient;
			std::map<std::string, int> fMilestoneNumbers;
};

} // namespace issueskit

#endif // ISSUESKIT_GITHUB_SYNC_SERVICE_H
