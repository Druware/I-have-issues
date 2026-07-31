/*
 * TestSupport.h -- a hand-rolled assertion harness.
 *
 * No gtest, no external framework: the point of this suite is that it builds
 * anywhere with nothing but a C++17 compiler, so it can be run on a developer's
 * macOS or Linux box long before anyone reaches a Haiku machine.
 *
 * Usage:
 *
 *     TestRun run;
 *     CHECK(run, 2 + 2 == 4, "arithmetic works");
 *     return run.Report("My Suite");
 */
#ifndef ISSUESKIT_TEST_SUPPORT_H
#define ISSUESKIT_TEST_SUPPORT_H

#include <cstdio>
#include <string>

namespace issueskit {
namespace tests {

class TestRun {
public:
	TestRun()
		:
		fChecks(0),
		fFailures(0)
	{
	}

	//! Records one assertion. Prints only failures, with file and line.
	void Check(bool passed, const char* what, const char* file, int line)
	{
		fChecks++;
		if (passed)
			return;
		fFailures++;
		printf("    FAIL  %s\n          at %s:%d\n", what, file, line);
	}

	//! Prints the suite summary. Returns the failure count.
	int Report(const char* suite) const
	{
		printf("  %-34s %4d checks, %d failures%s\n", suite, fChecks, fFailures,
			fFailures == 0 ? "" : "   <-- FAILED");
		return fFailures;
	}

	int Checks() const { return fChecks; }
	int Failures() const { return fFailures; }

	//! Folds another run's totals in, for suites built from several parts.
	void Absorb(const TestRun& other)
	{
		fChecks += other.fChecks;
		fFailures += other.fFailures;
	}

private:
	int	fChecks;
	int	fFailures;
};


#define CHECK(run, condition, what) \
	(run).Check((condition), (what), __FILE__, __LINE__)


/*!	Locates apps/Apple/sample/Example.issues.

	\param argv0 The test binary's path, used to walk back up to the repository.
	\param override An explicit path from the command line, or NULL.
	\return The resolved path, or an empty string when the file was not found.
*/
std::string FindSampleDocument(const char* argv0, const char* override);

//! Reads a whole file. Returns false when it cannot be opened.
bool ReadWholeFile(const std::string& path, std::string& outContents);

// Each suite returns its failure count and adds its totals to `run`.
int RunCoreTests(TestRun& run);
int RunTokenStoreTests(TestRun& run);
int RunSyncTests(TestRun& run);
int RunRoundTripTests(TestRun& run, const std::string& samplePath);

} // namespace tests
} // namespace issueskit

#endif // ISSUESKIT_TEST_SUPPORT_H
