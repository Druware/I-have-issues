/*
 * GitHubSyncDialog.cpp
 */
#include "GitHubSyncDialog.h"

#include <cstdio>

#include "SoupHttpClient.h"

#include <issueskit/GitHubSyncService.h>
#include <issueskit/HttpClient.h>
#include <issueskit/StringUtils.h>
#include <issueskit/TokenStore.h>

using namespace issueskit;

namespace ihaveissues {

/*!	Everything the worker thread needs, copied before it starts.

	The worker owns this and the idle handler deletes it. Nothing in it aliases
	a widget; `owner` is only dereferenced back on the main loop, and only when
	`alive` still says the dialog exists.
*/
struct SyncJob {
						SyncJob()
							:
							owner(NULL),
							completed(false)
						{
						}

	GitHubSyncDialog*	owner;
	std::shared_ptr<bool> alive;
	std::string			token;
	GitHubIntegration	integration;
	std::vector<Issue>	issues;
	SyncResult			result;
	std::string			fatalError;
	bool				completed;
};


GitHubSyncDialog::GitHubSyncDialog(const IssuesDocumentModel& model,
	const IssuesHandler& onIssuesUpdated)
	:
	fModel(model),
	fOnIssuesUpdated(onIssuesUpdated),
	fHasStoredToken(false),
	fIsSyncing(false),
	fAlive(new bool(true)),
	fDialog(NULL),
	fOwnerRow(NULL),
	fRepositoryRow(NULL),
	fTokenRow(NULL),
	fTokenStateRow(NULL),
	fRemoveButton(NULL),
	fSyncButton(NULL),
	fDoneButton(NULL),
	fSpinner(NULL),
	fStatusLabel(NULL),
	fResultBin(NULL)
{
	_BuildUi();
	_Populate();
	_UpdateEnabledState();
}


GitHubSyncDialog::~GitHubSyncDialog()
{
	// A sync still running will find this false and drop its result rather than
	// writing into freed memory.
	*fAlive = false;
}


void
GitHubSyncDialog::_DestroyOwner(gpointer data)
{
	delete static_cast<GitHubSyncDialog*>(data);
}


// #pragma mark - Layout

void
GitHubSyncDialog::_BuildUi()
{
	AdwPreferencesPage* page
		= ADW_PREFERENCES_PAGE(adw_preferences_page_new());

	// Repository ------------------------------------------------------------
	AdwPreferencesGroup* repository
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(repository, "Repository");
	adw_preferences_group_set_description(repository,
		"Set the owner and repository in Project Settings.");

	fOwnerRow = ADW_ACTION_ROW(adw_action_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fOwnerRow), "Owner");
	adw_preferences_group_add(repository, GTK_WIDGET(fOwnerRow));

	fRepositoryRow = ADW_ACTION_ROW(adw_action_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fRepositoryRow),
		"Repository");
	adw_preferences_group_add(repository, GTK_WIDGET(fRepositoryRow));

	adw_preferences_page_add(page, repository);

	// Authentication --------------------------------------------------------
	AdwPreferencesGroup* auth
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(auth, "Authentication");
	adw_preferences_group_set_description(auth,
		"Requires the repo scope. The token is stored in the system keyring, "
		"never in the document.");

	fTokenRow = ADW_PASSWORD_ENTRY_ROW(adw_password_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fTokenRow),
		"Personal access token");
	// GtkEditable::changed, so Sync enables as soon as anything is typed --
	// the same gate the Apple sheet uses.
	g_signal_connect(fTokenRow, "changed", G_CALLBACK(_OnTokenChanged), this);
	adw_preferences_group_add(auth, GTK_WIDGET(fTokenRow));

	fTokenStateRow = ADW_ACTION_ROW(adw_action_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fTokenStateRow),
		"No token saved");

	fRemoveButton = gtk_button_new_with_label("Remove");
	gtk_widget_add_css_class(fRemoveButton, "destructive-action");
	gtk_widget_set_valign(fRemoveButton, GTK_ALIGN_CENTER);
	g_signal_connect(fRemoveButton, "clicked", G_CALLBACK(_OnRemoveClicked),
		this);
	adw_action_row_add_suffix(fTokenStateRow, fRemoveButton);

	adw_preferences_group_add(auth, GTK_WIDGET(fTokenStateRow));
	adw_preferences_page_add(page, auth);

	// Result ----------------------------------------------------------------
	AdwPreferencesGroup* result
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(result, "Result");

	GtkWidget* statusBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	fSpinner = gtk_spinner_new();
	gtk_widget_set_visible(fSpinner, FALSE);
	gtk_widget_set_valign(fSpinner, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(statusBox), fSpinner);

	GtkWidget* status = gtk_label_new("");
	fStatusLabel = GTK_LABEL(status);
	gtk_label_set_wrap(fStatusLabel, TRUE);
	gtk_label_set_wrap_mode(fStatusLabel, PANGO_WRAP_WORD_CHAR);
	gtk_label_set_xalign(fStatusLabel, 0.0f);
	gtk_label_set_selectable(fStatusLabel, TRUE);
	gtk_widget_set_hexpand(status, TRUE);
	gtk_widget_add_css_class(status, "ihi-sync-summary");
	gtk_box_append(GTK_BOX(statusBox), status);

	adw_preferences_group_add(result, statusBox);

	// The per-issue error list is rebuilt wholesale each run, so it lives in a
	// bin rather than being emptied row by row.
	fResultBin = ADW_BIN(adw_bin_new());
	adw_preferences_group_add(result, GTK_WIDGET(fResultBin));

	adw_preferences_page_add(page, result);

	// The dialog shell ------------------------------------------------------
	GtkWidget* header = adw_header_bar_new();
	adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(header), FALSE);
	adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(header), FALSE);

	fDoneButton = gtk_button_new_with_label("Done");
	g_signal_connect(fDoneButton, "clicked", G_CALLBACK(_OnDoneClicked), this);
	adw_header_bar_pack_start(ADW_HEADER_BAR(header), fDoneButton);

	fSyncButton = gtk_button_new_with_label("Sync");
	gtk_widget_add_css_class(fSyncButton, "suggested-action");
	g_signal_connect(fSyncButton, "clicked", G_CALLBACK(_OnSyncClicked), this);
	adw_header_bar_pack_end(ADW_HEADER_BAR(header), fSyncButton);

	GtkWidget* toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), GTK_WIDGET(page));

	fDialog = ADW_DIALOG(adw_dialog_new());
	adw_dialog_set_title(fDialog, "Sync to GitHub");
	adw_dialog_set_content_width(fDialog, 520);
	adw_dialog_set_content_height(fDialog, 620);
	adw_dialog_set_child(fDialog, toolbar);

	g_object_set_data_full(G_OBJECT(fDialog), "ihi-dialog-owner", this,
		_DestroyOwner);
}


// #pragma mark - State

void
GitHubSyncDialog::_Populate()
{
	const std::optional<GitHubIntegration>& integration
		= fModel.integrations.github;

	std::string owner = "—";
	std::string repository = "—";
	if (integration.has_value()) {
		if (!integration->owner.empty())
			owner = integration->owner;
		if (!integration->repository.empty())
			repository = integration->repository;
	} else {
		_SetStatus("No GitHub repository configured.");
	}

	gchar* escapedOwner = g_markup_escape_text(owner.c_str(), -1);
	adw_action_row_set_subtitle(fOwnerRow, escapedOwner);
	g_free(escapedOwner);

	gchar* escapedRepository = g_markup_escape_text(repository.c_str(), -1);
	adw_action_row_set_subtitle(fRepositoryRow, escapedRepository);
	g_free(escapedRepository);

	std::string account = TokenStore::AccountFor(integration);
	if (account.empty() && integration.has_value()) {
		// The coordinates are present but unusable -- blank, or carrying a
		// slash, which would let a typed repository inject extra path segments
		// into every API URL. Naming the reason beats an unexplained dead Sync
		// button.
		_SetStatus("The owner and repository are not usable. Neither may be "
			"blank or contain a slash. Fix them in Project Settings.");
	}

	// VERIFY: this is a blocking libsecret call on the main loop. It normally
	// returns immediately, but a locked keyring makes the secret service put a
	// modal unlock prompt in front of the user first, and the UI is frozen
	// until that is answered. See the risk list in README.md.
	fHasStoredToken = !account.empty() && fTokenStore.HasToken(account);
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fTokenStateRow),
		fHasStoredToken ? "Token saved" : "No token saved");
}


void
GitHubSyncDialog::_UpdateEnabledState()
{
	// The repository gate is the shared account key itself, not a hand-rolled
	// owner/repository test. TokenStore::AccountFor() returns an empty string
	// for every case in which no token operation is possible -- no integration,
	// a blank owner or repository, and an owner or repository containing a
	// slash -- so asking it is both simpler and stricter than re-deriving the
	// same rules here, and it cannot drift from the other ports.
	std::string account = TokenStore::AccountFor(fModel.integrations.github);
	bool hasRepository = !account.empty();

	const char* typed = gtk_editable_get_text(GTK_EDITABLE(fTokenRow));
	bool hasToken = fHasStoredToken
		|| !Trim(typed != NULL ? typed : "").empty();

	// The Apple sheet's canSync, term for term.
	gtk_widget_set_sensitive(fSyncButton,
		hasRepository && hasToken && !fIsSyncing);
	gtk_widget_set_sensitive(fRemoveButton, fHasStoredToken && !fIsSyncing);
	gtk_widget_set_sensitive(GTK_WIDGET(fTokenRow), !fIsSyncing);
	gtk_widget_set_sensitive(fDoneButton, !fIsSyncing);
}


void
GitHubSyncDialog::_SetStatus(const std::string& text)
{
	if (fStatusLabel != NULL)
		gtk_label_set_text(fStatusLabel, text.c_str());
}


void
GitHubSyncDialog::_ShowErrors(const std::vector<std::string>& errors)
{
	if (errors.empty()) {
		adw_bin_set_child(fResultBin, NULL);
		return;
	}

	GtkWidget* list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
	gtk_widget_add_css_class(list, "boxed-list");

	// Every error is listed, including duplicates. The Apple sheet keys its
	// list by the error string itself, so two identical messages collide and
	// one silently vanishes -- its own issue #2.
	for (size_t i = 0; i < errors.size(); i++) {
		AdwActionRow* row = ADW_ACTION_ROW(adw_action_row_new());
		gchar* escaped = g_markup_escape_text(errors[i].c_str(), -1);
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), escaped);
		g_free(escaped);
		adw_preferences_row_set_title_lines(ADW_PREFERENCES_ROW(row), 0);
		gtk_list_box_append(GTK_LIST_BOX(list), GTK_WIDGET(row));
	}

	adw_bin_set_child(fResultBin, list);
}


// #pragma mark - Token handling

bool
GitHubSyncDialog::_SaveToken()
{
	const char* text = gtk_editable_get_text(GTK_EDITABLE(fTokenRow));
	std::string typed = text != NULL ? std::string(text) : std::string();
	std::string account = TokenStore::AccountFor(fModel.integrations.github);

	if (account.empty()) {
		// No usable account means nothing to scope a token to, so a typed token
		// cannot be stored anywhere. Say so rather than dropping it silently.
		if (typed.empty())
			return true;
		_SetStatus("Set a valid owner and repository in Project Settings "
			"before saving a token. Neither may be blank or contain a slash.");
		return false;
	}

	if (!typed.empty()) {
		if (!fTokenStore.Save(account, typed)) {
			_SetStatus("The token could not be written to the system keyring.");
			return false;
		}
		fHasStoredToken = true;
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fTokenStateRow),
			"Token saved");
	}
	// An empty field means "keep whatever is already stored", never "clear it".
	// Clearing is the explicit Remove button.
	return true;
}


void
GitHubSyncDialog::_RemoveToken()
{
	std::string account = TokenStore::AccountFor(fModel.integrations.github);
	if (account.empty())
		return;

	fTokenStore.Remove(account);
	fHasStoredToken = false;
	gtk_editable_set_text(GTK_EDITABLE(fTokenRow), "");
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fTokenStateRow),
		"No token saved");
	_UpdateEnabledState();
}


// #pragma mark - Sync

gpointer
GitHubSyncDialog::_SyncThread(gpointer data)
{
	SyncJob* job = static_cast<SyncJob*>(data);

	// The client is created, used and destroyed entirely inside this thread:
	// its SoupSession must not be shared across threads.
	HttpClient* client = CreateSoupHttpClient();
	GitHubSyncService service(job->token, job->integration, client);
	job->completed = service.Sync(job->issues, job->result, job->fatalError);
	delete client;

	// The token has done its work. Drop it before the job crosses back to the
	// main loop, so it lives no longer than the requests that needed it.
	job->token.clear();

	g_idle_add(_SyncCompleted, job);
	return NULL;
}


gboolean
GitHubSyncDialog::_SyncCompleted(gpointer data)
{
	SyncJob* job = static_cast<SyncJob*>(data);

	if (job->alive && *job->alive && job->owner != NULL)
		job->owner->_SyncFinished(job);

	delete job;
	return G_SOURCE_REMOVE;
}


void
GitHubSyncDialog::_StartSync()
{
	if (fIsSyncing)
		return;
	if (!_SaveToken())
		return;
	if (!fModel.integrations.github.has_value())
		return;

	std::string account = TokenStore::AccountFor(fModel.integrations.github);
	std::string token;
	if (account.empty() || !fTokenStore.Load(account, token)) {
		_SetStatus("No GitHub token saved for this repository. Enter one and "
			"try again.");
		return;
	}

	SyncJob* job = new SyncJob();
	job->owner = this;
	job->alive = fAlive;
	job->token = token;
	job->integration = *fModel.integrations.github;
	job->issues = fModel.issues;

	GError* error = NULL;
	GThread* thread = g_thread_try_new("github-sync", _SyncThread, job, &error);
	if (thread == NULL) {
		delete job;
		_SetStatus(error != NULL && error->message != NULL
			? std::string(error->message)
			: std::string("The sync thread could not be started."));
		g_clear_error(&error);
		return;
	}
	// Detached: nothing joins it, the idle handler is the rendezvous.
	g_thread_unref(thread);

	fIsSyncing = true;
	// Closing mid-sync would be harmless (the job checks fAlive), but the
	// result would be silently thrown away, so the sheet stays put instead.
	adw_dialog_set_can_close(fDialog, FALSE);
	gtk_widget_set_visible(fSpinner, TRUE);
	gtk_spinner_start(GTK_SPINNER(fSpinner));
	_SetStatus("Syncing…");
	_ShowErrors(std::vector<std::string>());
	_UpdateEnabledState();
}


void
GitHubSyncDialog::_SyncFinished(SyncJob* job)
{
	fIsSyncing = false;
	adw_dialog_set_can_close(fDialog, TRUE);
	gtk_spinner_stop(GTK_SPINNER(fSpinner));
	gtk_widget_set_visible(fSpinner, FALSE);

	if (!job->completed) {
		_SetStatus(job->fatalError.empty()
			? std::string("The sync did not complete.") : job->fatalError);
		_ShowErrors(std::vector<std::string>());
	} else {
		char counts[128];
		snprintf(counts, sizeof(counts),
			"%d created, %d updated, %d failed.", job->result.created,
			job->result.updated, job->result.failed);
		_SetStatus(counts);
		_ShowErrors(job->result.errors);

		// Keep the local snapshot in step so a second sync in the same session
		// pushes the refreshed remote links, not the stale ones.
		fModel.issues = job->issues;
		if (fOnIssuesUpdated)
			fOnIssuesUpdated(job->issues);
	}

	_UpdateEnabledState();
}


// #pragma mark - Signals

void
GitHubSyncDialog::_OnSyncClicked(GtkButton* button, gpointer data)
{
	(void)button;
	static_cast<GitHubSyncDialog*>(data)->_StartSync();
}


void
GitHubSyncDialog::_OnDoneClicked(GtkButton* button, gpointer data)
{
	(void)button;
	GitHubSyncDialog* self = static_cast<GitHubSyncDialog*>(data);
	// A token typed but never synced is still saved, and a failure to save it
	// keeps the sheet open rather than dropping it.
	if (self->_SaveToken())
		adw_dialog_close(self->fDialog);
}


void
GitHubSyncDialog::_OnRemoveClicked(GtkButton* button, gpointer data)
{
	(void)button;
	static_cast<GitHubSyncDialog*>(data)->_RemoveToken();
}


void
GitHubSyncDialog::_OnTokenChanged(GtkEditable* editable, gpointer data)
{
	(void)editable;
	static_cast<GitHubSyncDialog*>(data)->_UpdateEnabledState();
}


void
GitHubSyncDialog::Present(GtkWidget* parent)
{
	adw_dialog_present(fDialog, parent);
}

} // namespace ihaveissues
