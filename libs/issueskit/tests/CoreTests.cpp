/*
 * CoreTests.cpp -- the .issues format behavioural contract.
 *
 * These assertions mirror the Apple IssuesKit test suite documented in the
 * format spec: the same rules, checked against this port. They are the contract
 * the three clients share, so a change here means a change to the on-disk
 * format.
 */
#include "TestSupport.h"

#include <string>

#include <issueskit/IssueDate.h>
#include <issueskit/IssueModel.h>
#include <issueskit/IssuesJsonCoder.h>
#include <issueskit/IssuesMarkdown.h>
#include <issueskit/LegacyMarkdownImporter.h>

namespace issueskit {
namespace tests {

namespace {

bool
Decode(const std::string& json, IssuesDocumentModel& model, IssuesError& error)
{
	return IssuesJsonCoder::Decode(json, model, error);
}


std::string
Encode(const IssuesDocumentModel& model)
{
	std::string json;
	IssuesError error;
	IssuesJsonCoder::Encode(model, json, error);
	return json;
}

} // unnamed namespace


int
RunCoreTests(TestRun& run)
{
	IssuesDocumentModel m;
	IssuesError e;

	// --- Documented defaults ------------------------------------------------
	CHECK(run, Decode("{\"schemaVersion\":1,\"issues\":[{\"title\":\"Bare\"}]}",
		m, e), "minimal document decodes");
	CHECK(run, m.issues.size() == 1, "one issue decoded");
	if (m.issues.size() == 1) {
		const Issue& i0 = m.issues[0];
		CHECK(run, i0.number == 0 && i0.type == kIssueTypeTask
			&& i0.priority == kIssuePriorityMedium
			&& i0.status == kIssueStatusOpen
			&& !i0.resolutionKind.has_value() && i0.labels.empty()
			&& i0.assignees.empty() && !i0.milestone.has_value()
			&& !i0.estimate.has_value() && !i0.closedAt.has_value(),
			"every absent issue field takes its documented default");
	}
	CHECK(run, m.exportSettings.preambleMarkdown
		== ExportSettings::DefaultPreambleMarkdown(),
		"absent export block yields the default preamble");
	CHECK(run, !m.integrations.github.has_value()
		&& !m.integrations.azureDevOps.has_value(),
		"absent integrations stay unset");

	// --- schemaVersion is required and version-gated ------------------------
	CHECK(run, !Decode("{\"issues\":[]}", m, e)
		&& e.GetCode() == IssuesError::kMissingSchemaVersion,
		"missing schemaVersion is rejected");
	CHECK(run, !Decode("{\"schemaVersion\":99}", m, e)
		&& e.GetCode() == IssuesError::kUnsupportedSchemaVersion,
		"a newer schema version is refused, not silently opened");
	CHECK(run, Decode("{\"schemaVersion\":1}", m, e), "version 1 decodes");
	CHECK(run, Decode("{\"schemaVersion\":0}", m, e) && m.schemaVersion == 0,
		"an older schema version is accepted as-is");

	// --- Unknown keys are ignored, not fatal --------------------------------
	CHECK(run, Decode("{\"schemaVersion\":1,\"nope\":5,"
		"\"project\":{\"name\":\"P\",\"zz\":1},"
		"\"issues\":[{\"title\":\"T\",\"qq\":true}]}", m, e),
		"unknown keys at every level are ignored");
	CHECK(run, m.project.name == "P" && m.issues[0].title == "T",
		"known siblings of an unknown key still decode");

	// --- Unknown enum values fall back --------------------------------------
	CHECK(run, Decode("{\"schemaVersion\":1,\"issues\":[{\"type\":\"epic\","
		"\"priority\":\"zzz\",\"status\":\"vibing\","
		"\"resolutionKind\":\"unknown\",\"relations\":[{\"kind\":\"weird\","
		"\"issueID\":\"A1F4C7D2-5E90-4B3A-8C61-0D2E7F5A9B44\"}]}]}", m, e),
		"unrecognized enum values decode");
	CHECK(run, m.issues[0].type == kIssueTypeTask, "unknown type -> task");
	CHECK(run, m.issues[0].priority == kIssuePriorityMedium,
		"unknown priority -> medium");
	CHECK(run, m.issues[0].status == kIssueStatusOpen, "unknown status -> open");
	CHECK(run, !m.issues[0].resolutionKind.has_value(),
		"unknown resolutionKind -> unset, never a guessed close reason");
	CHECK(run, m.issues[0].relations[0].kind == kRelationKindRelatedTo,
		"unknown relation kind -> relatedTo");

	// --- RemoteProvider preserves unknown values ----------------------------
	CHECK(run, Decode("{\"schemaVersion\":1,\"issues\":[{\"remoteLinks\":"
		"[{\"provider\":\"gitlab\",\"identifier\":\"9\"}]}]}", m, e),
		"unknown provider decodes");
	{
		const RemoteProvider& p = m.issues[0].remoteLinks[0].provider;
		CHECK(run, !p.IsGitHub() && !p.IsAzureDevOps(),
			"an unknown provider is neither GitHub nor Azure DevOps");
		CHECK(run, p.RawValue() == "gitlab" && p.DisplayName() == "gitlab",
			"an unknown provider shows its own raw value");
	}
	{
		std::string re = Encode(m);
		CHECK(run, re.find("\"provider\" : \"gitlab\"") != std::string::npos,
			"unknown provider re-encodes verbatim");
		CHECK(run, re.find("\"provider\" : \"github\"") == std::string::npos,
			"unknown provider is never coerced to github");
		IssuesDocumentModel m2;
		CHECK(run, Decode(re, m2, e) && Encode(m2) == re,
			"unknown provider survives a full encode/decode cycle byte-identically");
	}
	CHECK(run, Decode("{\"schemaVersion\":1,\"issues\":[{\"remoteLinks\":"
		"[{\"identifier\":\"9\"}]}]}", m, e)
		&& m.issues[0].remoteLinks[0].provider.IsGitHub(),
		"an entirely absent provider defaults to github");
	CHECK(run, RemoteProvider::SelectableCases().size() == 2
		&& RemoteProvider::SelectableCases()[0].DisplayName() == "GitHub"
		&& RemoteProvider::SelectableCases()[1].DisplayName() == "Azure DevOps",
		"selectable providers are exactly GitHub and Azure DevOps");

	// --- relations[].issueID is the one field with no default ---------------
	CHECK(run, !Decode("{\"schemaVersion\":1,\"issues\":[{\"relations\":"
		"[{\"kind\":\"blocks\"}]}]}", m, e)
		&& e.GetCode() == IssuesError::kDecodingFailed,
		"a relation without issueID fails the decode");

	// --- Byte-level output rules --------------------------------------------
	CHECK(run, Decode("{\"schemaVersion\":1,\"issues\":[{\"remoteLinks\":"
		"[{\"url\":\"https://github.com/o/r/issues/412\"}]}]}", m, e),
		"a remote link URL decodes");
	{
		std::string out = Encode(m);
		CHECK(run, !out.empty() && out[out.size() - 1] == '\n',
			"encoded output ends with a trailing newline");
		CHECK(run, out.find("\\/") == std::string::npos,
			"slashes are never escaped");
		CHECK(run, out.find("https://github.com/o/r/issues/412")
			!= std::string::npos, "a URL survives verbatim");

		const char* order[] = { "\"export\"", "\"integrations\"", "\"issues\"",
			"\"labels\"", "\"milestones\"", "\"people\"", "\"project\"",
			"\"schemaVersion\"" };
		size_t previous = 0;
		bool sorted = true;
		for (int k = 0; k < 8; k++) {
			size_t at = out.find(order[k]);
			if (at == std::string::npos || at < previous)
				sorted = false;
			previous = at;
		}
		CHECK(run, sorted, "top-level keys are in alphabetical order");
		CHECK(run, out.find("\"labels\" : [\n\n  ]") != std::string::npos,
			"an empty array renders as bracket, blank line, bracket");
		CHECK(run, out.find("\"integrations\" : {\n\n  }") != std::string::npos,
			"an empty object renders as brace, blank line, brace");
		CHECK(run, out.find("\" : \"") != std::string::npos,
			"the key/value separator has a space on both sides of the colon");
	}

	// --- Document helpers ---------------------------------------------------
	m = IssuesDocumentModel();
	CHECK(run, m.NextNumber() == 1, "nextNumber on an empty document is 1");
	{
		Issue a; a.number = 3;
		Issue b; b.number = 7;
		Issue c; c.number = 5;
		m.issues.push_back(a);
		m.issues.push_back(b);
		m.issues.push_back(c);
		CHECK(run, m.NextNumber() == 8,
			"nextNumber is one past the highest, not a count");
		CHECK(run, m.IssueWithID(b.uuid) != NULL && m.IssueWithID("nope") == NULL,
			"issue lookup by uuid");
	}
	{
		std::string md
			= IssuesMarkdownSerializer::Export(IssuesDocumentModel());
		CHECK(run, md.find("## Open") != std::string::npos
			&& md.find("## Resolved") != std::string::npos,
			"an empty document exports both sections");
		CHECK(run, md.find("_No open issues._") != std::string::npos,
			"an empty section gets its placeholder");
	}

	// --- Markdown export ----------------------------------------------------
	m = IssuesDocumentModel();
	m.issues.clear();
	{
		Issue d;
		d.number = 7;
		d.title = "Login button does nothing";
		d.status = kIssueStatusInProgress;
		d.type = kIssueTypeBug;
		d.priority = kIssuePriorityHigh;
		IssueDate::Parse("2026-05-01", d.reported);
		d.reportedBy = "dru";
		d.area = "Views";
		d.estimate = 3.0;
		m.issues.push_back(d);

		std::string md = IssuesMarkdownSerializer::Export(m);
		CHECK(run, md.find("**Status:** In Progress") != std::string::npos,
			"export uses display names, never raw enum values");
		CHECK(run, md.find("### #007 \xE2\x80\x94 Login button does nothing")
			!= std::string::npos,
			"heading is zero-padded to three digits with an em dash");
		CHECK(run, md.find("- **Labels:**") == std::string::npos,
			"an empty Labels bullet is omitted entirely");
		CHECK(run, md.find("- **Estimate:** 3\n") != std::string::npos,
			"a whole estimate renders without a decimal point");
	}

	// --- Legacy markdown import: standard layout ----------------------------
	CHECK(run, !LegacyMarkdownImporter::Import(
		"# Just a title\n\nno level two headings", m, e)
		&& e.GetCode() == IssuesError::kMissingOpenSection,
		"a file with no level-2 heading is not an issues file");

	{
		std::string legacy =
			"# Issues\n\n## Open\n\n### bad heading with no number\n\n"
			"### #012 \xE2\x80\x94 Real one\n\n"
			"- **Type:** Bug\n- **Severity:** Critical\n- **Status:** Resolved\n"
			"- **Reported:** 2026-05-01\n- **Reported by:** dru\n"
			"- **Component:** API (Auth)\n- **Labels:** a, b\n"
			"- **Estimate:** 2.5\n- **GitHub:** 412\n\n"
			"**Description**\n\nDesc here.\n\n"
			"**Steps to reproduce**\n\n1. one\n2. two\n\n"
			"**Design Considerations**\n\nkeep me\n\n"
			"**Proposed Fix**\n\nfixed it\n\n"
			"**Comments**\n\n- **sam** (2026-05-06): hello\n  continued\n";

		CHECK(run, LegacyMarkdownImporter::Import(legacy, m, e),
			"a standard-layout legacy file imports");
		CHECK(run, m.issues.size() == 1,
			"a malformed heading is skipped without aborting the import");
		if (m.issues.size() == 1) {
			const Issue& x = m.issues[0];
			CHECK(run, x.number == 12 && x.title == "Real one",
				"number and title parse");
			CHECK(run, x.type == kIssueTypeBug
				&& x.priority == kIssuePriorityCritical,
				"Type parses and Severity maps onto Priority");
			CHECK(run, x.status == kIssueStatusResolved, "Status parses");
			CHECK(run, IssueDate::ToString(x.reported) == "2026-05-01",
				"Reported parses as a GMT calendar day");
			CHECK(run, x.area == "API (Auth)", "Component maps onto Area");
			CHECK(run, x.labels.size() == 2 && x.labels[0] == "a",
				"Labels split on commas");
			CHECK(run, x.estimate.has_value() && *x.estimate == 2.5,
				"Estimate parses");
			CHECK(run, x.remoteLinks.size() == 1
				&& x.remoteLinks[0].identifier == "412"
				&& x.remoteLinks[0].provider.IsGitHub(),
				"a GitHub bullet becomes a remote link");
			CHECK(run, x.description == "Desc here.", "Description section");
			CHECK(run, x.stepsToReproduce.size() == 2
				&& x.stepsToReproduce[1] == "two", "numbered steps parse");
			CHECK(run, x.resolution == "fixed it",
				"Proposed Fix maps onto Resolution");
			CHECK(run, x.notes.find("**Design Considerations**")
					!= std::string::npos
				&& x.notes.find("keep me") != std::string::npos,
				"an unrecognized section survives verbatim in Notes");
			CHECK(run, x.comments.size() == 1 && x.comments[0].author == "sam"
				&& x.comments[0].body == "hello\ncontinued",
				"a comment and its two-space continuation line parse");
			CHECK(run, IssueDate::ToString(x.comments[0].createdAt)
				== "2026-05-06", "comment date parses");
			CHECK(run, !x.uuid.empty(), "an imported issue gets an identity");
		}

		IssuesDocumentModel n1;
		IssuesDocumentModel n2;
		LegacyMarkdownImporter::Import(legacy, n1, e);
		LegacyMarkdownImporter::Import(legacy, n2, e);
		CHECK(run, n1.issues[0].uuid != n2.issues[0].uuid,
			"each import assigns fresh identity");
	}

	CHECK(run, LegacyMarkdownImporter::Import(
		"## Open\n\n### #1 - X\n\n- **Type:** Epic\n- **Priority:** Whenever\n"
		"- **Status:** Vibing\n- **Reported:** not-a-date\n", m, e),
		"unrecognized metadata does not abort an import");
	CHECK(run, m.issues[0].type == kIssueTypeTask
		&& m.issues[0].priority == kIssuePriorityMedium
		&& m.issues[0].status == kIssueStatusOpen,
		"unrecognized display names fall back to enum defaults");
	CHECK(run, m.issues[0].reported == IssueDate::Today(),
		"an unparsable Reported date falls back to today in GMT");

	// --- Legacy markdown import: free-form layout ---------------------------
	{
		std::string freeform =
			"# MLM\n\n## Bugs\n\n### Login endpoint never triggers lockout\n\n"
			"- **Severity:** High\n\n"
			"## Enhancements\n\n### ~~Old thing~~ \xE2\x9C\x93 Fixed\n\n"
			"- **Severity:** Low\n\n"
			"## Questions\n\n### Why?\n\n- **Severity:** Medium\n";

		CHECK(run, LegacyMarkdownImporter::Import(freeform, m, e),
			"a free-form legacy file imports");
		CHECK(run, m.issues.size() == 3, "all free-form entries import");
		if (m.issues.size() == 3) {
			CHECK(run, m.issues[0].type == kIssueTypeBug
				&& m.issues[1].type == kIssueTypeFeature
				&& m.issues[2].type == kIssueTypeQuestion,
				"the section heading sets each entry's type");
			CHECK(run, m.issues[0].number == 1 && m.issues[1].number == 2
				&& m.issues[2].number == 3,
				"unnumbered free-form entries auto-increment");
			CHECK(run, m.issues[0].priority == kIssuePriorityHigh
				&& m.issues[1].priority == kIssuePriorityLow,
				"free-form Severity maps onto Priority");
			CHECK(run, m.issues[1].status == kIssueStatusResolved
				&& m.issues[1].title == "Old thing",
				"a struck-through title marks the entry resolved");
			CHECK(run, m.exportSettings.preambleMarkdown == "",
				"a free-form document has no preamble");
		}
	}

	// --- Fixed-UTC date handling --------------------------------------------
	{
		Timestamp t = 0;
		CHECK(run, IssueDate::Parse("2026-05-01", t)
			&& IssueDate::ToISO8601(t) == "2026-05-01T00:00:00Z",
			"a calendar day becomes midnight UTC");
		CHECK(run, IssueDate::ParseISO8601("2026-05-07T16:42:00Z", t)
			&& IssueDate::ToString(t) == "2026-05-07",
			"a timestamp renders back to its UTC day");
		CHECK(run, IssueDate::ParseISO8601("2026-05-07T16:42:00.123456Z", t),
			"fractional seconds are tolerated on input");
		CHECK(run, IssueDate::ParseISO8601("1969-12-31T23:59:59Z", t) && t == -1,
			"pre-epoch timestamps parse");
		CHECK(run, IssueDate::ToString(-1) == "1969-12-31",
			"pre-epoch days floor correctly");
	}

	return run.Report("Core format contract");
}

} // namespace tests
} // namespace issueskit
