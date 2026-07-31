/*
 * IssueEnums.h -- the six enumerations of the .issues format.
 *
 * The raw strings below ARE the on-disk format and must never change. Display
 * names are presentation only: reword them freely.
 *
 * Decoding rules, straight from the Apple implementation:
 *   - IssueType / IssuePriority / IssueStatus / RelationKind: an absent or
 *     unrecognized raw value becomes that enum's documented default;
 *   - ResolutionKind: an absent or unrecognized raw value becomes "no value",
 *     never a guessed default -- inventing a reason a bug closed misreports it;
 *   - RemoteProvider: an unrecognized raw value is PRESERVED verbatim and
 *     re-encodes byte-identically. It is sync identity, so coercing "gitlab" to
 *     "github" would point GitHub sync at an unrelated issue. Only an entirely
 *     absent key defaults to github.
 */
#ifndef ISSUESKIT_ISSUE_ENUMS_H
#define ISSUESKIT_ISSUE_ENUMS_H

#include <string>
#include <vector>

namespace issueskit {

// #pragma mark - IssueType

enum IssueType {
	kIssueTypeBug = 0,
	kIssueTypeFeature,
	kIssueTypeTask,
	kIssueTypeQuestion
};

const int kIssueTypeCount = 4;
const IssueType kDefaultIssueType = kIssueTypeTask;

const char* IssueTypeRawValue(IssueType value);
const char* IssueTypeDisplayName(IssueType value);
//! Unrecognized raw values become kDefaultIssueType.
IssueType IssueTypeFromRawValue(const std::string& raw);
//! Case-insensitive display-name match; returns false when nothing matches.
bool IssueTypeFromDisplayName(const std::string& displayName,
	IssueType& outValue);
IssueType IssueTypeAt(int index);

// #pragma mark - IssuePriority

enum IssuePriority {
	kIssuePriorityLow = 0,
	kIssuePriorityMedium,
	kIssuePriorityHigh,
	kIssuePriorityCritical
};

const int kIssuePriorityCount = 4;
const IssuePriority kDefaultIssuePriority = kIssuePriorityMedium;

const char* IssuePriorityRawValue(IssuePriority value);
const char* IssuePriorityDisplayName(IssuePriority value);
IssuePriority IssuePriorityFromRawValue(const std::string& raw);
bool IssuePriorityFromDisplayName(const std::string& displayName,
	IssuePriority& outValue);
IssuePriority IssuePriorityAt(int index);

// #pragma mark - IssueStatus

enum IssueStatus {
	kIssueStatusOpen = 0,
	kIssueStatusInProgress,
	kIssueStatusBlocked,
	kIssueStatusResolved
};

const int kIssueStatusCount = 4;
const IssueStatus kDefaultIssueStatus = kIssueStatusOpen;

const char* IssueStatusRawValue(IssueStatus value);
const char* IssueStatusDisplayName(IssueStatus value);
IssueStatus IssueStatusFromRawValue(const std::string& raw);
bool IssueStatusFromDisplayName(const std::string& displayName,
	IssueStatus& outValue);
IssueStatus IssueStatusAt(int index);

// #pragma mark - ResolutionKind

enum ResolutionKind {
	kResolutionKindFixed = 0,
	kResolutionKindWontFix,
	kResolutionKindDuplicate,
	kResolutionKindCannotReproduce,
	kResolutionKindByDesign
};

const int kResolutionKindCount = 5;

const char* ResolutionKindRawValue(ResolutionKind value);
const char* ResolutionKindDisplayName(ResolutionKind value);
//! No default: an unrecognized value must not become a guessed close reason.
bool ResolutionKindFromRawValue(const std::string& raw,
	ResolutionKind& outValue);
bool ResolutionKindFromDisplayName(const std::string& displayName,
	ResolutionKind& outValue);
ResolutionKind ResolutionKindAt(int index);

// #pragma mark - RelationKind

enum RelationKind {
	kRelationKindBlocks = 0,
	kRelationKindBlockedBy,
	kRelationKindDuplicateOf,
	kRelationKindRelatedTo,
	kRelationKindParent,
	kRelationKindChild
};

const int kRelationKindCount = 6;
const RelationKind kDefaultRelationKind = kRelationKindRelatedTo;

const char* RelationKindRawValue(RelationKind value);
const char* RelationKindDisplayName(RelationKind value);
RelationKind RelationKindFromRawValue(const std::string& raw);
bool RelationKindFromDisplayName(const std::string& displayName,
	RelationKind& outValue);
RelationKind RelationKindAt(int index);

// #pragma mark - RemoteProvider

/*!	The external tracker a RemoteLink points at.

	Not a plain enum: an unknown provider keeps its raw string so it survives a
	round trip untouched. Sync code must gate on IsGitHub()/IsAzureDevOps() and
	never on a raw-string comparison.
*/
class RemoteProvider {
public:
								//! Defaults to GitHub, matching the format.
								RemoteProvider();
	explicit					RemoteProvider(const std::string& rawValue);

	static	RemoteProvider		GitHub();
	static	RemoteProvider		AzureDevOps();

	//! The providers a UI picker should offer. ".other" has no finite domain.
	static	std::vector<RemoteProvider> SelectableCases();

			const std::string&	RawValue() const { return fRawValue; }
			//! An unknown provider shows its raw value, not a borrowed name.
			std::string			DisplayName() const;

			bool				IsGitHub() const { return fKind == kGitHub; }
			bool				IsAzureDevOps() const
									{ return fKind == kAzureDevOps; }

			bool				operator==(const RemoteProvider& other) const;
			bool				operator!=(const RemoteProvider& other) const;

private:
			enum Kind {
				kGitHub,
				kAzureDevOps,
				kOther
			};

			Kind				fKind;
			std::string			fRawValue;
};

} // namespace issueskit

#endif // ISSUESKIT_ISSUE_ENUMS_H
