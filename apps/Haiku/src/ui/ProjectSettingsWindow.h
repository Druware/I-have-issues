/*
 * ProjectSettingsWindow.h -- per-document project identity and tracker
 * coordinates.
 *
 * Like the issue editor this edits a local draft and only writes back on Save.
 *
 * NO CREDENTIALS are edited or stored here. .issues documents are committed to
 * project repositories, so tokens live in the system key store (see
 * KeyStoreTokenStore) and only the non-secret coordinates below reach the file.
 *
 * The Azure DevOps block exists because the format and the Apple UI have it. No
 * Azure DevOps sync exists on any platform -- see the README.
 */
#ifndef IHAVEISSUES_PROJECT_SETTINGS_WINDOW_H
#define IHAVEISSUES_PROJECT_SETTINGS_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <issueskit/IssueModel.h>

class BBox;
class BButton;
class BCheckBox;
class BTextControl;

namespace ihaveissues {

class ProjectSettingsWindow : public BWindow {
public:
								ProjectSettingsWindow(BWindow* parent,
									const issueskit::IssuesDocumentModel& model,
									const BMessenger& target);
	virtual						~ProjectSettingsWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_BuildLayout();
			void				_Populate();
			void				_Save();
			void				_UpdateEnabledState();

	static	std::optional<std::string> _Optional(const char* text);

			issueskit::IssuesDocumentModel fModel;
			BMessenger			fTarget;

			BTextControl*		fNameControl;
			BTextControl*		fSummaryControl;

			BCheckBox*			fGitHubBox;
			BTextControl*		fOwnerControl;
			BTextControl*		fRepositoryControl;
			BTextControl*		fDefaultLabelsControl;
			BTextControl*		fDefaultAssigneesControl;
			BTextControl*		fDefaultMilestoneControl;

			BCheckBox*			fAzureBox;
			BTextControl*		fOrganizationControl;
			BTextControl*		fProjectControl;
			BTextControl*		fTeamControl;
			BTextControl*		fAreaPathControl;
			BTextControl*		fIterationPathControl;
			BTextControl*		fWorkItemTypeControl;

			BButton*			fCancelButton;
			BButton*			fSaveButton;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_PROJECT_SETTINGS_WINDOW_H
