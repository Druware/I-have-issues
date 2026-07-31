/*
 * KeyStoreTokenStore.h -- issueskit::TokenStore backed by Haiku's BKeyStore.
 *
 * BKeyStore is Haiku's analogue of Apple's Keychain, GNOME's libsecret and KDE's
 * KWallet. Only the backend differs between the three ports; the account key
 * derivation is shared, in issueskit::TokenStore::AccountFor().
 *
 * Owner and repository are NOT stored here: they are per-project coordinates
 * that live in the .issues document and are edited in Project Settings. Only the
 * token lives in the key store, and it is never written into a document, because
 * those files are committed to project repositories.
 */
#ifndef IHAVEISSUES_KEY_STORE_TOKEN_STORE_H
#define IHAVEISSUES_KEY_STORE_TOKEN_STORE_H

#include <string>

#include <issueskit/TokenStore.h>

namespace ihaveissues {

class KeyStoreTokenStore : public issueskit::TokenStore {
public:
	virtual						~KeyStoreTokenStore();

	//! Upserts the token for \a account. Returns false if the store refused it.
	virtual	bool				Save(const std::string& account,
									const std::string& token);

	//! Reads the token back, or returns false when none is stored.
	virtual	bool				Load(const std::string& account,
									std::string& outToken);

	/*!	Whether a token exists for \a account.

		BKeyStore has no existence-only query, so this reads the token and
		discards it internally. The value never leaves this class, which is what
		the TokenStore contract requires.
	*/
	virtual	bool				HasToken(const std::string& account);

	virtual	bool				Remove(const std::string& account);
};

} // namespace ihaveissues

#endif // IHAVEISSUES_KEY_STORE_TOKEN_STORE_H
