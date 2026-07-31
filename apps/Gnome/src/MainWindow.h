/*
 * MainWindow.h -- one .issues document window.
 *
 * The GNOME analogue of the Apple app's ContentView and Haiku's MainWindow: a
 * sidebar listing issues grouped "Open" and "Resolved", and a detail pane, held
 * in an AdwNavigationSplitView so the window shows both side by side when it is
 * wide and collapses to a single navigable page when it is narrow. The collapse
 * is driven by an AdwBreakpoint on the window, never by a hand-written resize
 * handler.
 *
 * The window owns the model. Every mutation goes through _ApplyIssue or
 * _DeleteIssue, which mark the document dirty and rebuild the list.
 *
 * There is deliberately NO search, filter or sort: the Apple app has none, and
 * adding them would be new product, not a port.
 *
 * This is a plain C++ class that drives an AdwApplicationWindow rather than a
 * GObject subclass of one. It holds real C++ members (the document model,
 * std::string, std::shared_ptr), which a GObject instance struct cannot: GObject
 * allocates and frees that struct without running constructors or destructors.
 * The object's life is tied to the window with g_object_set_data_full().
 */
#ifndef IHAVEISSUES_MAIN_WINDOW_H
#define IHAVEISSUES_MAIN_WINDOW_H

#include <memory>
#include <string>

#include <adwaita.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <issueskit/IssueModel.h>

namespace ihaveissues {

class MainWindow {
public:
	//! Builds an untitled, empty document window belonging to \a application.
								MainWindow(AdwApplication* application);
								~MainWindow();

	//! The window this object drives. Owned by the application.
			GtkWindow*			Window() const { return GTK_WINDOW(fWindow); }

	/*!	Loads \a file into this window, replacing whatever it held.

		Reports its own failure to the user.
		\return false when the file could not be read or decoded.
	*/
			bool				OpenFile(GFile* file);

	//! Whether this window holds an untouched, unsaved, empty document.
			bool				IsPristine() const;

private:
	// #pragma mark Construction
			void				_BuildUi();
			GtkWidget*			_BuildSidebar();
			GtkWidget*			_BuildContent();
			GtkWidget*			_BuildListSection(const char* title,
									const char* emptyText,
									GListStore** outStore,
									GtkListBox** outList);
			GMenuModel*			_BuildPrimaryMenu();
			void				_InstallActions();

	// #pragma mark List and selection
			void				_RebuildList();
			void				_UpdateDetail();
			void				_UpdateTitles();
			void				_UpdateActionState();
			const issueskit::Issue* _SelectedIssue() const;
			void				_SelectStoredID();

	// #pragma mark Issue actions
			void				_AddIssue();
			void				_EditIssue();
			void				_DeleteIssue();
			void				_ApplyIssue(const issueskit::Issue& issue);

	// #pragma mark Dialogs
			void				_ShowProjectSettings();
			void				_ShowGitHubSync();
			void				_ShowError(const char* heading,
									const std::string& body);

	// #pragma mark Files
			void				_Open();
			void				_Save();
			void				_SaveAs();
			void				_ExportMarkdown();
			void				_ImportMarkdown();
			void				_PerformImport(GFile* file);
			bool				_ReadFile(GFile* file, std::string& outContents,
									std::string& outError);
			bool				_WriteFile(GFile* file,
									const std::string& contents,
									std::string& outError);
			bool				_SaveToFile(GFile* file);
			void				_AfterSuccessfulSave();
			void				_SetFile(GFile* file);
			void				_SetPendingImport(GFile* file);
			GtkFileDialog*		_MakeFileDialog(const char* title,
									bool markdown);

	// #pragma mark Callbacks
	static	GtkWidget*			_CreateRow(gpointer item, gpointer data);
	static	void				_OnRowSelected(GtkListBox* box,
									GtkListBoxRow* row, gpointer data);
	static	void				_OnRowActivated(GtkListBox* box,
									GtkListBoxRow* row, gpointer data);
	static	gboolean			_OnCloseRequest(GtkWindow* window,
									gpointer data);
	static	void				_OnFileDialogFinished(GObject* source,
									GAsyncResult* result, gpointer data);
	static	void				_OnImportResponse(AdwAlertDialog* dialog,
									const char* response, gpointer data);
	static	void				_OnDeleteResponse(AdwAlertDialog* dialog,
									const char* response, gpointer data);
	static	void				_OnCloseResponse(AdwAlertDialog* dialog,
									const char* response, gpointer data);
	static	void				_DestroyOwner(gpointer data);

	// GAction handlers. One per menu entry; all take `this` as user data.
	static	void				_ActionOpen(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionSave(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionSaveAs(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionExportMarkdown(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionImportMarkdown(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionProjectSettings(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionGitHubSync(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionAddIssue(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionEditIssue(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionDeleteIssue(GSimpleAction* action,
									GVariant* parameter, gpointer data);
	static	void				_ActionCloseWindow(GSimpleAction* action,
									GVariant* parameter, gpointer data);

			issueskit::IssuesDocumentModel fModel;
			//! NULL until the document has been saved or opened.
			GFile*				fFile;
			bool				fDirty;
			//! The uuid of the selected issue, or empty. Never an array index.
			std::string			fSelectedIssueID;

			//! Guards the two list boxes against each other's deselections.
			bool				fUpdatingSelection;
			//! Set once the user has answered the unsaved-changes prompt.
			bool				fForceClose;
			//! Whether the pending save was started in order to close.
			bool				fCloseAfterSave;
			//! The file an import is waiting on confirmation for.
			GFile*				fPendingImport;

			/*!	Cleared by the destructor.

				Every asynchronous continuation this window starts -- a
				GtkFileDialog callback, an alert response -- carries a copy and
				checks it, so a callback that arrives after the window is gone
				does nothing instead of writing into freed memory.
			*/
			std::shared_ptr<bool> fAlive;

			GtkWidget*			fWindow;
			AdwNavigationSplitView* fSplitView;
			AdwNavigationPage*	fSidebarPage;
			AdwNavigationPage*	fContentPage;
			AdwBin*				fDetailBin;

			GListStore*			fOpenStore;
			GListStore*			fResolvedStore;
			GtkListBox*			fOpenList;
			GtkListBox*			fResolvedList;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_MAIN_WINDOW_H
