/*
 * Application.h -- the AdwApplication and its app-wide actions.
 *
 * One process, many document windows: "activate" opens an empty one, "open"
 * (G_APPLICATION_HANDLES_OPEN) opens one per file handed over by the shell or
 * the command line. That is the GNOME analogue of Haiku's ReadyToRun /
 * RefsReceived pair and of SwiftUI's DocumentGroup.
 *
 * The application ID is the reverse-DNS com.druware.IHaveIssues, matching the
 * .desktop file, the AppStream metainfo and the GResource prefix.
 */
#ifndef IHAVEISSUES_APPLICATION_H
#define IHAVEISSUES_APPLICATION_H

#include <adwaita.h>

namespace ihaveissues {

//! The application ID, used for the bus name, .desktop file and icon name.
extern const char* const kApplicationId;

/*!	Creates the application.

	\return A new reference the caller owns and must unref after
		g_application_run() returns.
*/
AdwApplication* CreateApplication();

} // namespace ihaveissues

#endif // IHAVEISSUES_APPLICATION_H
