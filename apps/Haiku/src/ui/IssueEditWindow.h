/*
 * IssueEditWindow.h -- the modal add/edit sheet.
 *
 * Edits a LOCAL COPY of the issue and only writes back on Save, so Cancel
 * discards everything -- the same contract as the Apple app's IssueEditView.
 * The result is handed back as a kMsgIssueEdited BMessage carrying the issue
 * encoded as JSON: BMessage copies its payload, so nothing is shared across the
 * two BLoopers.
 *
 * DEVIATIONS from Apple, both documented in the README:
 *   - the five long-text sections live in a BTabView rather than one long
 *     scrolling form, because a Haiku window cannot scroll a whole form;
 *   - "Reported" is a YYYY-MM-DD text field, because the Haiku API has no public
 *     date picker. The value is still fixed-UTC, so no time zone can shift it.
 */
#ifndef IHAVEISSUES_ISSUE_EDIT_WINDOW_H
#define IHAVEISSUES_ISSUE_EDIT_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <issueskit/IssueModel.h>

class BButton;
class BMenuField;
class BPopUpMenu;
class BTextControl;
class BTextView;

namespace ihaveissues {

class IssueEditWindow : public BWindow {
public:
	/*!	\param parent The document window; used to place and subset this one.
		\param issue The issue to edit, copied immediately.
		\param target Receives kMsgIssueEdited on Save.
	*/
								IssueEditWindow(BWindow* parent,
									const issueskit::Issue& issue,
									const BMessenger& target);
	virtual						~IssueEditWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_BuildLayout();
			void				_Populate();
			void				_Save();

	static	BPopUpMenu*			_MakeMenu(const char* name);
			void				_MarkMenuItem(BMenuField* field, int32 index);
			int32				_MarkedIndex(BMenuField* field) const;
	static	BTextView*			_MakeTextArea(const char* name);
	static	void				_SetTextArea(BTextView* view,
									const std::string& text);
	static	std::string			_TextAreaText(BTextView* view);

			issueskit::Issue	fDraft;
			BMessenger			fTarget;

			BTextControl*		fTitleControl;
			BMenuField*			fTypeField;
			BMenuField*			fPriorityField;
			BMenuField*			fStatusField;
			BMenuField*			fResolutionField;

			BTextControl*		fReportedControl;
			BTextControl*		fReportedByControl;
			BTextControl*		fAreaControl;
			BTextControl*		fMilestoneControl;
			BTextControl*		fEstimateControl;
			BTextControl*		fLabelsControl;
			BTextControl*		fAssigneesControl;

			BTextView*			fDescriptionView;
			BTextView*			fStepsView;
			BTextView*			fEnvironmentView;
			BTextView*			fNotesView;
			BTextView*			fResolutionView;

			BButton*			fCancelButton;
			BButton*			fSaveButton;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_EDIT_WINDOW_H
