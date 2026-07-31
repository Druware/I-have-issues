/*
 * SecretTokenStore.cpp
 *
 * VERIFY (whole file): libsecret's simple password API. Declared in
 * <libsecret/secret.h>, linked with libsecret-1. None of this has ever been
 * compiled. Check:
 *   - that SecretSchema is a plain struct initialised as below, with the
 *     attribute array terminated by a { NULL, ... } entry;
 *   - secret_password_store_sync(const SecretSchema*, const gchar* collection,
 *       const gchar* label, const gchar* password, GCancellable*, GError**,
 *       ...attribute name/value pairs..., NULL);
 *   - secret_password_lookup_sync(const SecretSchema*, GCancellable*, GError**,
 *       ..., NULL) returning a gchar* the caller frees;
 *   - secret_password_clear_sync(const SecretSchema*, GCancellable*, GError**,
 *       ..., NULL) returning TRUE only when something was actually removed;
 *   - SECRET_COLLECTION_DEFAULT being the right collection constant;
 *   - secret_password_free() -- some builds also have secret_password_wipe(),
 *     which zeroes the memory first and would be preferable if present;
 *   - that <libsecret/secret.h> compiles without #define SECRET_API_SUBJECT_TO_CHANGE.
 *     Only the SecretService/SecretItem "complete" API ever needed that guard,
 *     and nothing here uses it, but old libsecret releases applied it more
 *     broadly. If the build complains, define it above the include.
 *
 * The SecretSchema aggregate below assumes the documented struct layout:
 *   const gchar *name; SecretSchemaFlags flags; SecretSchemaAttribute[32];
 *   then one gint and seven gpointer reserved fields.
 * VERIFY that field count -- a short initialiser list is legal C++ and would
 * compile against a different layout while silently mis-assigning.
 *
 * The FIRST call may raise a modal keyring-unlock prompt from the secret
 * service. That is a system dialog outside this process, but it blocks the
 * *_sync call until it is answered.
 */
#include "SecretTokenStore.h"

#include <glib.h>
#include <libsecret/secret.h>

namespace ihaveissues {

namespace {

/*!	The Apple app's kSecAttrService value, reused verbatim.

	Haiku uses the same string as its BKeyStore identifier. Keeping it identical
	everywhere means a keyring entry is recognisable as this app's on any
	desktop, and it pairs with the shared account key from
	issueskit::TokenStore::AccountFor().
*/
const char* kServiceIdentifier = "IHaveIssues-GitHubToken";

/*!	The schema the two attributes are stored under.

	SECRET_SCHEMA_NONE means the schema name is matched as an attribute like any
	other, which is what keeps entries written by this app distinct from
	everything else in the user's default keyring.
*/
const SecretSchema kTokenSchema = {
	"com.druware.IHaveIssues.Token",
	SECRET_SCHEMA_NONE,
	{
		{ "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
		{ "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
		{ NULL, (SecretSchemaAttributeType)0 }
	},
	// The trailing reserved fields of SecretSchema. They exist so the struct
	// can grow without breaking ABI and must be zero.
	0, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

} // unnamed namespace


SecretTokenStore::~SecretTokenStore()
{
}


bool
SecretTokenStore::Save(const std::string& account, const std::string& token)
{
	if (account.empty() || token.empty())
		return false;

	// The label is what a keyring manager shows the user. It names the
	// repository so several entries are told apart at a glance.
	gchar* label = g_strdup_printf("I Have Issues - GitHub token for %s",
		account.c_str());

	GError* error = NULL;
	// store_sync replaces any existing secret carrying the same attributes, so
	// this is an upsert and needs no delete-then-add pair.
	gboolean stored = secret_password_store_sync(&kTokenSchema,
		SECRET_COLLECTION_DEFAULT, label, token.c_str(), NULL, &error,
		"service", kServiceIdentifier,
		"account", account.c_str(),
		NULL);

	g_free(label);
	if (error != NULL) {
		g_warning("I Have Issues: storing the GitHub token failed: %s",
			error->message);
		g_clear_error(&error);
		return false;
	}
	return stored != FALSE;
}


bool
SecretTokenStore::Load(const std::string& account, std::string& outToken)
{
	if (account.empty())
		return false;

	GError* error = NULL;
	gchar* password = secret_password_lookup_sync(&kTokenSchema, NULL, &error,
		"service", kServiceIdentifier,
		"account", account.c_str(),
		NULL);

	if (error != NULL) {
		g_warning("I Have Issues: reading the GitHub token failed: %s",
			error->message);
		g_clear_error(&error);
		return false;
	}
	if (password == NULL)
		return false;

	outToken = password;
	secret_password_free(password);
	return !outToken.empty();
}


bool
SecretTokenStore::HasToken(const std::string& account)
{
	if (account.empty())
		return false;

	// CONTRACT (issueskit::TokenStore): answer without exposing the secret.
	// libsecret's simple API has no existence-only query -- the alternative is
	// secret_service_search_sync() with SECRET_SEARCH_NONE, which returns
	// SecretItems whose values are not loaded -- so this reads the password and
	// frees it here. It is never returned, never copied and never reaches view
	// state. Haiku's BKeyStore backend has the identical limitation.
	GError* error = NULL;
	gchar* password = secret_password_lookup_sync(&kTokenSchema, NULL, &error,
		"service", kServiceIdentifier,
		"account", account.c_str(),
		NULL);

	if (error != NULL) {
		g_warning("I Have Issues: checking for a GitHub token failed: %s",
			error->message);
		g_clear_error(&error);
		return false;
	}
	if (password == NULL)
		return false;

	bool present = *password != '\0';
	secret_password_free(password);
	return present;
}


bool
SecretTokenStore::Remove(const std::string& account)
{
	if (account.empty())
		return false;

	GError* error = NULL;
	gboolean removed = secret_password_clear_sync(&kTokenSchema, NULL, &error,
		"service", kServiceIdentifier,
		"account", account.c_str(),
		NULL);

	if (error != NULL) {
		g_warning("I Have Issues: removing the GitHub token failed: %s",
			error->message);
		g_clear_error(&error);
		return false;
	}
	return removed != FALSE;
}

} // namespace ihaveissues
