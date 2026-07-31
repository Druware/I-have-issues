/*
 * IssueDetailView.cpp
 */
#include "IssueDetailView.h"

#include <cstdio>
#include <string>
#include <vector>

#include "IssuePresentation.h"

#include <issueskit/IssueDate.h>
#include <issueskit/IssueEnums.h>
#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

namespace {

/*!	Adds one "field: value" row.

	AdwActionRow renders its title as Pango markup and its subtitle as markup
	too, so both are escaped: an issue title containing "<" would otherwise
	either vanish or produce a warning.
*/
void
AppendMetadataRow(AdwPreferencesGroup* group, const char* title,
	const std::string& value, const char* iconName, const char* styleClass)
{
	AdwActionRow* row = ADW_ACTION_ROW(adw_action_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);

	gchar* escaped = g_markup_escape_text(value.c_str(), -1);
	adw_action_row_set_subtitle(row, escaped);
	g_free(escaped);

	// VERIFY: adw_action_row_set_subtitle_lines(row, 0) means "no limit" and
	// exists since libadwaita 1.3. Without it a long value is ellipsized to one
	// line instead of wrapping.
	adw_action_row_set_subtitle_lines(row, 0);

	if (iconName != NULL) {
		GtkWidget* icon = gtk_image_new_from_icon_name(iconName);
		if (styleClass != NULL)
			gtk_widget_add_css_class(icon, styleClass);
		adw_action_row_add_prefix(row, icon);
	}

	adw_preferences_group_add(group, GTK_WIDGET(row));
}


//! A wrapped, selectable, non-interpreting body of free text.
GtkWidget*
CreateBodyLabel(const std::string& text)
{
	GtkWidget* label = gtk_label_new(NULL);

	// gtk_label_set_text, not set_markup: the .issues format keeps markdown in
	// these fields and it is shown verbatim, exactly as the Haiku port does.
	gtk_label_set_text(GTK_LABEL(label), text.c_str());
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);
	gtk_widget_add_css_class(label, "ihi-body-text");

	return label;
}


//! Adds a titled section of free text, or nothing at all when it is empty.
void
AppendTextSection(AdwPreferencesPage* page, const char* title,
	const std::string& body)
{
	if (body.empty())
		return;

	AdwPreferencesGroup* group
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(group, title);
	adw_preferences_group_add(group, CreateBodyLabel(body));
	adw_preferences_page_add(page, group);
}


//! "1. first\n2. second" -- the numbering the markdown export uses.
std::string
NumberedSteps(const std::vector<std::string>& steps)
{
	std::string text;
	for (size_t i = 0; i < steps.size(); i++) {
		char prefix[32];
		snprintf(prefix, sizeof(prefix), "%lu. ", (unsigned long)(i + 1));
		if (i > 0)
			text += "\n";
		text += prefix;
		text += steps[i];
	}
	return text;
}

} // unnamed namespace


GtkWidget*
CreateNoSelectionPage()
{
	GtkWidget* status = adw_status_page_new();
	// VERIFY: "view-list-bullet-symbolic" is an Adwaita name, not a
	// freedesktop-standard one. "view-list-symbolic" is the fallback.
	adw_status_page_set_icon_name(ADW_STATUS_PAGE(status),
		"view-list-bullet-symbolic");
	adw_status_page_set_title(ADW_STATUS_PAGE(status), "No Issue Selected");
	adw_status_page_set_description(ADW_STATUS_PAGE(status),
		"Select an issue from the list, or add a new one.");
	return status;
}


GtkWidget*
CreateIssueDetailPage(const Issue& issue, const IssuesDocumentModel& model)
{
	AdwPreferencesPage* page
		= ADW_PREFERENCES_PAGE(adw_preferences_page_new());

	// #pragma mark Header metadata

	AdwPreferencesGroup* header
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	{
		std::string heading = issue.DisplayNumber() + "  "
			+ IssueTitlePlain(issue);
		gchar* escaped = g_markup_escape_text(heading.c_str(), -1);
		adw_preferences_group_set_title(header, escaped);
		g_free(escaped);
	}

	AppendMetadataRow(header, "Type", IssueTypeDisplayName(issue.type),
		IssueTypeIconName(issue.type), IssueTypeStyleClass(issue.type));
	AppendMetadataRow(header, "Priority",
		IssuePriorityDisplayName(issue.priority),
		IssuePriorityIconName(issue.priority),
		IssuePriorityStyleClass(issue.priority));
	AppendMetadataRow(header, "Status", IssueStatusDisplayName(issue.status),
		IssueStatusIconName(issue.status),
		IssueStatusStyleClass(issue.status));

	if (issue.resolutionKind.has_value()) {
		AppendMetadataRow(header, "Resolution",
			ResolutionKindDisplayName(*issue.resolutionKind), NULL, NULL);
	}

	AppendMetadataRow(header, "Reported", IssueDate::ToString(issue.reported),
		NULL, NULL);
	if (!issue.reportedBy.empty())
		AppendMetadataRow(header, "Reported by", issue.reportedBy, NULL, NULL);
	if (!issue.area.empty())
		AppendMetadataRow(header, "Area", issue.area, NULL, NULL);
	if (!issue.labels.empty())
		AppendMetadataRow(header, "Labels", Join(issue.labels, ", "), NULL, NULL);
	if (!issue.assignees.empty()) {
		AppendMetadataRow(header, "Assignees", Join(issue.assignees, ", "), NULL,
			NULL);
	}
	if (issue.milestone.has_value() && !issue.milestone->empty())
		AppendMetadataRow(header, "Milestone", *issue.milestone, NULL, NULL);
	if (issue.estimate.has_value()) {
		AppendMetadataRow(header, "Estimate", FormatDouble(*issue.estimate),
			NULL, NULL);
	}

	adw_preferences_page_add(page, header);

	// #pragma mark Free-text sections

	AppendTextSection(page, "Description", issue.description);
	AppendTextSection(page, "Steps to Reproduce",
		NumberedSteps(issue.stepsToReproduce));
	AppendTextSection(page, "Environment", issue.environment);
	AppendTextSection(page, "Notes / Investigation", issue.notes);
	AppendTextSection(page, "Resolution", issue.resolution);

	// #pragma mark Comments

	if (!issue.comments.empty()) {
		AdwPreferencesGroup* group
			= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
		adw_preferences_group_set_title(group, "Comments");

		for (size_t i = 0; i < issue.comments.size(); i++) {
			const Comment& comment = issue.comments[i];
			std::string author = comment.author.empty()
				? std::string("Unknown") : comment.author;
			std::string title = author + " ("
				+ IssueDate::ToString(comment.createdAt) + ")";
			AppendMetadataRow(group, title.c_str(), comment.body, NULL, NULL);
		}
		adw_preferences_page_add(page, group);
	}

	// #pragma mark Related issues

	if (!issue.relations.empty()) {
		AdwPreferencesGroup* group
			= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
		adw_preferences_group_set_title(group, "Related Issues");

		for (size_t i = 0; i < issue.relations.size(); i++) {
			const Relation& relation = issue.relations[i];
			// A relation pointing at a deleted issue is shown literally as
			// "Missing issue", never repaired and never reported as an error --
			// the same silent-dangling behaviour Apple and Haiku have.
			const Issue* target = model.IssueWithID(relation.issueID);
			std::string description = target != NULL
				? target->DisplayNumber() + " " + IssueTitlePlain(*target)
				: std::string("Missing issue");
			AppendMetadataRow(group, RelationKindDisplayName(relation.kind),
				description, NULL, NULL);
		}
		adw_preferences_page_add(page, group);
	}

	// #pragma mark Remote links

	if (!issue.remoteLinks.empty()) {
		AdwPreferencesGroup* group
			= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
		adw_preferences_group_set_title(group, "Remote Links");

		for (size_t i = 0; i < issue.remoteLinks.size(); i++) {
			const RemoteLink& link = issue.remoteLinks[i];

			AdwActionRow* row = ADW_ACTION_ROW(adw_action_row_new());
			gchar* escapedTitle = g_markup_escape_text(
				link.provider.DisplayName().c_str(), -1);
			adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row),
				escapedTitle);
			g_free(escapedTitle);

			gchar* escapedSubtitle = g_markup_escape_text(
				link.identifier.c_str(), -1);
			adw_action_row_set_subtitle(row, escapedSubtitle);
			g_free(escapedSubtitle);

			if (link.url.has_value() && !link.url->empty()) {
				// GtkLinkButton launches the URI itself, so this needs no
				// window pointer and the pane stays stateless.
				GtkWidget* button
					= gtk_link_button_new_with_label(link.url->c_str(), "Open");
				gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
				adw_action_row_add_suffix(row, button);
			}

			adw_preferences_group_add(group, GTK_WIDGET(row));
		}
		adw_preferences_page_add(page, group);
	}

	return GTK_WIDGET(page);
}

} // namespace ihaveissues
