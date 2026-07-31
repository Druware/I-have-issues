/*
 * IssuesJsonCoder.h -- reads and writes the .issues JSON document format.
 *
 * Output is deliberately diff-friendly, because .issues files live in project
 * repositories: keys sorted at every level, two-space pretty printing, " : "
 * around the colon, unescaped slashes, ISO-8601 UTC timestamps truncated to
 * whole seconds, and a trailing newline. Encoding the same model twice produces
 * identical bytes.
 *
 * Decoding is tolerant in exactly the ways the format documents:
 *   - schemaVersion is REQUIRED; a newer version than kSupportedSchemaVersion is
 *     refused rather than silently opened with fields dropped;
 *   - every other absent key falls back to its documented default;
 *   - unknown keys are ignored (and, like the Apple app, are NOT preserved
 *     through a round trip);
 *   - unknown enum raw values fall back to the enum default, except
 *     resolutionKind (becomes unset) and remoteLinks[].provider (preserved);
 *   - relations[].issueID is the one field with no default: a relation without
 *     it makes the whole decode fail.
 */
#ifndef ISSUESKIT_ISSUES_JSON_CODER_H
#define ISSUESKIT_ISSUES_JSON_CODER_H

#include <string>

#include <issueskit/IssueModel.h>
#include <issueskit/IssuesError.h>
#include <issueskit/JsonValue.h>

namespace issueskit {

class IssuesJsonCoder {
public:
	//! Encodes a document, including the trailing newline.
	static	bool				Encode(const IssuesDocumentModel& model,
									std::string& outJson,
									IssuesError& outError);

	//! Decodes a document.
	static	bool				Decode(const std::string& json,
									IssuesDocumentModel& outModel,
									IssuesError& outError);

	/*!	Encodes a single issue.

		Used by the UI to hand an edited Issue between windows inside a BMessage:
		BMessage copies its data, so shipping JSON avoids passing a pointer
		across two BLoopers.
	*/
	static	JsonValue			EncodeIssue(const Issue& issue);

	//! The inverse of EncodeIssue.
	static	bool				DecodeIssue(const JsonValue& value,
									Issue& outIssue, IssuesError& outError);

private:
	static	JsonValue			_EncodeDocument(
									const IssuesDocumentModel& model);
	static	JsonValue			_EncodeComment(const Comment& comment);
	static	JsonValue			_EncodeRelation(const Relation& relation);
	static	JsonValue			_EncodeRemoteLink(const RemoteLink& link);
	static	JsonValue			_EncodeLabel(const LabelDefinition& label);
	static	JsonValue			_EncodeMilestone(const Milestone& milestone);
	static	JsonValue			_EncodePerson(const Person& person);
	static	JsonValue			_EncodeIntegrations(
									const IntegrationSettings& settings);
	static	JsonValue			_EncodeGitHub(
									const GitHubIntegration& integration);
	static	JsonValue			_EncodeAzure(
									const AzureDevOpsIntegration& integration);
	static	JsonValue			_EncodeProject(const ProjectInfo& project);
	static	JsonValue			_EncodeStringArray(
									const std::vector<std::string>& values);

	static	bool				_DecodeComment(const JsonValue& value,
									Comment& outComment, IssuesError& outError);
	static	bool				_DecodeRelation(const JsonValue& value,
									Relation& outRelation,
									IssuesError& outError);
	static	bool				_DecodeRemoteLink(const JsonValue& value,
									RemoteLink& outLink, IssuesError& outError);
	static	bool				_DecodeLabel(const JsonValue& value,
									LabelDefinition& outLabel,
									IssuesError& outError);
	static	bool				_DecodeMilestone(const JsonValue& value,
									Milestone& outMilestone,
									IssuesError& outError);
	static	bool				_DecodePerson(const JsonValue& value,
									Person& outPerson, IssuesError& outError);
	static	bool				_DecodeIntegrations(const JsonValue& value,
									IntegrationSettings& outSettings,
									IssuesError& outError);
	static	bool				_DecodeGitHub(const JsonValue& value,
									GitHubIntegration& outIntegration,
									IssuesError& outError);
	static	bool				_DecodeAzure(const JsonValue& value,
									AzureDevOpsIntegration& outIntegration,
									IssuesError& outError);
	static	bool				_DecodeProject(const JsonValue& value,
									ProjectInfo& outProject,
									IssuesError& outError);
};

} // namespace issueskit

#endif // ISSUESKIT_ISSUES_JSON_CODER_H
