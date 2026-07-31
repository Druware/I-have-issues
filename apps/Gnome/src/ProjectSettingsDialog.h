/*
 * ProjectSettingsDialog.h -- per-document project and integration coordinates.
 *
 * The GNOME analogue of the Apple app's ProjectSettingsView and Haiku's
 * ProjectSettingsWindow. It edits a local copy of the project info and the two
 * integration blocks, each behind a switch, and hands the result back on Save.
 *
 * No credentials are edited here and none are stored in the document: the
 * GitHub personal access token lives in the system keyring, reached from
 * GitHubSyncDialog.
 *
 * LIFETIME: as IssueEditDialog -- `new`, Present() once, deleted with its
 * AdwDialog.
 */
#ifndef IHAVEISSUES_PROJECT_SETTINGS_DIALOG_H
#define IHAVEISSUES_PROJECT_SETTINGS_DIALOG_H

#include <functional>
#include <optional>
#include <string>

#include <adwaita.h>
#include <gtk/gtk.h>

#include <issueskit/IssueModel.h>

namespace ihaveissues {

class ProjectSettingsDialog {
public:
			typedef std::function<void(const issueskit::ProjectInfo&,
				const issueskit::IntegrationSettings&)> SaveHandler;

								ProjectSettingsDialog(
									const issueskit::IssuesDocumentModel& model,
									const SaveHandler& onSave);
								~ProjectSettingsDialog();

			void				Present(GtkWidget* parent);

private:
			void				_BuildUi();
			void				_Populate();
			void				_Save();

	static	void				_OnCancelClicked(GtkButton* button,
									gpointer data);
	static	void				_OnSaveClicked(GtkButton* button, gpointer data);
	static	void				_DestroyOwner(gpointer data);

	static	AdwEntryRow*		_MakeEntryRow(AdwPreferencesGroup* group,
									const char* title);
	static	std::string			_RowText(gpointer editableRow);
	//! Blank becomes "no value", never an empty string.
	static	std::optional<std::string> _Optional(const std::string& text);

			issueskit::ProjectInfo fProject;
			issueskit::IntegrationSettings fIntegrations;
			SaveHandler			fOnSave;

			AdwDialog*			fDialog;

			AdwEntryRow*		fNameRow;
			AdwEntryRow*		fSummaryRow;

			AdwSwitchRow*		fGitHubSwitch;
			AdwEntryRow*		fOwnerRow;
			AdwEntryRow*		fRepositoryRow;
			AdwEntryRow*		fDefaultLabelsRow;
			AdwEntryRow*		fDefaultAssigneesRow;
			AdwEntryRow*		fDefaultMilestoneRow;

			AdwSwitchRow*		fAzureSwitch;
			AdwEntryRow*		fOrganizationRow;
			AdwEntryRow*		fProjectRow;
			AdwEntryRow*		fTeamRow;
			AdwEntryRow*		fAreaPathRow;
			AdwEntryRow*		fIterationPathRow;
			AdwEntryRow*		fWorkItemTypeRow;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_PROJECT_SETTINGS_DIALOG_H
