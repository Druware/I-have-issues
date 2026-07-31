/*
 * main.cpp -- the entry point.
 *
 * Everything else is in Application.cpp; this exists so the process's only
 * top-level statement is the run loop.
 */
#include <adwaita.h>

#include "Application.h"

int
main(int argc, char* argv[])
{
	// AdwApplication calls adw_init() for us during startup, so there is no
	// explicit gtk_init()/adw_init() pair here.
	AdwApplication* application = ihaveissues::CreateApplication();

	int status = g_application_run(G_APPLICATION(application), argc, argv);

	g_object_unref(application);
	return status;
}
