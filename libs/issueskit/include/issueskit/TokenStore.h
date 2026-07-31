/*
 * TokenStore.h -- where a GitHub personal access token lives.
 *
 * The token is NEVER part of a .issues document. Those files are committed to
 * project repositories, so a token stored there would be published to everyone
 * with read access, to every fork, and to every CI log. It belongs in whatever
 * secret store the host platform provides:
 *
 *     Haiku   BKeyStore          apps/Haiku/src/github/KeyStoreTokenStore
 *     GNOME   libsecret          (to be supplied by the GNOME app)
 *     KDE     KWallet            (to be supplied by the KDE app)
 *
 * Three real backends is why this interface exists; it is not speculative
 * generality. What is shared is the part that must not diverge between them:
 * the account key derivation in AccountFor(), which decides WHICH token is sent
 * to WHICH repository.
 *
 * This header is part of the portable library and must stay free of any
 * platform toolkit.
 */
#ifndef ISSUESKIT_TOKEN_STORE_H
#define ISSUESKIT_TOKEN_STORE_H

#include <optional>
#include <string>

#include <issueskit/IssueModel.h>

namespace issueskit {

class TokenStore {
public:
	virtual						~TokenStore() {}

	/*!	Stores \a token for \a account, replacing any existing entry.

		\return false when the store refused it.
	*/
	virtual	bool				Save(const std::string& account,
									const std::string& token) = 0;

	/*!	Reads the token back.

		\return false when no token is stored for \a account.
	*/
	virtual	bool				Load(const std::string& account,
									std::string& outToken) = 0;

	/*!	Whether a token exists for \a account.

		CONTRACT: implementations must answer this WITHOUT exposing the secret to
		the caller. The sync UI only needs to know an entry exists so it can
		offer to replace or remove it; it must never hold the token to answer
		that question.

		A backend whose API cannot test for existence without decrypting (Haiku's
		BKeyStore is one) may read and immediately discard the value internally,
		but the secret must not leave the implementation.
	*/
	virtual	bool				HasToken(const std::string& account) = 0;

	//! Deletes the entry for \a account. Returns false when there was none.
	virtual	bool				Remove(const std::string& account) = 0;

	/*!	The account key that scopes a token to one repository.

		"<owner>/<repository>", trimmed and lowercased, so the same repository
		always resolves to the same entry. A token issued for one repository must
		never be sent to another, which is the whole reason tokens are keyed
		rather than stored globally.

		Returns an empty string, meaning "no token operation is possible", when:

		  - the integration is absent;
		  - either coordinate is blank after trimming -- there is then no
		    repository to scope to, so there is nothing to save, load or delete;
		  - either coordinate contains a '/'. A slash would make the key
		    ambiguous, since owner "a/b" with repository "c" and owner "a" with
		    repository "b/c" would otherwise both derive "a/b/c", letting a token
		    saved for one repository reach a different one. No legal GitHub name
		    contains a slash, so nothing valid is rejected.

		Callers must treat an empty account as a refusal, never as a usable key.

		Shared deliberately: every platform must derive the identical key, or a
		token saved on one desktop would be invisible to another reading the same
		document.
	*/
	static	std::string			AccountFor(
									const std::optional<GitHubIntegration>&
										integration);
};

} // namespace issueskit

#endif // ISSUESKIT_TOKEN_STORE_H
