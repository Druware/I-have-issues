/*
 * TestMain.cpp -- entry point and shared helpers for the test suite.
 *
 * Exit code is the total number of failed assertions, so a failure is non-zero
 * and `make test` fails the build.
 */
#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace issueskit {
namespace tests {

bool
ReadWholeFile(const std::string& path, std::string& outContents)
{
	std::ifstream in(path.c_str(), std::ios::binary);
	if (!in)
		return false;
	std::ostringstream buffer;
	buffer << in.rdbuf();
	outContents = buffer.str();
	return true;
}


namespace {

bool
FileExists(const std::string& path)
{
	std::ifstream in(path.c_str(), std::ios::binary);
	return in.good();
}


//! The directory part of \a path, or "." when there is none.
std::string
DirectoryOf(const std::string& path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return std::string(".");
	if (slash == 0)
		return std::string("/");
	return path.substr(0, slash);
}

} // unnamed namespace


std::string
FindSampleDocument(const char* argv0, const char* override)
{
	// An explicit path always wins, so the Makefile can hand one over and a
	// developer can point the suite at a copy somewhere else.
	if (override != NULL && override[0] != '\0') {
		if (FileExists(override))
			return std::string(override);
		return std::string();
	}

	// Otherwise walk up from the binary looking for the repository root. This is
	// layout-independent on purpose: the suite has already moved once (from
	// apps/Haiku/tests/ to libs/issueskit/tests/) and will be built from
	// Meson and CMake build directories too, each at its own depth.
	const char* kSuffix = "/apps/Apple/sample/Example.issues";

	std::string prefix = DirectoryOf(argv0 != NULL ? argv0 : ".");
	for (int depth = 0; depth < 10; depth++) {
		if (FileExists(prefix + kSuffix))
			return prefix + kSuffix;
		prefix += "/..";
	}
	return std::string();
}

} // namespace tests
} // namespace issueskit


int
main(int argc, char** argv)
{
	using namespace issueskit::tests;

	printf("issueskit -- shared core test suite\n");
	printf("--------------------------------------------------------------\n");

	TestRun total;
	int failures = 0;

	TestRun core;
	failures += RunCoreTests(core);
	total.Absorb(core);

	TestRun tokenStore;
	failures += RunTokenStoreTests(tokenStore);
	total.Absorb(tokenStore);

	TestRun sync;
	failures += RunSyncTests(sync);
	total.Absorb(sync);

	std::string sample = FindSampleDocument(argv[0], argc > 1 ? argv[1] : NULL);
	if (sample.empty()) {
		printf("  %-34s could not locate Example.issues\n", "Round trip");
		printf("\n  The byte-exact round-trip test needs "
			"apps/Apple/sample/Example.issues.\n"
			"  Pass its path as the first argument:\n"
			"      %s /path/to/Example.issues\n", argv[0]);
		failures++;
	} else {
		TestRun roundTrip;
		failures += RunRoundTripTests(roundTrip, sample);
		total.Absorb(roundTrip);
	}

	printf("--------------------------------------------------------------\n");
	printf("  TOTAL: %d checks, %d failures\n", total.Checks(),
		total.Failures());
	printf("%s\n", failures == 0 ? "PASS" : "FAIL");

	// Non-zero exit on any failure, so `make test` fails.
	return failures == 0 ? 0 : 1;
}
