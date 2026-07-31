/*
 * IHaveIssuesApp.h -- the BApplication.
 *
 * Document-oriented, like the Apple app's DocumentGroup scene: one MainWindow
 * per open .issues file, plus one empty window at launch when Tracker did not
 * hand the app any files. The app quits when its last window closes.
 */
#ifndef IHAVEISSUES_APP_H
#define IHAVEISSUES_APP_H

#include <Application.h>

namespace ihaveissues {

class IHaveIssuesApp : public BApplication {
public:
								IHaveIssuesApp();
	virtual						~IHaveIssuesApp();

	virtual	void				ReadyToRun();
	virtual	void				RefsReceived(BMessage* message);
	virtual	void				ArgvReceived(int32 argc, char** argv);
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				AboutRequested();

private:
			void				_NewWindow();
			bool				_OpenRef(const entry_ref& ref);
			void				_RegisterFileType();

			//! Windows opened so far, counted so the app can quit at zero.
			int32				fWindowCount;
			//! Set once ReadyToRun has decided whether to open a blank document.
			bool				fLaunchHandled;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_APP_H
