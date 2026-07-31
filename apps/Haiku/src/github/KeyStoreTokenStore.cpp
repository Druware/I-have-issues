/*
 * KeyStoreTokenStore.cpp
 *
 * VERIFY (whole file): BKeyStore is Haiku's system key store, backed by
 * keystore_server. It is declared in <KeyStore.h> with BKey / BPasswordKey in
 * <Key.h>, and both live in libbe.so -- no extra library is needed. None of this
 * has been compiled. Check:
 *   - the header names and that they are in the default include path;
 *   - that BPasswordKey's constructor is
 *       BPasswordKey(const char* password, BKeyPurpose, const char* identifier,
 *                    const char* secondaryIdentifier = NULL);
 *   - that BKeyStore::GetKey(BKeyType, const char* identifier,
 *       const char* secondaryIdentifier, bool secondaryIdentifierOptional,
 *       BKey&) exists in the target release;
 *   - the first call may prompt the user to unlock the master keyring. That
 *     prompt is a modal system dialog, which is exactly why every call here
 *     happens on a worker thread, never on a BLooper's message thread.
 */
#include "KeyStoreTokenStore.h"

#include <Key.h>
#include <KeyStore.h>
#include <SupportDefs.h>

namespace ihaveissues {

namespace {

//! The Apple app's kSecAttrService value, reused verbatim for cross-readability.
const char* kServiceIdentifier = "IHaveIssues-GitHubToken";

} // unnamed namespace


KeyStoreTokenStore::~KeyStoreTokenStore()
{
}


bool
KeyStoreTokenStore::Save(const std::string& account, const std::string& token)
{
	if (account.empty() || token.empty())
		return false;

	// Upsert: remove any existing entry first, exactly as the Apple app does
	// with SecItemDelete followed by SecItemAdd.
	Remove(account);

	BKeyStore keyStore;
	BPasswordKey key(token.c_str(), B_KEY_PURPOSE_GENERIC, kServiceIdentifier,
		account.c_str());
	return keyStore.AddKey(key) == B_OK;
}


bool
KeyStoreTokenStore::Load(const std::string& account, std::string& outToken)
{
	if (account.empty())
		return false;

	BKeyStore keyStore;
	BPasswordKey key;
	// secondaryIdentifierOptional = false: an entry saved for a different
	// repository must never satisfy this lookup.
	status_t status = keyStore.GetKey(B_KEY_TYPE_PASSWORD, kServiceIdentifier,
		account.c_str(), false, key);
	if (status != B_OK)
		return false;

	const char* password = key.Password();
	if (password == NULL)
		return false;

	outToken = password;
	return !outToken.empty();
}


bool
KeyStoreTokenStore::HasToken(const std::string& account)
{
	// DEVIATION from the Apple app: GitHubKeychain.hasToken(for:) can ask the
	// Keychain whether an item exists without reading its data. BKeyStore has no
	// equivalent existence query -- every accessor returns the key itself -- so
	// this reads the token and immediately drops it. The value never leaves this
	// function and is never stored in view state.
	std::string token;
	return Load(account, token);
}


bool
KeyStoreTokenStore::Remove(const std::string& account)
{
	if (account.empty())
		return false;

	BKeyStore keyStore;
	BPasswordKey key;
	if (keyStore.GetKey(B_KEY_TYPE_PASSWORD, kServiceIdentifier,
			account.c_str(), false, key) != B_OK) {
		return false;
	}
	return keyStore.RemoveKey(key) == B_OK;
}

} // namespace ihaveissues
