/*
 * SecretTokenStore.h -- issueskit::TokenStore backed by libsecret.
 *
 * libsecret is GNOME's analogue of Apple's Keychain, Haiku's BKeyStore and
 * KDE's KWallet. Only the backend differs between the ports; the account key
 * derivation is shared, in issueskit::TokenStore::AccountFor(), and must never
 * be reimplemented here -- a token saved by the Haiku or KDE app for the same
 * repository has to be found by this one.
 *
 * Owner and repository are NOT stored here: they are per-project coordinates
 * that live in the .issues document and are edited in Project Settings. Only
 * the token lives in the keyring, and it is never written into a document,
 * because those files are committed to project repositories.
 *
 * THREADING. Every method below is a *_sync call onto the D-Bus secret service.
 * That can block -- for as long as it takes the user to answer a keyring unlock
 * prompt. Load() is only ever called from the sync worker thread. Save(),
 * Remove() and HasToken() are called from the main loop in response to a button
 * press or a dialog opening; see the risk list in README.md.
 */
#ifndef IHAVEISSUES_SECRET_TOKEN_STORE_H
#define IHAVEISSUES_SECRET_TOKEN_STORE_H

#include <string>

#include <issueskit/TokenStore.h>

namespace ihaveissues {

class SecretTokenStore : public issueskit::TokenStore {
public:
	virtual						~SecretTokenStore();

	//! Upserts the token for \a account. Returns false if the store refused it.
	virtual	bool				Save(const std::string& account,
									const std::string& token);

	//! Reads the token back, or returns false when none is stored.
	virtual	bool				Load(const std::string& account,
									std::string& outToken);

	/*!	Whether a token exists for \a account.

		libsecret's simple password API has no existence-only query, so this
		looks the secret up and frees it immediately. The value never leaves
		this class, which is what the TokenStore contract requires.
	*/
	virtual	bool				HasToken(const std::string& account);

	virtual	bool				Remove(const std::string& account);
};

} // namespace ihaveissues

#endif // IHAVEISSUES_SECRET_TOKEN_STORE_H
