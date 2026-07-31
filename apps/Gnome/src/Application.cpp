/*
 * Application.cpp
 */
#include "Application.h"

#include "MainWindow.h"

namespace ihaveissues {

const char* const kApplicationId = "com.druware.IHaveIssues";

namespace {

const char* kResourceBase = "/com/druware/IHaveIssues";
const char* kVersion = "1.0.0";


//! Builds a window, shows it, and hands it back so a caller can load into it.
MainWindow*
NewWindow(AdwApplication* application)
{
	// The MainWindow attaches itself to the AdwApplicationWindow it creates and
	// is destroyed with it, so this pointer is not owned here.
	MainWindow* window = new MainWindow(application);
	gtk_window_present(window->Window());
	return window;
}


void
LoadStyleSheet()
{
	GdkDisplay* display = gdk_display_get_default();
	if (display == NULL)
		return;

	GtkCssProvider* provider = gtk_css_provider_new();

	// VERIFY: gtk_css_provider_load_from_resource() is not among the loaders
	// GTK 4.12 deprecated (load_from_data / _file / _path were, in favour of
	// load_from_string / load_from_bytes). If this build warns, read the
	// resource with g_resources_lookup_data() and use load_from_bytes().
	gchar* path = g_strconcat(kResourceBase, "/style.css", NULL);
	gtk_css_provider_load_from_resource(provider, path);
	g_free(path);

	// VERIFY: gtk_style_context_add_provider_for_display() is the only way to
	// install a display-wide provider in GTK 4. GtkStyleContext itself is
	// deprecated from 4.10, but this function is the documented survivor.
	gtk_style_context_add_provider_for_display(display,
		GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_object_unref(provider);
}


void
OnStartup(GApplication* application, gpointer data)
{
	(void)data;

	LoadStyleSheet();

	GtkApplication* gtkApplication = GTK_APPLICATION(application);

	struct {
		const char*	action;
		const char*	accelerator;
	} accelerators[] = {
		{ "app.new", "<Control>n" },
		{ "app.quit", "<Control>q" },
		{ "win.open", "<Control>o" },
		{ "win.save", "<Control>s" },
		{ "win.save-as", "<Control><Shift>s" },
		{ "win.close", "<Control>w" },
		{ "win.add-issue", "<Control>i" },
		{ "win.edit-issue", "<Control>e" },
		{ "win.project-settings", "<Control>comma" },
		{ "win.github-sync", "<Control><Shift>g" }
	};

	for (size_t i = 0; i < G_N_ELEMENTS(accelerators); i++) {
		// The array is NULL-terminated: one accelerator per action.
		const char* keys[2] = { accelerators[i].accelerator, NULL };
		gtk_application_set_accels_for_action(gtkApplication,
			accelerators[i].action, keys);
	}
}


void
OnActivate(GApplication* application, gpointer data)
{
	(void)data;
	NewWindow(ADW_APPLICATION(application));
}


void
OnOpen(GApplication* application, GFile** files, gint fileCount,
	const gchar* hint, gpointer data)
{
	(void)hint;
	(void)data;

	if (fileCount <= 0) {
		NewWindow(ADW_APPLICATION(application));
		return;
	}

	for (gint i = 0; i < fileCount; i++) {
		MainWindow* window = NewWindow(ADW_APPLICATION(application));
		// The window is shown either way: on failure OpenFile has already told
		// the user why, and an empty document window is a better outcome than
		// no window at all.
		window->OpenFile(files[i]);
	}
}


void
ActionNew(GSimpleAction* action, GVariant* parameter, gpointer data)
{
	(void)action;
	(void)parameter;
	NewWindow(ADW_APPLICATION(data));
}


void
ActionQuit(GSimpleAction* action, GVariant* parameter, gpointer data)
{
	(void)action;
	(void)parameter;

	GtkApplication* application = GTK_APPLICATION(data);

	// Closing each window rather than quitting outright, so every dirty
	// document gets its unsaved-changes prompt. The list is copied first
	// because closing a window mutates it.
	GList* windows = g_list_copy(gtk_application_get_windows(application));
	for (GList* item = windows; item != NULL; item = item->next)
		gtk_window_close(GTK_WINDOW(item->data));
	g_list_free(windows);
}


void
ActionAbout(GSimpleAction* action, GVariant* parameter, gpointer data)
{
	(void)action;
	(void)parameter;

	GtkApplication* application = GTK_APPLICATION(data);
	GtkWindow* parent = gtk_application_get_active_window(application);

	// VERIFY: AdwAboutDialog and its setters are libadwaita 1.5. The older
	// AdwAboutWindow is the 1.2 equivalent if this build predates 1.5.
	AdwAboutDialog* about = ADW_ABOUT_DIALOG(adw_about_dialog_new());
	adw_about_dialog_set_application_name(about, "I Have Issues");
	adw_about_dialog_set_application_icon(about, kApplicationId);
	adw_about_dialog_set_version(about, kVersion);
	adw_about_dialog_set_developer_name(about, "Druware");
	adw_about_dialog_set_comments(about,
		"A document-based issue tracker for small projects. Reads and writes "
		"the shared .issues format.");
	adw_about_dialog_set_license_type(about, GTK_LICENSE_GPL_3_0);
	adw_about_dialog_set_website(about,
		"https://github.com/Druware/I-have-issues");

	adw_dialog_present(ADW_DIALOG(about),
		parent != NULL ? GTK_WIDGET(parent) : NULL);
}

} // unnamed namespace


AdwApplication*
CreateApplication()
{
	AdwApplication* application = ADW_APPLICATION(g_object_new(
		ADW_TYPE_APPLICATION,
		"application-id", kApplicationId,
		"flags", G_APPLICATION_HANDLES_OPEN,
		NULL));

	static const GActionEntry kEntries[] = {
		{ "new", ActionNew, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "quit", ActionQuit, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "about", ActionAbout, NULL, NULL, NULL, { 0, 0, 0 } }
	};
	g_action_map_add_action_entries(G_ACTION_MAP(application), kEntries,
		G_N_ELEMENTS(kEntries), application);

	g_signal_connect(application, "startup", G_CALLBACK(OnStartup), NULL);
	g_signal_connect(application, "activate", G_CALLBACK(OnActivate), NULL);
	g_signal_connect(application, "open", G_CALLBACK(OnOpen), NULL);

	return application;
}

} // namespace ihaveissues
