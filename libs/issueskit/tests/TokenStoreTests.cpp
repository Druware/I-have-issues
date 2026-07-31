/*
 * TokenStoreTests.cpp -- issueskit::TokenStore::AccountFor().
 *
 * This function is small but load-bearing in a way the rest of the library is
 * not. Three separate desktop backends -- BKeyStore on Haiku, libsecret on
 * GNOME, KWallet on KDE -- look a token up by the key it returns. If any two of
 * them derived a different key for the same repository, a token saved on one
 * desktop would be silently invisible on another, and the user would experience
 * that as "sync forgot my token" with nothing pointing at key derivation.
 *
 * Sharing the derivation is what prevents that; these assertions are what keep
 * the shared version honest. They pin the CURRENT behaviour, including the two
 * edges flagged in the README -- they are documentation of what is, not a
 * statement that it is ideal.
 */
#include "TestSupport.h"

#include <string>
#include <vector>

#include <issueskit/IssueModel.h>
#include <issueskit/TokenStore.h>

namespace issueskit {
namespace tests {

namespace {

std::optional<GitHubIntegration>
Integration(const std::string& owner, const std::string& repository)
{
	GitHubIntegration integration;
	integration.owner = owner;
	integration.repository = repository;
	return integration;
}


std::string
Account(const std::string& owner, const std::string& repository)
{
	return TokenStore::AccountFor(Integration(owner, repository));
}

} // unnamed namespace


int
RunTokenStoreTests(TestRun& run)
{
	const std::string kCanonical = "openbcm/i-have-issues";

	// --- The happy path ------------------------------------------------------
	CHECK(run, Account("openbcm", "i-have-issues") == kCanonical,
		"an already-canonical owner and repository join with a slash");
	CHECK(run, Account("openbcm", "i-have-issues").find('/') == 7,
		"owner and repository are separated by exactly one slash");

	// --- Case folding --------------------------------------------------------
	CHECK(run, Account("OpenBCM", "i-have-issues") == kCanonical,
		"a mixed-case owner is lowercased");
	CHECK(run, Account("openbcm", "I-Have-Issues") == kCanonical,
		"a mixed-case repository is lowercased");
	CHECK(run, Account("OPENBCM", "I-HAVE-ISSUES") == kCanonical,
		"fully uppercase coordinates are lowercased");

	// --- Whitespace trimming, both fields, both ends -------------------------
	CHECK(run, Account("  openbcm", "i-have-issues") == kCanonical,
		"leading spaces on the owner are trimmed");
	CHECK(run, Account("openbcm  ", "i-have-issues") == kCanonical,
		"trailing spaces on the owner are trimmed");
	CHECK(run, Account("openbcm", "  i-have-issues") == kCanonical,
		"leading spaces on the repository are trimmed");
	CHECK(run, Account("openbcm", "i-have-issues  ") == kCanonical,
		"trailing spaces on the repository are trimmed");
	CHECK(run, Account("\topenbcm\t", "\ti-have-issues\t") == kCanonical,
		"tabs are trimmed from both fields");
	CHECK(run, Account(" \t openbcm \t ", " \t i-have-issues \t ") == kCanonical,
		"mixed runs of spaces and tabs are trimmed");

	// --- Stability: everything that should collapse to one key does ----------
	//
	// This is the assertion that actually protects the user. Any spelling a
	// person might type must reach the same stored token.
	{
		const char* owners[] = {
			"openbcm", "OpenBCM", "OPENBCM", "  openbcm  ", "\tOpenBCM\t",
			" OPENBCM "
		};
		const char* repositories[] = {
			"i-have-issues", "I-Have-Issues", "I-HAVE-ISSUES",
			"  i-have-issues  ", "\tI-Have-Issues\t", " I-HAVE-ISSUES "
		};
		bool allSame = true;
		for (size_t i = 0; i < 6; i++) {
			for (size_t j = 0; j < 6; j++) {
				if (Account(owners[i], repositories[j]) != kCanonical)
					allSame = false;
			}
		}
		CHECK(run, allSame,
			"all 36 case and padding spellings derive the identical key");
	}

	CHECK(run, Account("OpenBCM", "I-Have-Issues")
			== Account("  openbcm  ", "\ti-have-issues\t"),
		"two differently-spelled inputs agree with each other, not just with a literal");

	// --- Determinism ---------------------------------------------------------
	CHECK(run, Account("OpenBCM", "  I-Have-Issues ")
			== Account("OpenBCM", "  I-Have-Issues "),
		"the same input derives the same key every time");

	// --- The empty contract --------------------------------------------------
	//
	// An empty result means "no token operation is possible". Callers must treat
	// it as a refusal rather than as a usable key.
	CHECK(run, TokenStore::AccountFor(std::optional<GitHubIntegration>()).empty(),
		"an absent integration yields no key");
	CHECK(run, Account("", "i-have-issues").empty(),
		"an empty owner yields no key");
	CHECK(run, Account("openbcm", "").empty(),
		"an empty repository yields no key");
	CHECK(run, Account("", "").empty(),
		"two empty coordinates yield no key");
	CHECK(run, Account("   ", "i-have-issues").empty(),
		"a whitespace-only owner yields no key, not a key with an empty half");
	CHECK(run, Account("openbcm", " \t ").empty(),
		"a whitespace-only repository yields no key");
	CHECK(run, Account(" ", " ").empty(),
		"two whitespace-only coordinates yield no key");

	// --- Characters that must survive ----------------------------------------
	CHECK(run, Account("user-name-9", "repo.name_2") == "user-name-9/repo.name_2",
		"digits, hyphens, dots and underscores survive unchanged");

	// --- Documented edge 1: lowercasing is ASCII-only ------------------------
	//
	// Deliberate, and the reason this derivation is shared rather than
	// reimplemented per platform: g_utf8_strdown() on GNOME and QString::toLower()
	// on KDE both perform FULL UNICODE case folding, so a per-platform
	// implementation would disagree with this one the moment a non-ASCII byte
	// appeared. GitHub does not permit non-ASCII in owner or repository names, so
	// this can only arise from a typo -- but all three ports must still agree on
	// what the typo maps to.
	{
		const std::string kOWithDiaeresis = "\xC3\x96";	// U+00D6, uppercase
		std::string key = Account(kOWithDiaeresis + "penBCM", "repo");
		CHECK(run, key == kOWithDiaeresis + "penbcm/repo",
			"non-ASCII bytes pass through uncased, and ASCII around them still folds");
	}

	// --- Documented edge 2: only spaces and tabs are trimmed -----------------
	//
	// Trim() mirrors Swift's `.whitespaces`, which excludes carriage returns and
	// newlines. A value carrying one -- from a hand-edited .issues file, or from
	// another client that wrote it -- keeps it, and derives a DIFFERENT key from
	// the same repository spelled cleanly. See the README.
	CHECK(run, Account("openbcm\n", "i-have-issues") != kCanonical,
		"a trailing newline is NOT trimmed and changes the derived key");
	CHECK(run, Account("openbcm\r", "i-have-issues") != kCanonical,
		"a trailing carriage return is NOT trimmed and changes the derived key");
	CHECK(run, Account("openbcm\n", "i-have-issues") == "openbcm\n/i-have-issues",
		"the untrimmed newline lands inside the key verbatim");

	// --- A slash in either coordinate is rejected ----------------------------
	//
	// Without this guard the key is plain concatenation, so owner "a/b" with
	// repository "c" and owner "a" with repository "b/c" both derived "a/b/c" --
	// a token saved for one repository could be handed to a different one, which
	// is exactly what keying exists to prevent. Both spellings now refuse.
	//
	// Asserted as "each is empty" rather than "they are equal to each other":
	// equality alone would still pass if they collided on some shared value.
	CHECK(run, Account("a/b", "c").empty(),
		"a slash in the owner yields no key");
	CHECK(run, Account("a", "b/c").empty(),
		"a slash in the repository yields no key");
	CHECK(run, Account("a/b", "c/d").empty(),
		"a slash in both coordinates yields no key");
	CHECK(run, Account("a/b", "c").empty() && Account("a", "b/c").empty(),
		"the two formerly-colliding spellings BOTH refuse, rather than sharing a key");

	CHECK(run, Account("/openbcm", "i-have-issues").empty(),
		"a leading slash on the owner yields no key");
	CHECK(run, Account("openbcm", "i-have-issues/").empty(),
		"a trailing slash on the repository yields no key");
	CHECK(run, Account("/", "/").empty(),
		"coordinates that are nothing but a slash yield no key");
	CHECK(run, Account("  a/b  ", "c").empty(),
		"trimming does not rescue a slash hidden inside padding");
	CHECK(run, Account("A/B", "C").empty(),
		"the slash is rejected regardless of case");

	// The guard must not be over-eager: the separator this refuses to accept
	// inside a coordinate is still the one the key itself is built from.
	CHECK(run, Account("openbcm", "i-have-issues") == kCanonical,
		"rejecting embedded slashes leaves ordinary coordinates working");

	return run.Report("TokenStore account keys");
}

} // namespace tests
} // namespace issueskit
