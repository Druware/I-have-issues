/*
 * GitHubSyncDialog.h -- the push-to-GitHub sheet.
 *
 * The GNOME analogue of the Apple app's GitHubSyncView and Haiku's
 * GitHubSyncWindow. Owner and repository are read-only here (they are edited in
 * Project Settings); what this sheet owns is the personal access token and the
 * sync run itself.
 *
 * THREADING. issueskit::GitHubSyncService is synchronous, so the whole sync
 * runs on a GThread and its result is handed back to the main loop with
 * g_idle_add(). Nothing on the worker side touches a widget.
 *
 * LIFETIME. As the other dialogs, this object is deleted with its AdwDialog.
 * The worker outlives neither: the job carries a shared "still alive" flag that
 * the destructor clears, so a sync finishing after the dialog has gone simply
 * drops its result instead of writing into freed memory.
 */
#ifndef IHAVEISSUES_GITHUB_SYNC_DIALOG_H
#define IHAVEISSUES_GITHUB_SYNC_DIALOG_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <adwaita.h>
#include <gtk/gtk.h>

#include <issueskit/IssueModel.h>

#include "SecretTokenStore.h"

namespace ihaveissues {

struct SyncJob;

class GitHubSyncDialog {
public:
			typedef std::function<void(const std::vector<issueskit::Issue>&)>
				IssuesHandler;

	/*!	Builds the sheet for \a model.

		\param onIssuesUpdated Called on the main loop with the rewritten issue
			array after a completed sync. The caller replaces its document's
			issues with it wholesale.
	*/
								GitHubSyncDialog(
									const issueskit::IssuesDocumentModel& model,
									const IssuesHandler& onIssuesUpdated);
								~GitHubSyncDialog();

			void				Present(GtkWidget* parent);

private:
			void				_BuildUi();
			void				_Populate();
			void				_UpdateEnabledState();
			void				_SetStatus(const std::string& text);
			void				_ShowErrors(
									const std::vector<std::string>& errors);

			//! Persists a newly typed token. False means "do not proceed".
			bool				_SaveToken();
			void				_RemoveToken();
			void				_StartSync();
			void				_SyncFinished(SyncJob* job);

	static	void				_OnSyncClicked(GtkButton* button, gpointer data);
	static	void				_OnDoneClicked(GtkButton* button, gpointer data);
	static	void				_OnRemoveClicked(GtkButton* button,
									gpointer data);
	static	void				_OnTokenChanged(GtkEditable* editable,
									gpointer data);
	static	void				_DestroyOwner(gpointer data);

	static	gpointer			_SyncThread(gpointer data);
	static	gboolean			_SyncCompleted(gpointer data);

			issueskit::IssuesDocumentModel fModel;
			IssuesHandler		fOnIssuesUpdated;
			SecretTokenStore	fTokenStore;

			bool				fHasStoredToken;
			bool				fIsSyncing;
			//! Cleared by the destructor; read by the idle handler.
			std::shared_ptr<bool> fAlive;

			AdwDialog*			fDialog;
			AdwActionRow*		fOwnerRow;
			AdwActionRow*		fRepositoryRow;
			AdwPasswordEntryRow* fTokenRow;
			AdwActionRow*		fTokenStateRow;
			GtkWidget*			fRemoveButton;
			GtkWidget*			fSyncButton;
			GtkWidget*			fDoneButton;
			GtkWidget*			fSpinner;
			GtkLabel*			fStatusLabel;
			AdwBin*				fResultBin;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_GITHUB_SYNC_DIALOG_H
