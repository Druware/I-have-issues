/*
 * IHaveIssuesApp.cpp
 */
#include "IHaveIssuesApp.h"

#include <Alert.h>
#include <Entry.h>
#include <Mime.h>

#include "AppMessages.h"
#include "MainWindow.h"

namespace ihaveissues {

IHaveIssuesApp::IHaveIssuesApp()
	:
	BApplication(kApplicationSignature),
	fWindowCount(0),
	fLaunchHandled(false)
{
}


IHaveIssuesApp::~IHaveIssuesApp()
{
}


void
IHaveIssuesApp::ReadyToRun()
{
	_RegisterFileType();

	// RefsReceived and ArgvReceived both arrive before ReadyToRun, so a launch
	// that already opened a document must not also open a blank one.
	if (!fLaunchHandled && fWindowCount == 0)
		_NewWindow();
	fLaunchHandled = true;
}


void
IHaveIssuesApp::RefsReceived(BMessage* message)
{
	entry_ref ref;
	for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
		if (_OpenRef(ref))
			fLaunchHandled = true;
	}
}


void
IHaveIssuesApp::ArgvReceived(int32 argc, char** argv)
{
	for (int32 i = 1; i < argc; i++) {
		BEntry entry(argv[i], true);
		entry_ref ref;
		if (entry.InitCheck() == B_OK && entry.Exists()
			&& entry.GetRef(&ref) == B_OK) {
			if (_OpenRef(ref))
				fLaunchHandled = true;
		}
	}
}


void
IHaveIssuesApp::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgNewDocument:
			_NewWindow();
			break;

		case kMsgWindowClosed:
			fWindowCount--;
			if (fWindowCount <= 0)
				PostMessage(B_QUIT_REQUESTED);
			break;

		default:
			BApplication::MessageReceived(message);
			break;
	}
}


void
IHaveIssuesApp::AboutRequested()
{
	BAlert* alert = new BAlert("About I Have Issues",
		"I Have Issues\n\nA document-based issue tracker for small projects.\n"
		"Reads and writes the shared .issues format.",
		"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
	alert->Go();
}


void
IHaveIssuesApp::_NewWindow()
{
	MainWindow* window = new MainWindow();
	fWindowCount++;
	window->Show();
}


bool
IHaveIssuesApp::_OpenRef(const entry_ref& ref)
{
	MainWindow* window = new MainWindow();
	// The window is shown either way: on failure OpenDocument has already told
	// the user why, and an empty document window is a safer outcome than tearing
	// down a BWindow whose looper has not been started yet.
	bool opened = window->OpenDocument(ref);
	fWindowCount++;
	window->Show();
	return opened;
}


void
IHaveIssuesApp::_RegisterFileType()
{
	// Declares the .issues MIME type and points it at this app, so Tracker opens
	// double-clicked documents here. Harmless to repeat on every launch.
	//
	// VERIFY: BMimeType::Install() returns B_FILE_EXISTS when the type is
	// already registered, which is the normal case after the first run.
	BMimeType type(kIssuesMimeType);
	if (type.InitCheck() != B_OK)
		return;

	if (!type.IsInstalled())
		type.Install();

	type.SetShortDescription("Issues Document");
	type.SetLongDescription("I Have Issues tracker document");
	type.SetPreferredApp(kApplicationSignature);

	// The extension -> type mapping Tracker's mimeset uses.
	BMessage extensions;
	extensions.AddString("extensions", "issues");
	type.SetFileExtensions(&extensions);
}

} // namespace ihaveissues


int
main()
{
	ihaveissues::IHaveIssuesApp app;
	app.Run();
	return 0;
}
