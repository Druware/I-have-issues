/*
 * IssueModel.cpp
 */
#include <issueskit/IssueModel.h>

#include <issueskit/StringUtils.h>
#include <issueskit/Uuid.h>

namespace issueskit {

Comment::Comment()
	:
	id(GenerateUuid()),
	createdAt(IssueDate::Now())
{
}


Relation::Relation()
	:
	kind(kDefaultRelationKind)
{
}


RemoteLink::RemoteLink()
	:
	provider(RemoteProvider::GitHub())
{
}


Issue::Issue()
	:
	uuid(GenerateUuid()),
	number(0),
	type(kDefaultIssueType),
	priority(kDefaultIssuePriority),
	status(kDefaultIssueStatus),
	reported(IssueDate::Now()),
	createdAt(IssueDate::Now()),
	updatedAt(IssueDate::Now())
{
}


std::string
Issue::DisplayNumber() const
{
	return "#" + FormatIssueNumber(number);
}


LabelDefinition::LabelDefinition()
{
}


Milestone::Milestone()
	:
	isClosed(false)
{
}


Person::Person()
{
}


GitHubIntegration::GitHubIntegration()
{
}


AzureDevOpsIntegration::AzureDevOpsIntegration()
	:
	defaultWorkItemType("Issue")
{
}


IntegrationSettings::IntegrationSettings()
{
}


ProjectInfo::ProjectInfo()
	:
	id(GenerateUuid())
{
}


const char*
ExportSettings::DefaultPreambleMarkdown()
{
	// Byte-for-byte the same literal as ExportSettings.defaultPreambleMarkdown
	// in IssuesDocumentModel.swift, including the U+2014 em dash on the template
	// heading and the closing "---\n".
	return R"ISSUES(# Issues

A running log of known issues, bugs, and feature requests.

## How to use this file

- Add new issues under **Open**, most recent first.
- Move issues to **Resolved** when closed, noting the fix and date.
- Use the template below for each entry. Keep IDs sequential (e.g. `#001`).

### Template

```
### #000 — Short descriptive title

- **Type:** Bug | Feature | Task | Question
- **Priority:** Low | Medium | High | Critical
- **Status:** Open | In Progress | Blocked | Resolved
- **Reported:** YYYY-MM-DD
- **Reported by:**
- **Area:** (e.g. Networking, Views, Session)

**Description**

What is the problem or request? What is the expected vs. actual behavior?

**Steps to reproduce** (bugs only)

1.
2.
3.

**Notes / Investigation**

Findings, related files, or discussion.

**Resolution** (when closed)

What was changed, and where. Reference commit/PR if applicable.
```

---
)ISSUES";
}


ExportSettings::ExportSettings()
	:
	preambleMarkdown(DefaultPreambleMarkdown())
{
}


IssuesDocumentModel::IssuesDocumentModel()
	:
	schemaVersion(kSupportedSchemaVersion)
{
}


int
IssuesDocumentModel::NextNumber() const
{
	int highest = 0;
	for (size_t i = 0; i < issues.size(); i++) {
		if (issues[i].number > highest)
			highest = issues[i].number;
	}
	return highest + 1;
}


std::vector<Issue>
IssuesDocumentModel::OpenIssues() const
{
	std::vector<Issue> result;
	for (size_t i = 0; i < issues.size(); i++) {
		if (!issues[i].IsResolved())
			result.push_back(issues[i]);
	}
	return result;
}


std::vector<Issue>
IssuesDocumentModel::ResolvedIssues() const
{
	std::vector<Issue> result;
	for (size_t i = 0; i < issues.size(); i++) {
		if (issues[i].IsResolved())
			result.push_back(issues[i]);
	}
	return result;
}


const Issue*
IssuesDocumentModel::IssueWithID(const std::string& uuid) const
{
	for (size_t i = 0; i < issues.size(); i++) {
		if (issues[i].uuid == uuid)
			return &issues[i];
	}
	return NULL;
}


Issue*
IssuesDocumentModel::IssueWithID(const std::string& uuid)
{
	for (size_t i = 0; i < issues.size(); i++) {
		if (issues[i].uuid == uuid)
			return &issues[i];
	}
	return NULL;
}

} // namespace issueskit
