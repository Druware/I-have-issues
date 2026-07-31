/*
 * MainWindow.h -- one .issues document window.
 *
 * The Haiku analogue of the Apple app's ContentView: a menu bar, a left list
 * pane grouped into "Open" and "Resolved", and a right detail pane, split by a
 * BSplitView (the counterpart of NavigationSplitView).
 *
 * The window owns the model. Every mutation goes through _ApplyIssue or
 * _DeleteIssue, which mark the document dirty and rebuild the list.
 *
 * There is deliberately NO search, filter or sort: the Apple app has none, and
 * adding them would be new product, not a port.
 */
#ifndef IHAVEISSUES_MAIN_WINDOW_H
#define IHAVEISSUES_MAIN_WINDOW_H

#include <Entry.h>
#include <Window.h>

#include <issueskit/IssueModel.h>

class BButton;
class BFilePanel;
class BMenuBar;
class BOutlineListView;
class BStringItem;

namespace ihaveissues {

class IssueDetailView;

class MainWindow : public BWindow {
public:
	//! An untitled, empty document.
								MainWindow();
	virtual						~MainWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

	//! Loads \a ref into this window, replacing whatever it held.
			bool				OpenDocument(const entry_ref& ref);

	//! Whether this window holds an untouched, unsaved, empty document.
			bool				IsPristine() const;

private:
			void				_BuildMenuBar();
			void				_BuildLayout();
			void				_BuildFilePanels();

			void				_RebuildList();
			void				_UpdateTitle();
			void				_SelectionChanged();
			const issueskit::Issue* _SelectedIssue() const;

			void				_AddIssue();
			void				_EditIssue();
			void				_DeleteIssue();
			void				_ApplyIssue(const issueskit::Issue& issue);
			void				_HandleIssueEdited(BMessage* message);
			void				_HandleProjectSettingsSaved(BMessage* message);
			void				_HandleSyncIssuesUpdated(BMessage* message);

			void				_ShowProjectSettings();
			void				_ShowGitHubSync();

			void				_Save();
			void				_SaveAs();
			void				_ExportMarkdown();
			void				_ImportMarkdown();
			void				_HandleSavePanel(BMessage* message);
			void				_HandleOpenPanel(BMessage* message);
			void				_PerformImport(const entry_ref& ref);

			bool				_WriteFile(const entry_ref& directory,
									const char* name,
									const std::string& contents,
									const char* mimeType,
									entry_ref* outRef);
			bool				_ReadFile(const entry_ref& ref,
									std::string& outContents);
			bool				_SaveToCurrentRef();

			void				_ShowError(const char* title,
									const std::string& message);

			issueskit::IssuesDocumentModel fModel;
			entry_ref			fDocumentRef;
			bool				fHasDocumentRef;
			bool				fDirty;
			std::string			fSelectedIssueID;

			BMenuBar*			fMenuBar;
			BOutlineListView*	fListView;
			BStringItem*		fOpenGroup;
			BStringItem*		fResolvedGroup;
			IssueDetailView*	fDetailView;
			BButton*			fAddButton;
			BButton*			fEditButton;
			BButton*			fDeleteButton;

			// One panel per purpose, each with a fixed reply message, so no
			// message has to be rebuilt or re-owned between invocations.
			BFilePanel*			fSaveDocumentPanel;
			BFilePanel*			fExportPanel;
			BFilePanel*			fOpenDocumentPanel;
			BFilePanel*			fImportPanel;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_MAIN_WINDOW_H
