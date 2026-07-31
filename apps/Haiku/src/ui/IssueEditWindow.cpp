/*
 * IssueEditWindow.cpp
 */
#include "IssueEditWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>

#include "AppMessages.h"
#include <issueskit/IssueDate.h>
#include <issueskit/IssuesJsonCoder.h>
#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

namespace {

const char* kNoResolutionLabel = "None";

} // unnamed namespace


IssueEditWindow::IssueEditWindow(BWindow* parent, const Issue& issue,
	const BMessenger& target)
	:
	// VERIFY: B_MODAL_SUBSET_WINDOW_FEEL plus AddToSubset() is the Haiku idiom
	// for a dialog modal to one window rather than to the whole app. If the
	// dialog refuses to come forward, B_FLOATING_SUBSET_WINDOW_FEEL is the
	// documented alternative.
	BWindow(BRect(0, 0, 520, 620), "Edit Issue", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS
			| B_NOT_ZOOMABLE),
	fDraft(issue),
	fTarget(target)
{
	SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
	if (parent != NULL) {
		AddToSubset(parent);
		CenterIn(parent->Frame());
	} else {
		CenterOnScreen();
	}

	SetTitle(fDraft.DisplayNumber().c_str());
	_BuildLayout();
	_Populate();
}


IssueEditWindow::~IssueEditWindow()
{
}


// #pragma mark - Layout

BPopUpMenu*
IssueEditWindow::_MakeMenu(const char* name)
{
	BPopUpMenu* menu = new BPopUpMenu(name);
	menu->SetRadioMode(true);
	menu->SetLabelFromMarked(true);
	return menu;
}


BTextView*
IssueEditWindow::_MakeTextArea(const char* name)
{
	BTextView* view = new BTextView(name);
	view->SetWordWrap(true);
	view->SetStylable(false);
	view->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	view->SetLowUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	view->SetHighUIColor(B_DOCUMENT_TEXT_COLOR);
	view->SetInsets(4.0f, 4.0f, 4.0f, 4.0f);
	return view;
}


void
IssueEditWindow::_BuildLayout()
{
	fTitleControl = new BTextControl("title", "Title:", "", NULL);
	fReportedControl = new BTextControl("reported", "Reported:", "", NULL);
	fReportedByControl = new BTextControl("reportedBy", "Reported by:", "",
		NULL);
	fAreaControl = new BTextControl("area", "Area:", "", NULL);
	fMilestoneControl = new BTextControl("milestone", "Milestone:", "", NULL);
	fEstimateControl = new BTextControl("estimate", "Estimate:", "", NULL);
	fLabelsControl = new BTextControl("labels", "Labels:", "", NULL);
	fAssigneesControl = new BTextControl("assignees", "Assignees:", "", NULL);

	BPopUpMenu* typeMenu = _MakeMenu("type");
	for (int i = 0; i < kIssueTypeCount; i++)
		typeMenu->AddItem(new BMenuItem(IssueTypeDisplayName(IssueTypeAt(i)),
			NULL));
	fTypeField = new BMenuField("typeField", "Type:", typeMenu);

	BPopUpMenu* priorityMenu = _MakeMenu("priority");
	for (int i = 0; i < kIssuePriorityCount; i++) {
		priorityMenu->AddItem(new BMenuItem(
			IssuePriorityDisplayName(IssuePriorityAt(i)), NULL));
	}
	fPriorityField = new BMenuField("priorityField", "Priority:", priorityMenu);

	BPopUpMenu* statusMenu = _MakeMenu("status");
	for (int i = 0; i < kIssueStatusCount; i++) {
		statusMenu->AddItem(new BMenuItem(
			IssueStatusDisplayName(IssueStatusAt(i)), NULL));
	}
	fStatusField = new BMenuField("statusField", "Status:", statusMenu);

	// ResolutionKind has no default case, so the picker carries an explicit
	// "None" entry at index 0 that maps back to "unset".
	BPopUpMenu* resolutionMenu = _MakeMenu("resolution");
	resolutionMenu->AddItem(new BMenuItem(kNoResolutionLabel, NULL));
	for (int i = 0; i < kResolutionKindCount; i++) {
		resolutionMenu->AddItem(new BMenuItem(
			ResolutionKindDisplayName(ResolutionKindAt(i)), NULL));
	}
	fResolutionField = new BMenuField("resolutionField", "Resolution:",
		resolutionMenu);

	fDescriptionView = _MakeTextArea("description");
	fStepsView = _MakeTextArea("steps");
	fEnvironmentView = _MakeTextArea("environment");
	fNotesView = _MakeTextArea("notes");
	fResolutionView = _MakeTextArea("resolutionText");

	BTabView* tabs = new BTabView("sections", B_WIDTH_FROM_LABEL);
	tabs->SetExplicitMinSize(BSize(B_SIZE_UNSET, 220.0f));

	BScrollView* descriptionScroll = new BScrollView("descriptionScroll",
		fDescriptionView, 0, false, true, B_FANCY_BORDER);
	BTab* tab = new BTab();
	tabs->AddTab(descriptionScroll, tab);
	tab->SetLabel("Description");

	// A steps tab needs its "one step per line" hint next to the editor.
	BView* stepsPane = new BView("stepsPane", 0);
	stepsPane->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(stepsPane, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_INSETS)
		.Add(new BScrollView("stepsScroll", fStepsView, 0, false, true,
			B_FANCY_BORDER))
		.Add(new BStringView("stepsHint", "One step per line."))
		.End();
	tab = new BTab();
	tabs->AddTab(stepsPane, tab);
	tab->SetLabel("Steps");

	tab = new BTab();
	tabs->AddTab(new BScrollView("environmentScroll", fEnvironmentView, 0,
		false, true, B_FANCY_BORDER), tab);
	tab->SetLabel("Environment");

	tab = new BTab();
	tabs->AddTab(new BScrollView("notesScroll", fNotesView, 0, false, true,
		B_FANCY_BORDER), tab);
	tab->SetLabel("Notes");

	tab = new BTab();
	tabs->AddTab(new BScrollView("resolutionScroll", fResolutionView, 0, false,
		true, B_FANCY_BORDER), tab);
	tab->SetLabel("Resolution");

	fCancelButton = new BButton("cancel", "Cancel", new BMessage(kMsgCancel));
	fSaveButton = new BButton("save", "Save", new BMessage(kMsgSave));

	BStringView* listHint = new BStringView("listHint",
		"Separate multiple labels or assignees with commas.");

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGrid(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
			.AddTextControl(fTitleControl, 0, 0)
			.AddMenuField(fTypeField, 0, 1)
			.AddMenuField(fPriorityField, 0, 2)
			.AddMenuField(fStatusField, 0, 3)
			.AddMenuField(fResolutionField, 0, 4)
			.AddTextControl(fReportedControl, 0, 5)
			.AddTextControl(fReportedByControl, 0, 6)
			.AddTextControl(fAreaControl, 0, 7)
			.AddTextControl(fMilestoneControl, 0, 8)
			.AddTextControl(fEstimateControl, 0, 9)
			.AddTextControl(fLabelsControl, 0, 10)
			.AddTextControl(fAssigneesControl, 0, 11)
			.End()
		.Add(listHint)
		.Add(tabs, 1.0f)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fSaveButton)
			.End()
		.End();

	fSaveButton->MakeDefault(true);
	fTitleControl->MakeFocus(true);
}


// #pragma mark - Populate and save

void
IssueEditWindow::_MarkMenuItem(BMenuField* field, int32 index)
{
	BMenu* menu = field->Menu();
	if (menu == NULL)
		return;
	BMenuItem* item = menu->ItemAt(index);
	if (item != NULL)
		item->SetMarked(true);
}


int32
IssueEditWindow::_MarkedIndex(BMenuField* field) const
{
	BMenu* menu = field->Menu();
	if (menu == NULL)
		return 0;
	BMenuItem* marked = menu->FindMarked();
	if (marked == NULL)
		return 0;
	return menu->IndexOf(marked);
}


void
IssueEditWindow::_SetTextArea(BTextView* view, const std::string& text)
{
	view->SetText(text.c_str());
}


std::string
IssueEditWindow::_TextAreaText(BTextView* view)
{
	const char* text = view->Text();
	return text != NULL ? std::string(text) : std::string();
}


void
IssueEditWindow::_Populate()
{
	fTitleControl->SetText(fDraft.title.c_str());
	_MarkMenuItem(fTypeField, (int32)fDraft.type);
	_MarkMenuItem(fPriorityField, (int32)fDraft.priority);
	_MarkMenuItem(fStatusField, (int32)fDraft.status);
	_MarkMenuItem(fResolutionField, fDraft.resolutionKind.has_value()
		? (int32)*fDraft.resolutionKind + 1 : 0);

	fReportedControl->SetText(IssueDate::ToString(fDraft.reported).c_str());
	fReportedByControl->SetText(fDraft.reportedBy.c_str());
	fAreaControl->SetText(fDraft.area.c_str());
	fMilestoneControl->SetText(fDraft.milestone.has_value()
		? fDraft.milestone->c_str() : "");
	fEstimateControl->SetText(fDraft.estimate.has_value()
		? FormatDouble(*fDraft.estimate).c_str() : "");
	fLabelsControl->SetText(Join(fDraft.labels, ", ").c_str());
	fAssigneesControl->SetText(Join(fDraft.assignees, ", ").c_str());

	_SetTextArea(fDescriptionView, fDraft.description);
	_SetTextArea(fStepsView, Join(fDraft.stepsToReproduce, "\n"));
	_SetTextArea(fEnvironmentView, fDraft.environment);
	_SetTextArea(fNotesView, fDraft.notes);
	_SetTextArea(fResolutionView, fDraft.resolution);
}


void
IssueEditWindow::_Save()
{
	Issue result = fDraft;

	result.title = fTitleControl->Text();
	result.type = IssueTypeAt((int)_MarkedIndex(fTypeField));
	result.priority = IssuePriorityAt((int)_MarkedIndex(fPriorityField));

	IssueStatus previousStatus = result.status;
	result.status = IssueStatusAt((int)_MarkedIndex(fStatusField));

	int32 resolutionIndex = _MarkedIndex(fResolutionField);
	if (resolutionIndex <= 0)
		result.resolutionKind.reset();
	else
		result.resolutionKind = ResolutionKindAt((int)resolutionIndex - 1);

	// An unparsable reported date keeps the previous value rather than silently
	// jumping to today.
	Timestamp reported = 0;
	if (IssueDate::Parse(fReportedControl->Text(), reported))
		result.reported = reported;

	result.reportedBy = fReportedByControl->Text();
	result.area = fAreaControl->Text();

	std::string milestone = Trim(fMilestoneControl->Text());
	if (milestone.empty())
		result.milestone.reset();
	else
		result.milestone = milestone;

	// Invalid numeric input clears the estimate rather than failing the save,
	// matching Double(estimateText) in the Apple editor.
	double estimate = 0.0;
	if (ParseDouble(Trim(fEstimateControl->Text()), estimate))
		result.estimate = estimate;
	else
		result.estimate.reset();

	result.labels = SplitTrimNonEmpty(fLabelsControl->Text(), ',');
	result.assignees = SplitTrimNonEmpty(fAssigneesControl->Text(), ',');

	result.description = _TextAreaText(fDescriptionView);
	result.stepsToReproduce = SplitTrimNonEmpty(_TextAreaText(fStepsView), '\n');
	result.environment = _TextAreaText(fEnvironmentView);
	result.notes = _TextAreaText(fNotesView);
	result.resolution = _TextAreaText(fResolutionView);

	result.updatedAt = IssueDate::Now();

	// FIX relative to the Apple app: closedAt is stamped when the status becomes
	// Resolved and cleared when it moves off Resolved. No Apple code path ever
	// writes that field, which is a bug (its own gap list, item 2).
	bool wasResolved = previousStatus == kIssueStatusResolved;
	bool isResolved = result.status == kIssueStatusResolved;
	if (isResolved && !wasResolved)
		result.closedAt = result.updatedAt;
	else if (!isResolved)
		result.closedAt.reset();

	BMessage message(kMsgIssueEdited);
	message.AddString("issue",
		IssuesJsonCoder::EncodeIssue(result).Write().c_str());
	fTarget.SendMessage(&message);

	PostMessage(B_QUIT_REQUESTED);
}


void
IssueEditWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSave:
			_Save();
			break;

		case kMsgCancel:
			PostMessage(B_QUIT_REQUESTED);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

} // namespace ihaveissues
