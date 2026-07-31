/*
 * IssueEditDialog.cpp
 */
#include "IssueEditDialog.h"

#include <issueskit/IssueDate.h>
#include <issueskit/IssueEnums.h>
#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

namespace {

//! ResolutionKind has no default case, so the picker carries an explicit entry.
const char* kNoResolutionLabel = "None";

} // unnamed namespace


IssueEditDialog::IssueEditDialog(const Issue& issue, const SaveHandler& onSave)
	:
	fDraft(issue),
	fOnSave(onSave),
	fDialog(NULL),
	fTitleRow(NULL),
	fTypeRow(NULL),
	fPriorityRow(NULL),
	fStatusRow(NULL),
	fResolutionRow(NULL),
	fReportedRow(NULL),
	fCalendar(NULL),
	fReportedByRow(NULL),
	fAreaRow(NULL),
	fMilestoneRow(NULL),
	fEstimateRow(NULL),
	fLabelsRow(NULL),
	fAssigneesRow(NULL),
	fDescriptionView(NULL),
	fStepsView(NULL),
	fEnvironmentView(NULL),
	fNotesView(NULL),
	fResolutionView(NULL)
{
	_BuildUi();
	_Populate();
}


IssueEditDialog::~IssueEditDialog()
{
	// Reached from _DestroyOwner while the AdwDialog is being finalised, so
	// fDialog is already unusable. Nothing to release: every widget is owned by
	// the dialog's widget tree.
}


void
IssueEditDialog::_DestroyOwner(gpointer data)
{
	delete static_cast<IssueEditDialog*>(data);
}


// #pragma mark - Small builders

GtkWidget*
IssueEditDialog::_MakeTextArea(GtkTextView** outView)
{
	GtkWidget* view = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 6);
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 6);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 6);

	GtkWidget* scroller = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroller),
		120);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), view);
	// "card" is a stock libadwaita style class: it gives the scroller the same
	// rounded, bordered look a boxed list has.
	gtk_widget_add_css_class(scroller, "card");

	*outView = GTK_TEXT_VIEW(view);
	return scroller;
}


std::string
IssueEditDialog::_TextAreaText(GtkTextView* view)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(view);
	GtkTextIter start;
	GtkTextIter end;
	gtk_text_buffer_get_bounds(buffer, &start, &end);

	// FALSE: do not include hidden characters. There are none here, but the
	// argument is not optional.
	gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	std::string result = text != NULL ? std::string(text) : std::string();
	g_free(text);
	return result;
}


void
IssueEditDialog::_SetTextArea(GtkTextView* view, const std::string& text)
{
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(view);
	gtk_text_buffer_set_text(buffer, text.c_str(), (int)text.size());
}


AdwComboRow*
IssueEditDialog::_MakeComboRow(const char* title, const char* const* items)
{
	AdwComboRow* row = ADW_COMBO_ROW(adw_combo_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);

	// gtk_string_list_new copies the strings, so the caller's array may be a
	// local. The model is floating; set_model sinks it into the row.
	GtkStringList* model = gtk_string_list_new(items);
	adw_combo_row_set_model(row, G_LIST_MODEL(model));

	return row;
}


std::string
IssueEditDialog::_RowText(gpointer editableRow)
{
	// AdwEntryRow implements GtkEditable, so the text is read through that
	// interface rather than through a GtkEntry it does not derive from.
	const char* text = gtk_editable_get_text(GTK_EDITABLE(editableRow));
	return text != NULL ? std::string(text) : std::string();
}


// #pragma mark - Layout

void
IssueEditDialog::_BuildUi()
{
	AdwPreferencesPage* page
		= ADW_PREFERENCES_PAGE(adw_preferences_page_new());

	// Summary -------------------------------------------------------------
	AdwPreferencesGroup* summary
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());

	fTitleRow = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fTitleRow), "Title");
	adw_preferences_group_add(summary, GTK_WIDGET(fTitleRow));
	adw_preferences_page_add(page, summary);

	// Classification -------------------------------------------------------
	AdwPreferencesGroup* classification
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(classification, "Classification");

	const char* typeItems[kIssueTypeCount + 1];
	for (int i = 0; i < kIssueTypeCount; i++)
		typeItems[i] = IssueTypeDisplayName(IssueTypeAt(i));
	typeItems[kIssueTypeCount] = NULL;
	fTypeRow = _MakeComboRow("Type", typeItems);
	adw_preferences_group_add(classification, GTK_WIDGET(fTypeRow));

	const char* priorityItems[kIssuePriorityCount + 1];
	for (int i = 0; i < kIssuePriorityCount; i++)
		priorityItems[i] = IssuePriorityDisplayName(IssuePriorityAt(i));
	priorityItems[kIssuePriorityCount] = NULL;
	fPriorityRow = _MakeComboRow("Priority", priorityItems);
	adw_preferences_group_add(classification, GTK_WIDGET(fPriorityRow));

	const char* statusItems[kIssueStatusCount + 1];
	for (int i = 0; i < kIssueStatusCount; i++)
		statusItems[i] = IssueStatusDisplayName(IssueStatusAt(i));
	statusItems[kIssueStatusCount] = NULL;
	fStatusRow = _MakeComboRow("Status", statusItems);
	adw_preferences_group_add(classification, GTK_WIDGET(fStatusRow));

	// Index 0 is "None" and maps back to "unset"; every other index is
	// ResolutionKindAt(index - 1).
	const char* resolutionItems[kResolutionKindCount + 2];
	resolutionItems[0] = kNoResolutionLabel;
	for (int i = 0; i < kResolutionKindCount; i++)
		resolutionItems[i + 1] = ResolutionKindDisplayName(ResolutionKindAt(i));
	resolutionItems[kResolutionKindCount + 1] = NULL;
	fResolutionRow = _MakeComboRow("Resolution", resolutionItems);
	adw_preferences_group_add(classification, GTK_WIDGET(fResolutionRow));

	adw_preferences_page_add(page, classification);

	// Details --------------------------------------------------------------
	AdwPreferencesGroup* details
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(details, "Details");
	adw_preferences_group_set_description(details,
		"Reported is a fixed-UTC calendar day, written YYYY-MM-DD.");

	fReportedRow = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fReportedRow),
		"Reported");

	// The date is edited as text -- that is the value actually saved -- with a
	// calendar popover as a convenience. Everything stays fixed-UTC: the
	// calendar is seeded from a UTC GDateTime and only ever writes back the
	// YYYY-MM-DD it displays, so no time zone can shift the day.
	GtkWidget* calendar = gtk_calendar_new();
	fCalendar = GTK_CALENDAR(calendar);
	GtkWidget* popover = gtk_popover_new();
	gtk_popover_set_child(GTK_POPOVER(popover), calendar);

	GtkWidget* calendarButton = gtk_menu_button_new();
	// VERIFY: "x-office-calendar-symbolic" is an Adwaita icon name.
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(calendarButton),
		"x-office-calendar-symbolic");
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(calendarButton), popover);
	gtk_widget_set_valign(calendarButton, GTK_ALIGN_CENTER);
	gtk_widget_add_css_class(calendarButton, "flat");
	adw_entry_row_add_suffix(fReportedRow, calendarButton);

	g_signal_connect(calendar, "day-selected",
		G_CALLBACK(_OnCalendarDaySelected), this);

	adw_preferences_group_add(details, GTK_WIDGET(fReportedRow));

	fReportedByRow = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fReportedByRow),
		"Reported by");
	adw_preferences_group_add(details, GTK_WIDGET(fReportedByRow));

	fAreaRow = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fAreaRow), "Area");
	adw_preferences_group_add(details, GTK_WIDGET(fAreaRow));

	fMilestoneRow = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fMilestoneRow),
		"Milestone");
	adw_preferences_group_add(details, GTK_WIDGET(fMilestoneRow));

	fEstimateRow = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fEstimateRow),
		"Estimate");
	adw_preferences_group_add(details, GTK_WIDGET(fEstimateRow));

	adw_preferences_page_add(page, details);

	// Labels and people ----------------------------------------------------
	AdwPreferencesGroup* people
		= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(people, "Labels & People");
	adw_preferences_group_set_description(people,
		"Separate multiple entries with commas.");

	fLabelsRow = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fLabelsRow), "Labels");
	adw_preferences_group_add(people, GTK_WIDGET(fLabelsRow));

	fAssigneesRow = ADW_ENTRY_ROW(adw_entry_row_new());
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(fAssigneesRow),
		"Assignees");
	adw_preferences_group_add(people, GTK_WIDGET(fAssigneesRow));

	adw_preferences_page_add(page, people);

	// The five long-text sections ------------------------------------------
	struct {
		const char*		title;
		const char*		description;
		GtkTextView**	view;
	} sections[] = {
		{ "Description", NULL, &fDescriptionView },
		{ "Steps to Reproduce", "One step per line.", &fStepsView },
		{ "Environment", NULL, &fEnvironmentView },
		{ "Notes / Investigation", NULL, &fNotesView },
		{ "Resolution", NULL, &fResolutionView }
	};

	for (size_t i = 0; i < G_N_ELEMENTS(sections); i++) {
		AdwPreferencesGroup* group
			= ADW_PREFERENCES_GROUP(adw_preferences_group_new());
		adw_preferences_group_set_title(group, sections[i].title);
		if (sections[i].description != NULL)
			adw_preferences_group_set_description(group, sections[i].description);
		adw_preferences_group_add(group, _MakeTextArea(sections[i].view));
		adw_preferences_page_add(page, group);
	}

	// The dialog shell -----------------------------------------------------
	GtkWidget* header = adw_header_bar_new();
	// The sheet is committed with its own Cancel/Save, so the window controls
	// would be a second, contradictory way to dismiss it.
	adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(header), FALSE);
	adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(header), FALSE);

	GtkWidget* cancelButton = gtk_button_new_with_label("Cancel");
	g_signal_connect(cancelButton, "clicked", G_CALLBACK(_OnCancelClicked),
		this);
	adw_header_bar_pack_start(ADW_HEADER_BAR(header), cancelButton);

	GtkWidget* saveButton = gtk_button_new_with_label("Save");
	gtk_widget_add_css_class(saveButton, "suggested-action");
	g_signal_connect(saveButton, "clicked", G_CALLBACK(_OnSaveClicked), this);
	adw_header_bar_pack_end(ADW_HEADER_BAR(header), saveButton);

	GtkWidget* toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), GTK_WIDGET(page));

	fDialog = ADW_DIALOG(adw_dialog_new());
	adw_dialog_set_title(fDialog, fDraft.DisplayNumber().c_str());
	adw_dialog_set_content_width(fDialog, 560);
	adw_dialog_set_content_height(fDialog, 720);
	adw_dialog_set_child(fDialog, toolbar);

	// This object's life is now the dialog's: the destroy notify runs when the
	// dialog is finalised, which is after it closes.
	g_object_set_data_full(G_OBJECT(fDialog), "ihi-dialog-owner", this,
		_DestroyOwner);
}


// #pragma mark - Populate and save

void
IssueEditDialog::_Populate()
{
	gtk_editable_set_text(GTK_EDITABLE(fTitleRow), fDraft.title.c_str());

	adw_combo_row_set_selected(fTypeRow, (guint)fDraft.type);
	adw_combo_row_set_selected(fPriorityRow, (guint)fDraft.priority);
	adw_combo_row_set_selected(fStatusRow, (guint)fDraft.status);
	adw_combo_row_set_selected(fResolutionRow, fDraft.resolutionKind.has_value()
		? (guint)*fDraft.resolutionKind + 1 : 0);

	// The calendar first: selecting a day emits "day-selected", which writes
	// the same date into the entry row. Setting the entry afterwards would be
	// overwritten; setting it before would be pointless.
	GDateTime* reported = g_date_time_new_from_unix_utc(fDraft.reported);
	if (reported != NULL) {
		gtk_calendar_select_day(fCalendar, reported);
		g_date_time_unref(reported);
	}
	gtk_editable_set_text(GTK_EDITABLE(fReportedRow),
		IssueDate::ToString(fDraft.reported).c_str());

	gtk_editable_set_text(GTK_EDITABLE(fReportedByRow),
		fDraft.reportedBy.c_str());
	gtk_editable_set_text(GTK_EDITABLE(fAreaRow), fDraft.area.c_str());
	gtk_editable_set_text(GTK_EDITABLE(fMilestoneRow),
		fDraft.milestone.has_value() ? fDraft.milestone->c_str() : "");
	gtk_editable_set_text(GTK_EDITABLE(fEstimateRow),
		fDraft.estimate.has_value()
			? FormatDouble(*fDraft.estimate).c_str() : "");
	gtk_editable_set_text(GTK_EDITABLE(fLabelsRow),
		Join(fDraft.labels, ", ").c_str());
	gtk_editable_set_text(GTK_EDITABLE(fAssigneesRow),
		Join(fDraft.assignees, ", ").c_str());

	_SetTextArea(fDescriptionView, fDraft.description);
	_SetTextArea(fStepsView, Join(fDraft.stepsToReproduce, "\n"));
	_SetTextArea(fEnvironmentView, fDraft.environment);
	_SetTextArea(fNotesView, fDraft.notes);
	_SetTextArea(fResolutionView, fDraft.resolution);
}


void
IssueEditDialog::_Save()
{
	Issue result = fDraft;

	result.title = _RowText(fTitleRow);

	guint typeIndex = adw_combo_row_get_selected(fTypeRow);
	if (typeIndex != GTK_INVALID_LIST_POSITION)
		result.type = IssueTypeAt((int)typeIndex);

	guint priorityIndex = adw_combo_row_get_selected(fPriorityRow);
	if (priorityIndex != GTK_INVALID_LIST_POSITION)
		result.priority = IssuePriorityAt((int)priorityIndex);

	IssueStatus previousStatus = result.status;
	guint statusIndex = adw_combo_row_get_selected(fStatusRow);
	if (statusIndex != GTK_INVALID_LIST_POSITION)
		result.status = IssueStatusAt((int)statusIndex);

	guint resolutionIndex = adw_combo_row_get_selected(fResolutionRow);
	if (resolutionIndex == GTK_INVALID_LIST_POSITION || resolutionIndex == 0)
		result.resolutionKind.reset();
	else
		result.resolutionKind = ResolutionKindAt((int)resolutionIndex - 1);

	// An unparsable reported date keeps the previous value rather than silently
	// jumping to today. Parse() is fixed-UTC, so the stored day cannot shift.
	Timestamp reported = 0;
	if (IssueDate::Parse(_RowText(fReportedRow), reported))
		result.reported = reported;

	result.reportedBy = _RowText(fReportedByRow);
	result.area = _RowText(fAreaRow);

	std::string milestone = Trim(_RowText(fMilestoneRow));
	if (milestone.empty())
		result.milestone.reset();
	else
		result.milestone = milestone;

	// Invalid numeric input clears the estimate rather than failing the save,
	// matching Double(estimateText) in the Apple editor.
	double estimate = 0.0;
	if (ParseDouble(Trim(_RowText(fEstimateRow)), estimate))
		result.estimate = estimate;
	else
		result.estimate.reset();

	result.labels = SplitTrimNonEmpty(_RowText(fLabelsRow), ',');
	result.assignees = SplitTrimNonEmpty(_RowText(fAssigneesRow), ',');

	result.description = _TextAreaText(fDescriptionView);
	result.stepsToReproduce = SplitTrimNonEmpty(_TextAreaText(fStepsView), '\n');
	result.environment = _TextAreaText(fEnvironmentView);
	result.notes = _TextAreaText(fNotesView);
	result.resolution = _TextAreaText(fResolutionView);

	result.updatedAt = IssueDate::Now();

	// FIX relative to the Apple app: closedAt is stamped when the status becomes
	// Resolved and cleared when it moves off Resolved. No Apple code path ever
	// writes that field, which is item 2 on its own gap list. Haiku does the
	// same thing.
	bool wasResolved = previousStatus == kIssueStatusResolved;
	bool isResolved = result.status == kIssueStatusResolved;
	if (isResolved && !wasResolved)
		result.closedAt = result.updatedAt;
	else if (!isResolved)
		result.closedAt.reset();

	if (fOnSave)
		fOnSave(result);
}


// #pragma mark - Signals

void
IssueEditDialog::_OnCancelClicked(GtkButton* button, gpointer data)
{
	(void)button;
	IssueEditDialog* self = static_cast<IssueEditDialog*>(data);
	adw_dialog_close(self->fDialog);
}


void
IssueEditDialog::_OnSaveClicked(GtkButton* button, gpointer data)
{
	(void)button;
	IssueEditDialog* self = static_cast<IssueEditDialog*>(data);
	self->_Save();
	adw_dialog_close(self->fDialog);
}


void
IssueEditDialog::_OnCalendarDaySelected(GtkCalendar* calendar, gpointer data)
{
	(void)calendar;
	IssueEditDialog* self = static_cast<IssueEditDialog*>(data);

	// VERIFY: gtk_calendar_get_date() returns a GDateTime the caller owns, in
	// the local time zone. Only its displayed Y/M/D are used, and they are
	// re-parsed through IssueDate::Parse on save, so the stored value is
	// midnight UTC of the day the user actually clicked.
	GDateTime* date = gtk_calendar_get_date(calendar);
	if (date == NULL)
		return;

	gchar* text = g_date_time_format(date, "%Y-%m-%d");
	if (text != NULL) {
		gtk_editable_set_text(GTK_EDITABLE(self->fReportedRow), text);
		g_free(text);
	}
	g_date_time_unref(date);
}


void
IssueEditDialog::Present(GtkWidget* parent)
{
	// VERIFY: adw_dialog_present() takes the dialog's floating reference and
	// parents it to the widget's window. Nothing here unrefs the dialog; it is
	// destroyed when it closes, which is what deletes this object.
	adw_dialog_present(fDialog, parent);
}

} // namespace ihaveissues
