/*
 * IssueDetailView.h -- the read-only detail pane.
 *
 * Stateless by design: the pane is rebuilt wholesale on every selection change
 * and dropped into an AdwBin, so there is nothing to keep in sync and no widget
 * that can outlive the issue it was drawn from.
 *
 * Section order matches the Apple app's IssueDetailView exactly -- header
 * metadata, Description, Steps to Reproduce, Environment, Notes / Investigation,
 * Resolution, Comments, Related Issues, Remote Links -- and every section and
 * every metadata row is omitted entirely when empty.
 *
 * Markdown in body text is shown as-is rather than rendered, as on Haiku:
 * GTK has no markdown renderer and the .issues format keeps markdown in these
 * fields purely as the user typed it.
 */
#ifndef IHAVEISSUES_ISSUE_DETAIL_VIEW_H
#define IHAVEISSUES_ISSUE_DETAIL_VIEW_H

#include <adwaita.h>
#include <gtk/gtk.h>

#include <issueskit/IssueModel.h>

namespace ihaveissues {

/*!	Builds the detail pane for \a issue.

	\param model The whole document, so a relation can be resolved to the title
		of the issue it points at.
	\return A new floating AdwPreferencesPage. The caller takes ownership by
		parenting it, normally with adw_bin_set_child().
*/
GtkWidget* CreateIssueDetailPage(const issueskit::Issue& issue,
	const issueskit::IssuesDocumentModel& model);

//! The placeholder shown when nothing is selected. Floating, as above.
GtkWidget* CreateNoSelectionPage();

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_DETAIL_VIEW_H
