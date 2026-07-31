/*
 * IssueDetailView.h -- the read-only detail pane.
 *
 * Shows the same sections, in the same order, as the Apple app's
 * IssueDetailView: the metadata header, then Description, Steps to Reproduce,
 * Environment, Notes / Investigation, Resolution, Comments, Related Issues and
 * Remote Links -- each omitted entirely when empty.
 *
 * DEVIATION from Apple: this is a read-only BTextView with bold runs rather than
 * a GroupBox grid of laid-out labels, and markdown in the body text is shown
 * as-is instead of being rendered. Haiku has no markdown renderer in the API,
 * and a text view is the idiom for a long scrolling read-only document.
 */
#ifndef IHAVEISSUES_ISSUE_DETAIL_VIEW_H
#define IHAVEISSUES_ISSUE_DETAIL_VIEW_H

#include <TextView.h>

#include <issueskit/IssueModel.h>

namespace ihaveissues {

class IssueDetailView : public BTextView {
public:
								IssueDetailView(const char* name);
	virtual						~IssueDetailView();

	//! Shows one issue. \a model resolves relations to their target titles.
			void				SetIssue(const issueskit::Issue& issue,
									const issueskit::IssuesDocumentModel&
										model);

	//! Shows the "nothing selected" placeholder.
			void				Clear();

private:
			void				_Rebuild(const issueskit::Issue& issue,
									const issueskit::IssuesDocumentModel&
										model);
			void				_AppendHeading(const char* text);
			void				_AppendLabelled(const char* label,
									const std::string& value);
			void				_AppendPlain(const std::string& text);
			void				_Apply();

			std::string			fText;
			//! Byte ranges to draw bold: pairs of [start, end).
			std::vector<int32>	fBoldRanges;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_DETAIL_VIEW_H
