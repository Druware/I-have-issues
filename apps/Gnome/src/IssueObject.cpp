/*
 * IssueObject.cpp
 */
#include "IssueObject.h"

#include "IssuePresentation.h"

using namespace issueskit;

struct _IhiIssue {
	GObject			parent_instance;

	char*			uuid;
	char*			titleMarkup;
	char*			numberText;
	IssueType		kind;
	IssuePriority	priority;
};

G_DEFINE_TYPE(IhiIssue, ihi_issue, G_TYPE_OBJECT)


static void
ihi_issue_finalize(GObject* object)
{
	IhiIssue* self = IHI_ISSUE(object);

	g_clear_pointer(&self->uuid, g_free);
	g_clear_pointer(&self->titleMarkup, g_free);
	g_clear_pointer(&self->numberText, g_free);

	G_OBJECT_CLASS(ihi_issue_parent_class)->finalize(object);
}


static void
ihi_issue_class_init(IhiIssueClass* klass)
{
	G_OBJECT_CLASS(klass)->finalize = ihi_issue_finalize;
}


static void
ihi_issue_init(IhiIssue* self)
{
	self->uuid = NULL;
	self->titleMarkup = NULL;
	self->numberText = NULL;
	self->kind = kDefaultIssueType;
	self->priority = kDefaultIssuePriority;
}


IhiIssue*
ihi_issue_new(const Issue& issue)
{
	IhiIssue* self = IHI_ISSUE(g_object_new(IHI_TYPE_ISSUE, NULL));

	self->uuid = g_strdup(issue.uuid.c_str());
	self->titleMarkup = g_strdup(ihaveissues::IssueTitleMarkup(issue).c_str());
	self->numberText = g_strdup(issue.DisplayNumber().c_str());
	self->kind = issue.type;
	self->priority = issue.priority;

	return self;
}


const char*
ihi_issue_get_uuid(IhiIssue* self)
{
	g_return_val_if_fail(IHI_IS_ISSUE(self), "");
	return self->uuid != NULL ? self->uuid : "";
}


const char*
ihi_issue_get_title_markup(IhiIssue* self)
{
	g_return_val_if_fail(IHI_IS_ISSUE(self), "");
	return self->titleMarkup != NULL ? self->titleMarkup : "";
}


const char*
ihi_issue_get_number_text(IhiIssue* self)
{
	g_return_val_if_fail(IHI_IS_ISSUE(self), "");
	return self->numberText != NULL ? self->numberText : "";
}


IssueType
ihi_issue_get_kind(IhiIssue* self)
{
	g_return_val_if_fail(IHI_IS_ISSUE(self), kDefaultIssueType);
	return self->kind;
}


IssuePriority
ihi_issue_get_priority(IhiIssue* self)
{
	g_return_val_if_fail(IHI_IS_ISSUE(self), kDefaultIssuePriority);
	return self->priority;
}
