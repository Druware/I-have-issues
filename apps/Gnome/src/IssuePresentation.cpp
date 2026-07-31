/*
 * IssuePresentation.cpp
 */
#include "IssuePresentation.h"

#include <glib.h>

using namespace issueskit;

namespace ihaveissues {

const char*
IssueTypeIconName(IssueType type)
{
	// VERIFY (icon names, whole function): these must exist in the installed
	// adwaita-icon-theme, or GTK draws "image-missing". All five are either in
	// the freedesktop icon naming specification (dialog-warning, dialog-question)
	// or long-standing Adwaita symbolics (starred, object-select). Check with
	//   gtk4-icon-browser
	// on the target system and swap any that are absent.
	switch (type) {
		case kIssueTypeBug:			return "dialog-warning-symbolic";
		case kIssueTypeFeature:		return "starred-symbolic";
		case kIssueTypeTask:		return "object-select-symbolic";
		case kIssueTypeQuestion:	return "dialog-question-symbolic";
	}
	return "object-select-symbolic";
}


const char*
IssueTypeStyleClass(IssueType type)
{
	switch (type) {
		case kIssueTypeBug:			return "error";
		case kIssueTypeFeature:		return "accent";
		case kIssueTypeTask:		return NULL;
		case kIssueTypeQuestion:	return "dim-label";
	}
	return NULL;
}


const char*
IssuePriorityIconName(IssuePriority priority)
{
	// VERIFY: as above. go-down/go-up are freedesktop standard names;
	// emblem-important-symbolic and view-more-horizontal-symbolic are Adwaita.
	switch (priority) {
		case kIssuePriorityLow:		return "go-down-symbolic";
		case kIssuePriorityMedium:	return "view-more-horizontal-symbolic";
		case kIssuePriorityHigh:	return "go-up-symbolic";
		case kIssuePriorityCritical: return "emblem-important-symbolic";
	}
	return "view-more-horizontal-symbolic";
}


const char*
IssuePriorityStyleClass(IssuePriority priority)
{
	switch (priority) {
		case kIssuePriorityLow:		return "dim-label";
		case kIssuePriorityMedium:	return "dim-label";
		case kIssuePriorityHigh:	return "warning";
		case kIssuePriorityCritical: return "error";
	}
	return "dim-label";
}


const char*
IssueStatusIconName(IssueStatus status)
{
	// VERIFY: media-record-symbolic and content-loading-symbolic are Adwaita
	// names; action-unavailable-symbolic is Adwaita and may be absent on very
	// old themes -- process-stop-symbolic is the freedesktop fallback.
	switch (status) {
		case kIssueStatusOpen:		return "media-record-symbolic";
		case kIssueStatusInProgress: return "content-loading-symbolic";
		case kIssueStatusBlocked:	return "action-unavailable-symbolic";
		case kIssueStatusResolved:	return "emblem-ok-symbolic";
	}
	return "media-record-symbolic";
}


const char*
IssueStatusStyleClass(IssueStatus status)
{
	switch (status) {
		case kIssueStatusOpen:		return "dim-label";
		case kIssueStatusInProgress: return "accent";
		case kIssueStatusBlocked:	return "warning";
		case kIssueStatusResolved:	return "success";
	}
	return "dim-label";
}


std::string
IssueTitlePlain(const Issue& issue)
{
	return issue.title.empty() ? std::string("Untitled") : issue.title;
}


std::string
IssueTitleMarkup(const Issue& issue)
{
	std::string plain = IssueTitlePlain(issue);

	// g_markup_escape_text returns a newly allocated string the caller owns.
	gchar* escaped = g_markup_escape_text(plain.c_str(), -1);
	std::string markup = std::string("<b>") + escaped + "</b>";
	g_free(escaped);
	return markup;
}

} // namespace ihaveissues
