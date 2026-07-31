/*
 * IssueEditDialog.h -- the add/edit issue sheet.
 *
 * The GNOME analogue of the Apple app's IssueEditView and Haiku's
 * IssueEditWindow. It edits a local copy of the issue: Cancel discards it,
 * Save hands the finished copy to the caller, which owns persistence.
 *
 * LIFETIME. This C++ object is attached to its AdwDialog with
 * g_object_set_data_full(), so it is deleted when the dialog is finalised.
 * Construct it with `new`, call Present() once, and never delete it by hand.
 */
#ifndef IHAVEISSUES_ISSUE_EDIT_DIALOG_H
#define IHAVEISSUES_ISSUE_EDIT_DIALOG_H

#include <functional>
#include <string>

#include <adwaita.h>
#include <gtk/gtk.h>

#include <issueskit/IssueModel.h>

namespace ihaveissues {

class IssueEditDialog {
public:
			typedef std::function<void(const issueskit::Issue&)> SaveHandler;

	/*!	Builds the sheet for \a issue.

		\param onSave Called on the main loop with the edited issue, before the
			dialog closes. Never called when the user cancels.
	*/
								IssueEditDialog(const issueskit::Issue& issue,
									const SaveHandler& onSave);
								~IssueEditDialog();

	//! Shows the sheet over \a parent. Call exactly once.
			void				Present(GtkWidget* parent);

private:
			void				_BuildUi();
			void				_Populate();
			void				_Save();

	static	void				_OnCancelClicked(GtkButton* button,
									gpointer data);
	static	void				_OnSaveClicked(GtkButton* button, gpointer data);
	static	void				_OnCalendarDaySelected(GtkCalendar* calendar,
									gpointer data);
	static	void				_DestroyOwner(gpointer data);

	//! A framed, scrolling multi-line editor. Returns the scroller to pack.
	static	GtkWidget*			_MakeTextArea(GtkTextView** outView);
	static	std::string			_TextAreaText(GtkTextView* view);
	static	void				_SetTextArea(GtkTextView* view,
									const std::string& text);
	static	AdwComboRow*		_MakeComboRow(const char* title,
									const char* const* items);
	static	std::string			_RowText(gpointer editableRow);

			issueskit::Issue	fDraft;
			SaveHandler			fOnSave;

			AdwDialog*			fDialog;

			AdwEntryRow*		fTitleRow;
			AdwComboRow*		fTypeRow;
			AdwComboRow*		fPriorityRow;
			AdwComboRow*		fStatusRow;
			AdwComboRow*		fResolutionRow;
			AdwEntryRow*		fReportedRow;
			GtkCalendar*		fCalendar;
			AdwEntryRow*		fReportedByRow;
			AdwEntryRow*		fAreaRow;
			AdwEntryRow*		fMilestoneRow;
			AdwEntryRow*		fEstimateRow;
			AdwEntryRow*		fLabelsRow;
			AdwEntryRow*		fAssigneesRow;

			GtkTextView*		fDescriptionView;
			GtkTextView*		fStepsView;
			GtkTextView*		fEnvironmentView;
			GtkTextView*		fNotesView;
			GtkTextView*		fResolutionView;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_EDIT_DIALOG_H
