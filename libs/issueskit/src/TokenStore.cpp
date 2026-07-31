/*
 * TokenStore.cpp
 */
#include <issueskit/TokenStore.h>

#include <issueskit/StringUtils.h>

namespace issueskit {

std::string
TokenStore::AccountFor(const std::optional<GitHubIntegration>& integration)
{
	if (!integration.has_value())
		return std::string();

	std::string owner = ToLower(Trim(integration->owner));
	std::string repository = ToLower(Trim(integration->repository));
	if (owner.empty() || repository.empty())
		return std::string();

	// A slash in either coordinate would make the key ambiguous: owner "a/b"
	// with repository "c", and owner "a" with repository "b/c", would both
	// derive "a/b/c" -- so a token saved for one repository could be handed to a
	// different one. That is precisely what keying exists to prevent.
	//
	// Neither spelling is a legal GitHub name, so nothing valid is rejected.
	// GitHubSyncService::_BuildUrl already percent-encodes '/' out of every URL
	// path segment for the same reason; this is the matching guard for the key.
	if (owner.find('/') != std::string::npos
		|| repository.find('/') != std::string::npos) {
		return std::string();
	}

	return owner + "/" + repository;
}

} // namespace issueskit
