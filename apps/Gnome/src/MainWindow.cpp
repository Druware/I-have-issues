/*
 * MainWindow.cpp
 */
#include "MainWindow.h"

#include "GitHubSyncDialog.h"
#include "IssueDetailView.h"
#include "IssueEditDialog.h"
#include "IssueObject.h"
#include "IssuePresentation.h"
#include "ProjectSettingsDialog.h"

#include <issueskit/IssueDate.h>
#include <issueskit/IssuesJsonCoder.h>
#include <issueskit/IssuesMarkdown.h>
#include <issueskit/LegacyMarkdownImporter.h>
#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

namespace {

const char* kUntitled = "Untitled.issues";
const char* kIssuesMimeType = "application/vnd.druware.issues+json";
const char* kMarkdownMimeType = "text/markdown";

//! Which file dialog a completion callback belongs to.
enum FilePurpose {
	kPurposeOpenDocument,
	kPurposeSaveDocument,
	kPurposeExportMarkdown,
	kPurposeImportMarkdown
};


/*!	The context one asynchronous GtkFileDialog call carries.

	Heap allocated per call and deleted by the completion callback, which is
	guaranteed to run exactly once whether the user picked a file or dismissed
	the dialog.
*/
struct FileOp {
	MainWindow*				owner;
	std::shared_ptr<bool>	alive;
	FilePurpose				purpose;
};

} // unnamed namespace


MainWindow::MainWindow(AdwApplication* application)
	:
	fFile(NULL),
	fDirty(false),
	fUpdatingSelection(false),
	fForceClose(false),
	fCloseAfterSave(false),
	fPendingImport(NULL),
	fAlive(new bool(true)),
	fWindow(NULL),
	fSplitView(NULL),
	fSidebarPage(NULL),
	fContentPage(NULL),
	fDetailBin(NULL),
	fOpenStore(NULL),
	fResolvedStore(NULL),
	fOpenList(NULL),
	fResolvedList(NULL)
{
	// The window is created with its application already set, so GtkApplication
	// takes the floating reference and owns it from here on.
	fWindow = GTK_WIDGET(g_object_new(ADW_TYPE_APPLICATION_WINDOW,
		"application", application,
		"default-width", 1000,
		"default-height", 700,
		NULL));

	_BuildUi();
	_InstallActions();
	_RebuildList();
	_UpdateTitles();

	g_signal_connect(fWindow, "close-request", G_CALLBACK(_OnCloseRequest),
		this);

	// This object's life is now the window's.
	g_object_set_data_full(G_OBJECT(fWindow), "ihi-window-owner", this,
		_DestroyOwner);
}


MainWindow::~MainWindow()
{
	*fAlive = false;

	g_clear_object(&fFile);
	g_clear_object(&fPendingImport);
	g_clear_object(&fOpenStore);
	g_clear_object(&fResolvedStore);
}


void
MainWindow::_DestroyOwner(gpointer data)
{
	delete static_cast<MainWindow*>(data);
}


// #pragma mark - Construction

GtkWidget*
MainWindow::_BuildListSection(const char* title, const char* emptyText,
	GListStore** outStore, GtkListBox** outList)
{
	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_top(box, 6);
	gtk_widget_set_margin_bottom(box, 6);
	gtk_widget_set_margin_start(box, 12);
	gtk_widget_set_margin_end(box, 12);

	GtkWidget* heading = gtk_label_new(title);
	gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
	// "heading" is a stock GTK style class -- bold, at the list's own size.
	gtk_widget_add_css_class(heading, "heading");
	gtk_box_append(GTK_BOX(box), heading);

	GtkWidget* list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
	// Single click selects; double click (or Enter) opens the editor, so
	// "row-activated" stays distinct from "row-selected".
	gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), FALSE);
	gtk_widget_add_css_class(list, "boxed-list");

	// Shown by GtkListBox whenever the bound model is empty. The Apple sidebar
	// shows the same grey "No open issues" / "No resolved issues" text.
	GtkWidget* placeholder = gtk_label_new(emptyText);
	gtk_widget_add_css_class(placeholder, "dim-label");
	gtk_widget_set_margin_top(placeholder, 12);
	gtk_widget_set_margin_bottom(placeholder, 12);
	gtk_list_box_set_placeholder(GTK_LIST_BOX(list), placeholder);

	// A GListStore of IhiIssue, so every row's identity is the issue's uuid and
	// nothing anywhere addresses an issue by its position.
	GListStore* store = g_list_store_new(IHI_TYPE_ISSUE);
	gtk_list_box_bind_model(GTK_LIST_BOX(list), G_LIST_MODEL(store), _CreateRow,
		this, NULL);

	g_signal_connect(list, "row-selected", G_CALLBACK(_OnRowSelected), this);
	g_signal_connect(list, "row-activated", G_CALLBACK(_OnRowActivated), this);

	gtk_box_append(GTK_BOX(box), list);

	*outStore = store;
	*outList = GTK_LIST_BOX(list);
	return box;
}


GMenuModel*
MainWindow::_BuildPrimaryMenu()
{
	GMenu* menu = g_menu_new();

	GMenu* fileSection = g_menu_new();
	g_menu_append(fileSection, "_New Window", "app.new");
	g_menu_append(fileSection, "_Open…", "win.open");
	g_menu_append(fileSection, "_Save", "win.save");
	g_menu_append(fileSection, "Save _As…", "win.save-as");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(fileSection));
	g_object_unref(fileSection);

	GMenu* markdownSection = g_menu_new();
	g_menu_append(markdownSection, "_Export as Markdown…",
		"win.export-markdown");
	g_menu_append(markdownSection, "_Import Legacy Markdown…",
		"win.import-markdown");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(markdownSection));
	g_object_unref(markdownSection);

	GMenu* projectSection = g_menu_new();
	g_menu_append(projectSection, "_Project Settings…", "win.project-settings");
	g_menu_append(projectSection, "Sync to _GitHub…", "win.github-sync");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(projectSection));
	g_object_unref(projectSection);

	GMenu* appSection = g_menu_new();
	g_menu_append(appSection, "_About I Have Issues", "app.about");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(appSection));
	g_object_unref(appSection);

	return G_MENU_MODEL(menu);
}


GtkWidget*
MainWindow::_BuildSidebar()
{
	GtkWidget* header = adw_header_bar_new();

	GtkWidget* addButton = gtk_button_new_from_icon_name("list-add-symbolic");
	gtk_widget_set_tooltip_text(addButton, "Add Issue");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(addButton), "win.add-issue");
	adw_header_bar_pack_start(ADW_HEADER_BAR(header), addButton);

	GtkWidget* menuButton = gtk_menu_button_new();
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menuButton),
		"open-menu-symbolic");
	gtk_widget_set_tooltip_text(menuButton, "Main Menu");
	GMenuModel* menu = _BuildPrimaryMenu();
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menuButton), menu);
	// set_menu_model takes its own reference.
	g_object_unref(menu);
	adw_header_bar_pack_end(ADW_HEADER_BAR(header), menuButton);

	GtkWidget* sections = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_append(GTK_BOX(sections),
		_BuildListSection("Open", "No open issues", &fOpenStore, &fOpenList));
	gtk_box_append(GTK_BOX(sections),
		_BuildListSection("Resolved", "No resolved issues", &fResolvedStore,
			&fResolvedList));

	GtkWidget* scroller = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), sections);
	gtk_widget_set_vexpand(scroller, TRUE);

	GtkWidget* toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), scroller);

	return toolbar;
}


GtkWidget*
MainWindow::_BuildContent()
{
	GtkWidget* header = adw_header_bar_new();

	GtkWidget* editButton
		= gtk_button_new_from_icon_name("document-edit-symbolic");
	gtk_widget_set_tooltip_text(editButton, "Edit Issue");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(editButton),
		"win.edit-issue");
	adw_header_bar_pack_start(ADW_HEADER_BAR(header), editButton);

	GtkWidget* deleteButton
		= gtk_button_new_from_icon_name("user-trash-symbolic");
	gtk_widget_set_tooltip_text(deleteButton, "Delete Issue");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(deleteButton),
		"win.delete-issue");
	adw_header_bar_pack_end(ADW_HEADER_BAR(header), deleteButton);

	fDetailBin = ADW_BIN(adw_bin_new());
	gtk_widget_set_vexpand(GTK_WIDGET(fDetailBin), TRUE);

	GtkWidget* toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar),
		GTK_WIDGET(fDetailBin));

	return toolbar;
}


void
MainWindow::_BuildUi()
{
	fSidebarPage = adw_navigation_page_new(_BuildSidebar(), "Issues");
	fContentPage = adw_navigation_page_new(_BuildContent(), "Issue");

	fSplitView
		= ADW_NAVIGATION_SPLIT_VIEW(adw_navigation_split_view_new());
	adw_navigation_split_view_set_sidebar(fSplitView, fSidebarPage);
	adw_navigation_split_view_set_content(fSplitView, fContentPage);
	// The Apple sidebar constrains its column width for the same reason: below
	// roughly 240pt an issue row has no room left for its title.
	adw_navigation_split_view_set_min_sidebar_width(fSplitView, 260);
	adw_navigation_split_view_set_max_sidebar_width(fSplitView, 420);
	adw_navigation_split_view_set_sidebar_width_fraction(fSplitView, 0.32);

	adw_application_window_set_content(ADW_APPLICATION_WINDOW(fWindow),
		GTK_WIDGET(fSplitView));

	// The adaptive rule, and the only one: below this width the split view
	// becomes a single navigable page (sidebar, push to detail). There is no
	// pixel check anywhere else in this app.
	//
	// VERIFY: adw_breakpoint_condition_parse() takes the same syntax the .ui
	// "condition" property does; adw_breakpoint_new() takes ownership of the
	// condition, and adw_application_window_add_breakpoint() takes ownership of
	// the breakpoint. Both are libadwaita 1.4 API.
	AdwBreakpointCondition* condition
		= adw_breakpoint_condition_parse("max-width: 600px");
	AdwBreakpoint* breakpoint = adw_breakpoint_new(condition);

	GValue collapsed = G_VALUE_INIT;
	g_value_init(&collapsed, G_TYPE_BOOLEAN);
	g_value_set_boolean(&collapsed, TRUE);
	adw_breakpoint_add_setter(breakpoint, G_OBJECT(fSplitView), "collapsed",
		&collapsed);
	g_value_unset(&collapsed);

	adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(fWindow),
		breakpoint);
}


void
MainWindow::_InstallActions()
{
	static const GActionEntry kEntries[] = {
		{ "open", _ActionOpen, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "save", _ActionSave, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "save-as", _ActionSaveAs, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "export-markdown", _ActionExportMarkdown, NULL, NULL, NULL,
			{ 0, 0, 0 } },
		{ "import-markdown", _ActionImportMarkdown, NULL, NULL, NULL,
			{ 0, 0, 0 } },
		{ "project-settings", _ActionProjectSettings, NULL, NULL, NULL,
			{ 0, 0, 0 } },
		{ "github-sync", _ActionGitHubSync, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "add-issue", _ActionAddIssue, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "edit-issue", _ActionEditIssue, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "delete-issue", _ActionDeleteIssue, NULL, NULL, NULL, { 0, 0, 0 } },
		{ "close", _ActionCloseWindow, NULL, NULL, NULL, { 0, 0, 0 } }
	};

	// AdwApplicationWindow derives from GtkApplicationWindow, which implements
	// GActionMap, so these become the "win." action group.
	g_action_map_add_action_entries(G_ACTION_MAP(fWindow), kEntries,
		G_N_ELEMENTS(kEntries), this);

	_UpdateActionState();
}


// #pragma mark - List and selection

GtkWidget*
MainWindow::_CreateRow(gpointer item, gpointer data)
{
	(void)data;
	IhiIssue* issue = IHI_ISSUE(item);

	AdwActionRow* row = ADW_ACTION_ROW(adw_action_row_new());

	// The markup is built and escaped in IssuePresentation; AdwPreferencesRow
	// interprets its title as Pango markup by default.
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row),
		ihi_issue_get_title_markup(issue));
	// VERIFY: adw_preferences_row_set_title_lines(row, 1) exists since
	// libadwaita 1.3 and makes a long title ellipsize instead of wrapping.
	adw_preferences_row_set_title_lines(ADW_PREFERENCES_ROW(row), 1);
	adw_action_row_set_subtitle(row, ihi_issue_get_number_text(issue));

	IssueType kind = ihi_issue_get_kind(issue);
	GtkWidget* typeIcon
		= gtk_image_new_from_icon_name(IssueTypeIconName(kind));
	const char* typeClass = IssueTypeStyleClass(kind);
	if (typeClass != NULL)
		gtk_widget_add_css_class(typeIcon, typeClass);
	adw_action_row_add_prefix(row, typeIcon);

	IssuePriority priority = ihi_issue_get_priority(issue);
	GtkWidget* priorityIcon
		= gtk_image_new_from_icon_name(IssuePriorityIconName(priority));
	const char* priorityClass = IssuePriorityStyleClass(priority);
	if (priorityClass != NULL)
		gtk_widget_add_css_class(priorityIcon, priorityClass);
	gtk_widget_set_tooltip_text(priorityIcon,
		IssuePriorityDisplayName(priority));
	adw_action_row_add_suffix(row, priorityIcon);

	// The row carries the uuid, so a selection never has to be translated back
	// through a list position.
	g_object_set_data_full(G_OBJECT(row), "ihi-uuid",
		g_strdup(ihi_issue_get_uuid(issue)), g_free);

	return GTK_WIDGET(row);
}


void
MainWindow::_RebuildList()
{
	// Refilling the stores makes both list boxes emit "row-selected" with NULL.
	// That is not the user deselecting anything, so the handler is muted for
	// the duration and the selection is restored explicitly afterwards.
	fUpdatingSelection = true;

	g_list_store_remove_all(fOpenStore);
	std::vector<Issue> open = fModel.OpenIssues();
	for (size_t i = 0; i < open.size(); i++) {
		IhiIssue* object = ihi_issue_new(open[i]);
		g_list_store_append(fOpenStore, object);
		// append takes its own reference.
		g_object_unref(object);
	}

	g_list_store_remove_all(fResolvedStore);
	std::vector<Issue> resolved = fModel.ResolvedIssues();
	for (size_t i = 0; i < resolved.size(); i++) {
		IhiIssue* object = ihi_issue_new(resolved[i]);
		g_list_store_append(fResolvedStore, object);
		g_object_unref(object);
	}

	// A selected issue that no longer exists (deleted, or replaced wholesale by
	// an import) drops the selection rather than keeping a dangling uuid.
	if (!fSelectedIssueID.empty()
		&& fModel.IssueWithID(fSelectedIssueID) == NULL) {
		fSelectedIssueID.clear();
	}
	_SelectStoredID();

	fUpdatingSelection = false;

	_UpdateDetail();
}


void
MainWindow::_SelectStoredID()
{
	if (fSelectedIssueID.empty()) {
		gtk_list_box_unselect_all(fOpenList);
		gtk_list_box_unselect_all(fResolvedList);
		return;
	}

	GtkListBox* boxes[] = { fOpenList, fResolvedList };
	for (size_t b = 0; b < G_N_ELEMENTS(boxes); b++) {
		for (int i = 0; ; i++) {
			GtkListBoxRow* row = gtk_list_box_get_row_at_index(boxes[b], i);
			if (row == NULL)
				break;
			const char* uuid = (const char*)g_object_get_data(G_OBJECT(row),
				"ihi-uuid");
			if (uuid != NULL && fSelectedIssueID == uuid) {
				gtk_list_box_select_row(boxes[b], row);
				gtk_list_box_unselect_all(boxes[b == 0 ? 1 : 0]);
				return;
			}
		}
	}

	gtk_list_box_unselect_all(fOpenList);
	gtk_list_box_unselect_all(fResolvedList);
}


const Issue*
MainWindow::_SelectedIssue() const
{
	if (fSelectedIssueID.empty())
		return NULL;
	return fModel.IssueWithID(fSelectedIssueID);
}


void
MainWindow::_UpdateDetail()
{
	const Issue* issue = _SelectedIssue();

	if (issue != NULL) {
		adw_bin_set_child(fDetailBin, CreateIssueDetailPage(*issue, fModel));
		adw_navigation_page_set_title(fContentPage,
			issue->DisplayNumber().c_str());
	} else {
		adw_bin_set_child(fDetailBin, CreateNoSelectionPage());
		adw_navigation_page_set_title(fContentPage, "Issue");
	}

	_UpdateActionState();
}


void
MainWindow::_UpdateActionState()
{
	bool hasSelection = _SelectedIssue() != NULL;

	const char* selectionActions[] = { "edit-issue", "delete-issue" };
	for (size_t i = 0; i < G_N_ELEMENTS(selectionActions); i++) {
		GAction* action = g_action_map_lookup_action(G_ACTION_MAP(fWindow),
			selectionActions[i]);
		if (action != NULL) {
			g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
				hasSelection ? TRUE : FALSE);
		}
	}
}


void
MainWindow::_UpdateTitles()
{
	std::string name;
	if (fFile != NULL) {
		gchar* basename = g_file_get_basename(fFile);
		name = basename != NULL ? basename : kUntitled;
		g_free(basename);
	} else {
		std::string projectName = Trim(fModel.project.name);
		name = projectName.empty() ? kUntitled : projectName + ".issues";
	}
	if (fDirty)
		name += " •";
	gtk_window_set_title(GTK_WINDOW(fWindow), name.c_str());

	std::string projectName = Trim(fModel.project.name);
	adw_navigation_page_set_title(fSidebarPage,
		projectName.empty() ? "Issues" : projectName.c_str());
}


// #pragma mark - Issue actions

void
MainWindow::_AddIssue()
{
	Issue issue;
	issue.number = fModel.NextNumber();
	issue.reported = IssueDate::Today();

	MainWindow* self = this;
	std::shared_ptr<bool> alive = fAlive;
	IssueEditDialog* dialog = new IssueEditDialog(issue,
		[self, alive](const Issue& edited) {
			if (*alive)
				self->_ApplyIssue(edited);
		});
	dialog->Present(fWindow);
}


void
MainWindow::_EditIssue()
{
	const Issue* issue = _SelectedIssue();
	if (issue == NULL)
		return;

	MainWindow* self = this;
	std::shared_ptr<bool> alive = fAlive;
	IssueEditDialog* dialog = new IssueEditDialog(*issue,
		[self, alive](const Issue& edited) {
			if (*alive)
				self->_ApplyIssue(edited);
		});
	dialog->Present(fWindow);
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
	_UpdateTitles();
}


void
MainWindow::_DeleteIssue()
{
	const Issue* issue = _SelectedIssue();
	if (issue == NULL)
		return;

	std::string heading = "Delete " + issue->DisplayNumber() + "?";

	AdwAlertDialog* alert = ADW_ALERT_DIALOG(adw_alert_dialog_new(
		heading.c_str(),
		"This removes the issue from the document. This cannot be undone from "
		"here."));
	adw_alert_dialog_add_responses(alert, "cancel", "Cancel", "delete",
		"Delete Issue", NULL);
	adw_alert_dialog_set_response_appearance(alert, "delete",
		ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_default_response(alert, "cancel");
	// LOAD-BEARING, and the same on all three confirmations in this file: the
	// close response is the harmless one. An alert is a child of the window, so
	// destroying the window closes it, which emits "response" with exactly this
	// value -- and every handler below returns early on it. The handlers take a
	// bare `this` rather than the shared alive-flag the async file callbacks
	// use, which is only safe because of that. GObject destroys a window's
	// dialogs in dispose and its qdata (this object) in finalize, so `self` is
	// still valid at that point; it is the early return that keeps the handler
	// from touching half-disposed widgets.
	adw_alert_dialog_set_close_response(alert, "cancel");

	g_signal_connect(alert, "response", G_CALLBACK(_OnDeleteResponse), this);
	adw_dialog_present(ADW_DIALOG(alert), fWindow);
}


void
MainWindow::_OnDeleteResponse(AdwAlertDialog* dialog, const char* response,
	gpointer data)
{
	(void)dialog;
	MainWindow* self = static_cast<MainWindow*>(data);

	if (response == NULL || g_strcmp0(response, "delete") != 0)
		return;

	// The uuid is re-read here rather than captured: the alert is asynchronous,
	// so the selection is only authoritative now.
	std::string uuid = self->fSelectedIssueID;
	if (uuid.empty())
		return;

	// Deleting leaves any relation that pointed here dangling, on purpose: the
	// detail pane shows "Missing issue" and nothing is silently rewritten. That
	// is the Apple and Haiku behaviour; changing it is a product decision.
	for (size_t i = 0; i < self->fModel.issues.size(); i++) {
		if (self->fModel.issues[i].uuid == uuid) {
			self->fModel.issues.erase(self->fModel.issues.begin() + (long)i);
			break;
		}
	}

	self->fSelectedIssueID.clear();
	self->fDirty = true;
	self->_RebuildList();
	self->_UpdateTitles();
}


// #pragma mark - Dialogs

void
MainWindow::_ShowProjectSettings()
{
	MainWindow* self = this;
	std::shared_ptr<bool> alive = fAlive;

	ProjectSettingsDialog* dialog = new ProjectSettingsDialog(fModel,
		[self, alive](const ProjectInfo& project,
			const IntegrationSettings& integrations) {
			if (!*alive)
				return;
			// Only the settings are taken: the issue array stays whatever this
			// window currently holds.
			self->fModel.project = project;
			self->fModel.integrations = integrations;
			self->fDirty = true;
			self->_UpdateTitles();
		});
	dialog->Present(fWindow);
}


void
MainWindow::_ShowGitHubSync()
{
	MainWindow* self = this;
	std::shared_ptr<bool> alive = fAlive;

	GitHubSyncDialog* dialog = new GitHubSyncDialog(fModel,
		[self, alive](const std::vector<Issue>& issues) {
			if (!*alive)
				return;
			self->fModel.issues = issues;
			self->fDirty = true;
			self->_RebuildList();
			self->_UpdateTitles();
		});
	dialog->Present(fWindow);
}


void
MainWindow::_ShowError(const char* heading, const std::string& body)
{
	AdwAlertDialog* alert
		= ADW_ALERT_DIALOG(adw_alert_dialog_new(heading, body.c_str()));
	adw_alert_dialog_add_responses(alert, "ok", "OK", NULL);
	adw_alert_dialog_set_default_response(alert, "ok");
	adw_alert_dialog_set_close_response(alert, "ok");
	adw_dialog_present(ADW_DIALOG(alert), fWindow);
}


// #pragma mark - Files

bool
MainWindow::_ReadFile(GFile* file, std::string& outContents,
	std::string& outError)
{
	gchar* contents = NULL;
	gsize length = 0;
	GError* error = NULL;

	if (!g_file_load_contents(file, NULL, &contents, &length, NULL, &error)) {
		gchar* basename = g_file_get_basename(file);
		outError = IssuesError::FileReadFailed(
			error != NULL && error->message != NULL
				? error->message
				: (basename != NULL ? basename : "")).Message();
		g_free(basename);
		g_clear_error(&error);
		return false;
	}

	outContents.assign(contents != NULL ? contents : "", length);
	g_free(contents);
	return true;
}


bool
MainWindow::_WriteFile(GFile* file, const std::string& contents,
	std::string& outError)
{
	GError* error = NULL;
	gboolean ok = g_file_replace_contents(file, contents.data(),
		contents.size(), NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);

	if (!ok) {
		gchar* basename = g_file_get_basename(file);
		outError = IssuesError::FileWriteFailed(
			error != NULL && error->message != NULL
				? error->message
				: (basename != NULL ? basename : "")).Message();
		g_free(basename);
		g_clear_error(&error);
		return false;
	}
	return true;
}


void
MainWindow::_SetFile(GFile* file)
{
	if (fFile == file)
		return;
	g_clear_object(&fFile);
	fFile = file != NULL ? G_FILE(g_object_ref(file)) : NULL;
}


void
MainWindow::_SetPendingImport(GFile* file)
{
	if (fPendingImport == file)
		return;
	g_clear_object(&fPendingImport);
	fPendingImport = file != NULL ? G_FILE(g_object_ref(file)) : NULL;
}


bool
MainWindow::OpenFile(GFile* file)
{
	std::string contents;
	std::string readError;
	if (!_ReadFile(file, contents, readError)) {
		_ShowError("Open Failed", readError);
		return false;
	}

	IssuesDocumentModel model;
	IssuesError error;
	if (!IssuesJsonCoder::Decode(contents, model, error)) {
		_ShowError("Open Failed", error.Message());
		return false;
	}

	fModel = model;
	_SetFile(file);
	fDirty = false;
	fSelectedIssueID.clear();
	_RebuildList();
	_UpdateTitles();
	return true;
}


bool
MainWindow::IsPristine() const
{
	return fFile == NULL && !fDirty && fModel.issues.empty();
}


bool
MainWindow::_SaveToFile(GFile* file)
{
	std::string json;
	IssuesError error;
	if (!IssuesJsonCoder::Encode(fModel, json, error)) {
		_ShowError("Save Failed", error.Message());
		return false;
	}

	std::string writeError;
	if (!_WriteFile(file, json, writeError)) {
		_ShowError("Save Failed", writeError);
		return false;
	}

	_SetFile(file);
	fDirty = false;
	_UpdateTitles();
	return true;
}


void
MainWindow::_AfterSuccessfulSave()
{
	if (!fCloseAfterSave)
		return;
	fCloseAfterSave = false;
	fForceClose = true;
	gtk_window_destroy(GTK_WINDOW(fWindow));
}


GtkFileDialog*
MainWindow::_MakeFileDialog(const char* title, bool markdown)
{
	// VERIFY: GtkFileDialog is the GTK 4.10 async replacement for
	// GtkFileChooserNative, which is deprecated from 4.10 on. Everything below
	// -- set_filters taking a GListModel of GtkFileFilter, set_initial_name,
	// and the open/save + _finish pairs -- is 4.10 API.
	GtkFileDialog* dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, title);
	gtk_file_dialog_set_modal(dialog, TRUE);

	GtkFileFilter* primary = gtk_file_filter_new();
	if (markdown) {
		gtk_file_filter_set_name(primary, "Markdown");
		gtk_file_filter_add_mime_type(primary, kMarkdownMimeType);
		gtk_file_filter_add_pattern(primary, "*.md");
		gtk_file_filter_add_pattern(primary, "*.markdown");
	} else {
		gtk_file_filter_set_name(primary, "Issues Documents");
		gtk_file_filter_add_mime_type(primary, kIssuesMimeType);
		gtk_file_filter_add_pattern(primary, "*.issues");
	}

	GtkFileFilter* all = gtk_file_filter_new();
	gtk_file_filter_set_name(all, "All Files");
	gtk_file_filter_add_pattern(all, "*");

	GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
	g_list_store_append(filters, primary);
	g_list_store_append(filters, all);
	gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
	gtk_file_dialog_set_default_filter(dialog, primary);

	// The store and both filters are now held by the dialog.
	//
	// VERIFY: GtkFileFilter in GTK 4 derives from GtkFilter, a plain GObject,
	// so gtk_file_filter_new() returns a full reference and these unrefs are
	// correct. In GTK 3 it was a GInitiallyUnowned; if that is somehow still
	// true, these unrefs are harmless but the filters leak.
	g_object_unref(filters);
	g_object_unref(primary);
	g_object_unref(all);

	return dialog;
}


void
MainWindow::_Open()
{
	GtkFileDialog* dialog = _MakeFileDialog("Open Issues Document", false);

	FileOp* op = new FileOp();
	op->owner = this;
	op->alive = fAlive;
	op->purpose = kPurposeOpenDocument;

	// VERIFY: gtk_file_dialog_open()/save() keep their own reference on the
	// dialog for the duration of the asynchronous call, so releasing ours here
	// is correct. The same unref appears after every launch below.
	gtk_file_dialog_open(dialog, GTK_WINDOW(fWindow), NULL,
		_OnFileDialogFinished, op);
	g_object_unref(dialog);
}


void
MainWindow::_Save()
{
	if (fFile != NULL) {
		if (_SaveToFile(fFile))
			_AfterSuccessfulSave();
		return;
	}
	_SaveAs();
}


void
MainWindow::_SaveAs()
{
	GtkFileDialog* dialog = _MakeFileDialog("Save Issues Document", false);

	std::string name;
	if (fFile != NULL) {
		gchar* basename = g_file_get_basename(fFile);
		name = basename != NULL ? basename : kUntitled;
		g_free(basename);
	} else {
		std::string projectName = Trim(fModel.project.name);
		name = projectName.empty() ? kUntitled : projectName + ".issues";
	}
	gtk_file_dialog_set_initial_name(dialog, name.c_str());

	FileOp* op = new FileOp();
	op->owner = this;
	op->alive = fAlive;
	op->purpose = kPurposeSaveDocument;

	gtk_file_dialog_save(dialog, GTK_WINDOW(fWindow), NULL,
		_OnFileDialogFinished, op);
	g_object_unref(dialog);
}


void
MainWindow::_ExportMarkdown()
{
	GtkFileDialog* dialog = _MakeFileDialog("Export as Markdown", true);

	std::string projectName = Trim(fModel.project.name);
	std::string name = (projectName.empty() ? std::string("Issues")
		: projectName) + ".md";
	gtk_file_dialog_set_initial_name(dialog, name.c_str());

	FileOp* op = new FileOp();
	op->owner = this;
	op->alive = fAlive;
	op->purpose = kPurposeExportMarkdown;

	gtk_file_dialog_save(dialog, GTK_WINDOW(fWindow), NULL,
		_OnFileDialogFinished, op);
	g_object_unref(dialog);
}


void
MainWindow::_ImportMarkdown()
{
	GtkFileDialog* dialog = _MakeFileDialog("Import Legacy Markdown", true);

	FileOp* op = new FileOp();
	op->owner = this;
	op->alive = fAlive;
	op->purpose = kPurposeImportMarkdown;

	gtk_file_dialog_open(dialog, GTK_WINDOW(fWindow), NULL,
		_OnFileDialogFinished, op);
	g_object_unref(dialog);
}


void
MainWindow::_OnFileDialogFinished(GObject* source, GAsyncResult* result,
	gpointer data)
{
	FileOp* op = static_cast<FileOp*>(data);
	GtkFileDialog* dialog = GTK_FILE_DIALOG(source);

	GError* error = NULL;
	GFile* file = NULL;
	if (op->purpose == kPurposeSaveDocument
		|| op->purpose == kPurposeExportMarkdown) {
		file = gtk_file_dialog_save_finish(dialog, result, &error);
	} else {
		file = gtk_file_dialog_open_finish(dialog, result, &error);
	}

	// The window may have gone while the dialog was up.
	MainWindow* self = op->alive && *op->alive ? op->owner : NULL;

	if (file == NULL) {
		// VERIFY: a dismissed GtkFileDialog reports GTK_DIALOG_ERROR_DISMISSED
		// (GtkDialogError, GTK 4.10). Cancelling is not a failure and must not
		// raise an alert.
		if (self != NULL && error != NULL
			&& !g_error_matches(error, GTK_DIALOG_ERROR,
				GTK_DIALOG_ERROR_DISMISSED)) {
			self->_ShowError("File Error",
				error->message != NULL ? error->message
					: "The file could not be used.");
		}
		if (self != NULL && op->purpose == kPurposeSaveDocument) {
			// A cancelled Save As cancels the close it was started for.
			self->fCloseAfterSave = false;
		}
		g_clear_error(&error);
		delete op;
		return;
	}
	g_clear_error(&error);

	if (self != NULL) {
		switch (op->purpose) {
			case kPurposeOpenDocument:
				// A pristine window is reused; anything else gets its own.
				if (self->IsPristine()) {
					self->OpenFile(file);
				} else {
					GtkApplication* application = gtk_window_get_application(
						GTK_WINDOW(self->fWindow));
					if (application != NULL) {
						GFile* files[1] = { file };
						g_application_open(G_APPLICATION(application), files, 1,
							"");
					}
				}
				break;

			case kPurposeSaveDocument:
				if (self->_SaveToFile(file))
					self->_AfterSuccessfulSave();
				else
					self->fCloseAfterSave = false;
				break;

			case kPurposeExportMarkdown:
			{
				std::string markdown
					= IssuesMarkdownSerializer::Export(self->fModel);
				std::string writeError;
				if (!self->_WriteFile(file, markdown, writeError))
					self->_ShowError("Export Failed", writeError);
				break;
			}

			case kPurposeImportMarkdown:
				self->_PerformImport(file);
				break;
		}
	}

	g_object_unref(file);
	delete op;
}


void
MainWindow::_PerformImport(GFile* file)
{
	_SetPendingImport(file);

	// The same destructive confirmation the Apple app shows, word for word.
	AdwAlertDialog* alert = ADW_ALERT_DIALOG(adw_alert_dialog_new(
		"Replace this document with the imported markdown?",
		"Every issue and setting in this document is discarded and replaced by "
		"the markdown file."));
	adw_alert_dialog_add_responses(alert, "cancel", "Cancel", "replace",
		"Replace Contents", NULL);
	adw_alert_dialog_set_response_appearance(alert, "replace",
		ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_default_response(alert, "cancel");
	adw_alert_dialog_set_close_response(alert, "cancel");

	g_signal_connect(alert, "response", G_CALLBACK(_OnImportResponse), this);
	adw_dialog_present(ADW_DIALOG(alert), fWindow);
}


void
MainWindow::_OnImportResponse(AdwAlertDialog* dialog, const char* response,
	gpointer data)
{
	(void)dialog;
	MainWindow* self = static_cast<MainWindow*>(data);

	GFile* file = self->fPendingImport;
	if (file == NULL)
		return;
	// Held while this runs, so clearing the member below cannot free it.
	g_object_ref(file);
	self->_SetPendingImport(NULL);

	if (response != NULL && g_strcmp0(response, "replace") == 0) {
		std::string markdown;
		std::string readError;
		if (!self->_ReadFile(file, markdown, readError)) {
			self->_ShowError("Import Failed", readError);
		} else {
			IssuesDocumentModel imported;
			IssuesError error;
			if (!LegacyMarkdownImporter::Import(markdown, imported, error)) {
				self->_ShowError("Import Failed", error.Message());
			} else {
				// Wholesale replacement, never a merge -- and the document
				// keeps whatever file it was already attached to, so the import
				// is not written to disk until the user says so.
				self->fModel = imported;
				self->fSelectedIssueID.clear();
				self->fDirty = true;
				self->_RebuildList();
				self->_UpdateTitles();
			}
		}
	}

	g_object_unref(file);
}


// #pragma mark - Closing

gboolean
MainWindow::_OnCloseRequest(GtkWindow* window, gpointer data)
{
	(void)window;
	MainWindow* self = static_cast<MainWindow*>(data);

	if (self->fForceClose || !self->fDirty) {
		// FALSE: do not stop the default handler, i.e. let the window close.
		return FALSE;
	}

	AdwAlertDialog* alert = ADW_ALERT_DIALOG(adw_alert_dialog_new(
		"Save changes before closing?",
		"This document has unsaved changes."));
	adw_alert_dialog_add_responses(alert, "cancel", "Cancel", "discard",
		"Discard", "save", "Save", NULL);
	adw_alert_dialog_set_response_appearance(alert, "discard",
		ADW_RESPONSE_DESTRUCTIVE);
	adw_alert_dialog_set_response_appearance(alert, "save",
		ADW_RESPONSE_SUGGESTED);
	adw_alert_dialog_set_default_response(alert, "save");
	adw_alert_dialog_set_close_response(alert, "cancel");

	g_signal_connect(alert, "response", G_CALLBACK(_OnCloseResponse), self);
	adw_dialog_present(ADW_DIALOG(alert), self->fWindow);

	// TRUE: block this close. Whichever button the user picks either destroys
	// the window itself or leaves it open.
	return TRUE;
}


void
MainWindow::_OnCloseResponse(AdwAlertDialog* dialog, const char* response,
	gpointer data)
{
	(void)dialog;
	MainWindow* self = static_cast<MainWindow*>(data);

	if (response == NULL || g_strcmp0(response, "cancel") == 0)
		return;

	if (g_strcmp0(response, "discard") == 0) {
		self->fForceClose = true;
		gtk_window_destroy(GTK_WINDOW(self->fWindow));
		return;
	}

	// "save": _Save closes the window once it has actually written, which for
	// an unsaved document means after the Save As sheet comes back.
	self->fCloseAfterSave = true;
	self->_Save();
}


// #pragma mark - List signals

void
MainWindow::_OnRowSelected(GtkListBox* box, GtkListBoxRow* row, gpointer data)
{
	MainWindow* self = static_cast<MainWindow*>(data);
	if (self->fUpdatingSelection)
		return;

	if (row == NULL) {
		// A genuine deselection. Selecting in the other box arrives here too,
		// but only inside the guard set below, so it is never seen.
		self->fSelectedIssueID.clear();
		self->_UpdateDetail();
		return;
	}

	const char* uuid
		= (const char*)g_object_get_data(G_OBJECT(row), "ihi-uuid");
	self->fSelectedIssueID = uuid != NULL ? uuid : "";

	// The two sections are two list boxes, so the other one has to be cleared
	// by hand for the pair to behave as one selection.
	self->fUpdatingSelection = true;
	gtk_list_box_unselect_all(box == self->fOpenList
		? self->fResolvedList : self->fOpenList);
	self->fUpdatingSelection = false;

	self->_UpdateDetail();

	// When the window is narrow the split view is a single page, so selecting
	// has to push the detail page. Harmless when it is not collapsed.
	adw_navigation_split_view_set_show_content(self->fSplitView, TRUE);
}


void
MainWindow::_OnRowActivated(GtkListBox* box, GtkListBoxRow* row, gpointer data)
{
	(void)box;
	(void)row;
	static_cast<MainWindow*>(data)->_EditIssue();
}


// #pragma mark - Actions

void
MainWindow::_ActionOpen(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_Open();
}


void
MainWindow::_ActionSave(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_Save();
}


void
MainWindow::_ActionSaveAs(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_SaveAs();
}


void
MainWindow::_ActionExportMarkdown(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_ExportMarkdown();
}


void
MainWindow::_ActionImportMarkdown(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_ImportMarkdown();
}


void
MainWindow::_ActionProjectSettings(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_ShowProjectSettings();
}


void
MainWindow::_ActionGitHubSync(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_ShowGitHubSync();
}


void
MainWindow::_ActionAddIssue(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_AddIssue();
}


void
MainWindow::_ActionEditIssue(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_EditIssue();
}


void
MainWindow::_ActionDeleteIssue(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	static_cast<MainWindow*>(data)->_DeleteIssue();
}


void
MainWindow::_ActionCloseWindow(GSimpleAction* action, GVariant* parameter,
	gpointer data)
{
	(void)action;
	(void)parameter;
	MainWindow* self = static_cast<MainWindow*>(data);
	// close(), not destroy(): this must go through the unsaved-changes prompt.
	gtk_window_close(GTK_WINDOW(self->fWindow));
}

} // namespace ihaveissues
