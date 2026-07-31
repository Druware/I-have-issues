/*
 * IssueListItem.cpp
 */
#include "IssueListItem.h"

#include <Font.h>
#include <InterfaceDefs.h>
#include <View.h>

#include "IssuePresentation.h"

using namespace issueskit;

namespace ihaveissues {

namespace {

const float kBadgeWidth = 20.0f;
const float kSpacing = 6.0f;

} // unnamed namespace


IssueListItem::IssueListItem(const Issue& issue, uint32 outlineLevel)
	:
	BListItem(outlineLevel),
	fIssueID(issue.uuid.c_str()),
	fTitle(issue.title.empty() ? "Untitled" : issue.title.c_str()),
	fNumber(issue.DisplayNumber().c_str()),
	fTypeBadge(IssueTypeBadge(issue.type)),
	fPriorityBadge(IssuePriorityBadge(issue.priority)),
	fTypeTint(IssueTypeTint(issue.type)),
	fPriorityTint(IssuePriorityTint(issue.priority)),
	fTitleHeight(0.0f),
	fNumberHeight(0.0f)
{
}


IssueListItem::~IssueListItem()
{
}


void
IssueListItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);

	// Two stacked text lines: the title in the list font, the number one size
	// smaller. SetHeight is what makes BListView give the row enough room.
	font_height titleHeight;
	font->GetHeight(&titleHeight);
	fTitleHeight = titleHeight.ascent + titleHeight.descent
		+ titleHeight.leading;

	BFont smallFont(font);
	smallFont.SetSize(font->Size() * 0.85f);
	font_height numberHeight;
	smallFont.GetHeight(&numberHeight);
	fNumberHeight = numberHeight.ascent + numberHeight.descent
		+ numberHeight.leading;

	SetHeight(fTitleHeight + fNumberHeight + 6.0f);
}


void
IssueListItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	rgb_color lowColor = owner->LowColor();
	rgb_color textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);

	if (IsSelected() || complete) {
		if (IsSelected()) {
			owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
			textColor = ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);
		} else {
			owner->SetLowColor(owner->ViewColor());
		}
		owner->FillRect(frame, B_SOLID_LOW);
	}

	BFont listFont;
	owner->GetFont(&listFont);
	BFont smallFont(listFont);
	smallFont.SetSize(listFont.Size() * 0.85f);

	font_height titleHeight;
	listFont.GetHeight(&titleHeight);

	float left = frame.left + kSpacing;
	float right = frame.right - kSpacing;

	// Leading type badge.
	owner->SetHighColor(fTypeTint);
	owner->SetFont(&smallFont);
	float typeWidth = smallFont.StringWidth(fTypeBadge.String());
	owner->DrawString(fTypeBadge.String(),
		BPoint(left + (kBadgeWidth - typeWidth) / 2.0f,
			frame.top + 3.0f + titleHeight.ascent));

	// Trailing priority badge.
	owner->SetHighColor(fPriorityTint);
	float priorityWidth = smallFont.StringWidth(fPriorityBadge.String());
	owner->DrawString(fPriorityBadge.String(),
		BPoint(right - priorityWidth,
			frame.top + 3.0f + titleHeight.ascent));

	float textLeft = left + kBadgeWidth + kSpacing;
	float textRight = right - priorityWidth - kSpacing;
	if (textRight < textLeft + 10.0f)
		textRight = textLeft + 10.0f;

	// Title, bold, truncated to the space that is left.
	BFont boldFont(listFont);
	boldFont.SetFace(B_BOLD_FACE);
	owner->SetFont(&boldFont);
	owner->SetHighColor(textColor);

	BString title(fTitle);
	boldFont.TruncateString(&title, B_TRUNCATE_END, textRight - textLeft);
	owner->DrawString(title.String(),
		BPoint(textLeft, frame.top + 3.0f + titleHeight.ascent));

	// "#NNN" underneath, dimmed.
	owner->SetFont(&smallFont);
	if (!IsSelected()) {
		rgb_color dimmed = textColor;
		dimmed.red = (uint8)((dimmed.red + 128) / 2);
		dimmed.green = (uint8)((dimmed.green + 128) / 2);
		dimmed.blue = (uint8)((dimmed.blue + 128) / 2);
		owner->SetHighColor(dimmed);
	}
	font_height numberHeight;
	smallFont.GetHeight(&numberHeight);
	owner->DrawString(fNumber.String(),
		BPoint(textLeft, frame.top + 3.0f + fTitleHeight + numberHeight.ascent));

	owner->SetFont(&listFont);
	owner->SetLowColor(lowColor);
	owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
}

} // namespace ihaveissues
