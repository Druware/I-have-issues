/*
 * IssuesMarkdown.cpp
 */
#include <issueskit/IssuesMarkdown.h>

#include <cstdio>

#include <issueskit/StringUtils.h>

namespace issueskit {

namespace {

//! U+2014 EM DASH, as UTF-8. The heading separator the template uses.
const char* kEmDash = "\xE2\x80\x94";

} // unnamed namespace


std::string
IssuesMarkdownSerializer::Export(const IssuesDocumentModel& model)
{
	std::string openBody = _SectionBody(model.OpenIssues(),
		"_No open issues._");
	std::string resolvedBody = _SectionBody(model.ResolvedIssues(),
		"_No resolved issues._");

	return model.exportSettings.preambleMarkdown
		+ "## Open\n\n"
		+ openBody
		+ "\n\n---\n\n## Resolved\n\n"
		+ resolvedBody
		+ "\n";
}


std::string
IssuesMarkdownSerializer::_SectionBody(const std::vector<Issue>& issues,
	const char* emptyText)
{
	if (issues.empty())
		return std::string(emptyText);

	std::vector<std::string> bodies;
	for (size_t i = 0; i < issues.size(); i++)
		bodies.push_back(_ExportIssue(issues[i]));
	return Join(bodies, "\n\n");
}


std::string
IssuesMarkdownSerializer::_ExportIssue(const Issue& issue)
{
	std::vector<std::string> lines;
	lines.push_back("### #" + FormatIssueNumber(issue.number) + " " + kEmDash
		+ " " + issue.title);
	lines.push_back("");
	lines.push_back(std::string("- **Type:** ")
		+ IssueTypeDisplayName(issue.type));
	lines.push_back(std::string("- **Priority:** ")
		+ IssuePriorityDisplayName(issue.priority));
	lines.push_back(std::string("- **Status:** ")
		+ IssueStatusDisplayName(issue.status));
	lines.push_back("- **Reported:** " + IssueDate::ToString(issue.reported));
	lines.push_back("- **Reported by:** " + issue.reportedBy);
	lines.push_back("- **Area:** " + issue.area);

	// Labels, Assignees, Milestone and Estimate are omitted entirely when
	// empty -- never emitted as a blank bullet.
	_AppendMetadata(lines, "Labels", Join(issue.labels, ", "));
	_AppendMetadata(lines, "Assignees", Join(issue.assignees, ", "));
	_AppendMetadata(lines, "Milestone",
		issue.milestone.has_value() ? *issue.milestone : std::string());
	if (issue.estimate.has_value()) {
		lines.push_back("- **Estimate:** " + FormatDouble(*issue.estimate));
	}

	_AppendTextSection(lines, "Description", issue.description);
	_AppendSteps(lines, issue.stepsToReproduce);
	_AppendTextSection(lines, "Environment", issue.environment);
	_AppendTextSection(lines, "Notes / Investigation", issue.notes);
	_AppendTextSection(lines, "Resolution", issue.resolution);
	_AppendComments(lines, issue.comments);

	return Join(lines, "\n");
}


void
IssuesMarkdownSerializer::_AppendMetadata(std::vector<std::string>& lines,
	const char* key, const std::string& value)
{
	if (value.empty())
		return;
	lines.push_back(std::string("- **") + key + ":** " + value);
}


void
IssuesMarkdownSerializer::_AppendTextSection(std::vector<std::string>& lines,
	const char* header, const std::string& content)
{
	if (content.empty())
		return;
	lines.push_back("");
	lines.push_back(std::string("**") + header + "**");
	lines.push_back("");
	std::vector<std::string> contentLines = SplitLines(content);
	for (size_t i = 0; i < contentLines.size(); i++)
		lines.push_back(contentLines[i]);
}


void
IssuesMarkdownSerializer::_AppendSteps(std::vector<std::string>& lines,
	const std::vector<std::string>& steps)
{
	if (steps.empty())
		return;
	lines.push_back("");
	lines.push_back("**Steps to reproduce**");
	lines.push_back("");
	for (size_t i = 0; i < steps.size(); i++) {
		char prefix[32];
		snprintf(prefix, sizeof(prefix), "%lu. ", (unsigned long)(i + 1));
		lines.push_back(std::string(prefix) + steps[i]);
	}
}


void
IssuesMarkdownSerializer::_AppendComments(std::vector<std::string>& lines,
	const std::vector<Comment>& comments)
{
	if (comments.empty())
		return;
	lines.push_back("");
	lines.push_back("**Comments**");
	lines.push_back("");
	for (size_t i = 0; i < comments.size(); i++) {
		const Comment& comment = comments[i];
		std::vector<std::string> bodyLines = SplitLines(comment.body);
		std::string date = IssueDate::ToString(comment.createdAt);
		lines.push_back("- **" + comment.author + "** (" + date + "): "
			+ (bodyLines.empty() ? std::string() : bodyLines[0]));
		// Continuation lines are indented exactly two spaces; that is how
		// LegacyMarkdownImporter tells them from a new comment bullet.
		for (size_t j = 1; j < bodyLines.size(); j++)
			lines.push_back("  " + bodyLines[j]);
	}
}

} // namespace issueskit
