/*
 * IssuesJsonCoder.cpp
 */
#include <issueskit/IssuesJsonCoder.h>

#include <issueskit/JsonParser.h>
#include <issueskit/StringUtils.h>
#include <issueskit/Uuid.h>

namespace issueskit {

namespace {

/*!	The "value present and usable" test shared by every decode helper.

	JSON null is treated as absent, matching Foundation's decodeIfPresent, which
	returns nil for an explicit null rather than throwing.
*/
const JsonValue*
Member(const JsonValue& object, const char* key)
{
	if (!object.IsObject())
		return NULL;
	const JsonValue* value = object.Find(key);
	if (value == NULL || value->IsNull())
		return NULL;
	return value;
}


bool
TypeMismatch(const char* key, const char* expected, IssuesError& outError)
{
	outError = IssuesError::DecodingFailed(std::string("\"") + key
		+ "\" is not " + expected + ".");
	return false;
}


bool
DecodeString(const JsonValue& object, const char* key, std::string& outValue,
	IssuesError& outError, bool& outPresent)
{
	outPresent = false;
	const JsonValue* value = Member(object, key);
	if (value == NULL)
		return true;
	if (!value->IsString())
		return TypeMismatch(key, "a string", outError);
	outValue = value->StringValue();
	outPresent = true;
	return true;
}


bool
DecodeOptionalString(const JsonValue& object, const char* key,
	std::optional<std::string>& outValue, IssuesError& outError)
{
	std::string text;
	bool present = false;
	if (!DecodeString(object, key, text, outError, present))
		return false;
	if (present)
		outValue = text;
	return true;
}


bool
DecodeInt(const JsonValue& object, const char* key, int& outValue,
	IssuesError& outError, bool& outPresent)
{
	outPresent = false;
	const JsonValue* value = Member(object, key);
	if (value == NULL)
		return true;
	if (!value->IsNumber())
		return TypeMismatch(key, "a number", outError);
	outValue = (int)value->IntegerValue();
	outPresent = true;
	return true;
}


bool
DecodeBool(const JsonValue& object, const char* key, bool& outValue,
	IssuesError& outError)
{
	const JsonValue* value = Member(object, key);
	if (value == NULL)
		return true;
	if (!value->IsBool())
		return TypeMismatch(key, "a boolean", outError);
	outValue = value->BoolValue();
	return true;
}


bool
DecodeOptionalDouble(const JsonValue& object, const char* key,
	std::optional<double>& outValue, IssuesError& outError)
{
	const JsonValue* value = Member(object, key);
	if (value == NULL)
		return true;
	if (!value->IsNumber())
		return TypeMismatch(key, "a number", outError);
	outValue = value->DoubleValue();
	return true;
}


bool
DecodeTimestamp(const JsonValue& object, const char* key, Timestamp& outValue,
	IssuesError& outError, bool& outPresent)
{
	outPresent = false;
	const JsonValue* value = Member(object, key);
	if (value == NULL)
		return true;
	if (!value->IsString())
		return TypeMismatch(key, "an ISO-8601 date string", outError);
	Timestamp parsed = 0;
	if (!IssueDate::ParseISO8601(value->StringValue(), parsed)) {
		outError = IssuesError::DecodingFailed(std::string("\"") + key
			+ "\" is not a valid ISO-8601 date: " + value->StringValue());
		return false;
	}
	outValue = parsed;
	outPresent = true;
	return true;
}


bool
DecodeOptionalTimestamp(const JsonValue& object, const char* key,
	std::optional<Timestamp>& outValue, IssuesError& outError)
{
	Timestamp parsed = 0;
	bool present = false;
	if (!DecodeTimestamp(object, key, parsed, outError, present))
		return false;
	if (present)
		outValue = parsed;
	return true;
}


bool
DecodeUuid(const JsonValue& object, const char* key, std::string& outValue,
	IssuesError& outError, bool& outPresent)
{
	outPresent = false;
	const JsonValue* value = Member(object, key);
	if (value == NULL)
		return true;
	if (!value->IsString())
		return TypeMismatch(key, "a UUID string", outError);
	std::string normalized = NormalizeUuid(value->StringValue());
	if (normalized.empty()) {
		outError = IssuesError::DecodingFailed(std::string("\"") + key
			+ "\" is not a valid UUID: " + value->StringValue());
		return false;
	}
	outValue = normalized;
	outPresent = true;
	return true;
}


bool
DecodeStringArray(const JsonValue& object, const char* key,
	std::vector<std::string>& outValues, IssuesError& outError)
{
	const JsonValue* value = Member(object, key);
	if (value == NULL)
		return true;
	if (!value->IsArray())
		return TypeMismatch(key, "an array", outError);
	outValues.clear();
	for (size_t i = 0; i < value->CountItems(); i++) {
		const JsonValue& item = value->ItemAt(i);
		if (!item.IsString())
			return TypeMismatch(key, "an array of strings", outError);
		outValues.push_back(item.StringValue());
	}
	return true;
}


//! Reads an array member, or leaves \a outArray NULL when the key is absent.
const JsonValue*
ArrayMember(const JsonValue& object, const char* key, IssuesError& outError,
	bool& outValid)
{
	outValid = true;
	const JsonValue* value = Member(object, key);
	if (value == NULL)
		return NULL;
	if (!value->IsArray()) {
		TypeMismatch(key, "an array", outError);
		outValid = false;
		return NULL;
	}
	return value;
}

} // unnamed namespace


// #pragma mark - Encoding

JsonValue
IssuesJsonCoder::_EncodeStringArray(const std::vector<std::string>& values)
{
	JsonValue array = JsonValue::Array();
	for (size_t i = 0; i < values.size(); i++)
		array.Append(JsonValue::String(values[i]));
	return array;
}


JsonValue
IssuesJsonCoder::_EncodeComment(const Comment& comment)
{
	JsonValue object = JsonValue::Object();
	object.Set("author", JsonValue::String(comment.author));
	object.Set("body", JsonValue::String(comment.body));
	object.Set("createdAt",
		JsonValue::String(IssueDate::ToISO8601(comment.createdAt)));
	object.Set("id", JsonValue::String(comment.id));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeRelation(const Relation& relation)
{
	JsonValue object = JsonValue::Object();
	object.Set("issueID", JsonValue::String(relation.issueID));
	object.Set("kind", JsonValue::String(RelationKindRawValue(relation.kind)));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeRemoteLink(const RemoteLink& link)
{
	JsonValue object = JsonValue::Object();
	object.Set("identifier", JsonValue::String(link.identifier));
	if (link.lastSyncedAt.has_value()) {
		object.Set("lastSyncedAt",
			JsonValue::String(IssueDate::ToISO8601(*link.lastSyncedAt)));
	}
	// The raw value, never a coerced one: an unknown provider re-encodes
	// verbatim so a "gitlab" link never turns into a "github" link.
	object.Set("provider", JsonValue::String(link.provider.RawValue()));
	if (link.remoteUpdatedAt.has_value()) {
		object.Set("remoteUpdatedAt",
			JsonValue::String(IssueDate::ToISO8601(*link.remoteUpdatedAt)));
	}
	if (link.url.has_value())
		object.Set("url", JsonValue::String(*link.url));
	return object;
}


JsonValue
IssuesJsonCoder::EncodeIssue(const Issue& issue)
{
	JsonValue object = JsonValue::Object();
	object.Set("area", JsonValue::String(issue.area));
	object.Set("assignees", _EncodeStringArray(issue.assignees));
	if (issue.closedAt.has_value()) {
		object.Set("closedAt",
			JsonValue::String(IssueDate::ToISO8601(*issue.closedAt)));
	}

	JsonValue comments = JsonValue::Array();
	for (size_t i = 0; i < issue.comments.size(); i++)
		comments.Append(_EncodeComment(issue.comments[i]));
	object.Set("comments", comments);

	object.Set("createdAt",
		JsonValue::String(IssueDate::ToISO8601(issue.createdAt)));
	object.Set("description", JsonValue::String(issue.description));
	object.Set("environment", JsonValue::String(issue.environment));
	if (issue.estimate.has_value())
		object.Set("estimate", JsonValue::Number(*issue.estimate));
	object.Set("labels", _EncodeStringArray(issue.labels));
	if (issue.milestone.has_value())
		object.Set("milestone", JsonValue::String(*issue.milestone));
	object.Set("notes", JsonValue::String(issue.notes));
	object.Set("number", JsonValue::Integer(issue.number));
	object.Set("priority",
		JsonValue::String(IssuePriorityRawValue(issue.priority)));

	JsonValue relations = JsonValue::Array();
	for (size_t i = 0; i < issue.relations.size(); i++)
		relations.Append(_EncodeRelation(issue.relations[i]));
	object.Set("relations", relations);

	JsonValue remoteLinks = JsonValue::Array();
	for (size_t i = 0; i < issue.remoteLinks.size(); i++)
		remoteLinks.Append(_EncodeRemoteLink(issue.remoteLinks[i]));
	object.Set("remoteLinks", remoteLinks);

	object.Set("reported",
		JsonValue::String(IssueDate::ToISO8601(issue.reported)));
	object.Set("reportedBy", JsonValue::String(issue.reportedBy));
	object.Set("resolution", JsonValue::String(issue.resolution));
	if (issue.resolutionKind.has_value()) {
		object.Set("resolutionKind",
			JsonValue::String(ResolutionKindRawValue(*issue.resolutionKind)));
	}
	object.Set("status", JsonValue::String(IssueStatusRawValue(issue.status)));
	object.Set("stepsToReproduce", _EncodeStringArray(issue.stepsToReproduce));
	object.Set("title", JsonValue::String(issue.title));
	object.Set("type", JsonValue::String(IssueTypeRawValue(issue.type)));
	object.Set("updatedAt",
		JsonValue::String(IssueDate::ToISO8601(issue.updatedAt)));
	object.Set("uuid", JsonValue::String(issue.uuid));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeLabel(const LabelDefinition& label)
{
	JsonValue object = JsonValue::Object();
	if (label.colorHex.has_value())
		object.Set("colorHex", JsonValue::String(*label.colorHex));
	object.Set("description", JsonValue::String(label.description));
	object.Set("name", JsonValue::String(label.name));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeMilestone(const Milestone& milestone)
{
	JsonValue object = JsonValue::Object();
	if (milestone.dueOn.has_value()) {
		object.Set("dueOn",
			JsonValue::String(IssueDate::ToISO8601(*milestone.dueOn)));
	}
	object.Set("isClosed", JsonValue::Bool(milestone.isClosed));
	object.Set("name", JsonValue::String(milestone.name));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodePerson(const Person& person)
{
	JsonValue object = JsonValue::Object();
	object.Set("displayName", JsonValue::String(person.displayName));
	object.Set("email", JsonValue::String(person.email));
	object.Set("handle", JsonValue::String(person.handle));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeGitHub(const GitHubIntegration& integration)
{
	JsonValue object = JsonValue::Object();
	object.Set("defaultAssignees",
		_EncodeStringArray(integration.defaultAssignees));
	object.Set("defaultLabels", _EncodeStringArray(integration.defaultLabels));
	if (integration.defaultMilestone.has_value()) {
		object.Set("defaultMilestone",
			JsonValue::String(*integration.defaultMilestone));
	}
	object.Set("owner", JsonValue::String(integration.owner));
	object.Set("repository", JsonValue::String(integration.repository));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeAzure(const AzureDevOpsIntegration& integration)
{
	JsonValue object = JsonValue::Object();
	if (integration.areaPath.has_value())
		object.Set("areaPath", JsonValue::String(*integration.areaPath));
	object.Set("defaultWorkItemType",
		JsonValue::String(integration.defaultWorkItemType));
	if (integration.iterationPath.has_value()) {
		object.Set("iterationPath",
			JsonValue::String(*integration.iterationPath));
	}
	object.Set("organization", JsonValue::String(integration.organization));
	object.Set("project", JsonValue::String(integration.project));
	if (integration.team.has_value())
		object.Set("team", JsonValue::String(*integration.team));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeIntegrations(const IntegrationSettings& settings)
{
	JsonValue object = JsonValue::Object();
	if (settings.azureDevOps.has_value())
		object.Set("azureDevOps", _EncodeAzure(*settings.azureDevOps));
	if (settings.github.has_value())
		object.Set("github", _EncodeGitHub(*settings.github));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeProject(const ProjectInfo& project)
{
	JsonValue object = JsonValue::Object();
	object.Set("id", JsonValue::String(project.id));
	object.Set("name", JsonValue::String(project.name));
	object.Set("summary", JsonValue::String(project.summary));
	return object;
}


JsonValue
IssuesJsonCoder::_EncodeDocument(const IssuesDocumentModel& model)
{
	JsonValue root = JsonValue::Object();

	JsonValue exportSettings = JsonValue::Object();
	exportSettings.Set("preambleMarkdown",
		JsonValue::String(model.exportSettings.preambleMarkdown));
	root.Set("export", exportSettings);

	root.Set("integrations", _EncodeIntegrations(model.integrations));

	JsonValue issues = JsonValue::Array();
	for (size_t i = 0; i < model.issues.size(); i++)
		issues.Append(EncodeIssue(model.issues[i]));
	root.Set("issues", issues);

	JsonValue labels = JsonValue::Array();
	for (size_t i = 0; i < model.labels.size(); i++)
		labels.Append(_EncodeLabel(model.labels[i]));
	root.Set("labels", labels);

	JsonValue milestones = JsonValue::Array();
	for (size_t i = 0; i < model.milestones.size(); i++)
		milestones.Append(_EncodeMilestone(model.milestones[i]));
	root.Set("milestones", milestones);

	JsonValue people = JsonValue::Array();
	for (size_t i = 0; i < model.people.size(); i++)
		people.Append(_EncodePerson(model.people[i]));
	root.Set("people", people);

	root.Set("project", _EncodeProject(model.project));
	root.Set("schemaVersion", JsonValue::Integer(model.schemaVersion));
	return root;
}


bool
IssuesJsonCoder::Encode(const IssuesDocumentModel& model, std::string& outJson,
	IssuesError& outError)
{
	(void)outError;	// Nothing here can fail; the signature mirrors Decode().
	outJson = _EncodeDocument(model).Write();
	if (outJson.empty() || outJson[outJson.size() - 1] != '\n')
		outJson += '\n';
	return true;
}


// #pragma mark - Decoding

bool
IssuesJsonCoder::_DecodeComment(const JsonValue& value, Comment& outComment,
	IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed("A comment is not an object.");
		return false;
	}
	bool present = false;
	if (!DecodeUuid(value, "id", outComment.id, outError, present))
		return false;
	if (!DecodeString(value, "author", outComment.author, outError, present))
		return false;
	if (!DecodeTimestamp(value, "createdAt", outComment.createdAt, outError,
			present)) {
		return false;
	}
	if (!DecodeString(value, "body", outComment.body, outError, present))
		return false;
	return true;
}


bool
IssuesJsonCoder::_DecodeRelation(const JsonValue& value, Relation& outRelation,
	IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed("A relation is not an object.");
		return false;
	}

	std::string kindRaw;
	bool present = false;
	if (!DecodeString(value, "kind", kindRaw, outError, present))
		return false;
	outRelation.kind = present ? RelationKindFromRawValue(kindRaw)
		: kDefaultRelationKind;

	// The one field in the format with no default.
	if (!DecodeUuid(value, "issueID", outRelation.issueID, outError, present))
		return false;
	if (!present) {
		outError = IssuesError::DecodingFailed(
			"A relation has no \"issueID\"; a relation pointing nowhere is not "
			"a relation.");
		return false;
	}
	return true;
}


bool
IssuesJsonCoder::_DecodeRemoteLink(const JsonValue& value, RemoteLink& outLink,
	IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed(
			"A remote link is not an object.");
		return false;
	}

	std::string providerRaw;
	bool present = false;
	if (!DecodeString(value, "provider", providerRaw, outError, present))
		return false;
	// Absent -> github. Present but unknown -> preserved verbatim.
	outLink.provider = present ? RemoteProvider(providerRaw)
		: RemoteProvider::GitHub();

	if (!DecodeString(value, "identifier", outLink.identifier, outError,
			present)) {
		return false;
	}
	if (!DecodeOptionalString(value, "url", outLink.url, outError))
		return false;
	if (!DecodeOptionalTimestamp(value, "lastSyncedAt", outLink.lastSyncedAt,
			outError)) {
		return false;
	}
	if (!DecodeOptionalTimestamp(value, "remoteUpdatedAt",
			outLink.remoteUpdatedAt, outError)) {
		return false;
	}
	return true;
}


bool
IssuesJsonCoder::DecodeIssue(const JsonValue& value, Issue& outIssue,
	IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed("An issue is not an object.");
		return false;
	}

	bool present = false;
	if (!DecodeUuid(value, "uuid", outIssue.uuid, outError, present))
		return false;
	if (!DecodeInt(value, "number", outIssue.number, outError, present))
		return false;
	if (!DecodeString(value, "title", outIssue.title, outError, present))
		return false;

	std::string raw;
	if (!DecodeString(value, "type", raw, outError, present))
		return false;
	outIssue.type = present ? IssueTypeFromRawValue(raw) : kDefaultIssueType;

	if (!DecodeString(value, "priority", raw, outError, present))
		return false;
	outIssue.priority = present ? IssuePriorityFromRawValue(raw)
		: kDefaultIssuePriority;

	if (!DecodeString(value, "status", raw, outError, present))
		return false;
	outIssue.status = present ? IssueStatusFromRawValue(raw)
		: kDefaultIssueStatus;

	// Unknown or absent resolutionKind stays unset: never a guessed default.
	if (!DecodeString(value, "resolutionKind", raw, outError, present))
		return false;
	if (present) {
		ResolutionKind kind;
		if (ResolutionKindFromRawValue(raw, kind))
			outIssue.resolutionKind = kind;
	}

	if (!DecodeStringArray(value, "labels", outIssue.labels, outError))
		return false;
	if (!DecodeStringArray(value, "assignees", outIssue.assignees, outError))
		return false;
	if (!DecodeOptionalString(value, "milestone", outIssue.milestone, outError))
		return false;
	if (!DecodeString(value, "area", outIssue.area, outError, present))
		return false;
	if (!DecodeOptionalDouble(value, "estimate", outIssue.estimate, outError))
		return false;

	if (!DecodeString(value, "reportedBy", outIssue.reportedBy, outError,
			present)) {
		return false;
	}
	if (!DecodeTimestamp(value, "reported", outIssue.reported, outError,
			present)) {
		return false;
	}
	if (!DecodeTimestamp(value, "createdAt", outIssue.createdAt, outError,
			present)) {
		return false;
	}
	if (!DecodeTimestamp(value, "updatedAt", outIssue.updatedAt, outError,
			present)) {
		return false;
	}
	if (!DecodeOptionalTimestamp(value, "closedAt", outIssue.closedAt,
			outError)) {
		return false;
	}

	if (!DecodeString(value, "description", outIssue.description, outError,
			present)) {
		return false;
	}
	if (!DecodeStringArray(value, "stepsToReproduce", outIssue.stepsToReproduce,
			outError)) {
		return false;
	}
	if (!DecodeString(value, "environment", outIssue.environment, outError,
			present)) {
		return false;
	}
	if (!DecodeString(value, "notes", outIssue.notes, outError, present))
		return false;
	if (!DecodeString(value, "resolution", outIssue.resolution, outError,
			present)) {
		return false;
	}

	bool valid = true;
	const JsonValue* array = ArrayMember(value, "comments", outError, valid);
	if (!valid)
		return false;
	if (array != NULL) {
		outIssue.comments.clear();
		for (size_t i = 0; i < array->CountItems(); i++) {
			Comment comment;
			if (!_DecodeComment(array->ItemAt(i), comment, outError))
				return false;
			outIssue.comments.push_back(comment);
		}
	}

	array = ArrayMember(value, "relations", outError, valid);
	if (!valid)
		return false;
	if (array != NULL) {
		outIssue.relations.clear();
		for (size_t i = 0; i < array->CountItems(); i++) {
			Relation relation;
			if (!_DecodeRelation(array->ItemAt(i), relation, outError))
				return false;
			outIssue.relations.push_back(relation);
		}
	}

	array = ArrayMember(value, "remoteLinks", outError, valid);
	if (!valid)
		return false;
	if (array != NULL) {
		outIssue.remoteLinks.clear();
		for (size_t i = 0; i < array->CountItems(); i++) {
			RemoteLink link;
			if (!_DecodeRemoteLink(array->ItemAt(i), link, outError))
				return false;
			outIssue.remoteLinks.push_back(link);
		}
	}

	return true;
}


bool
IssuesJsonCoder::_DecodeLabel(const JsonValue& value, LabelDefinition& outLabel,
	IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed("A label is not an object.");
		return false;
	}
	bool present = false;
	if (!DecodeString(value, "name", outLabel.name, outError, present))
		return false;
	if (!DecodeOptionalString(value, "colorHex", outLabel.colorHex, outError))
		return false;
	if (!DecodeString(value, "description", outLabel.description, outError,
			present)) {
		return false;
	}
	return true;
}


bool
IssuesJsonCoder::_DecodeMilestone(const JsonValue& value,
	Milestone& outMilestone, IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed("A milestone is not an object.");
		return false;
	}
	bool present = false;
	if (!DecodeString(value, "name", outMilestone.name, outError, present))
		return false;
	if (!DecodeOptionalTimestamp(value, "dueOn", outMilestone.dueOn, outError))
		return false;
	if (!DecodeBool(value, "isClosed", outMilestone.isClosed, outError))
		return false;
	return true;
}


bool
IssuesJsonCoder::_DecodePerson(const JsonValue& value, Person& outPerson,
	IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed("A person is not an object.");
		return false;
	}
	bool present = false;
	if (!DecodeString(value, "handle", outPerson.handle, outError, present))
		return false;
	if (!DecodeString(value, "displayName", outPerson.displayName, outError,
			present)) {
		return false;
	}
	if (!DecodeString(value, "email", outPerson.email, outError, present))
		return false;
	return true;
}


bool
IssuesJsonCoder::_DecodeGitHub(const JsonValue& value,
	GitHubIntegration& outIntegration, IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed(
			"\"integrations.github\" is not an object.");
		return false;
	}
	bool present = false;
	if (!DecodeString(value, "owner", outIntegration.owner, outError, present))
		return false;
	if (!DecodeString(value, "repository", outIntegration.repository, outError,
			present)) {
		return false;
	}
	if (!DecodeStringArray(value, "defaultLabels", outIntegration.defaultLabels,
			outError)) {
		return false;
	}
	if (!DecodeStringArray(value, "defaultAssignees",
			outIntegration.defaultAssignees, outError)) {
		return false;
	}
	if (!DecodeOptionalString(value, "defaultMilestone",
			outIntegration.defaultMilestone, outError)) {
		return false;
	}
	return true;
}


bool
IssuesJsonCoder::_DecodeAzure(const JsonValue& value,
	AzureDevOpsIntegration& outIntegration, IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed(
			"\"integrations.azureDevOps\" is not an object.");
		return false;
	}
	bool present = false;
	if (!DecodeString(value, "organization", outIntegration.organization,
			outError, present)) {
		return false;
	}
	if (!DecodeString(value, "project", outIntegration.project, outError,
			present)) {
		return false;
	}
	if (!DecodeOptionalString(value, "team", outIntegration.team, outError))
		return false;
	if (!DecodeOptionalString(value, "areaPath", outIntegration.areaPath,
			outError)) {
		return false;
	}
	if (!DecodeOptionalString(value, "iterationPath",
			outIntegration.iterationPath, outError)) {
		return false;
	}
	if (!DecodeString(value, "defaultWorkItemType",
			outIntegration.defaultWorkItemType, outError, present)) {
		return false;
	}
	return true;
}


bool
IssuesJsonCoder::_DecodeIntegrations(const JsonValue& value,
	IntegrationSettings& outSettings, IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed(
			"\"integrations\" is not an object.");
		return false;
	}

	const JsonValue* github = Member(value, "github");
	if (github != NULL) {
		GitHubIntegration integration;
		if (!_DecodeGitHub(*github, integration, outError))
			return false;
		outSettings.github = integration;
	}

	const JsonValue* azure = Member(value, "azureDevOps");
	if (azure != NULL) {
		AzureDevOpsIntegration integration;
		if (!_DecodeAzure(*azure, integration, outError))
			return false;
		outSettings.azureDevOps = integration;
	}
	return true;
}


bool
IssuesJsonCoder::_DecodeProject(const JsonValue& value, ProjectInfo& outProject,
	IssuesError& outError)
{
	if (!value.IsObject()) {
		outError = IssuesError::DecodingFailed("\"project\" is not an object.");
		return false;
	}
	bool present = false;
	if (!DecodeUuid(value, "id", outProject.id, outError, present))
		return false;
	if (!DecodeString(value, "name", outProject.name, outError, present))
		return false;
	if (!DecodeString(value, "summary", outProject.summary, outError, present))
		return false;
	return true;
}


bool
IssuesJsonCoder::Decode(const std::string& json, IssuesDocumentModel& outModel,
	IssuesError& outError)
{
	JsonValue root;
	std::string parseError;
	if (!JsonParser::Parse(json, root, parseError)) {
		outError = IssuesError::DecodingFailed(parseError);
		return false;
	}
	if (!root.IsObject()) {
		outError = IssuesError::DecodingFailed(
			"The document root is not a JSON object.");
		return false;
	}

	IssuesDocumentModel model;

	// schemaVersion is required, and a newer document is refused outright rather
	// than opened with its unknown fields silently dropped.
	bool present = false;
	int version = 0;
	if (!DecodeInt(root, "schemaVersion", version, outError, present))
		return false;
	if (!present) {
		outError = IssuesError::MissingSchemaVersion();
		return false;
	}
	if (version > IssuesDocumentModel::kSupportedSchemaVersion) {
		outError = IssuesError::UnsupportedSchemaVersion(version,
			IssuesDocumentModel::kSupportedSchemaVersion);
		return false;
	}
	// Migration seam: only version 1 exists today, so an older version is read
	// as-is. When version 2 lands, branch here -- decode the old shape,
	// transform it, and set schemaVersion to the migrated value so the next save
	// writes the new shape.
	model.schemaVersion = version;

	const JsonValue* project = Member(root, "project");
	if (project != NULL && !_DecodeProject(*project, model.project, outError))
		return false;

	const JsonValue* integrations = Member(root, "integrations");
	if (integrations != NULL
		&& !_DecodeIntegrations(*integrations, model.integrations, outError)) {
		return false;
	}

	bool valid = true;
	const JsonValue* array = ArrayMember(root, "labels", outError, valid);
	if (!valid)
		return false;
	if (array != NULL) {
		for (size_t i = 0; i < array->CountItems(); i++) {
			LabelDefinition label;
			if (!_DecodeLabel(array->ItemAt(i), label, outError))
				return false;
			model.labels.push_back(label);
		}
	}

	array = ArrayMember(root, "milestones", outError, valid);
	if (!valid)
		return false;
	if (array != NULL) {
		for (size_t i = 0; i < array->CountItems(); i++) {
			Milestone milestone;
			if (!_DecodeMilestone(array->ItemAt(i), milestone, outError))
				return false;
			model.milestones.push_back(milestone);
		}
	}

	array = ArrayMember(root, "people", outError, valid);
	if (!valid)
		return false;
	if (array != NULL) {
		for (size_t i = 0; i < array->CountItems(); i++) {
			Person person;
			if (!_DecodePerson(array->ItemAt(i), person, outError))
				return false;
			model.people.push_back(person);
		}
	}

	const JsonValue* exportSettings = Member(root, "export");
	if (exportSettings != NULL) {
		if (!exportSettings->IsObject()) {
			outError = IssuesError::DecodingFailed(
				"\"export\" is not an object.");
			return false;
		}
		if (!DecodeString(*exportSettings, "preambleMarkdown",
				model.exportSettings.preambleMarkdown, outError, present)) {
			return false;
		}
	}

	array = ArrayMember(root, "issues", outError, valid);
	if (!valid)
		return false;
	if (array != NULL) {
		for (size_t i = 0; i < array->CountItems(); i++) {
			Issue issue;
			if (!DecodeIssue(array->ItemAt(i), issue, outError))
				return false;
			model.issues.push_back(issue);
		}
	}

	outModel = model;
	return true;
}

} // namespace issueskit
