/*
 * IssueModel.h -- the .issues domain model.
 *
 * A direct port of IssuesKit's Issue / Comment / Relation / RemoteLink,
 * LabelDefinition / Milestone / Person, IntegrationSettings, ProjectInfo,
 * ExportSettings and IssuesDocumentModel.
 *
 * These are plain data structs with public fields (no fMember prefix): they
 * carry no behaviour beyond a constructor that installs the documented
 * defaults. Types with behaviour elsewhere in this library use the
 * fMember / _PrivateMethod convention.
 *
 * Naming note: Swift's `export` property is spelled `exportSettings` here,
 * because `export` is a C++ keyword. The JSON key stays "export".
 */
#ifndef ISSUESKIT_ISSUE_MODEL_H
#define ISSUESKIT_ISSUE_MODEL_H

#include <optional>
#include <string>
#include <vector>

#include <issueskit/IssueDate.h>
#include <issueskit/IssueEnums.h>

namespace issueskit {

// #pragma mark - Comment

struct Comment {
					Comment();

	std::string		id;			//!< UUID
	std::string		author;		//!< a Person handle
	Timestamp		createdAt;
	std::string		body;		//!< Markdown
};

// #pragma mark - Relation

struct Relation {
					Relation();

	RelationKind	kind;
	//! The uuid of the other issue. Required: a relation pointing nowhere is
	//! not a relation, so decoding fails when it is absent.
	std::string		issueID;
};

// #pragma mark - RemoteLink

struct RemoteLink {
					RemoteLink();

	RemoteProvider				provider;
	//! GitHub issue number or Azure DevOps work-item id, stored as text.
	std::string					identifier;
	std::optional<std::string>	url;
	std::optional<Timestamp>	lastSyncedAt;
	std::optional<Timestamp>	remoteUpdatedAt;
};

// #pragma mark - Issue

struct Issue {
	/*!	Builds an issue with every documented default.

		uuid is a fresh UUID and reported/createdAt/updatedAt are "now", exactly
		as Swift's memberwise initializer defaults do.
	*/
					Issue();

	//! The stable sync identity. Never renumbered; referenced by relations.
	std::string					uuid;
	//! The human display number, rendered "#007". Safe to renumber.
	int							number;

	std::string					title;
	IssueType					type;
	IssuePriority				priority;
	IssueStatus					status;
	//! Why the issue closed, or unset while it is open.
	std::optional<ResolutionKind> resolutionKind;

	std::vector<std::string>	labels;
	std::vector<std::string>	assignees;
	std::optional<std::string>	milestone;
	std::string					area;
	std::optional<double>		estimate;

	std::string					reportedBy;
	//! Day-normalized: midnight UTC of a calendar day.
	Timestamp					reported;

	Timestamp					createdAt;
	Timestamp					updatedAt;
	std::optional<Timestamp>	closedAt;

	std::string					description;
	std::vector<std::string>	stepsToReproduce;
	std::string					environment;
	std::string					notes;
	std::string					resolution;

	std::vector<Comment>		comments;
	std::vector<Relation>		relations;
	std::vector<RemoteLink>		remoteLinks;

	//! Whether the entry belongs under "## Resolved" on export.
	bool			IsResolved() const { return status == kIssueStatusResolved; }
	//! "#007".
	std::string		DisplayNumber() const;
};

// #pragma mark - Catalogs

struct LabelDefinition {
					LabelDefinition();

	std::string					name;
	//! "RRGGBB" or "#RRGGBB", or unset.
	std::optional<std::string>	colorHex;
	std::string					description;
};


struct Milestone {
					Milestone();

	std::string					name;
	//! Day-normalized, like Issue::reported.
	std::optional<Timestamp>	dueOn;
	bool						isClosed;
};


struct Person {
					Person();

	std::string		handle;
	std::string		displayName;
	std::string		email;
};

// #pragma mark - Integrations

/*!	Where a project's issues live on GitHub.

	Carries NO credentials, by design: .issues files are committed to project
	repositories, so a token stored here would be published to every fork and CI
	log. The personal access token lives in the system key store.
*/
struct GitHubIntegration {
					GitHubIntegration();

	std::string					owner;
	std::string					repository;
	std::vector<std::string>	defaultLabels;
	std::vector<std::string>	defaultAssignees;
	std::optional<std::string>	defaultMilestone;
};


//! Present in the format and editable in Project Settings, but no Azure DevOps
//! sync exists on any platform -- see README.
struct AzureDevOpsIntegration {
					AzureDevOpsIntegration();

	std::string					organization;
	std::string					project;
	std::optional<std::string>	team;
	std::optional<std::string>	areaPath;
	std::optional<std::string>	iterationPath;
	std::string					defaultWorkItemType;	//!< defaults to "Issue"
};


struct IntegrationSettings {
					IntegrationSettings();

	std::optional<GitHubIntegration>		github;
	std::optional<AzureDevOpsIntegration>	azureDevOps;
};

// #pragma mark - Project and export

struct ProjectInfo {
					ProjectInfo();

	std::string		id;			//!< UUID
	std::string		name;
	std::string		summary;
};


struct ExportSettings {
					ExportSettings();

	//! Everything emitted before "## Open" on markdown export.
	std::string		preambleMarkdown;

	//! The preamble a new document exports with.
	static const char* DefaultPreambleMarkdown();
};

// #pragma mark - Document

struct IssuesDocumentModel {
					IssuesDocumentModel();

	//! The highest schemaVersion this build can read.
	static const int kSupportedSchemaVersion = 1;

	int							schemaVersion;
	ProjectInfo					project;
	IntegrationSettings			integrations;
	std::vector<LabelDefinition> labels;
	std::vector<Milestone>		milestones;
	std::vector<Person>			people;
	ExportSettings				exportSettings;		//!< JSON key "export"
	std::vector<Issue>			issues;

	//! One past the highest existing number, or 1 when empty.
	int							NextNumber() const;
	//! The entries that belong under "## Open", in document order.
	std::vector<Issue>			OpenIssues() const;
	//! The entries that belong under "## Resolved", in document order.
	std::vector<Issue>			ResolvedIssues() const;
	//! The issue with the given uuid, or NULL.
	const Issue*				IssueWithID(const std::string& uuid) const;
	Issue*						IssueWithID(const std::string& uuid);
};

} // namespace issueskit

#endif // ISSUESKIT_ISSUE_MODEL_H
