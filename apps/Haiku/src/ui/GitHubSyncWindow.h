/*
 * GitHubSyncWindow.h -- the "Sync to GitHub" sheet.
 *
 * Owner and repository are read-only here: they belong to the document and are
 * edited once, in Project Settings.
 *
 * The token field only ever holds what the user just typed. A stored token is
 * never read back into the UI -- the sheet needs to know one exists, not what it
 * is. Leaving the field blank means "keep what is stored", not "delete it";
 * removal is the Remove button's job.
 *
 * The sync itself runs on a spawned worker thread. Everything it needs (token,
 * integration coordinates, a snapshot of the issues) is copied into a context
 * struct before the thread starts, and the result comes back as a BMessage, so
 * the worker never touches this window's members.
 */
#ifndef IHAVEISSUES_GITHUB_SYNC_WINDOW_H
#define IHAVEISSUES_GITHUB_SYNC_WINDOW_H

#include <Messenger.h>
#include <OS.h>
#include <Window.h>

#include <issueskit/IssueModel.h>
#include <issueskit/TokenStore.h>

class BButton;
class BStringView;
class BTextControl;
class BTextView;

namespace ihaveissues {

class GitHubSyncWindow : public BWindow {
public:
								GitHubSyncWindow(BWindow* parent,
									const issueskit::IssuesDocumentModel& model,
									const BMessenger& target);
	virtual						~GitHubSyncWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			void				_BuildLayout();
			void				_Populate();
			bool				_SaveToken();
			void				_RemoveToken();
			void				_StartSync();
			void				_SyncFinished(BMessage* message);
			void				_UpdateEnabledState();
			void				_SetStatus(const char* text);

	static	status_t			_SyncThread(void* data);

			issueskit::IssuesDocumentModel fModel;
			BMessenger			fTarget;

			/*!	The platform's secret store.

				Held as the shared interface, not as the Haiku class, so this
				window is the one piece of sync UI the GNOME and KDE ports can
				read as a direct model for their own.
			*/
			issueskit::TokenStore*	fTokenStore;

			bool				fHasStoredToken;
			bool				fIsSyncing;
			thread_id			fWorker;

			BStringView*		fOwnerView;
			BStringView*		fRepositoryView;
			BStringView*		fTokenStateView;
			BTextControl*		fTokenControl;
			BButton*			fRemoveButton;
			BButton*			fSyncButton;
			BButton*			fDoneButton;
			BTextView*			fResultView;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_GITHUB_SYNC_WINDOW_H
