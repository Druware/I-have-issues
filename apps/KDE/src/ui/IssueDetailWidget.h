/*
 * IssueDetailWidget.h -- the read-only detail pane.
 *
 * The KDE counterpart of the Apple IssueDetailView and the Haiku
 * IssueDetailView: same sections, same order, same omit-when-empty rules.
 *
 * It is a QTextBrowser rather than a laid-out grid of labels because a
 * QTextBrowser already gives scrolling, word wrap, text selection and clickable
 * links for nothing, and the pane is read-only by definition -- editing is a
 * separate dialog. Body text is shown verbatim inside a pre-wrap block: markdown
 * is NOT rendered, matching the Haiku port. See the README for why.
 *
 * Relations and remote links are rendered from the values themselves, never from
 * an array offset (the Apple detail view's known issue #3).
 */
#ifndef IHAVEISSUES_ISSUE_DETAIL_WIDGET_H
#define IHAVEISSUES_ISSUE_DETAIL_WIDGET_H

#include <QTextBrowser>

#include <issueskit/IssueModel.h>

namespace ihaveissues
{

class IssueDetailWidget : public QTextBrowser
{
    Q_OBJECT

public:
    explicit IssueDetailWidget(QWidget *parent = nullptr);
    ~IssueDetailWidget() override;

    //! Shows the "no issue selected" placeholder.
    void clearIssue();

    /*! Shows \a issue.
     *
     *  \param model The whole document, so relations can be resolved to titles.
     */
    void setIssue(const issueskit::Issue &issue,
                  const issueskit::IssuesDocumentModel &model);
};

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_DETAIL_WIDGET_H
