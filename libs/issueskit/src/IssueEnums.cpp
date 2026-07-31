/*
 * IssueEnums.cpp
 */
#include <issueskit/IssueEnums.h>

#include <issueskit/StringUtils.h>

namespace issueskit {

namespace {

struct EnumEntry {
	const char*	rawValue;
	const char*	displayName;
};

const EnumEntry kIssueTypes[kIssueTypeCount] = {
	{ "bug", "Bug" },
	{ "feature", "Feature" },
	{ "task", "Task" },
	{ "question", "Question" }
};

const EnumEntry kIssuePriorities[kIssuePriorityCount] = {
	{ "low", "Low" },
	{ "medium", "Medium" },
	{ "high", "High" },
	{ "critical", "Critical" }
};

const EnumEntry kIssueStatuses[kIssueStatusCount] = {
	{ "open", "Open" },
	{ "inProgress", "In Progress" },
	{ "blocked", "Blocked" },
	{ "resolved", "Resolved" }
};

const EnumEntry kResolutionKinds[kResolutionKindCount] = {
	{ "fixed", "Fixed" },
	{ "wontFix", "Won't Fix" },
	{ "duplicate", "Duplicate" },
	{ "cannotReproduce", "Cannot Reproduce" },
	{ "byDesign", "By Design" }
};

const EnumEntry kRelationKinds[kRelationKindCount] = {
	{ "blocks", "Blocks" },
	{ "blockedBy", "Blocked By" },
	{ "duplicateOf", "Duplicate Of" },
	{ "relatedTo", "Related To" },
	{ "parent", "Parent" },
	{ "child", "Child" }
};


int
IndexOfRawValue(const EnumEntry* entries, int count, const std::string& raw)
{
	for (int i = 0; i < count; i++) {
		if (raw == entries[i].rawValue)
			return i;
	}
	return -1;
}


int
IndexOfDisplayName(const EnumEntry* entries, int count,
	const std::string& displayName)
{
	std::string needle = ToLower(Trim(displayName));
	for (int i = 0; i < count; i++) {
		if (needle == ToLower(std::string(entries[i].displayName)))
			return i;
	}
	return -1;
}


int
ClampIndex(int index, int count)
{
	if (index < 0)
		return 0;
	if (index >= count)
		return count - 1;
	return index;
}

} // unnamed namespace


// #pragma mark - IssueType

const char*
IssueTypeRawValue(IssueType value)
{
	return kIssueTypes[ClampIndex((int)value, kIssueTypeCount)].rawValue;
}


const char*
IssueTypeDisplayName(IssueType value)
{
	return kIssueTypes[ClampIndex((int)value, kIssueTypeCount)].displayName;
}


IssueType
IssueTypeFromRawValue(const std::string& raw)
{
	int index = IndexOfRawValue(kIssueTypes, kIssueTypeCount, raw);
	return index < 0 ? kDefaultIssueType : (IssueType)index;
}


bool
IssueTypeFromDisplayName(const std::string& displayName, IssueType& outValue)
{
	int index = IndexOfDisplayName(kIssueTypes, kIssueTypeCount, displayName);
	if (index < 0)
		return false;
	outValue = (IssueType)index;
	return true;
}


IssueType
IssueTypeAt(int index)
{
	return (IssueType)ClampIndex(index, kIssueTypeCount);
}


// #pragma mark - IssuePriority

const char*
IssuePriorityRawValue(IssuePriority value)
{
	return kIssuePriorities[ClampIndex((int)value, kIssuePriorityCount)]
		.rawValue;
}


const char*
IssuePriorityDisplayName(IssuePriority value)
{
	return kIssuePriorities[ClampIndex((int)value, kIssuePriorityCount)]
		.displayName;
}


IssuePriority
IssuePriorityFromRawValue(const std::string& raw)
{
	int index = IndexOfRawValue(kIssuePriorities, kIssuePriorityCount, raw);
	return index < 0 ? kDefaultIssuePriority : (IssuePriority)index;
}


bool
IssuePriorityFromDisplayName(const std::string& displayName,
	IssuePriority& outValue)
{
	int index = IndexOfDisplayName(kIssuePriorities, kIssuePriorityCount,
		displayName);
	if (index < 0)
		return false;
	outValue = (IssuePriority)index;
	return true;
}


IssuePriority
IssuePriorityAt(int index)
{
	return (IssuePriority)ClampIndex(index, kIssuePriorityCount);
}


// #pragma mark - IssueStatus

const char*
IssueStatusRawValue(IssueStatus value)
{
	return kIssueStatuses[ClampIndex((int)value, kIssueStatusCount)].rawValue;
}


const char*
IssueStatusDisplayName(IssueStatus value)
{
	return kIssueStatuses[ClampIndex((int)value, kIssueStatusCount)]
		.displayName;
}


IssueStatus
IssueStatusFromRawValue(const std::string& raw)
{
	int index = IndexOfRawValue(kIssueStatuses, kIssueStatusCount, raw);
	return index < 0 ? kDefaultIssueStatus : (IssueStatus)index;
}


bool
IssueStatusFromDisplayName(const std::string& displayName,
	IssueStatus& outValue)
{
	int index = IndexOfDisplayName(kIssueStatuses, kIssueStatusCount,
		displayName);
	if (index < 0)
		return false;
	outValue = (IssueStatus)index;
	return true;
}


IssueStatus
IssueStatusAt(int index)
{
	return (IssueStatus)ClampIndex(index, kIssueStatusCount);
}


// #pragma mark - ResolutionKind

const char*
ResolutionKindRawValue(ResolutionKind value)
{
	return kResolutionKinds[ClampIndex((int)value, kResolutionKindCount)]
		.rawValue;
}


const char*
ResolutionKindDisplayName(ResolutionKind value)
{
	return kResolutionKinds[ClampIndex((int)value, kResolutionKindCount)]
		.displayName;
}


bool
ResolutionKindFromRawValue(const std::string& raw, ResolutionKind& outValue)
{
	int index = IndexOfRawValue(kResolutionKinds, kResolutionKindCount, raw);
	if (index < 0)
		return false;
	outValue = (ResolutionKind)index;
	return true;
}


bool
ResolutionKindFromDisplayName(const std::string& displayName,
	ResolutionKind& outValue)
{
	int index = IndexOfDisplayName(kResolutionKinds, kResolutionKindCount,
		displayName);
	if (index < 0)
		return false;
	outValue = (ResolutionKind)index;
	return true;
}


ResolutionKind
ResolutionKindAt(int index)
{
	return (ResolutionKind)ClampIndex(index, kResolutionKindCount);
}


// #pragma mark - RelationKind

const char*
RelationKindRawValue(RelationKind value)
{
	return kRelationKinds[ClampIndex((int)value, kRelationKindCount)].rawValue;
}


const char*
RelationKindDisplayName(RelationKind value)
{
	return kRelationKinds[ClampIndex((int)value, kRelationKindCount)]
		.displayName;
}


RelationKind
RelationKindFromRawValue(const std::string& raw)
{
	int index = IndexOfRawValue(kRelationKinds, kRelationKindCount, raw);
	return index < 0 ? kDefaultRelationKind : (RelationKind)index;
}


bool
RelationKindFromDisplayName(const std::string& displayName,
	RelationKind& outValue)
{
	int index = IndexOfDisplayName(kRelationKinds, kRelationKindCount,
		displayName);
	if (index < 0)
		return false;
	outValue = (RelationKind)index;
	return true;
}


RelationKind
RelationKindAt(int index)
{
	return (RelationKind)ClampIndex(index, kRelationKindCount);
}


// #pragma mark - RemoteProvider

RemoteProvider::RemoteProvider()
	:
	fKind(kGitHub),
	fRawValue("github")
{
}


RemoteProvider::RemoteProvider(const std::string& rawValue)
	:
	fRawValue(rawValue)
{
	if (rawValue == "github")
		fKind = kGitHub;
	else if (rawValue == "azureDevOps")
		fKind = kAzureDevOps;
	else
		fKind = kOther;
}


RemoteProvider
RemoteProvider::GitHub()
{
	return RemoteProvider("github");
}


RemoteProvider
RemoteProvider::AzureDevOps()
{
	return RemoteProvider("azureDevOps");
}


std::vector<RemoteProvider>
RemoteProvider::SelectableCases()
{
	std::vector<RemoteProvider> cases;
	cases.push_back(GitHub());
	cases.push_back(AzureDevOps());
	return cases;
}


std::string
RemoteProvider::DisplayName() const
{
	switch (fKind) {
		case kGitHub:
			return "GitHub";
		case kAzureDevOps:
			return "Azure DevOps";
		case kOther:
		default:
			return fRawValue;
	}
}


bool
RemoteProvider::operator==(const RemoteProvider& other) const
{
	return fKind == other.fKind && fRawValue == other.fRawValue;
}


bool
RemoteProvider::operator!=(const RemoteProvider& other) const
{
	return !(*this == other);
}

} // namespace issueskit
