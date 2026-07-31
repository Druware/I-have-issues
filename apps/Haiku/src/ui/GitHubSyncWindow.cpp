/*
 * GitHubSyncWindow.cpp
 */
#include "GitHubSyncWindow.h"

#include <cstdio>

#include <Button.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include "AppMessages.h"
#include "KeyStoreTokenStore.h"
#include <issueskit/GitHubSyncService.h>
#include "ServicesKitHttpClient.h"

#include <issueskit/HttpClient.h>
#include <issueskit/IssuesJsonCoder.h>
#include <issueskit/JsonParser.h>
#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

namespace {

/*!	Everything the worker thread needs, copied before it starts.

	The worker owns this and deletes it. Nothing in it aliases the window.
*/
struct SyncContext {
	BMessenger					replyTo;
	std::string					token;
	GitHubIntegration			integration;
	std::vector<Issue>			issues;
};

//! Sent by the token field on every keystroke, so Sync enables as you type.
const uint32 kMsgTokenChanged = 'tkmd';

} // unnamed namespace


GitHubSyncWindow::GitHubSyncWindow(BWindow* parent,
	const IssuesDocumentModel& model, const BMessenger& target)
	:
	BWindow(BRect(0, 0, 460, 420), "Sync to GitHub", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE),
	fModel(model),
	fTarget(target),
	// The only line in the sync UI that names a platform backend. Swapping in
	// libsecret or KWallet is a one-line change in the corresponding port.
	fTokenStore(new KeyStoreTokenStore()),
	fHasStoredToken(false),
	fIsSyncing(false),
	fWorker(-1)
{
	SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
	if (parent != NULL) {
		AddToSubset(parent);
		CenterIn(parent->Frame());
	} else {
		CenterOnScreen();
	}

	_BuildLayout();
	_Populate();
	_UpdateEnabledState();
}


GitHubSyncWindow::~GitHubSyncWindow()
{
	delete fTokenStore;
}


void
GitHubSyncWindow::_BuildLayout()
{
	fOwnerView = new BStringView("owner", "Owner: --");
	fRepositoryView = new BStringView("repository", "Repository: --");
	fTokenStateView = new BStringView("tokenState", "");

	fTokenControl = new BTextControl("token", "Personal access token:", "",
		NULL);
	fTokenControl->SetModificationMessage(new BMessage(kMsgTokenChanged));
	// VERIFY: BTextControl has no secure mode. HideTyping() belongs to the
	// BTextView the control wraps, reached via TextView(). Confirm the accessor
	// name -- older trees expose it only as ChildAt(0).
	BTextView* tokenText = fTokenControl->TextView();
	if (tokenText != NULL)
		tokenText->HideTyping(true);

	fRemoveButton = new BButton("remove", "Remove",
		new BMessage(kMsgSyncTokenRemove));
	fSyncButton = new BButton("sync", "Sync", new BMessage(kMsgSyncStart));
	fDoneButton = new BButton("done", "Done", new BMessage(kMsgSyncClose));

	fResultView = new BTextView("result");
	fResultView->MakeEditable(false);
	fResultView->MakeSelectable(true);
	fResultView->SetWordWrap(true);
	fResultView->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	fResultView->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	fResultView->SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
	fResultView->SetInsets(4.0f, 4.0f, 4.0f, 4.0f);

	BStringView* hint = new BStringView("hint",
		"Requires the repo scope. The token is stored in the system key store, "
		"never in the document.");
	BStringView* coordinateHint = new BStringView("coordinateHint",
		"Set the owner and repository in Project Settings.");

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fOwnerView)
		.Add(fRepositoryView)
		.Add(coordinateHint)
		.AddStrut(B_USE_SMALL_SPACING)
		.Add(fTokenControl)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fTokenStateView)
			.AddGlue()
			.Add(fRemoveButton)
			.End()
		.Add(hint)
		.AddStrut(B_USE_SMALL_SPACING)
		.Add(new BScrollView("resultScroll", fResultView, 0, false, true,
			B_FANCY_BORDER), 1.0f)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.AddGlue()
			.Add(fDoneButton)
			.Add(fSyncButton)
			.End()
		.End();

	fSyncButton->MakeDefault(true);
	fTokenControl->MakeFocus(true);
}


void
GitHubSyncWindow::_Populate()
{
	const std::optional<GitHubIntegration>& integration
		= fModel.integrations.github;

	std::string owner = "Owner: ";
	std::string repository = "Repository: ";
	if (integration.has_value()) {
		owner += integration->owner.empty() ? std::string("--")
			: integration->owner;
		repository += integration->repository.empty() ? std::string("--")
			: integration->repository;
	} else {
		owner += "--";
		repository += "--";
		_SetStatus("No GitHub repository configured.");
	}
	fOwnerView->SetText(owner.c_str());
	fRepositoryView->SetText(repository.c_str());

	std::string account = TokenStore::AccountFor(integration);
	fHasStoredToken = !account.empty() && fTokenStore->HasToken(account);
	fTokenStateView->SetText(fHasStoredToken ? "Token saved" : "No token saved");
}


void
GitHubSyncWindow::_UpdateEnabledState()
{
	const std::optional<GitHubIntegration>& integration
		= fModel.integrations.github;
	bool hasRepository = integration.has_value()
		&& !Trim(integration->owner).empty()
		&& !Trim(integration->repository).empty();
	bool hasToken = fHasStoredToken
		|| !Trim(fTokenControl->Text()).empty();

	fSyncButton->SetEnabled(hasRepository && hasToken && !fIsSyncing);
	fRemoveButton->SetEnabled(fHasStoredToken && !fIsSyncing);
	fTokenControl->SetEnabled(!fIsSyncing);
	fDoneButton->SetEnabled(!fIsSyncing);
}


void
GitHubSyncWindow::_SetStatus(const char* text)
{
	fResultView->SetText(text);
}


// #pragma mark - Token handling

bool
GitHubSyncWindow::_SaveToken()
{
	std::string typed = fTokenControl->Text();
	std::string account
		= TokenStore::AccountFor(fModel.integrations.github);

	if (account.empty()) {
		// No repository means no account to scope a token to, so a typed token
		// cannot be stored anywhere. Say so rather than dropping it silently.
		if (typed.empty())
			return true;
		_SetStatus("Set the owner and repository in Project Settings before "
			"saving a token.");
		return false;
	}

	if (!typed.empty()) {
		if (!fTokenStore->Save(account, typed)) {
			_SetStatus("The token could not be written to the system key "
				"store.");
			return false;
		}
		fHasStoredToken = true;
		fTokenStateView->SetText("Token saved");
	}
	return true;
}


void
GitHubSyncWindow::_RemoveToken()
{
	std::string account
		= TokenStore::AccountFor(fModel.integrations.github);
	if (account.empty())
		return;

	fTokenStore->Remove(account);
	fHasStoredToken = false;
	fTokenControl->SetText("");
	fTokenStateView->SetText("No token saved");
	_UpdateEnabledState();
}


// #pragma mark - Sync

status_t
GitHubSyncWindow::_SyncThread(void* data)
{
	SyncContext* context = static_cast<SyncContext*>(data);

	HttpClient* client = CreateServicesKitHttpClient();
	GitHubSyncService service(context->token, context->integration, client);

	SyncResult result;
	std::string fatalError;
	bool completed = service.Sync(context->issues, result, fatalError);

	BMessage reply(kMsgSyncDone);
	reply.AddBool("completed", completed);
	reply.AddInt32("created", result.created);
	reply.AddInt32("updated", result.updated);
	reply.AddInt32("failed", result.failed);
	for (size_t i = 0; i < result.errors.size(); i++)
		reply.AddString("error", result.errors[i].c_str());
	if (!completed)
		reply.AddString("fatalError", fatalError.c_str());

	if (completed) {
		// Only the issue array crosses back, encoded as a JSON array so the
		// message owns a copy of everything.
		JsonValue array = JsonValue::Array();
		for (size_t i = 0; i < context->issues.size(); i++)
			array.Append(IssuesJsonCoder::EncodeIssue(context->issues[i]));
		reply.AddString("issues", array.Write().c_str());
	}

	context->replyTo.SendMessage(&reply);

	delete client;
	delete context;
	return B_OK;
}


void
GitHubSyncWindow::_StartSync()
{
	if (fIsSyncing)
		return;
	if (!_SaveToken())
		return;
	if (!fModel.integrations.github.has_value())
		return;

	std::string account
		= TokenStore::AccountFor(fModel.integrations.github);
	std::string token;
	if (account.empty() || !fTokenStore->Load(account, token)) {
		_SetStatus("No GitHub token saved for this repository. Enter one and "
			"try again.");
		return;
	}

	SyncContext* context = new SyncContext();
	context->replyTo = BMessenger(this);
	context->token = token;
	context->integration = *fModel.integrations.github;
	context->issues = fModel.issues;

	fWorker = spawn_thread(_SyncThread, "github sync", B_NORMAL_PRIORITY,
		context);
	if (fWorker < B_OK) {
		delete context;
		_SetStatus("The sync thread could not be started.");
		return;
	}

	fIsSyncing = true;
	_UpdateEnabledState();
	_SetStatus("Syncing...");
	resume_thread(fWorker);
}


void
GitHubSyncWindow::_SyncFinished(BMessage* message)
{
	fIsSyncing = false;
	fWorker = -1;

	bool completed = false;
	message->FindBool("completed", &completed);

	std::string summary;
	if (!completed) {
		const char* fatalError = NULL;
		if (message->FindString("fatalError", &fatalError) == B_OK
			&& fatalError != NULL) {
			summary = fatalError;
		} else {
			summary = "The sync did not complete.";
		}
	} else {
		int32 created = 0;
		int32 updated = 0;
		int32 failed = 0;
		message->FindInt32("created", &created);
		message->FindInt32("updated", &updated);
		message->FindInt32("failed", &failed);

		char counts[128];
		snprintf(counts, sizeof(counts),
			"%d created, %d updated, %d failed.\n", (int)created, (int)updated,
			(int)failed);
		summary = counts;

		// Every error is listed, including duplicates -- the Apple sheet keys
		// its list by the error string itself, so identical messages collide and
		// one silently vanishes (its own issue #2).
		const char* error = NULL;
		for (int32 i = 0; message->FindString("error", i, &error) == B_OK; i++) {
			if (error != NULL)
				summary += std::string("\n") + error;
		}

		const char* issuesJson = NULL;
		if (message->FindString("issues", &issuesJson) == B_OK
			&& issuesJson != NULL) {
			BMessage forward(kMsgSyncIssuesUpdated);
			forward.AddString("issues", issuesJson);
			fTarget.SendMessage(&forward);

			// Keep the local snapshot in step so a second sync in the same
			// session pushes the refreshed remote links, not the stale ones.
			JsonValue array;
			std::string parseError;
			if (JsonParser::Parse(issuesJson, array, parseError)
				&& array.IsArray()) {
				std::vector<Issue> issues;
				bool ok = true;
				for (size_t i = 0; i < array.CountItems() && ok; i++) {
					Issue issue;
					IssuesError decodeError;
					ok = IssuesJsonCoder::DecodeIssue(array.ItemAt(i), issue,
						decodeError);
					if (ok)
						issues.push_back(issue);
				}
				if (ok)
					fModel.issues = issues;
			}
		}
	}

	_SetStatus(summary.c_str());
	_UpdateEnabledState();
}


// #pragma mark - Messages

void
GitHubSyncWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSyncStart:
			_StartSync();
			break;

		case kMsgSyncTokenRemove:
			_RemoveToken();
			break;

		case kMsgTokenChanged:
			_UpdateEnabledState();
			break;

		case kMsgSyncDone:
			_SyncFinished(message);
			break;

		case kMsgSyncClose:
			if (_SaveToken())
				PostMessage(B_QUIT_REQUESTED);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
GitHubSyncWindow::QuitRequested()
{
	// Closing mid-sync would leave the worker holding a BMessenger to a dead
	// window. Refuse instead; the sync is short and the buttons are disabled.
	if (fIsSyncing)
		return false;
	return BWindow::QuitRequested();
}

} // namespace ihaveissues
