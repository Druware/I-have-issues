/*
 * RoundTripTests.cpp -- byte-exactness against a real .issues file.
 *
 * This is the single most important test in the suite. The .issues file is
 * shared between every client -- Apple, Android, Haiku, GNOME, KDE -- and is
 * committed to git,
 * so a save that reformats it would rewrite the whole file and destroy its
 * diffs. Decoding apps/Apple/sample/Example.issues and re-encoding it must
 * reproduce the original bytes exactly.
 */
#include "TestSupport.h"

#include <cstdio>
#include <string>

#include <issueskit/IssueModel.h>
#include <issueskit/IssuesJsonCoder.h>
#include <issueskit/IssuesMarkdown.h>
#include <issueskit/LegacyMarkdownImporter.h>

namespace issueskit {
namespace tests {

namespace {

//! Prints the first differing byte with surrounding context.
void
ReportFirstDifference(const std::string& expected, const std::string& actual)
{
	size_t limit = expected.size() < actual.size()
		? expected.size() : actual.size();
	size_t at = 0;
	while (at < limit && expected[at] == actual[at])
		at++;

	size_t start = at > 120 ? at - 120 : 0;
	printf("          first difference at byte %lu\n", (unsigned long)at);
	printf("          expected: [%s]\n",
		expected.substr(start, 240).c_str());
	printf("          actual  : [%s]\n", actual.substr(start, 240).c_str());
}

} // unnamed namespace


int
RunRoundTripTests(TestRun& run, const std::string& samplePath)
{
	std::string original;
	if (!ReadWholeFile(samplePath, original)) {
		CHECK(run, false, "the sample document could be read");
		return run.Report("Byte-exact round trip");
	}

	IssuesDocumentModel model;
	IssuesError error;
	if (!IssuesJsonCoder::Decode(original, model, error)) {
		printf("    decode failed: %s\n", error.Message().c_str());
		CHECK(run, false, "the sample document decodes");
		return run.Report("Byte-exact round trip");
	}
	CHECK(run, true, "the sample document decodes");
	CHECK(run, model.issues.size() == 3,
		"the sample document has its three issues");

	std::string encoded;
	IssuesError encodeError;
	CHECK(run, IssuesJsonCoder::Encode(model, encoded, encodeError),
		"the model re-encodes");

	bool identical = encoded == original;
	CHECK(run, identical,
		"re-encoding the sample reproduces the file byte for byte");
	if (!identical)
		ReportFirstDifference(original, encoded);

	std::string again;
	IssuesJsonCoder::Encode(model, again, encodeError);
	CHECK(run, again == encoded, "encoding the same model twice is identical");

	// Decoding our own output and re-encoding must also be a fixed point.
	IssuesDocumentModel reloaded;
	CHECK(run, IssuesJsonCoder::Decode(encoded, reloaded, error),
		"the re-encoded document decodes again");
	std::string third;
	IssuesJsonCoder::Encode(reloaded, third, encodeError);
	CHECK(run, third == encoded, "encode/decode/encode is a fixed point");

	// Markdown export is lossy by design, but exporting, importing that export
	// and exporting again must be stable for everything markdown can carry.
	std::string firstExport = IssuesMarkdownSerializer::Export(model);
	IssuesDocumentModel imported;
	IssuesError importError;
	if (LegacyMarkdownImporter::Import(firstExport, imported, importError)) {
		CHECK(run, true, "the markdown export imports back");
		std::string secondExport
			= IssuesMarkdownSerializer::Export(imported);
		CHECK(run, secondExport == firstExport,
			"export/import/export is stable");
		CHECK(run, imported.OpenIssues().size() == model.OpenIssues().size()
			&& imported.ResolvedIssues().size()
				== model.ResolvedIssues().size(),
			"the reimported document keeps its open/resolved split");
	} else {
		CHECK(run, false, "the markdown export imports back");
	}

	return run.Report("Byte-exact round trip");
}

} // namespace tests
} // namespace issueskit
