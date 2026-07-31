/*
 * IssueDetailWidget.cpp
 */
#include "IssueDetailWidget.h"

#include <QScrollBar>
#include <QString>
#include <QStringList>

#include <KLocalizedString>

#include <issueskit/IssueDate.h>
#include <issueskit/IssueEnums.h>
#include <issueskit/StringUtils.h>

using issueskit::Comment;
using issueskit::Issue;
using issueskit::IssuesDocumentModel;
using issueskit::Relation;
using issueskit::RemoteLink;

namespace ihaveissues
{

namespace
{

//! Everything that reaches the browser is escaped: issue text is not markup.
QString esc(const std::string &text)
{
    return QString::fromStdString(text).toHtmlEscaped();
}

QString esc(const QString &text)
{
    return text.toHtmlEscaped();
}

QString heading(const QString &text)
{
    return QStringLiteral("<h3 style=\"margin-top:14px; margin-bottom:4px;\">%1</h3>")
        .arg(esc(text));
}

//! One "Label: value" metadata row.
QString metadataRow(const QString &label, const QString &value)
{
    return QStringLiteral("<p style=\"margin:1px 0;\"><b>%1:</b> %2</p>")
        .arg(esc(label), esc(value));
}

/*! A free-text block, shown verbatim.
 *
 *  white-space:pre-wrap is part of Qt's supported CSS subset and is what keeps
 *  the author's line breaks without turning the text into markup.
 */
QString textBlock(const std::string &text)
{
    return QStringLiteral("<div style=\"white-space:pre-wrap; margin:2px 0;\">%1</div>")
        .arg(esc(text));
}

QString titleOrUntitled(const Issue &issue)
{
    return issue.title.empty() ? i18n("Untitled") : QString::fromStdString(issue.title);
}

} // unnamed namespace

IssueDetailWidget::IssueDetailWidget(QWidget *parent)
    : QTextBrowser(parent)
{
    setReadOnly(true);
    setOpenExternalLinks(true);
    setFrameShape(QFrame::StyledPanel);
    clearIssue();
}

IssueDetailWidget::~IssueDetailWidget() = default;

void IssueDetailWidget::clearIssue()
{
    setHtml(QStringLiteral("%1%2")
                .arg(heading(i18n("No Issue Selected")),
                     QStringLiteral("<p>%1</p>")
                         .arg(esc(i18n("Select an issue from the list, or add a new one.")))));
}

void IssueDetailWidget::setIssue(const Issue &issue, const IssuesDocumentModel &model)
{
    QString html;

    // 1. Header: "#NNN  Title", then the metadata rows, each omitted when empty.
    html += QStringLiteral("<h2 style=\"margin-bottom:6px;\">%1&nbsp;&nbsp;%2</h2>")
                .arg(esc(issue.DisplayNumber()), esc(titleOrUntitled(issue)));

    html += metadataRow(i18n("Type"),
                        QString::fromUtf8(issueskit::IssueTypeDisplayName(issue.type)));
    html += metadataRow(i18n("Priority"),
                        QString::fromUtf8(issueskit::IssuePriorityDisplayName(issue.priority)));
    html += metadataRow(i18n("Status"),
                        QString::fromUtf8(issueskit::IssueStatusDisplayName(issue.status)));
    if (issue.resolutionKind.has_value()) {
        html += metadataRow(i18n("Resolution"),
                            QString::fromUtf8(
                                issueskit::ResolutionKindDisplayName(*issue.resolutionKind)));
    }
    html += metadataRow(i18n("Reported"),
                        QString::fromStdString(issueskit::IssueDate::ToString(issue.reported)));
    if (!issue.reportedBy.empty()) {
        html += metadataRow(i18n("Reported by"), QString::fromStdString(issue.reportedBy));
    }
    if (!issue.area.empty()) {
        html += metadataRow(i18n("Area"), QString::fromStdString(issue.area));
    }
    if (!issue.labels.empty()) {
        html += metadataRow(i18n("Labels"),
                            QString::fromStdString(issueskit::Join(issue.labels, ", ")));
    }
    if (!issue.assignees.empty()) {
        html += metadataRow(i18n("Assignees"),
                            QString::fromStdString(issueskit::Join(issue.assignees, ", ")));
    }
    if (issue.milestone.has_value() && !issue.milestone->empty()) {
        html += metadataRow(i18n("Milestone"), QString::fromStdString(*issue.milestone));
    }
    if (issue.estimate.has_value()) {
        html += metadataRow(i18n("Estimate"),
                            QString::fromStdString(issueskit::FormatDouble(*issue.estimate)));
    }

    // 2. Free-text sections.
    if (!issue.description.empty()) {
        html += heading(i18n("Description"));
        html += textBlock(issue.description);
    }

    if (!issue.stepsToReproduce.empty()) {
        html += heading(i18n("Steps to Reproduce"));
        html += QStringLiteral("<ol style=\"margin-top:2px;\">");
        for (size_t i = 0; i < issue.stepsToReproduce.size(); i++) {
            html += QStringLiteral("<li>%1</li>").arg(esc(issue.stepsToReproduce[i]));
        }
        html += QStringLiteral("</ol>");
    }

    if (!issue.environment.empty()) {
        html += heading(i18n("Environment"));
        html += textBlock(issue.environment);
    }

    if (!issue.notes.empty()) {
        html += heading(i18n("Notes / Investigation"));
        html += textBlock(issue.notes);
    }

    if (!issue.resolution.empty()) {
        html += heading(i18n("Resolution"));
        html += textBlock(issue.resolution);
    }

    // 3. Comments.
    if (!issue.comments.empty()) {
        html += heading(i18n("Comments"));
        for (size_t i = 0; i < issue.comments.size(); i++) {
            const Comment &comment = issue.comments[i];
            const QString author = comment.author.empty()
                ? i18n("Unknown")
                : QString::fromStdString(comment.author);
            html += QStringLiteral("<p style=\"margin:6px 0 1px 0;\"><b>%1</b> (%2)</p>")
                        .arg(esc(author),
                             esc(issueskit::IssueDate::ToString(comment.createdAt)));
            html += textBlock(comment.body);
        }
    }

    // 4. Related issues. A relation pointing at a deleted issue is shown
    //    literally as "Missing issue", never repaired and never reported as an
    //    error -- the same silent-dangling behaviour Apple and Haiku have.
    if (!issue.relations.empty()) {
        html += heading(i18n("Related Issues"));
        for (size_t i = 0; i < issue.relations.size(); i++) {
            const Relation &relation = issue.relations[i];
            const Issue *target = model.IssueWithID(relation.issueID);
            const QString description = target != nullptr
                ? QStringLiteral("%1 %2").arg(QString::fromStdString(target->DisplayNumber()),
                                              titleOrUntitled(*target))
                : i18n("Missing issue");
            html += QStringLiteral("<p style=\"margin:1px 0;\">%1: %2</p>")
                        .arg(esc(QString::fromUtf8(
                                 issueskit::RelationKindDisplayName(relation.kind))),
                             esc(description));
        }
    }

    // 5. Remote links.
    if (!issue.remoteLinks.empty()) {
        html += heading(i18n("Remote Links"));
        for (size_t i = 0; i < issue.remoteLinks.size(); i++) {
            const RemoteLink &link = issue.remoteLinks[i];
            QString line = QStringLiteral("%1 %2")
                               .arg(esc(link.provider.DisplayName()),
                                    esc(link.identifier));
            if (link.url.has_value() && !link.url->empty()) {
                const QString url = QString::fromStdString(*link.url);
                line += QStringLiteral("&nbsp;&nbsp;<a href=\"%1\">%1</a>")
                            .arg(esc(url));
            }
            html += QStringLiteral("<p style=\"margin:1px 0;\">%1</p>").arg(line);
        }
    }

    setHtml(html);
    verticalScrollBar()->setValue(0);
}

} // namespace ihaveissues
