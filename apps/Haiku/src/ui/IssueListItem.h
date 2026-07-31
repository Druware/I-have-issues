/*
 * IssueListItem.h -- one row in the issue list.
 *
 * The Apple app's IssueRowView: a leading type indicator, the title, the #NNN
 * beneath it, and a trailing priority indicator. Nothing else -- no status, no
 * labels, no assignees.
 *
 * The item carries the issue's uuid, never a pointer or an index into the
 * document: the list is rebuilt from scratch on every model change, and the uuid
 * is the only identity that survives that.
 */
#ifndef IHAVEISSUES_ISSUE_LIST_ITEM_H
#define IHAVEISSUES_ISSUE_LIST_ITEM_H

#include <GraphicsDefs.h>
#include <ListItem.h>
#include <String.h>

#include <issueskit/IssueModel.h>

class BFont;
class BView;

namespace ihaveissues {

class IssueListItem : public BListItem {
public:
								IssueListItem(const issueskit::Issue& issue,
									uint32 outlineLevel);
	virtual						~IssueListItem();

	virtual	void				DrawItem(BView* owner, BRect frame,
									bool complete);
	virtual	void				Update(BView* owner, const BFont* font);

			const BString&		IssueID() const { return fIssueID; }

private:
			BString				fIssueID;
			BString				fTitle;
			BString				fNumber;
			BString				fTypeBadge;
			BString				fPriorityBadge;
			rgb_color			fTypeTint;
			rgb_color			fPriorityTint;
			float				fTitleHeight;
			float				fNumberHeight;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_LIST_ITEM_H
