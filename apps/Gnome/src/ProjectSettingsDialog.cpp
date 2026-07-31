/*
 * ProjectSettingsDialog.cpp
 */
#include "ProjectSettingsDialog.h"

#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

ProjectSettingsDialog::ProjectSettingsDialog(const IssuesDocumentModel& model,
	const SaveHandler& onSave)
	:
	fProject(model.project),
	fIntegrations(model.integrations),
	fOnSave(onSave),
	fDialog(NULL),
	fNameRow(NULL),
	fSummaryRow(NULL),
	fGitHubSwitch(NULL),
	fOwnerRow(NULL),
	fRepositoryRow(NULL),
	fDefaultLabelsRow(NULL),
	fDefaultAssigneesRow(NULL),
	fDefaultMilestoneRow(NULL),
	fAzureSwitch(NULL),
	fOrganizationRow(NULL),
	fProjectRow(NULL),
	fTeamRow(NULL),
	fAreaPathRow(NULL),
	fIterationPathRow(NULL),
	fWorkItemTypeRow(NULL)
{
	_BuildUi();
	_Populate();
}


ProjectSettingsDialog::~ProjectSettingsDialog()
{
}


void
ProjectSettingsDialog::_DestroyOwner(gpointer data)
{
	delete static_cast<ProjectSettingsDialog*>(data);
}


AdwEntryRow*
ProjectSettingsDialog::_MakeEntryRow(AdwPreferencesGroup* group,
	const char* title)
{
	AdwEntryRow* row = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
	adw_preferences_group_add(group, GTK_WIDGET(row));
	return row;
}


std::string
ProjectSettingsDialog::_RowText(gpointer editableRow)
{
	const char* text = gtk_editable_get_text(GTK_EDITABLE(editableRow));
	return text != NULL ? std::string(text) : std::string();
}


std::optional<std::string>
ProjectSettingsDialog::_Optional(const std::string& text)
{
	std::string trimmed = Trim(text);
	if (trimmed.empty())
		return std::optional<std::string>();
	return trimmed;
}


// #pragma mark - Layout

void
ProjectSettingsDialog::_BuildUi()
{
	AdwPreferencesPage* page
		= ADW_PREFERENCES_PAGE(adw_preferences_page_new());

	// Project ---------------------------------------------------------------
	AdwPreferencesGroup* project
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(project, "Project");
	fNameRow = _MakeEntryRow(project, "Name");
	fSummaryRow = _MakeEntryRow(project, "Summary");
	adw_preferences_page_add(page, project);

	// GitHub ----------------------------------------------------------------
	AdwPreferencesGroup* github
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(github, "GitHub");
	adw_preferences_group_set_description(github,
		"The personal access token is kept in the system keyring and is never "
		"saved into this document.");

	fGitHubSwitch = ADW_SWITCH_ROW(adw_switch_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fGitHubSwitch),
		"Configure GitHub");
	adw_preferences_group_add(github, GTK_WIDGET(fGitHubSwitch));

	fOwnerRow = _MakeEntryRow(github, "Owner");
	fRepositoryRow = _MakeEntryRow(github, "Repository");
	fDefaultLabelsRow = _MakeEntryRow(github, "Default labels");
	fDefaultAssigneesRow = _MakeEntryRow(github, "Default assignees");
	fDefaultMilestoneRow = _MakeEntryRow(github, "Default milestone");

	// The switch drives the rest of the block. A GBinding is used rather than
	// a handler so the two can never drift apart, and SYNC_CREATE applies the
	// current value the moment it is set up.
	AdwEntryRow* githubRows[] = { fOwnerRow, fRepositoryRow, fDefaultLabelsRow,
		fDefaultAssigneesRow, fDefaultMilestoneRow };
	for (size_t i = 0; i < G_N_ELEMENTS(githubRows); i++) {
		g_object_bind_property(fGitHubSwitch, "active", githubRows[i],
			"sensitive", G_BINDING_SYNC_CREATE);
	}

	adw_preferences_page_add(page, github);

	// Azure DevOps ----------------------------------------------------------
	AdwPreferencesGroup* azure
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(azure, "Azure DevOps");
	// These fields exist because the .issues FORMAT has them. No Azure DevOps
	// sync is implemented on any platform, so the description says so rather
	// than implying a working integration.
	adw_preferences_group_set_description(azure,
		"Stored in the document for other tools. This app does not sync with "
		"Azure DevOps.");

	fAzureSwitch = ADW_SWITCH_ROW(adw_switch_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fAzureSwitch),
		"Configure Azure DevOps");
	adw_preferences_group_add(azure, GTK_WIDGET(fAzureSwitch));

	fOrganizationRow = _MakeEntryRow(azure, "Organization");
	fProjectRow = _MakeEntryRow(azure, "Project");
	fTeamRow = _MakeEntryRow(azure, "Team");
	fAreaPathRow = _MakeEntryRow(azure, "Area path");
	fIterationPathRow = _MakeEntryRow(azure, "Iteration path");
	fWorkItemTypeRow = _MakeEntryRow(azure, "Default work item type");

	AdwEntryRow* azureRows[] = { fOrganizationRow, fProjectRow, fTeamRow,
		fAreaPathRow, fIterationPathRow, fWorkItemTypeRow };
	for (size_t i = 0; i < G_N_ELEMENTS(azureRows); i++) {
		g_object_bind_property(fAzureSwitch, "active", azureRows[i],
			"sensitive", G_BINDING_SYNC_CREATE);
	}

	adw_preferences_page_add(page, azure);

	// The dialog shell ------------------------------------------------------
	GtkWidget* header = adw_header_bar_new();
	adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(header), FALSE);
	adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(header), FALSE);

	GtkWidget* cancelButton = gtk_button_new_with_label("Cancel");
	g_signal_connect(cancelButton, "clicked", G_CALLBACK(_OnCancelClicked),
		this);
	adw_header_bar_pack_start(ADW_HEADER_BAR(header), cancelButton);

	GtkWidget* saveButton = gtk_button_new_with_label("Save");
	gtk_widget_add_css_class(saveButton, "suggested-action");
	g_signal_connect(saveButton, "clicked", G_CALLBACK(_OnSaveClicked), this);
	adw_header_bar_pack_end(ADW_HEADER_BAR(header), saveButton);

	GtkWidget* toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), GTK_WIDGET(page));

	fDialog = ADW_DIALOG(adw_dialog_new());
	adw_dialog_set_title(fDialog, "Project Settings");
	adw_dialog_set_content_width(fDialog, 560);
	adw_dialog_set_content_height(fDialog, 720);
	adw_dialog_set_child(fDialog, toolbar);

	g_object_set_data_full(G_OBJECT(fDialog), "ihi-dialog-owner", this,
		_DestroyOwner);
}


// #pragma mark - Populate and save

void
ProjectSettingsDialog::_Populate()
{
	gtk_editable_set_text(GTK_EDITABLE(fNameRow), fProject.name.c_str());
	gtk_editable_set_text(GTK_EDITABLE(fSummaryRow), fProject.summary.c_str());

	// DEVIATION from Apple: the Apple sheet pre-seeds blank GitHub coordinates
	// from legacy UserDefaults keys left by a pre-document build. GNOME has no
	// such legacy state, so the fields simply start blank -- the same choice
	// the Haiku port made.
	const std::optional<GitHubIntegration>& github = fIntegrations.github;
	adw_switch_row_set_active(fGitHubSwitch, github.has_value());
	if (github.has_value()) {
		gtk_editable_set_text(GTK_EDITABLE(fOwnerRow), github->owner.c_str());
		gtk_editable_set_text(GTK_EDITABLE(fRepositoryRow),
			github->repository.c_str());
		gtk_editable_set_text(GTK_EDITABLE(fDefaultLabelsRow),
			Join(github->defaultLabels, ", ").c_str());
		gtk_editable_set_text(GTK_EDITABLE(fDefaultAssigneesRow),
			Join(github->defaultAssignees, ", ").c_str());
		gtk_editable_set_text(GTK_EDITABLE(fDefaultMilestoneRow),
			github->defaultMilestone.has_value()
				? github->defaultMilestone->c_str() : "");
	}

	const std::optional<AzureDevOpsIntegration>& azure
		= fIntegrations.azureDevOps;
	adw_switch_row_set_active(fAzureSwitch, azure.has_value());
	if (azure.has_value()) {
		gtk_editable_set_text(GTK_EDITABLE(fOrganizationRow),
			azure->organization.c_str());
		gtk_editable_set_text(GTK_EDITABLE(fProjectRow),
			azure->project.c_str());
		gtk_editable_set_text(GTK_EDITABLE(fTeamRow),
			azure->team.has_value() ? azure->team->c_str() : "");
		gtk_editable_set_text(GTK_EDITABLE(fAreaPathRow),
			azure->areaPath.has_value() ? azure->areaPath->c_str() : "");
		gtk_editable_set_text(GTK_EDITABLE(fIterationPathRow),
			azure->iterationPath.has_value()
				? azure->iterationPath->c_str() : "");
		gtk_editable_set_text(GTK_EDITABLE(fWorkItemTypeRow),
			azure->defaultWorkItemType.c_str());
	} else {
		gtk_editable_set_text(GTK_EDITABLE(fWorkItemTypeRow), "Issue");
	}
}


void
ProjectSettingsDialog::_Save()
{
	fProject.name = Trim(_RowText(fNameRow));
	fProject.summary = _RowText(fSummaryRow);

	if (adw_switch_row_get_active(fGitHubSwitch)) {
		GitHubIntegration integration;
		integration.owner = Trim(_RowText(fOwnerRow));
		integration.repository = Trim(_RowText(fRepositoryRow));
		integration.defaultLabels
			= SplitTrimNonEmpty(_RowText(fDefaultLabelsRow), ',');
		integration.defaultAssignees
			= SplitTrimNonEmpty(_RowText(fDefaultAssigneesRow), ',');
		integration.defaultMilestone
			= _Optional(_RowText(fDefaultMilestoneRow));
		fIntegrations.github = integration;
	} else {
		fIntegrations.github.reset();
	}

	if (adw_switch_row_get_active(fAzureSwitch)) {
		AzureDevOpsIntegration integration;
		integration.organization = Trim(_RowText(fOrganizationRow));
		integration.project = Trim(_RowText(fProjectRow));
		integration.team = _Optional(_RowText(fTeamRow));
		integration.areaPath = _Optional(_RowText(fAreaPathRow));
		integration.iterationPath = _Optional(_RowText(fIterationPathRow));
		std::optional<std::string> workItemType
			= _Optional(_RowText(fWorkItemTypeRow));
		integration.defaultWorkItemType = workItemType.has_value()
			? *workItemType : std::string("Issue");
		fIntegrations.azureDevOps = integration;
	} else {
		fIntegrations.azureDevOps.reset();
	}

	if (fOnSave)
		fOnSave(fProject, fIntegrations);
}


// #pragma mark - Signals

void
ProjectSettingsDialog::_OnCancelClicked(GtkButton* button, gpointer data)
{
	(void)button;
	ProjectSettingsDialog* self = static_cast<ProjectSettingsDialog*>(data);
	adw_dialog_close(self->fDialog);
}


void
ProjectSettingsDialog::_OnSaveClicked(GtkButton* button, gpointer data)
{
	(void)button;
	ProjectSettingsDialog* self = static_cast<ProjectSettingsDialog*>(data);
	self->_Save();
	adw_dialog_close(self->fDialog);
}


void
ProjectSettingsDialog::Present(GtkWidget* parent)
{
	adw_dialog_present(fDialog, parent);
}

} // namespace ihaveissues
