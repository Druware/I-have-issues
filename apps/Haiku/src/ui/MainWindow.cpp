/*
 * MainWindow.cpp
 */
#include "MainWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Directory.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <NodeInfo.h>
#include <OutlineListView.h>
#include <ScrollView.h>
#include <SplitView.h>
// VERIFY: Haiku declares BStringItem in <StringItem.h>. On a tree where it is
// still declared in <ListItem.h> (the BeOS layout), swap this include.
#include <StringItem.h>

#include "AppMessages.h"
#include "GitHubSyncWindow.h"
#include "IssueDetailView.h"
#include "IssueEditWindow.h"
#include "IssueListItem.h"
#include <issueskit/IssuesJsonCoder.h>
#include <issueskit/IssuesMarkdown.h>
#include <issueskit/JsonParser.h>
#include <issueskit/LegacyMarkdownImporter.h>
#include "ProjectSettingsWindow.h"
#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

namespace {

const char* kUntitled = "Untitled.issues";
const char* kMarkdownMimeType = "text/markdown";

} // unnamed namespace


MainWindow::MainWindow()
	:
	BWindow(BRect(80, 80, 980, 700), kUntitled, B_DOCUMENT_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	fHasDocumentRef(false),
	fDirty(false),
	fSaveDocumentPanel(NULL),
	fExportPanel(NULL),
	fOpenDocumentPanel(NULL),
	fImportPanel(NULL)
{
	_BuildMenuBar();
	_BuildLayout();
	_BuildFilePanels();
	_RebuildList();
	_UpdateTitle();
}


MainWindow::~MainWindow()
{
	delete fSaveDocumentPanel;
	delete fExportPanel;
	delete fOpenDocumentPanel;
	delete fImportPanel;
}


// #pragma mark - Construction

void
MainWindow::_BuildMenuBar()
{
	fMenuBar = new BMenuBar("menuBar");

	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("New", new BMessage(kMsgNewDocument), 'N'));
	fileMenu->AddItem(new BMenuItem("Open" B_UTF8_ELLIPSIS,
		new BMessage(kMsgOpenDocument), 'O'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Save", new BMessage(kMsgSaveDocument),
		'S'));
	fileMenu->AddItem(new BMenuItem("Save As" B_UTF8_ELLIPSIS,
		new BMessage(kMsgSaveDocumentAs), 'S', B_SHIFT_KEY));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Export as Markdown" B_UTF8_ELLIPSIS,
		new BMessage(kMsgExportMarkdown)));
	fileMenu->AddItem(new BMenuItem("Import Legacy Markdown" B_UTF8_ELLIPSIS,
		new BMessage(kMsgImportMarkdown)));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Close", new BMessage(B_QUIT_REQUESTED),
		'W'));
	fileMenu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED),
		'Q'));
	// Quit targets the application; everything else targets this window.
	fileMenu->ItemAt(fileMenu->CountItems() - 1)->SetTarget(be_app);
	fMenuBar->AddItem(fileMenu);

	// Standard text-editing commands, forwarded to whichever view has focus.
	BMenu* editMenu = new BMenu("Edit");
	editMenu->AddItem(new BMenuItem("Undo", new BMessage(B_UNDO), 'Z'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Cut", new BMessage(B_CUT), 'X'));
	editMenu->AddItem(new BMenuItem("Copy", new BMessage(B_COPY), 'C'));
	editMenu->AddItem(new BMenuItem("Paste", new BMessage(B_PASTE), 'V'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Select All", new BMessage(B_SELECT_ALL),
		'A'));
	fMenuBar->AddItem(editMenu);

	BMenu* projectMenu = new BMenu("Project");
	projectMenu->AddItem(new BMenuItem("Add Issue" B_UTF8_ELLIPSIS,
		new BMessage(kMsgAddIssue), 'I'));
	projectMenu->AddItem(new BMenuItem("Edit Issue" B_UTF8_ELLIPSIS,
		new BMessage(kMsgEditIssue), 'E'));
	projectMenu->AddItem(new BMenuItem("Delete Issue",
		new BMessage(kMsgDeleteIssue)));
	projectMenu->AddSeparatorItem();
	projectMenu->AddItem(new BMenuItem("Project Settings" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowProjectSettings)));
	projectMenu->AddItem(new BMenuItem("Sync to GitHub" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowGitHubSync)));
	fMenuBar->AddItem(projectMenu);
}


void
MainWindow::_BuildLayout()
{
	fListView = new BOutlineListView("issueList", B_SINGLE_SELECTION_LIST);
	fListView->SetSelectionMessage(new BMessage(kMsgIssueSelected));
	fListView->SetInvocationMessage(new BMessage(kMsgIssueInvoked));
	BScrollView* listScroll = new BScrollView("listScroll", fListView, 0, false,
		true, B_FANCY_BORDER);

	fDetailView = new IssueDetailView("issueDetail");
	BScrollView* detailScroll = new BScrollView("detailScroll", fDetailView, 0,
		false, true, B_FANCY_BORDER);

	fAddButton = new BButton("add", "Add", new BMessage(kMsgAddIssue));
	fEditButton = new BButton("edit", "Edit", new BMessage(kMsgEditIssue));
	fDeleteButton = new BButton("delete", "Delete",
		new BMessage(kMsgDeleteIssue));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.AddSplit(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_SMALL_INSETS)
			.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 1.0f)
				.Add(listScroll, 1.0f)
				.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
					.Add(fAddButton)
					.Add(fEditButton)
					.Add(fDeleteButton)
					.AddGlue()
					.End()
				.End()
			.Add(detailScroll, 2.0f)
			.End()
		.End();
}


void
MainWindow::_BuildFilePanels()
{
	BMessenger self(this);

	// VERIFY: BFilePanel COPIES the model message it is given (Haiku's
	// TFilePanel::SetMessage does `fMessage = new BMessage(*message)`), so these
	// stack objects are correct and leak nothing. If a target release adopts the
	// pointer instead, these must become heap allocations.
	BMessage saveMessage(kMsgSavePanelDone);
	saveMessage.AddInt32("purpose", kPurposeSaveDocument);
	fSaveDocumentPanel = new BFilePanel(B_SAVE_PANEL, &self, NULL, 0, false,
		&saveMessage);

	BMessage exportMessage(kMsgSavePanelDone);
	exportMessage.AddInt32("purpose", kPurposeExportMarkdown);
	fExportPanel = new BFilePanel(B_SAVE_PANEL, &self, NULL, 0, false,
		&exportMessage);

	BMessage openMessage(kMsgOpenPanelDone);
	openMessage.AddInt32("purpose", kPurposeOpenDocument);
	fOpenDocumentPanel = new BFilePanel(B_OPEN_PANEL, &self, NULL, B_FILE_NODE,
		false, &openMessage);

	BMessage importMessage(kMsgOpenPanelDone);
	importMessage.AddInt32("purpose", kPurposeImportMarkdown);
	fImportPanel = new BFilePanel(B_OPEN_PANEL, &self, NULL, B_FILE_NODE, false,
		&importMessage);
}


// #pragma mark - List

void
MainWindow::_RebuildList()
{
	fListView->MakeEmpty();

	fOpenGroup = new BStringItem("Open", 0, true);
	fListView->AddItem(fOpenGroup);
	std::vector<Issue> open = fModel.OpenIssues();
	if (open.empty()) {
		BStringItem* empty = new BStringItem("No open issues", 1, true);
		empty->SetEnabled(false);
		fListView->AddUnder(empty, fOpenGroup);
	} else {
		for (size_t i = 0; i < open.size(); i++)
			fListView->AddUnder(new IssueListItem(open[i], 1), fOpenGroup);
	}

	fResolvedGroup = new BStringItem("Resolved", 0, true);
	fListView->AddItem(fResolvedGroup);
	std::vector<Issue> resolved = fModel.ResolvedIssues();
	if (resolved.empty()) {
		BStringItem* empty = new BStringItem("No resolved issues", 1, true);
		empty->SetEnabled(false);
		fListView->AddUnder(empty, fResolvedGroup);
	} else {
		for (size_t i = 0; i < resolved.size(); i++)
			fListView->AddUnder(new IssueListItem(resolved[i], 1),
				fResolvedGroup);
	}

	// Restore the previous selection if that issue still exists.
	if (!fSelectedIssueID.empty()) {
		for (int32 i = 0; i < fListView->FullListCountItems(); i++) {
			IssueListItem* item = dynamic_cast<IssueListItem*>(
				fListView->FullListItemAt(i));
			if (item != NULL
				&& fSelectedIssueID == item->IssueID().String()) {
				fListView->Select(fListView->IndexOf(item));
				break;
			}
		}
	}
	_SelectionChanged();
}


void
MainWindow::_SelectionChanged()
{
	const Issue* issue = _SelectedIssue();
	if (issue != NULL) {
		fSelectedIssueID = issue->uuid;
		fDetailView->SetIssue(*issue, fModel);
	} else {
		fSelectedIssueID.clear();
		fDetailView->Clear();
	}

	bool hasSelection = issue != NULL;
	fEditButton->SetEnabled(hasSelection);
	fDeleteButton->SetEnabled(hasSelection);
}


const Issue*
MainWindow::_SelectedIssue() const
{
	int32 index = fListView->CurrentSelection();
	if (index < 0)
		return NULL;
	IssueListItem* item
		= dynamic_cast<IssueListItem*>(fListView->ItemAt(index));
	if (item == NULL)
		return NULL;
	return fModel.IssueWithID(item->IssueID().String());
}


void
MainWindow::_UpdateTitle()
{
	std::string name;
	if (fHasDocumentRef) {
		name = fDocumentRef.name;
	} else {
		std::string projectName = Trim(fModel.project.name);
		name = projectName.empty() ? kUntitled : projectName + ".issues";
	}
	if (fDirty)
		name += " *";
	SetTitle(name.c_str());
}


// #pragma mark - Issue actions

void
MainWindow::_AddIssue()
{
	Issue issue;
	issue.number = fModel.NextNumber();
	issue.reported = IssueDate::Today();

	IssueEditWindow* editor = new IssueEditWindow(this, issue,
		BMessenger(this));
	editor->Show();
}


void
MainWindow::_EditIssue()
{
	const Issue* issue = _SelectedIssue();
	if (issue == NULL)
		return;

	IssueEditWindow* editor = new IssueEditWindow(this, *issue,
		BMessenger(this));
	editor->Show();
}


void
MainWindow::_DeleteIssue()
{
	const Issue* issue = _SelectedIssue();
	if (issue == NULL)
		return;

	// Copied before the alert runs: `issue` points into fModel.issues, and the
	// uuid is what identifies the row to remove afterwards.
	std::string uuid = issue->uuid;
	std::string title = "Delete " + issue->DisplayNumber() + "?";

	BAlert* alert = new BAlert(title.c_str(),
		"This removes the issue from the document. This cannot be undone from "
		"here.", "Cancel", "Delete Issue", NULL, B_WIDTH_AS_USUAL,
		B_WARNING_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	if (alert->Go() != 1)
		return;

	// Deleting leaves any relation that pointed here dangling, on purpose: the
	// detail view shows "Missing issue" and nothing is silently rewritten. That
	// is the Apple behaviour and changing it would be a product decision.
	for (size_t i = 0; i < fModel.issues.size(); i++) {
		if (fModel.issues[i].uuid == uuid) {
			fModel.issues.erase(fModel.issues.begin() + (long)i);
			break;
		}
	}
	fSelectedIssueID.clear();
	fDirty = true;
	_RebuildList();
	_UpdateTitle();
}


void
MainWindow::_ApplyIssue(const Issue& issue)
{
	bool replaced = false;
	for (size_t i = 0; i < fModel.issues.size(); i++) {
		if (fModel.issues[i].uuid == issue.uuid) {
			fModel.issues[i] = issue;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		fModel.issues.push_back(issue);

	fSelectedIssueID = issue.uuid;
	fDirty = true;
	_RebuildList();
	_UpdateTitle();
}


void
MainWindow::_HandleIssueEdited(BMessage* message)
{
	const char* json = NULL;
	if (message->FindString("issue", &json) != B_OK || json == NULL)
		return;

	JsonValue value;
	std::string parseError;
	if (!JsonParser::Parse(json, value, parseError)) {
		_ShowError("Edit Failed", parseError);
		return;
	}

	Issue issue;
	IssuesError error;
	if (!IssuesJsonCoder::DecodeIssue(value, issue, error)) {
		_ShowError("Edit Failed", error.Message());
		return;
	}
	_ApplyIssue(issue);
}


void
MainWindow::_HandleProjectSettingsSaved(BMessage* message)
{
	const char* json = NULL;
	if (message->FindString("document", &json) != B_OK || json == NULL)
		return;

	IssuesDocumentModel edited;
	IssuesError error;
	if (!IssuesJsonCoder::Decode(json, edited, error)) {
		_ShowError("Project Settings", error.Message());
		return;
	}

	// Only the settings are taken: the issue array stays whatever this window
	// currently holds.
	fModel.project = edited.project;
	fModel.integrations = edited.integrations;
	fDirty = true;
	_UpdateTitle();
}


void
MainWindow::_HandleSyncIssuesUpdated(BMessage* message)
{
	const char* json = NULL;
	if (message->FindString("issues", &json) != B_OK || json == NULL)
		return;

	JsonValue array;
	std::string parseError;
	if (!JsonParser::Parse(json, array, parseError) || !array.IsArray()) {
		_ShowError("Sync to GitHub", parseError.empty()
			? std::string("The sync result could not be read.") : parseError);
		return;
	}

	std::vector<Issue> issues;
	for (size_t i = 0; i < array.CountItems(); i++) {
		Issue issue;
		IssuesError error;
		if (!IssuesJsonCoder::DecodeIssue(array.ItemAt(i), issue, error)) {
			_ShowError("Sync to GitHub", error.Message());
			return;
		}
		issues.push_back(issue);
	}

	fModel.issues = issues;
	fDirty = true;
	_RebuildList();
	_UpdateTitle();
}


// #pragma mark - Dialogs

void
MainWindow::_ShowProjectSettings()
{
	ProjectSettingsWindow* settings = new ProjectSettingsWindow(this, fModel,
		BMessenger(this));
	settings->Show();
}


void
MainWindow::_ShowGitHubSync()
{
	GitHubSyncWindow* sync = new GitHubSyncWindow(this, fModel,
		BMessenger(this));
	sync->Show();
}


void
MainWindow::_ShowError(const char* title, const std::string& message)
{
	BAlert* alert = new BAlert(title, message.c_str(), "OK", NULL, NULL,
		B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	alert->Go();
}


// #pragma mark - Files

bool
MainWindow::_ReadFile(const entry_ref& ref, std::string& outContents)
{
	BFile file(&ref, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;

	off_t size = 0;
	if (file.GetSize(&size) != B_OK || size < 0)
		return false;

	outContents.resize((size_t)size);
	if (size > 0) {
		ssize_t read = file.Read(&outContents[0], (size_t)size);
		if (read != (ssize_t)size)
			return false;
	}
	return true;
}


bool
MainWindow::_WriteFile(const entry_ref& directory, const char* name,
	const std::string& contents, const char* mimeType, entry_ref* outRef)
{
	BDirectory parent(&directory);
	if (parent.InitCheck() != B_OK)
		return false;

	BFile file(&parent, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return false;

	if (!contents.empty()) {
		ssize_t written = file.Write(contents.data(), contents.size());
		if (written != (ssize_t)contents.size())
			return false;
	}

	// Set the file type so Tracker opens .issues files with this app.
	BNodeInfo info(&file);
	if (info.InitCheck() == B_OK)
		info.SetType(mimeType);

	if (outRef != NULL) {
		BEntry entry(&parent, name);
		if (entry.InitCheck() != B_OK || entry.GetRef(outRef) != B_OK)
			return false;
	}
	return true;
}


bool
MainWindow::OpenDocument(const entry_ref& ref)
{
	std::string contents;
	if (!_ReadFile(ref, contents)) {
		_ShowError("Open Failed",
			IssuesError::FileReadFailed(ref.name).Message());
		return false;
	}

	IssuesDocumentModel model;
	IssuesError error;
	if (!IssuesJsonCoder::Decode(contents, model, error)) {
		_ShowError("Open Failed", error.Message());
		return false;
	}

	fModel = model;
	fDocumentRef = ref;
	fHasDocumentRef = true;
	fDirty = false;
	fSelectedIssueID.clear();
	_RebuildList();
	_UpdateTitle();
	return true;
}


bool
MainWindow::IsPristine() const
{
	return !fHasDocumentRef && !fDirty && fModel.issues.empty();
}


bool
MainWindow::_SaveToCurrentRef()
{
	std::string json;
	IssuesError error;
	if (!IssuesJsonCoder::Encode(fModel, json, error)) {
		_ShowError("Save Failed", error.Message());
		return false;
	}

	entry_ref directory;
	BEntry entry(&fDocumentRef);
	BEntry parentEntry;
	if (entry.GetParent(&parentEntry) != B_OK
		|| parentEntry.GetRef(&directory) != B_OK) {
		_ShowError("Save Failed",
			IssuesError::FileWriteFailed(fDocumentRef.name).Message());
		return false;
	}

	if (!_WriteFile(directory, fDocumentRef.name, json, kIssuesMimeType,
			NULL)) {
		_ShowError("Save Failed",
			IssuesError::FileWriteFailed(fDocumentRef.name).Message());
		return false;
	}

	fDirty = false;
	_UpdateTitle();
	return true;
}


void
MainWindow::_Save()
{
	if (fHasDocumentRef)
		_SaveToCurrentRef();
	else
		_SaveAs();
}


void
MainWindow::_SaveAs()
{
	std::string name = fHasDocumentRef ? std::string(fDocumentRef.name)
		: std::string(kUntitled);
	fSaveDocumentPanel->SetSaveText(name.c_str());
	fSaveDocumentPanel->Show();
}


void
MainWindow::_ExportMarkdown()
{
	std::string projectName = Trim(fModel.project.name);
	std::string name = (projectName.empty() ? std::string("Issues")
		: projectName) + ".md";
	fExportPanel->SetSaveText(name.c_str());
	fExportPanel->Show();
}


void
MainWindow::_ImportMarkdown()
{
	fImportPanel->Show();
}


void
MainWindow::_HandleSavePanel(BMessage* message)
{
	entry_ref directory;
	const char* name = NULL;
	if (message->FindRef("directory", &directory) != B_OK
		|| message->FindString("name", &name) != B_OK || name == NULL) {
		return;
	}

	int32 purpose = 0;
	message->FindInt32("purpose", &purpose);

	if (purpose == (int32)kPurposeExportMarkdown) {
		std::string markdown = IssuesMarkdownSerializer::Export(fModel);
		if (!_WriteFile(directory, name, markdown, kMarkdownMimeType, NULL)) {
			_ShowError("Export Failed",
				IssuesError::FileWriteFailed(name).Message());
		}
		return;
	}

	std::string json;
	IssuesError error;
	if (!IssuesJsonCoder::Encode(fModel, json, error)) {
		_ShowError("Save Failed", error.Message());
		return;
	}

	entry_ref savedRef;
	if (!_WriteFile(directory, name, json, kIssuesMimeType, &savedRef)) {
		_ShowError("Save Failed", IssuesError::FileWriteFailed(name).Message());
		return;
	}

	fDocumentRef = savedRef;
	fHasDocumentRef = true;
	fDirty = false;
	_UpdateTitle();
}


void
MainWindow::_HandleOpenPanel(BMessage* message)
{
	entry_ref ref;
	if (message->FindRef("refs", &ref) != B_OK)
		return;

	int32 purpose = 0;
	message->FindInt32("purpose", &purpose);

	if (purpose == (int32)kPurposeImportMarkdown) {
		_PerformImport(ref);
		return;
	}

	// Opening a document reuses this window when it is still pristine, and
	// otherwise asks the application for a new one.
	if (IsPristine()) {
		OpenDocument(ref);
	} else {
		BMessage refsMessage(B_REFS_RECEIVED);
		refsMessage.AddRef("refs", &ref);
		be_app->PostMessage(&refsMessage);
	}
}


void
MainWindow::_PerformImport(const entry_ref& ref)
{
	// The same destructive confirmation the Apple app shows.
	BAlert* alert = new BAlert("Import Legacy Markdown",
		"Replace this document with the imported markdown?\n\nEvery issue and "
		"setting in this document is discarded and replaced by the markdown "
		"file.", "Cancel", "Replace Contents", NULL, B_WIDTH_AS_USUAL,
		B_WARNING_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	if (alert->Go() != 1)
		return;

	std::string markdown;
	if (!_ReadFile(ref, markdown)) {
		_ShowError("Import Failed",
			IssuesError::FileReadFailed(ref.name).Message());
		return;
	}

	IssuesDocumentModel imported;
	IssuesError error;
	if (!LegacyMarkdownImporter::Import(markdown, imported, error)) {
		_ShowError("Import Failed", error.Message());
		return;
	}

	// Wholesale replacement, never a merge -- and the document keeps whatever
	// file it was already attached to, so the import is not saved until the user
	// says so.
	fModel = imported;
	fSelectedIssueID.clear();
	fDirty = true;
	_RebuildList();
	_UpdateTitle();
}


// #pragma mark - Messages

void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgNewDocument:
			be_app->PostMessage(kMsgNewDocument);
			break;

		case kMsgOpenDocument:
			fOpenDocumentPanel->Show();
			break;

		case kMsgSaveDocument:
			_Save();
			break;

		case kMsgSaveDocumentAs:
			_SaveAs();
			break;

		case kMsgExportMarkdown:
			_ExportMarkdown();
			break;

		case kMsgImportMarkdown:
			_ImportMarkdown();
			break;

		case kMsgSavePanelDone:
			_HandleSavePanel(message);
			break;

		case kMsgOpenPanelDone:
			_HandleOpenPanel(message);
			break;

		case kMsgAddIssue:
			_AddIssue();
			break;

		case kMsgEditIssue:
		case kMsgIssueInvoked:
			_EditIssue();
			break;

		case kMsgDeleteIssue:
			_DeleteIssue();
			break;

		case kMsgIssueSelected:
			_SelectionChanged();
			break;

		case kMsgIssueEdited:
			_HandleIssueEdited(message);
			break;

		case kMsgShowProjectSettings:
			_ShowProjectSettings();
			break;

		case kMsgProjectSettingsSaved:
			_HandleProjectSettingsSaved(message);
			break;

		case kMsgShowGitHubSync:
			_ShowGitHubSync();
			break;

		case kMsgSyncIssuesUpdated:
			_HandleSyncIssuesUpdated(message);
			break;

		case B_REFS_RECEIVED:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				OpenDocument(ref);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MainWindow::QuitRequested()
{
	if (fDirty) {
		// VERIFY: BAlert::Go() blocks this window's message thread while the
		// alert runs its own looper. It is the standard Haiku pattern (see
		// StyledEdit), but confirm it does not deadlock against the modal
		// subset windows this window owns.
		BAlert* alert = new BAlert("Unsaved Changes",
			"This document has unsaved changes.", "Cancel", "Don't Save",
			"Save", B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->SetShortcut(0, B_ESCAPE);
		int32 choice = alert->Go();
		if (choice == 0)
			return false;
		if (choice == 2) {
			if (!fHasDocumentRef) {
				// Save As is asynchronous, so the window cannot close now.
				_SaveAs();
				return false;
			}
			if (!_SaveToCurrentRef())
				return false;
		}
	}

	be_app->PostMessage(kMsgWindowClosed);
	return true;
}

} // namespace ihaveissues
